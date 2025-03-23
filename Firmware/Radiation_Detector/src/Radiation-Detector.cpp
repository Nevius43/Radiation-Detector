/**
 * @file main.cpp
 * @brief Radiation Monitor with TFT UI, WiFi OTA, and hardware pulse counting on ESP32S3.
 *
 * This file reads pulses from a Geiger counter via the ESP32 PCNT peripheral, displays radiation
 * data on a TFT with LVGL, connects to WiFi with OTA updates, and charts data using fixed intervals.
 * 
 * The PCNT polling is now performed every 100ms with an accumulator so that high‐frequency pulses are less likely to be missed.
 */

/*******************************************************************************
 * Includes
 ******************************************************************************/ 
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "ui.h"           // Declarations for UI objects and functions
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>
#include "dashboard.h"    // Function getDashboardPage()
#include <WebServer.h>
#include <ElegantOTA.h>
#include <ESPmDNS.h>      // Include mDNS support
#include "driver/pcnt.h"  // ESP32 pulse counter driver
#include <ArduinoJson.h>
#include "radiation_data.h" // Export radiation data to other modules

/*******************************************************************************
 * Definitions & Constants
 ******************************************************************************/ 
static const uint16_t SCREEN_WIDTH  = 480;
static const uint16_t SCREEN_HEIGHT = 320;

static const uint8_t GEIGER_PULSE_PIN = 9;   ///< GPIO pin connected to the Geiger counter output
static const uint8_t BUZZER_PIN       = 38;    ///< GPIO pin for LEDC buzzer output

static const float CONVERSION_FACTOR = 153.8f; ///< Factor to convert CPM to µSv/h

// Chart settings:
// Chart1: 20 segments, each representing a 3-minute average (1 hour total)
const int CHART1_SEGMENTS         = 20;
static const int CHART1_INTERVAL_SECONDS = 180; // 3 minutes = 180 s

// Chart3: 24 segments, each representing a 1-hour average (24 hours total)
const int CHART3_SEGMENTS         = 24;
static const int CHART3_INTERVAL_SECONDS = 3600; // 1 hour = 3600 s

// Alarm tone parameters (in Hz and ms)
#define ALARM_FREQ1 1000
#define ALARM_FREQ2 1500
#define ALARM_TONE_DURATION 200   ///< Tone duration in ms
#define ALARM_PAUSE_DURATION 50   ///< Pause between tones in ms

// PCNT parameters – note the PCNT hardware counter is 16-bit
static const pcnt_unit_t PCNT_UNIT = PCNT_UNIT_0;
static const int PULSE_HISTORY_SIZE = 60;   ///< 60 one-second intervals in a ring buffer

// LVGL display buffer and UI chart series
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ SCREEN_WIDTH * SCREEN_HEIGHT / 10 ];

// Ring buffers for chart data
float chart1Buffer[CHART1_SEGMENTS] = {0.0f}; ///< 3‑minute averages for Chart1
float chart3Buffer[CHART3_SEGMENTS] = {0.0f}; ///< 1‑hour averages for Chart3
static int chart1Index = 0;  ///< Write index for Chart1
static int chart3Index = 0;  ///< Write index for Chart3

// Maximum values for dynamic chart scaling
static float chart1MaxValue = 1.0f; ///< Maximum value for Chart1, initialized to 1.0 minimum
static float chart3MaxValue = 1.0f; ///< Maximum value for Chart3, initialized to 1.0 minimum

// Scale factor for chart values (LVGL charts only support integers)
static const int CHART_SCALE_FACTOR = 100; ///< Multiply µSv/h by 100 to preserve 2 decimal places

// Accumulators for chart intervals
static float chart1AccumulatedCpm = 0.0f;
static float chart3AccumulatedCpm = 0.0f;
static float chart1ElapsedSec     = 0.0f;
static float chart3ElapsedSec     = 0.0f;

static lv_chart_series_t* chart1Series = nullptr;
static lv_chart_series_t* chart3Series = nullptr;

// UI objects declared in ui.h
extern lv_obj_t* ui_CurrentRad;
extern lv_obj_t* ui_AverageRad;
extern lv_obj_t* ui_MaximumRad;
extern lv_obj_t* ui_CumulativeRad;
extern lv_obj_t* ui_Chart1;
extern lv_obj_t* ui_Chart3;

extern lv_obj_t* ui_Startup_screen;       ///< Startup screen (shown while connecting)
extern lv_obj_t* ui_InitialScreen; ///< Screen shown after connection attempt
extern lv_obj_t* ui_OnStartup;     ///< Checkbox for auto–connect on startup
extern lv_obj_t* ui_CurrentSpinbox;    ///< Spinbox for current alarm value
extern lv_obj_t* ui_CumulativeSpinbox;  ///< Spinbox for cumulative alarm value
extern lv_obj_t* ui_EnableAlarmsCheckbox; ///< Checkbox to enable/disable alarms
extern lv_obj_t* ui_SSID;
extern lv_obj_t* ui_PASSWORD;
extern lv_obj_t* ui_WIFIINFO;
extern lv_obj_t* ui_Connect;

// Radiation measurement variables
float currentuSvHr      = 0.0f; ///< Instantaneous dose rate (µSv/h)
float averageuSvHr      = 0.0f; ///< Average dose rate (µSv/h)
float maxuSvHr          = 0.0f; ///< Maximum dose rate (µSv/h)
float cumulativemSv     = 0.0f; ///< Cumulative dose (mSv)

unsigned long totalCounts = 0;   ///< Total pulse count (software accumulation)
unsigned long startTime   = 0;     ///< Measurement start time (ms)
unsigned long lastLoop    = 0;     ///< Last loop time (ms)

// For pulse history: we store the number of pulses counted in each 1-second interval
static int pulseHistory[PULSE_HISTORY_SIZE] = {0};
static int pulseHistoryIndex                = 0;
// PCNT hardware returns a 16-bit value
static int16_t lastPCNTCount16 = 0; 
// We now poll the PCNT counter every 100ms
static unsigned long lastPulsePollTime = 0;
// And we accumulate differences over a 1-second period
static int pulseDiffAccumulator = 0;
static unsigned long pulseAccumulationTime = 0;

// LEDC buzzer alarm variables
bool alarmToneToggle = false;
unsigned long lastAlarmMillis = 0;

// New global flag to disable the alarm (set to disabled by default)
bool alarmDisabled = true;

// Preferences for WiFi settings
Preferences preferences;
static String wifi_ip = "";

// OTA support
WebServer server(80);
bool otaInitialized = false;
// Flag to track if user has acknowledged the OTA warning
bool otaWarningAcknowledged = false;

// Add this to the global variables section
static const int SMOOTHING_WINDOW_SIZE = 5;  // Size of moving average window
static float currentReadingsWindow[SMOOTHING_WINDOW_SIZE] = {0};
static int currentReadingIndex = 0;

// Battery monitoring variables
// Disable battery monitoring since the device is USB powered
// const int BATTERY_VOLTAGE_PIN = 34;        // ADC pin connected to the battery voltage divider
// const float BATTERY_LOW_THRESHOLD = 3.3;   // Voltage threshold for low battery warning
// const float BATTERY_CRITICAL_THRESHOLD = 3.0; // Voltage threshold for critical battery warning
// const unsigned long BATTERY_CHECK_INTERVAL = 60000; // Check battery every minute (in milliseconds)
// unsigned long lastBatteryCheckTime = 0;
// bool batteryLowWarningDisplayed = false;
// bool batteryCriticalWarningDisplayed = false;

// Add to global declarations
static const unsigned long DISPLAY_TIMEOUT = 300000;  // 5 minutes in milliseconds
static unsigned long lastUserInteractionTime = 0;
static bool isDisplayDimmed = false;
static uint8_t normalBrightness = 128;  // Adjust this value as needed
static uint8_t dimmedBrightness = 20;   // Adjust this value as needed

/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/ 
void initSerial();
void initTFT();
void initLVGL();
void assignChartSeries();
void clearCharts();
void initPulseCounter();
void initBuzzer();
void playAlarmTone(uint32_t freq);
void stopAlarmTone();
void updatePulseHistory();
int getRealTimeCPM();
void updateRealTimeStats(float cpm, float dtSec);
void updateLabels();
void accumulateCharts(float cpm, float dtSec);
void drawChart1();
void drawChart3();
void updateChartYAxis(lv_obj_t* chart, lv_chart_series_t* series, float maxValue);
static void chart_draw_event_cb(lv_event_t * e);
bool connectToWiFi(const char* ssid, const char* password);
static void connect_btn_event_cb(lv_event_t *e);
void tryAutoConnect();
static void wifi_connect_timer_cb(lv_timer_t * timer);
static void onstartup_checkbox_event_cb(lv_event_t * e);
static void alarms_checkbox_event_cb(lv_event_t * e);
static void spinbox_changed_event_cb(lv_event_t * e);
void saveAlarmSettings();
void loadAlarmSettings();
void checkAlarms();
// void checkBatteryLevel(); // Battery monitoring disabled - USB powered
// Make this static to avoid multiple definition conflicts with dashboard.cpp
static String getRadiationDataJson();
// Export function that can be called from other files
String getRadiationDataJsonExport();
void managePower();
void restoreDisplayBrightness();
static void touch_event_cb(lv_event_t * e);
void setupPowerManagement();
// Function to serve the OTA warning page
void serveOtaWarningPage();

/*******************************************************************************
 * Buzzer & LEDC Alarm Functions
 ******************************************************************************/ 
void initBuzzer() {
    ledcSetup(0, 2000, 8);  // Default frequency 2000 Hz
    ledcAttachPin(BUZZER_PIN, 0);
    ledcWrite(0, 0);
}

void playAlarmTone(uint32_t freq) {
    ledcSetup(0, freq, 8);
    ledcWrite(0, 128);
}

void stopAlarmTone() {
    ledcWrite(0, 0);
}

/*******************************************************************************
 * Alarm Checkbox Event Callback
 ******************************************************************************/ 
static void alarms_checkbox_event_cb(lv_event_t * e) {
    lv_obj_t * checkbox = lv_event_get_target(e);
    alarmDisabled = !lv_obj_has_state(checkbox, LV_STATE_CHECKED);
    
    // Save all alarm settings including the enabled state
    saveAlarmSettings();
    
    Serial.printf("Alarm %s via checkbox\n", alarmDisabled ? "DISABLED" : "ENABLED");
    
    // If alarms are disabled, make sure to stop any active tones
    if (alarmDisabled) {
        stopAlarmTone();
    }
}

/**
 * @brief Callback for when spinbox values change
 * 
 * This handler saves the alarm settings whenever a spinbox value changes.
 */
static void spinbox_changed_event_cb(lv_event_t * e) {
    // Save the updated spinbox values and current alarm state
    saveAlarmSettings();
    Serial.println("Spinbox value changed, alarm settings saved");
}

/*******************************************************************************
 * Alarm Settings Management
 ******************************************************************************/ 
void saveAlarmSettings() {
    preferences.begin("settings", false);
    
    // Save current alarm threshold from spinbox
    int32_t currentValue = lv_spinbox_get_value(ui_CurrentSpinbox);
    preferences.putInt("currentAlarm", currentValue);
    
    // Save cumulative alarm threshold from spinbox
    int32_t cumulativeValue = lv_spinbox_get_value(ui_CumulativeSpinbox);
    preferences.putInt("cumulativeAlarm", cumulativeValue);
    
    // Save alarm enabled state
    preferences.putBool("alarmEnabled", !alarmDisabled);
    
    preferences.end();
    Serial.printf("Alarm settings saved: Current=%.3f µSv/h, Cumulative=%.3f mSv, Alarms %s\n", 
                 (float)currentValue / 1000.0f, 
                 (float)cumulativeValue / 1000.0f, 
                 alarmDisabled ? "DISABLED" : "ENABLED");
}

void loadAlarmSettings() {
    preferences.begin("settings", true);
    
    // Load current alarm threshold - default 5.000 µSv/h (5000 in spinbox format)
    int32_t currentValue = preferences.getInt("currentAlarm", 5000);
    lv_spinbox_set_value(ui_CurrentSpinbox, currentValue);
    
    // Load cumulative alarm threshold - default 1.000 mSv (1000 in spinbox format)
    int32_t cumulativeValue = preferences.getInt("cumulativeAlarm", 1000);
    lv_spinbox_set_value(ui_CumulativeSpinbox, cumulativeValue);
    
    // Load alarm enabled state
    alarmDisabled = !preferences.getBool("alarmEnabled", false); // Default disabled
    
    // Update the checkbox state
    if (!alarmDisabled) {
        lv_obj_add_state(ui_EnableAlarmsCheckbox, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(ui_EnableAlarmsCheckbox, LV_STATE_CHECKED);
    }
    
    preferences.end();
    Serial.printf("Alarm settings loaded: Current=%.3f µSv/h, Cumulative=%.3f mSv, Alarms %s\n", 
                 (float)currentValue / 1000.0f, 
                 (float)cumulativeValue / 1000.0f, 
                 alarmDisabled ? "DISABLED" : "ENABLED");
}

/*******************************************************************************
 * Alarm Checker Function
 ******************************************************************************/ 
void checkAlarms() {
    // Check if alarm is disabled. If so, ensure no tone is played.
    if(alarmDisabled) {
        stopAlarmTone();
        return;
    }

    // Get thresholds from spinboxes 
    // Based on lv_spinbox_set_digit_format(ui_CurrentSpinbox, 4, 3), format is "0.000"
    int32_t currentValueRaw = lv_spinbox_get_value(ui_CurrentSpinbox);
    int32_t cumulativeValueRaw = lv_spinbox_get_value(ui_CumulativeSpinbox);
    
    // Convert from spinbox format to actual values
    // For current alarm (µSv/h): divide by 1000 to get actual value (e.g., 5.000 µSv/h is stored as 5000)
    float currentThreshold = (float)currentValueRaw / 1000.0f;
    
    // For cumulative alarm (mSv): divide by 1000 to get actual value (e.g., 1.000 mSv is stored as 1000)
    float cumulativeThreshold = (float)cumulativeValueRaw / 1000.0f;

    bool condition = (currentuSvHr > currentThreshold) || (cumulativemSv > cumulativeThreshold);
    
    // Debug output - uncomment when troubleshooting alarms
    static unsigned long lastDebugPrint = 0;
    if (millis() - lastDebugPrint > 5000) { // Print every 5 seconds to avoid flooding serial
        Serial.printf("Alarm check: Current %.3f/%.3f µSv/h, Cumulative %.3f/%.3f mSv, Alarm %s\n", 
                     currentuSvHr, currentThreshold, 
                     cumulativemSv, cumulativeThreshold,
                     condition ? "TRIGGERED" : "inactive");
        lastDebugPrint = millis();
    }
    
    unsigned long now = millis();
    if (condition) {
        if (now - lastAlarmMillis >= (ALARM_TONE_DURATION + ALARM_PAUSE_DURATION)) {
            stopAlarmTone();
            playAlarmTone(alarmToneToggle ? ALARM_FREQ1 : ALARM_FREQ2);
            alarmToneToggle = !alarmToneToggle;
            lastAlarmMillis = now;
        }
    } else {
        stopAlarmTone();
    }
}

/*******************************************************************************
 * PCNT (Pulse Counter) Functions
 ******************************************************************************/ 
void initPulseCounter() {
    pcnt_config_t pcnt_config = {};
    pcnt_config.pulse_gpio_num = GEIGER_PULSE_PIN;
    pcnt_config.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    pcnt_config.channel        = PCNT_CHANNEL_0;
    pcnt_config.unit           = PCNT_UNIT;
    pcnt_config.pos_mode       = PCNT_COUNT_INC;
    pcnt_config.neg_mode       = PCNT_COUNT_DIS;
    pcnt_config.lctrl_mode     = PCNT_MODE_KEEP;
    pcnt_config.hctrl_mode     = PCNT_MODE_KEEP;
    pcnt_config.counter_h_lim  = 32767;
    pcnt_config.counter_l_lim  = 0;

    pcnt_unit_config(&pcnt_config);
    // Optionally adjust filter settings: set filter value (in clock cycles) and enable filter.
    pcnt_set_filter_value(PCNT_UNIT, 10);
    pcnt_filter_enable(PCNT_UNIT);

    pcnt_counter_pause(PCNT_UNIT);
    pcnt_counter_clear(PCNT_UNIT);
    pcnt_counter_resume(PCNT_UNIT);

    lastPCNTCount16 = 0;
    lastPulsePollTime = millis();
    pulseAccumulationTime = millis();
    pulseDiffAccumulator = 0;

    Serial.println("PCNT initialized on GEIGER_PULSE_PIN.");
}

/**
 * @brief Polls the PCNT counter every 100 ms and accumulates pulse differences.
 *
 * Every 100ms, the PCNT value is read and the difference (since the last read)
 * is added to an accumulator. Once 1 second has passed, the accumulated difference
 * is stored into a 60-second ring buffer.
 */
void updatePulseHistory() {
    unsigned long now = millis();

    // Poll every 100 ms
    if (now - lastPulsePollTime >= 100) {
        int16_t currentCount16 = 0;
        pcnt_get_counter_value(PCNT_UNIT, &currentCount16);
        int diff = currentCount16 - lastPCNTCount16;
        // Ignore any negative differences (e.g., due to wrap-around or reset)
        if(diff < 0) {
            diff = 0;
        }
        lastPCNTCount16 = currentCount16;
        pulseDiffAccumulator += diff;
        lastPulsePollTime = now;
    }

    // Every 1 second, update the ring buffer with the accumulated difference
    if (now - pulseAccumulationTime >= 1000) {
        pulseHistory[pulseHistoryIndex] = pulseDiffAccumulator;
        pulseHistoryIndex = (pulseHistoryIndex + 1) % PULSE_HISTORY_SIZE;
        totalCounts += pulseDiffAccumulator;
        pulseDiffAccumulator = 0;
        pulseAccumulationTime = now;
    }
}

/**
 * @brief Returns the sum of pulses over the last 60 seconds.
 *
 * @return int Total pulses in the 60-second ring buffer.
 */
int getRealTimeCPM() {
    int sum = 0;
    for (int i = 0; i < PULSE_HISTORY_SIZE; i++) {
        sum += pulseHistory[i];
    }
    return sum;
}

/*******************************************************************************
 * Chart Helper Functions
 ******************************************************************************/ 
/**
 * @brief Clears both chart ring buffers and resets indices/accumulators.
 */
void clearCharts() {
    // Reset all buffers and accumulators
    memset(chart1Buffer, 0, sizeof(chart1Buffer));
    memset(chart3Buffer, 0, sizeof(chart3Buffer));
    
    chart1Index = 0;
    chart3Index = 0;
    chart1AccumulatedCpm = 0.0f;
    chart3AccumulatedCpm = 0.0f;
    chart1ElapsedSec = 0.0f;
    chart3ElapsedSec = 0.0f;
    
    // Reset maximum values for dynamic scaling
    chart1MaxValue = 1.0f;
    chart3MaxValue = 1.0f;
    
    // Clear LVGL chart objects if they exist
    if (chart1Series && ui_Chart1) {
        lv_chart_set_all_value(ui_Chart1, chart1Series, 0);
        lv_chart_refresh(ui_Chart1);
    }
    
    if (chart3Series && ui_Chart3) {
        lv_chart_set_all_value(ui_Chart3, chart3Series, 0);
        lv_chart_refresh(ui_Chart3);
    }
}

/**
 * @brief Accumulates CPM values for chart intervals.
 *
 * Called every loop() after real-time CPM is computed. When a chart interval
 * (3 minutes for Chart1 or 1 hour for Chart3) is reached, the average is computed,
 * stored in the corresponding ring buffer, and the chart is redrawn.
 *
 * @param cpm   Current counts per minute.
 * @param dtSec Time delta in seconds.
 */
void accumulateCharts(float cpm, float dtSec) {
    // Accumulate for Chart1 (1-hour chart with 3-minute intervals)
    chart1AccumulatedCpm += (cpm * dtSec);
    chart1ElapsedSec += dtSec;
    
    if (chart1ElapsedSec >= CHART1_INTERVAL_SECONDS) {
        // Compute average CPM for this interval
        float avgCpm = chart1AccumulatedCpm / chart1ElapsedSec;
        float value = avgCpm / CONVERSION_FACTOR; // Convert to µSv/h
        
        // Store in buffer and update chart
        if (chart1Index < CHART1_SEGMENTS) {
            // Still filling the buffer with initial values
            chart1Buffer[chart1Index] = value;
            chart1Index++;
        } else {
            // Buffer is full, shift values left and add new value at the end
            memmove(chart1Buffer, chart1Buffer + 1, (CHART1_SEGMENTS - 1) * sizeof(float));
            chart1Buffer[CHART1_SEGMENTS - 1] = value;
        }
        
        // Update max value for dynamic scaling
        chart1MaxValue = 1.0f; // Reset to minimum
        for (int i = 0; i < chart1Index; i++) {
            if (chart1Buffer[i] > chart1MaxValue) {
                chart1MaxValue = chart1Buffer[i];
            }
        }
        
        // Add 20% headroom to max value and round to next whole number
        chart1MaxValue = ceil(chart1MaxValue * 1.2f);
        if (chart1MaxValue < 1.0f) chart1MaxValue = 1.0f;
        
        // Draw updated chart
        drawChart1();
        
        // Reset accumulators for next interval
        chart1AccumulatedCpm = 0.0f;
        chart1ElapsedSec = 0.0f;
    }
    
    // Accumulate for Chart3 (24-hour chart with 1-hour intervals)
    chart3AccumulatedCpm += (cpm * dtSec);
    chart3ElapsedSec += dtSec;
    
    if (chart3ElapsedSec >= CHART3_INTERVAL_SECONDS) {
        // Compute average CPM for this interval
        float avgCpm = chart3AccumulatedCpm / chart3ElapsedSec;
        float value = avgCpm / CONVERSION_FACTOR; // Convert to µSv/h
        
        // Store in buffer and update chart
        if (chart3Index < CHART3_SEGMENTS) {
            // Still filling the buffer with initial values
            chart3Buffer[chart3Index] = value;
            chart3Index++;
        } else {
            // Buffer is full, shift values left and add new value at the end
            memmove(chart3Buffer, chart3Buffer + 1, (CHART3_SEGMENTS - 1) * sizeof(float));
            chart3Buffer[CHART3_SEGMENTS - 1] = value;
        }
        
        // Update max value for dynamic scaling
        chart3MaxValue = 1.0f; // Reset to minimum
        for (int i = 0; i < chart3Index; i++) {
            if (chart3Buffer[i] > chart3MaxValue) {
                chart3MaxValue = chart3Buffer[i];
            }
        }
        
        // Add 20% headroom to max value and round to next whole number
        chart3MaxValue = ceil(chart3MaxValue * 1.2f);
        if (chart3MaxValue < 1.0f) chart3MaxValue = 1.0f;
        
        // Draw updated chart
        drawChart3();
        
        // Reset accumulators for next interval
        chart3AccumulatedCpm = 0.0f;
        chart3ElapsedSec = 0.0f;
    }
}

/**
 * @brief Updates the Y-axis scale of a chart based on the maximum value.
 * 
 * This function dynamically adjusts the Y-axis range and ticks of a chart
 * based on the maximum value in the data.
 * 
 * @param chart The LVGL chart object to update
 * @param series The chart series to update
 * @param maxValue The maximum value to set as Y-axis upper bound
 */
void updateChartYAxis(lv_obj_t* chart, lv_chart_series_t* series, float maxValue) {
    if (!chart || !series) return;
    
    // Scale the maxValue for integer representation
    lv_coord_t scaledMax = (lv_coord_t)(maxValue * CHART_SCALE_FACTOR);
    
    // Set the Y range based on the max value (rounded up)
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, scaledMax);
    
    // Calculate appropriate tick interval based on the max value
    int majorTicks, minorTicks;
    
    if (maxValue <= 5) {
        majorTicks = 5;
        minorTicks = 1;
    } else if (maxValue <= 10) {
        majorTicks = 5;
        minorTicks = 1;
    } else if (maxValue <= 20) {
        majorTicks = 4;
        minorTicks = 5;
    } else if (maxValue <= 50) {
        majorTicks = 5;
        minorTicks = 5;
    } else if (maxValue <= 100) {
        majorTicks = 4;
        minorTicks = 5;
    } else {
        majorTicks = 5;
        minorTicks = 10;
    }
    
    // Update the axis ticks for better readability
    lv_chart_set_axis_tick(chart, LV_CHART_AXIS_PRIMARY_Y, 
                           10, // tick_length
                           5,  // major_tick_length
                           majorTicks, // major_tick_cnt
                           minorTicks, // minor_tick_cnt
                           true, // draw_size
                           50); // label_en
    
    Serial.printf("Chart Y-axis updated: max=%.2f, scaled=%d, ticks=%d/%d\n", 
                 maxValue, scaledMax, majorTicks, minorTicks);
}

/**
 * @brief Event callback for chart drawing to handle custom axis labels.
 * 
 * This callback is triggered during chart drawing and is used to override
 * the default axis labels with our custom values accounting for scaling.
 */
static void chart_draw_event_cb(lv_event_t * e) {
    lv_obj_t * chart = lv_event_get_target(e);
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
    
    // Only process axis tick drawing
    if(!dsc || dsc->type != LV_CHART_DRAW_PART_TICK_LABEL) return;
    
    // Only process y-axis labels
    if(dsc->id != LV_CHART_AXIS_PRIMARY_Y || !dsc->text) return;
    
    // Get the original value (which is scaled by CHART_SCALE_FACTOR)
    lv_coord_t value = dsc->value;
    float realValue = (float)value / CHART_SCALE_FACTOR;
    
    // Format the label with 1 decimal place
    snprintf(dsc->text, dsc->text_length, "%.1f", realValue);
}

/**
 * @brief Initialize the chart system with proper settings.
 */
void assignChartSeries() {
    // Get the chart series pointers
    chart1Series = lv_chart_get_series_next(ui_Chart1, NULL);
    chart3Series = lv_chart_get_series_next(ui_Chart3, NULL);
    
    if (!chart1Series)
        Serial.println("Warning: ui_Chart1 has no series!");
    if (!chart3Series)
        Serial.println("Warning: ui_Chart3 has no series!");
    
    // Reset maximum values for dynamic scaling
    chart1MaxValue = 1.0f;
    chart3MaxValue = 1.0f;
    
    // Configure Chart1 (1-hour chart with 3-minute intervals)
    if (ui_Chart1) {
        // First, reset the chart completely
        lv_chart_remove_series(ui_Chart1, chart1Series);
        chart1Series = lv_chart_add_series(ui_Chart1, lv_color_hex(0x89DE10), LV_CHART_AXIS_PRIMARY_Y);
        
        // Set chart type and point count before setting range
        lv_chart_set_type(ui_Chart1, LV_CHART_TYPE_BAR);
        lv_chart_set_point_count(ui_Chart1, CHART1_SEGMENTS);
        lv_chart_set_div_line_count(ui_Chart1, 5, 5);
        
        // Set initial Y range (will be updated dynamically later)
        // Apply the scale factor for integer representation
        updateChartYAxis(ui_Chart1, chart1Series, chart1MaxValue);
        
        // Add event handler for custom axis labels
        lv_obj_add_event_cb(ui_Chart1, chart_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
        
        // Initialize all data points to zero
        for (int i = 0; i < CHART1_SEGMENTS; i++) {
            lv_chart_set_value_by_id(ui_Chart1, chart1Series, i, 0);
        }
        
        // Improve bar appearance
        lv_obj_set_style_size(ui_Chart1, 0, LV_PART_INDICATOR);
    }
    
    // Configure Chart3 (24-hour chart with 1-hour intervals)
    if (ui_Chart3) {
        // First, reset the chart completely
        lv_chart_remove_series(ui_Chart3, chart3Series);
        chart3Series = lv_chart_add_series(ui_Chart3, lv_color_hex(0xE0C810), LV_CHART_AXIS_PRIMARY_Y);
        
        // Set chart type and point count before setting range
        lv_chart_set_type(ui_Chart3, LV_CHART_TYPE_BAR);
        lv_chart_set_point_count(ui_Chart3, CHART3_SEGMENTS);
        lv_chart_set_div_line_count(ui_Chart3, 5, 5);
        
        // Set initial Y range (will be updated dynamically later)
        // Apply the scale factor for integer representation
        updateChartYAxis(ui_Chart3, chart3Series, chart3MaxValue);
        
        // Add event handler for custom axis labels
        lv_obj_add_event_cb(ui_Chart3, chart_draw_event_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
        
        // Initialize all data points to zero
        for (int i = 0; i < CHART3_SEGMENTS; i++) {
            lv_chart_set_value_by_id(ui_Chart3, chart3Series, i, 0);
        }
        
        // Improve bar appearance
        lv_obj_set_style_size(ui_Chart3, 0, LV_PART_INDICATOR);
    }
    
    // Initialize all data structures with zeros
    memset(chart1Buffer, 0, sizeof(chart1Buffer));
    memset(chart3Buffer, 0, sizeof(chart3Buffer));
    
    chart1Index = 0;
    chart3Index = 0;
    chart1AccumulatedCpm = 0.0f;
    chart3AccumulatedCpm = 0.0f;
    chart1ElapsedSec = 0.0f;
    chart3ElapsedSec = 0.0f;
    
    // Refresh the charts to apply changes
    if (ui_Chart1) lv_chart_refresh(ui_Chart1);
    if (ui_Chart3) lv_chart_refresh(ui_Chart3);
    
    Serial.println("Chart series assigned and initialized with dynamic Y-axis scaling.");
}

/**
 * @brief Draws Chart1 using the current ring buffer values.
 */
void drawChart1() {
    if (!chart1Series || !ui_Chart1) return;
    
    // First, update the Y-axis range to fit the data
    updateChartYAxis(ui_Chart1, chart1Series, chart1MaxValue);
    
    // Clear the chart first 
    for (int i = 0; i < CHART1_SEGMENTS; i++) {
        lv_chart_set_value_by_id(ui_Chart1, chart1Series, i, 0);
    }
    
    // Only set values if we have collected data
    if (chart1Index > 0) {
        // Set values from our buffer
        for (int i = 0; i < chart1Index; i++) {
            float value = chart1Buffer[i];
            // Scale the float value to an integer for the chart
            lv_coord_t scaledValue = (lv_coord_t)(value * CHART_SCALE_FACTOR);
            lv_chart_set_value_by_id(ui_Chart1, chart1Series, i, scaledValue);
        }
    }
    
    // Refresh the chart display
    lv_chart_refresh(ui_Chart1);
}

/**
 * @brief Draws Chart3 using the current ring buffer values.
 */
void drawChart3() {
    if (!chart3Series || !ui_Chart3) return;
    
    // First, update the Y-axis range to fit the data
    updateChartYAxis(ui_Chart3, chart3Series, chart3MaxValue);
    
    // Clear the chart first
    for (int i = 0; i < CHART3_SEGMENTS; i++) {
        lv_chart_set_value_by_id(ui_Chart3, chart3Series, i, 0);
    }
    
    // Only set values if we have collected data
    if (chart3Index > 0) {
        // Set values from our buffer
        for (int i = 0; i < chart3Index; i++) {
            float value = chart3Buffer[i];
            // Scale the float value to an integer for the chart
            lv_coord_t scaledValue = (lv_coord_t)(value * CHART_SCALE_FACTOR);
            lv_chart_set_value_by_id(ui_Chart3, chart3Series, i, scaledValue);
        }
    }
    
    // Refresh the chart display
    lv_chart_refresh(ui_Chart3);
}

/*******************************************************************************
 * WiFi Connection Helper
 ******************************************************************************/ 
bool connectToWiFi(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    const int max_attempts = 20;
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
        delay(500);
        attempts++;
    }
    return (WiFi.status() == WL_CONNECTED);
}

/*******************************************************************************
 * setup() and loop()
 ******************************************************************************/ 
void setup() {
    initSerial();
    initTFT();
    initLVGL();
    
    // Initialize the pulse history with zeros first
    for (int i = 0; i < PULSE_HISTORY_SIZE; i++) {
        pulseHistory[i] = 0;
    }
    pulseHistoryIndex = 0;
    
    // Then initialize and validate charts
    assignChartSeries();
    
    // Additional validation for charts to ensure values appear properly on screen
    if (ui_Chart1 && ui_Chart3) {
        // Draw the charts with explicit zeros and refresh display
        drawChart1();
        drawChart3();
        
        // Process LVGL tasks to apply chart updates immediately
        lv_timer_handler();
        
        Serial.println("Charts explicitly zeroed and drawn on screen.");
    } else {
        Serial.println("WARNING: Charts not properly initialized!");
    }
    
    initPulseCounter();
    initBuzzer();

    // Attach UI event callbacks
    lv_obj_add_event_cb(ui_Connect, connect_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_OnStartup, onstartup_checkbox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_EnableAlarmsCheckbox, alarms_checkbox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Add event handlers for spinbox value changes
    lv_obj_add_event_cb(ui_CurrentSpinbox, spinbox_changed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_CumulativeSpinbox, spinbox_changed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Set up power management
    setupPowerManagement();

    // Restore saved settings
    preferences.begin("settings", true);
    bool onStartup = preferences.getBool("onstartup", false);
    preferences.end();
    
    if(onStartup)
        lv_obj_add_state(ui_OnStartup, LV_STATE_CHECKED);
    else
        lv_obj_clear_state(ui_OnStartup, LV_STATE_CHECKED);
        
    // Load alarm settings from preferences
    loadAlarmSettings();

    // Load startup screen and start WiFi timer
    lv_scr_load(ui_Startup_screen);
    lv_timer_create(wifi_connect_timer_cb, 2000, NULL);

    startTime = millis();
    lastLoop = startTime;
    Serial.println("Setup completed.");
}

void loop() {
    lv_timer_handler();

    // Update PCNT and pulse history (using 100ms polling)
    updatePulseHistory();

    unsigned long now = millis();
    float dtSec = (float)(now - lastLoop) / 1000.0f;
    lastLoop = now;

    int cpm = getRealTimeCPM();
    updateRealTimeStats((float)cpm, dtSec);
    updateLabels();
    accumulateCharts((float)cpm, dtSec);
    checkAlarms();
    
    // Battery monitoring disabled - device is USB powered
    // checkBatteryLevel();
    
    // Manage power consumption
    managePower();

    // Update WiFi info every second if connected
    static unsigned long lastTimeUpdate = 0;
    if (WiFi.status() == WL_CONNECTED && (now - lastTimeUpdate >= 1000)) {
        lastTimeUpdate = now;
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[64];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
            String info = String("IP: ") + wifi_ip + "\nTime: " + timeStr + " UTC";
            lv_label_set_text(ui_WIFIINFO, info.c_str());
        } else {
            String info = String("IP: ") + wifi_ip + "\nTime: NTP Failed";
            lv_label_set_text(ui_WIFIINFO, info.c_str());
        }
    }

    // Handle OTA updates
    if (otaInitialized) {
        server.handleClient();
    }
}

/*******************************************************************************
 * Other Functions (Initialization, UI Setup, etc.)
 ******************************************************************************/ 
void initSerial() {
    Serial.begin(115200);
    Serial.println("Initializing Serial...");
}

void initTFT() {
    static TFT_eSPI tft(SCREEN_WIDTH, SCREEN_HEIGHT);
    tft.begin();
    tft.setRotation(1);
    uint16_t calData[5] = {300, 3597, 187, 3579, 6};
    tft.setTouch(calData);
    Serial.println("TFT initialized.");
}

void initLVGL() {
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * SCREEN_HEIGHT / 10);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = [](lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
        static TFT_eSPI tftDisp(SCREEN_WIDTH, SCREEN_HEIGHT);
        static bool initted = false;
        if (!initted) {
            tftDisp.begin();
            tftDisp.setRotation(1);
            initted = true;
        }
        uint32_t w = (area->x2 - area->x1 + 1);
        uint32_t h = (area->y2 - area->y1 + 1);
        tftDisp.startWrite();
        tftDisp.setAddrWindow(area->x1, area->y1, w, h);
        tftDisp.pushColors((uint16_t *)&color_p->full, w * h, true);
        tftDisp.endWrite();
        lv_disp_flush_ready(drv);
    };
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = [](lv_indev_drv_t *drv, lv_indev_data_t *data) {
        static TFT_eSPI tftTouch(SCREEN_WIDTH, SCREEN_HEIGHT);
        static bool inittedTouch = false;
        if (!inittedTouch) {
            tftTouch.begin();
            tftTouch.setRotation(1);
            uint16_t cData[5] = {267, 3646, 198, 3669, 6};
            tftTouch.setTouch(cData);
            inittedTouch = true;
        }
        uint16_t x, y;
        bool touched = tftTouch.getTouch(&y, &x, 600);
        if (!touched)
            data->state = LV_INDEV_STATE_REL;
        else {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = x;
            data->point.y = y;
        }
    };
    lv_indev_drv_register(&indev_drv);

    ui_init();
    Serial.println("LVGL initialized + UI created.");
}

void updateRealTimeStats(float cpm, float dtSec) {
    // Calculate raw value
    float rawCurrentuSvHr = cpm / CONVERSION_FACTOR;
    
    // Add to the smoothing window
    currentReadingsWindow[currentReadingIndex] = rawCurrentuSvHr;
    currentReadingIndex = (currentReadingIndex + 1) % SMOOTHING_WINDOW_SIZE;
    
    // Calculate smoothed value (moving average)
    float sum = 0.0f;
    for (int i = 0; i < SMOOTHING_WINDOW_SIZE; i++) {
        sum += currentReadingsWindow[i];
    }
    currentuSvHr = sum / SMOOTHING_WINDOW_SIZE;
    
    // Update average
    unsigned long now = millis();
    float totalTimeMin = (float)(now - startTime) / 60000.0f;
    if (totalTimeMin > 0.0f) {
        float avgCPM = (float)totalCounts / totalTimeMin;
        averageuSvHr = avgCPM / CONVERSION_FACTOR;
    }
    
    // Update maximum if current reading is higher
    if (currentuSvHr > maxuSvHr)
        maxuSvHr = currentuSvHr;
    
    // Calculate dose increment in µSv, then convert to mSv (1 mSv = 1000 µSv)
    // Formula: (µSv/h) / (3600 sec/h) * dt / 1000
    float doseIncrement_mSv = (currentuSvHr / 3600.0f) * dtSec / 1000.0f;
    cumulativemSv += doseIncrement_mSv;
}

void updateLabels() {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.2f", currentuSvHr);
    lv_label_set_text(ui_CurrentRad, buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", averageuSvHr);
    lv_label_set_text(ui_AverageRad, buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", maxuSvHr);
    lv_label_set_text(ui_MaximumRad, buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", cumulativemSv);
    lv_label_set_text(ui_CumulativeRad, buffer);
}

/*******************************************************************************
 * WiFi/OTA Code
 ******************************************************************************/ 
// Function to serve the OTA warning page
void serveOtaWarningPage() {
    String warningPage = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    warningPage += "<title>OTA Update Warning</title>";
    warningPage += "<style>";
    warningPage += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #f5f5f5; }";
    warningPage += ".warning-container { max-width: 600px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }";
    warningPage += "h1 { color: #e53935; }";
    warningPage += ".warning-icon { margin-bottom: 20px; }";
    warningPage += ".warning-text { color: #333; text-align: left; margin: 20px 0; line-height: 1.5; }";
    warningPage += ".btn-container { display: flex; justify-content: center; gap: 10px; margin-top: 20px; }";
    warningPage += ".btn { display: inline-block; background-color: #e53935; color: white; padding: 12px 24px; text-decoration: none; border-radius: 4px; font-weight: bold; border: none; cursor: pointer; font-size: 16px; }";
    warningPage += ".btn:hover { background-color: #c62828; }";
    warningPage += ".back-btn { background-color: #757575; }";
    warningPage += ".back-btn:hover { background-color: #616161; }";
    warningPage += "form { display: inline-block; }";
    warningPage += "</style>";
    warningPage += "</head><body>";
    warningPage += "<div class='warning-container'>";
    warningPage += "<div class='warning-icon'><svg xmlns='http://www.w3.org/2000/svg' width='64' height='64' viewBox='0 0 24 24'><path fill='#e53935' d='M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z'/></svg></div>";
    warningPage += "<h1>WARNING: FIRMWARE UPDATE</h1>";
    warningPage += "<div class='warning-text'>";
    warningPage += "<p><strong>Uploading incorrect firmware can brick your device!</strong></p>";
    warningPage += "<p>Please ensure that:</p>";
    warningPage += "<ul style='text-align: left;'>";
    warningPage += "<li>You are uploading firmware specifically built for the Radiation Detector</li>";
    warningPage += "<li>The firmware version is compatible with your hardware</li>";
    warningPage += "<li>You have a stable power connection during the update</li>";
    warningPage += "<li>Your WiFi connection is stable during the update</li>";
    warningPage += "</ul>";
    warningPage += "<p>If the update fails, you may need to manually update the firmware using a USB connection.</p>";
    warningPage += "</div>";
    warningPage += "<div class='btn-container'>";
    warningPage += "<a href='/' class='btn back-btn'>Cancel</a>";
    warningPage += "<form method='post' action='/acknowledge-ota'><button type='submit' class='btn'>I Understand, Proceed</button></form>";
    warningPage += "</div>";
    warningPage += "</div></body></html>";
    
    server.send(200, "text/html", warningPage);
}

// Update the WiFi/OTA initialization in connect_btn_event_cb
static void connect_btn_event_cb(lv_event_t *e) {
    const char* ssid = lv_textarea_get_text(ui_SSID);
    const char* password = lv_textarea_get_text(ui_PASSWORD);
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    lv_label_set_text(ui_WIFIINFO, "Connecting");
    
    if (connectToWiFi(ssid, password)) {
        Serial.println("WiFi connected.");
        wifi_ip = WiFi.localIP().toString();
        
        // Initialize mDNS responder
        if (MDNS.begin("radiation")) {
            Serial.println("mDNS responder started - Device accessible at http://radiation.local");
            wifi_ip += "\nHostname: radiation.local";
        } else {
            Serial.println("Error setting up mDNS responder!");
        }
        
        preferences.begin("wifi", false);
        preferences.putString("ssid", ssid);
        preferences.putString("password", password);
        preferences.end();
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[64];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
            String info = String("IP: ") + wifi_ip + "\nTime: " + timeStr + " UTC";
            lv_label_set_text(ui_WIFIINFO, info.c_str());
            Serial.println(info);
        } else {
            String info = String("IP: ") + wifi_ip + "\nTime: NTP Failed";
            lv_label_set_text(ui_WIFIINFO, info.c_str());
            Serial.println(info);
        }
        if (!otaInitialized) {
            server.begin();
            
            // Reset acknowledgment flag when server initializes
            otaWarningAcknowledged = false;
            
            // Add route for the dashboard
            server.on("/", [](){
                String dashboard = getDashboardPage();
                server.send(200, "text/html", dashboard);
            });
            
            // Add a dedicated route for the warning page
            server.on("/warning", HTTP_GET, []() {
                serveOtaWarningPage();
            });
            
            // Add a route to handle the acknowledgment form submission
            server.on("/acknowledge-ota", HTTP_POST, []() {
                otaWarningAcknowledged = true;
                server.sendHeader("Location", "/update", true);
                server.send(302, "text/plain", "");
            });
            
            // Initialize ElegantOTA
            ElegantOTA.begin(&server);
            otaInitialized = true;
            Serial.println("OTA initialized with warning page.");
            
            // Modify the dashboard to link to /warning instead of /update directly
            server.on("/api/data", HTTP_GET, [](){
                server.sendHeader("Access-Control-Allow-Origin", "*");
                server.sendHeader("Access-Control-Allow-Methods", "GET");
                server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
                server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
                server.sendHeader("Pragma", "no-cache");
                server.sendHeader("Expires", "0");
                server.send(200, "application/json", getRadiationDataJsonExport());
            });
        }
    } else {
        Serial.println("WiFi connection failed.");
        lv_label_set_text(ui_WIFIINFO, "Disconnected\nFailed");
    }
}

// Modify the tryAutoConnect function similarly
void tryAutoConnect() {
    preferences.begin("wifi", true);
    String storedSsid = preferences.getString("ssid", "");
    String storedPassword = preferences.getString("password", "");
    preferences.end();
    if (storedSsid.length() > 0) {
        Serial.print("Found saved credentials. Trying to connect to: ");
        Serial.println(storedSsid);
        if (connectToWiFi(storedSsid.c_str(), storedPassword.c_str())) {
            Serial.println("Auto WiFi connection successful.");
            wifi_ip = WiFi.localIP().toString();
            
            // Initialize mDNS responder
            if (MDNS.begin("radiation")) {
                Serial.println("mDNS responder started - Device accessible at http://radiation.local");
                wifi_ip += "\nHostname: radiation.local";
            } else {
                Serial.println("Error setting up mDNS responder!");
            }
            
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                char timeStr[64];
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
                String info = String("IP: ") + wifi_ip + "\nTime: " + timeStr + " UTC";
                lv_label_set_text(ui_WIFIINFO, info.c_str());
                Serial.println(info);
            } else {
                String info = String("IP: ") + wifi_ip + "\nTime: NTP Failed";
                lv_label_set_text(ui_WIFIINFO, info.c_str());
                Serial.println(info);
            }
            if (!otaInitialized) {
                server.begin();
                
                // Reset acknowledgment flag when server initializes
                otaWarningAcknowledged = false;
                
                // Add route for the dashboard 
                server.on("/", [](){
                    String dashboard = getDashboardPage();
                    server.send(200, "text/html", dashboard);
                });
                
                // Add a dedicated route for the warning page
                server.on("/warning", HTTP_GET, []() {
                    serveOtaWarningPage();
                });
                
                // Add a route to handle the acknowledgment form submission
                server.on("/acknowledge-ota", HTTP_POST, []() {
                    otaWarningAcknowledged = true;
                    server.sendHeader("Location", "/update", true);
                    server.send(302, "text/plain", "");
                });
                
                // Initialize ElegantOTA
                ElegantOTA.begin(&server);
                otaInitialized = true;
                Serial.println("OTA initialized with warning page.");
                
                // Add JSON API endpoint
                server.on("/api/data", HTTP_GET, [](){
                    server.sendHeader("Access-Control-Allow-Origin", "*");
                    server.sendHeader("Access-Control-Allow-Methods", "GET");
                    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
                    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
                    server.sendHeader("Pragma", "no-cache");
                    server.sendHeader("Expires", "0");
                    server.send(200, "application/json", getRadiationDataJsonExport());
                });
            }
        } else {
            Serial.println("Auto WiFi connection failed.");
            lv_label_set_text(ui_WIFIINFO, "Disconnected\nFailed");
        }
    } else {
        Serial.println("No stored credentials found.");
    }
    lv_scr_load(ui_InitialScreen);
}

static void wifi_connect_timer_cb(lv_timer_t * timer) {
    if(lv_obj_has_state(ui_OnStartup, LV_STATE_CHECKED)) {
        Serial.println("OnStartup enabled. Attempting auto WiFi connection...");
        tryAutoConnect();
    } else {
        Serial.println("OnStartup disabled. Loading InitialScreen immediately.");
        lv_scr_load(ui_InitialScreen);
    }
    lv_timer_del(timer);
}

static void onstartup_checkbox_event_cb(lv_event_t * e) {
    lv_obj_t * checkbox = lv_event_get_target(e);
    bool checked = lv_obj_has_state(checkbox, LV_STATE_CHECKED);
    preferences.begin("settings", false);
    preferences.putBool("onstartup", checked);
    preferences.end();
    Serial.print("OnStartup checkbox state saved as: ");
    Serial.println(checked ? "ENABLED" : "DISABLED");
}

// Disable the battery check function implementation
void checkBatteryLevel() {
    // Battery monitoring disabled - device is USB powered
    return;
}

// Update the getRadiationDataJson function to remove battery voltage
static String getRadiationDataJson() {
    // Create a JsonDocument - with newer ArduinoJson versions, we don't need to specify capacity
    JsonDocument doc;
    
    // Add basic radiation data with field names matching what dashboard.js expects
    doc["current"] = currentuSvHr;
    doc["average"] = averageuSvHr;
    doc["maximum"] = maxuSvHr;
    doc["cumulative"] = cumulativemSv;
    doc["total_counts"] = totalCounts;
    
    // Add timestamp (seconds since start)
    doc["timestamp"] = millis() / 1000;
    
    // Battery monitoring disabled - device is USB powered
    // #ifdef BATTERY_VOLTAGE_PIN
    // int adcValue = analogRead(BATTERY_VOLTAGE_PIN);
    // float batteryVoltage = (adcValue / 4095.0) * 3.3 * 2; 
    // doc["battery_v"] = batteryVoltage;
    // #endif
    
    // Add hourly data (chart1Buffer) - use field name 'hourly' to match dashboard.js
    JsonArray hourlyData = doc["hourly"].to<JsonArray>();
    for (int i = 0; i < CHART1_SEGMENTS; i++) {
        // Calculate the actual index in the ring buffer
        int actualIndex = (chart1Index + i) % CHART1_SEGMENTS;
        hourlyData.add(chart1Buffer[actualIndex]);
    }
    
    // Add daily data (chart3Buffer) - use field name 'daily' to match dashboard.js
    JsonArray dailyData = doc["daily"].to<JsonArray>();
    for (int i = 0; i < CHART3_SEGMENTS; i++) {
        // Calculate the actual index in the ring buffer
        int actualIndex = (chart3Index + i) % CHART3_SEGMENTS;
        dailyData.add(chart3Buffer[actualIndex]);
    }
    
    // Serialize to String - use buffer for better performance
    String jsonString;
    // Reserve memory to avoid reallocations
    jsonString.reserve(measureJson(doc) + 1);
    serializeJson(doc, jsonString);
    
    return jsonString;
}

// Export function that can be called from other files
String getRadiationDataJsonExport() {
    return getRadiationDataJson();
}

// Function to manage power
void managePower() {
    unsigned long now = millis();
    
    // Check if it's time to dim the display
    if (!isDisplayDimmed && (now - lastUserInteractionTime >= DISPLAY_TIMEOUT)) {
        // Dim the display
        // tft.setDimmer(dimmedBrightness);
        isDisplayDimmed = true;
        Serial.println("Display dimmed to save power");
    }
}

// Function to restore normal brightness
void restoreDisplayBrightness() {
    if (isDisplayDimmed) {
        // tft.setDimmer(normalBrightness);
        isDisplayDimmed = false;
        Serial.println("Display brightness restored");
    }
}

// Touch event handler to track user interaction
static void touch_event_cb(lv_event_t * e) {
    lastUserInteractionTime = millis();
    if (isDisplayDimmed) {
        restoreDisplayBrightness();
    }
}

// Add this function to the initialization section in setup()
void setupPowerManagement() {
    // Initialize power management
    lastUserInteractionTime = millis();
    
    // Set initial display brightness
    // Assuming TFT_eSPI library's setDimmer or setBrightness method
    // This part may need to be adjusted based on your TFT library's functionality
    // tft.setDimmer(normalBrightness);
    
    // Register touch event handler to track user interaction
    // Attach to every UI element that users interact with
    if (ui_EnableAlarmsCheckbox) {
        lv_obj_add_event_cb(ui_EnableAlarmsCheckbox, touch_event_cb, LV_EVENT_PRESSED, NULL);
    }
    if (ui_CurrentSpinbox) {
        lv_obj_add_event_cb(ui_CurrentSpinbox, touch_event_cb, LV_EVENT_PRESSED, NULL);
    }
    if (ui_CumulativeSpinbox) {
        lv_obj_add_event_cb(ui_CumulativeSpinbox, touch_event_cb, LV_EVENT_PRESSED, NULL);
    }
    if (ui_Connect) {
        lv_obj_add_event_cb(ui_Connect, touch_event_cb, LV_EVENT_PRESSED, NULL);
    }
    
    Serial.println("Power management initialized");
}
