/**
 * @file main.cpp
 * @brief Radiation Monitor with TFT UI, WiFi OTA, and hardware pulse counting on ESP32S3.
 *
 * This file reads pulses from a Geiger counter via the ESP32 PCNT peripheral, displays radiation
 * data on a TFT with LVGL, connects to WiFi with OTA updates, and charts data using fixed intervals.
 * 
 * The PCNT polling is now performed every 100ms with an accumulator so that high‐frequency pulses are less likely to be missed.
 */

// Debug configuration
// Uncomment the line below to enable debug output at compile time
#define DEBUG

// Runtime debug flag - can be toggled via serial command
bool debugEnabled = true;  // Set to true by default to see debug output immediately

#ifdef DEBUG
    #define DEBUG_TIMESTAMP() do { \
        if (debugEnabled) { \
            unsigned long ms = millis(); \
            unsigned int seconds = ms / 1000; \
            unsigned int minutes = seconds / 60; \
            unsigned int hours = minutes / 60; \
            unsigned int days = hours / 24; \
            seconds %= 60; \
            minutes %= 60; \
            hours %= 24; \
            char timestamp[20]; \
            snprintf(timestamp, sizeof(timestamp), "[%d:%02d:%02d.%03d] ", \
                    (int)days * 24 + (int)hours, (int)minutes, (int)seconds, (int)(ms % 1000)); \
            Serial.print(timestamp); \
        } \
    } while(0)
    
    #define DEBUG_PRINT(x) do { if (debugEnabled) { DEBUG_TIMESTAMP(); Serial.print(x); } } while(0)
    #define DEBUG_PRINTLN(x) do { if (debugEnabled) { DEBUG_TIMESTAMP(); Serial.println(x); } } while(0)
    #define DEBUG_PRINTF(...) do { if (debugEnabled) { DEBUG_TIMESTAMP(); Serial.printf(__VA_ARGS__); } } while(0)
#else
    #define DEBUG_TIMESTAMP()
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/*******************************************************************************
 * Definitions & Constants
 ******************************************************************************/ 
static const uint16_t SCREEN_WIDTH  = 480;
static const uint16_t SCREEN_HEIGHT = 320;

static const uint8_t GEIGER_PULSE_PIN = 9;   ///< GPIO pin connected to the Geiger counter output
static const uint8_t BUZZER_PIN       = 38;    ///< GPIO pin for LEDC buzzer output

// Conversion factor from counts per minute (CPM) to µSv/h.
// For the SBM‑20 Geiger tube this value is approximately 153.8.
static const float CONVERSION_FACTOR = 153.8f;

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
extern lv_obj_t* ui_CurrentAlarm;    ///< Current radiation alarm threshold label
extern lv_obj_t* ui_CumulativeAlarm; ///< Cumulative dose alarm threshold label

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

// Core-specific task handles
TaskHandle_t pulseTaskHandle = NULL;
TaskHandle_t uiTaskHandle = NULL;

// Mutex for thread-safe data access
SemaphoreHandle_t dataMutex = NULL;

// Ring buffer for real-time pulse data
#define PULSE_BUFFER_SIZE 20  // 20 samples at 50ms = 1 second of data
struct PulseData {
    int16_t count;
    unsigned long timestamp;
};
PulseData pulseBuffer[PULSE_BUFFER_SIZE];
int pulseBufferIndex = 0;

// Function prototypes for core-specific tasks
void pulseTask(void *parameter);
void uiTask(void *parameter);

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

// Function to process serial commands
void processSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command == "debug on") {
            debugEnabled = true;
            Serial.println("Debug output enabled");
        } 
        else if (command == "debug off") {
            debugEnabled = false;
            Serial.println("Debug output disabled");
        }
        else if (command == "status") {
            Serial.println("=== Radiation Detector Status ===");
            Serial.printf("Debug: %s\n", debugEnabled ? "ON" : "OFF");
            Serial.printf("Current radiation: %.3f µSv/h\n", currentuSvHr);
            Serial.printf("Average radiation: %.3f µSv/h\n", averageuSvHr);
            Serial.printf("Maximum radiation: %.3f µSv/h\n", maxuSvHr);
            Serial.printf("Cumulative dose: %.3f mSv\n", cumulativemSv);
            Serial.printf("Total counts: %lu\n", totalCounts);
            Serial.printf("CPM: %d\n", getRealTimeCPM());
            Serial.printf("Alarms: %s\n", alarmDisabled ? "DISABLED" : "ENABLED");
            Serial.printf("WiFi: %s\n", WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED");
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("IP: %s\n", wifi_ip.c_str());
            }
        }
    }
}

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
    bool isChecked = lv_obj_has_state(checkbox, LV_STATE_CHECKED);
    
    // Update the global state
    alarmDisabled = !isChecked;
    
    DEBUG_PRINTF("Checkbox state changed: %s (alarm %s)\n", 
                isChecked ? "CHECKED" : "UNCHECKED",
                alarmDisabled ? "DISABLED" : "ENABLED");
    
    // Save all alarm settings including the enabled state
    saveAlarmSettings();
    
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
    lv_obj_t * spinbox = lv_event_get_target(e);
    char buffer[16];
    
    // Update corresponding alarm label when spinbox value changes
    if (spinbox == ui_CurrentSpinbox && ui_CurrentAlarm) {
        int32_t currentValue = lv_spinbox_get_value(ui_CurrentSpinbox);
        snprintf(buffer, sizeof(buffer), "%.1f", (float)currentValue / 10.0f);
        lv_label_set_text(ui_CurrentAlarm, buffer);
    } 
    else if (spinbox == ui_CumulativeSpinbox && ui_CumulativeAlarm) {
        int32_t cumulativeValue = lv_spinbox_get_value(ui_CumulativeSpinbox);
        snprintf(buffer, sizeof(buffer), "%.1f", (float)cumulativeValue / 10.0f);
        lv_label_set_text(ui_CumulativeAlarm, buffer);
    }
    
    // Save the updated spinbox values and current alarm state
    saveAlarmSettings();
    DEBUG_PRINTLN("Spinbox value changed, alarm settings saved");
}

/*******************************************************************************
 * Alarm Settings Management
 ******************************************************************************/ 
void saveAlarmSettings() {
    // Check if the UI components are initialized
    if (!ui_EnableAlarmsCheckbox || !ui_CurrentSpinbox || !ui_CumulativeSpinbox) {
        DEBUG_PRINTLN("ERROR: Cannot save alarm settings - UI not initialized!");
        return;
    }

    preferences.begin("settings", false);
    
    // Save current alarm threshold from spinbox
    int32_t currentValue = lv_spinbox_get_value(ui_CurrentSpinbox);
    preferences.putInt("currentAlarm", currentValue);
    
    // Save cumulative alarm threshold from spinbox
    int32_t cumulativeValue = lv_spinbox_get_value(ui_CumulativeSpinbox);
    preferences.putInt("cumulativeAlarm", cumulativeValue);
    
    // Save alarm enabled state
    bool isAlarmEnabled = !alarmDisabled;
    preferences.putBool("alarmEnabled", isAlarmEnabled);
    
    // Always flush to ensure data is written
    preferences.end();
    
    DEBUG_PRINTF("Alarm settings saved: Current=%.1f µSv/h, Cumulative=%.1f mSv, Alarms %s\n", 
                 (float)currentValue / 10.0f, 
                 (float)cumulativeValue / 10.0f, 
                 alarmDisabled ? "DISABLED" : "ENABLED");
}

void loadAlarmSettings() {
    // Check if the UI components are initialized
    if (!ui_EnableAlarmsCheckbox || !ui_CurrentSpinbox || !ui_CumulativeSpinbox) {
        DEBUG_PRINTLN("ERROR: Cannot load alarm settings - UI not initialized!");
        return;
    }

    preferences.begin("settings", true);
    
    // Load current alarm threshold - default 5.0 µSv/h (50 in spinbox format for xxx.x)
    int32_t currentValue = preferences.getInt("currentAlarm", 50);
    lv_spinbox_set_value(ui_CurrentSpinbox, currentValue);
    
    // Load cumulative alarm threshold - default 1.0 mSv (10 in spinbox format for xxx.x)
    int32_t cumulativeValue = preferences.getInt("cumulativeAlarm", 10);
    lv_spinbox_set_value(ui_CumulativeSpinbox, cumulativeValue);
    
    // Load alarm enabled state
    bool isAlarmEnabled = preferences.getBool("alarmEnabled", false); // Default disabled
    alarmDisabled = !isAlarmEnabled;
    
    DEBUG_PRINTF("Loaded preferences value alarmEnabled = %s\n", isAlarmEnabled ? "true" : "false");
    
    // Update the checkbox state
    if (isAlarmEnabled) {
        DEBUG_PRINTLN("Setting checkbox to CHECKED state");
        lv_obj_add_state(ui_EnableAlarmsCheckbox, LV_STATE_CHECKED);
    } else {
        DEBUG_PRINTLN("Setting checkbox to UNCHECKED state");
        lv_obj_clear_state(ui_EnableAlarmsCheckbox, LV_STATE_CHECKED);
    }
    
    // Update alarm threshold labels on the main screen
    char buffer[16];
    if (ui_CurrentAlarm) {
        snprintf(buffer, sizeof(buffer), "%.1f", (float)currentValue / 10.0f);
        lv_label_set_text(ui_CurrentAlarm, buffer);
    }
    
    if (ui_CumulativeAlarm) {
        snprintf(buffer, sizeof(buffer), "%.1f", (float)cumulativeValue / 10.0f);
        lv_label_set_text(ui_CumulativeAlarm, buffer);
    }
    
    preferences.end();
    
    DEBUG_PRINTF("Alarm settings loaded: Current=%.1f µSv/h, Cumulative=%.1f mSv, Alarms %s\n", 
                 (float)currentValue / 10.0f, 
                 (float)cumulativeValue / 10.0f, 
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
    // For UI format "xxx.x" - spinbox value 1234 represents 123.4 µSv/h or 123.4 mSv
    int32_t currentValueRaw = lv_spinbox_get_value(ui_CurrentSpinbox);
    int32_t cumulativeValueRaw = lv_spinbox_get_value(ui_CumulativeSpinbox);
    
    // Convert from spinbox format to actual values
    // The spinbox format is "xxx.x" so divide by 10 to get the actual value
    float currentThreshold = (float)currentValueRaw / 10.0f;
    float cumulativeThreshold = (float)cumulativeValueRaw / 10.0f;

    bool condition = (currentuSvHr > currentThreshold) || (cumulativemSv > cumulativeThreshold);
    
    // Debug output - uncomment when troubleshooting alarms
    static unsigned long lastDebugPrint = 0;
    if (millis() - lastDebugPrint > 5000) { // Print every 5 seconds to avoid flooding serial
        DEBUG_PRINTF("Alarm check: Current %.3f/%.3f µSv/h, Cumulative %.3f/%.3f mSv, Alarm %s\n", 
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

    DEBUG_PRINTLN("PCNT initialized on GEIGER_PULSE_PIN.");
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
 * @brief Returns the total counts per minute based on the pulse history.
 *
 * This function computes the counts per minute by summing the pulses in the 
 * 60-second history buffer. If the buffer is not yet full, it extrapolates
 * based on the available data to give an accurate CPM estimate.
 *
 * @return int Counts per minute (CPM)
 */
int getRealTimeCPM() {
    int sum = 0;
    int validSeconds = 0;
    
    // Count pulses and valid seconds in history buffer
    for (int i = 0; i < PULSE_HISTORY_SIZE; i++) {
        if (pulseHistory[i] >= 0) {  // Valid entries
            sum += pulseHistory[i];
            validSeconds++;
        }
    }
    
    // If we don't have a full minute of data yet, extrapolate
    if (validSeconds < PULSE_HISTORY_SIZE && validSeconds > 0) {
        return (int)((float)sum * ((float)PULSE_HISTORY_SIZE / (float)validSeconds));
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
    
    DEBUG_PRINTF("Chart Y-axis updated: max=%.2f, scaled=%d, ticks=%d/%d\n", 
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
        DEBUG_PRINTLN("Warning: ui_Chart1 has no series!");
    if (!chart3Series)
        DEBUG_PRINTLN("Warning: ui_Chart3 has no series!");
    
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
    
    DEBUG_PRINTLN("Chart series assigned and initialized with dynamic Y-axis scaling.");
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
 * Core-specific Task Functions
 ******************************************************************************/
void pulseTask(void *parameter) {
    // Configure PCNT for this core
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
    pcnt_set_filter_value(PCNT_UNIT, 10);
    pcnt_filter_enable(PCNT_UNIT);
    pcnt_counter_pause(PCNT_UNIT);
    pcnt_counter_clear(PCNT_UNIT);
    pcnt_counter_resume(PCNT_UNIT);

    DEBUG_PRINTLN("Pulse counting task started on Core 0");

    int16_t lastCount = 0;
    const TickType_t xDelay = pdMS_TO_TICKS(50); // 50ms polling interval

    while (true) {
        int16_t currentCount = 0;
        pcnt_get_counter_value(PCNT_UNIT, &currentCount);
        
        int16_t diff = currentCount - lastCount;
        if (diff < 0) diff = 0; // Handle counter wrap-around
        
        // Debug significant pulse activity
        if (diff > 5) {
            DEBUG_PRINTF("Pulse burst: %d counts detected\n", diff);
        }
        
        // Store in ring buffer with mutex protection
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            pulseBuffer[pulseBufferIndex].count = diff;
            pulseBuffer[pulseBufferIndex].timestamp = millis();
            
            // Also maintain the traditional pulse history for compatibility with existing code
            // and to ensure we have a full minute of data for accurate CPM
            static unsigned long lastSecondTimestamp = 0;
            static int currentSecondAccumulator = 0;
            
            // Add to current second accumulator
            currentSecondAccumulator += diff;
            
            // If we've passed a second boundary, update the pulse history
            unsigned long now = millis();
            if (now - lastSecondTimestamp >= 1000) {
                pulseHistory[pulseHistoryIndex] = currentSecondAccumulator;
                pulseHistoryIndex = (pulseHistoryIndex + 1) % PULSE_HISTORY_SIZE;
                totalCounts += currentSecondAccumulator;
                
                // Debug output for high pulse counts
                if (currentSecondAccumulator > 10) {
                    DEBUG_PRINTF("High pulse count: %d counts in last second\n", currentSecondAccumulator);
                }
                
                currentSecondAccumulator = 0;
                lastSecondTimestamp = now;
            }
            
            pulseBufferIndex = (pulseBufferIndex + 1) % PULSE_BUFFER_SIZE;
            xSemaphoreGive(dataMutex);
        }
        
        lastCount = currentCount;
        vTaskDelay(xDelay);
    }
}

void uiTask(void *parameter) {
    const TickType_t xDelay = pdMS_TO_TICKS(50); // 50ms UI update interval
    unsigned long lastTimeUpdate = 0;
    
    DEBUG_PRINTLN("UI task started on Core 1");
    
    // Give UI components time to initialize fully
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Load alarm settings once UI is fully initialized
    DEBUG_PRINTLN("UI task: Loading alarm settings now that UI is fully initialized");
    
    // Load spinbox values from preferences
    preferences.begin("settings", true);
    
    // Get alarm thresholds with proper defaults
    int32_t currentValue = preferences.getInt("currentAlarm", 50);  // Default 5.0 µSv/h
    int32_t cumulativeValue = preferences.getInt("cumulativeAlarm", 10);  // Default 1.0 mSv
    
    // Get alarm enabled state again (redundant but safer)
    bool alarmEnabled = preferences.getBool("alarmEnabled", false);
    
    preferences.end();
    
    DEBUG_PRINTF("UI task: Loaded alarm values: Current=%d, Cumulative=%d, Enabled=%s\n", 
                 currentValue, cumulativeValue, alarmEnabled ? "true" : "false");
    
    // Set spinbox values
    lv_spinbox_set_value(ui_CurrentSpinbox, currentValue);
    lv_spinbox_set_value(ui_CumulativeSpinbox, cumulativeValue);
    
    // Ensure alarm checkbox state is properly set
    if (alarmEnabled) {
        DEBUG_PRINTLN("UI task: Setting alarm checkbox to CHECKED state");
        lv_obj_add_state(ui_EnableAlarmsCheckbox, LV_STATE_CHECKED);
        alarmDisabled = false;
    } else {
        DEBUG_PRINTLN("UI task: Setting alarm checkbox to UNCHECKED state");
        lv_obj_clear_state(ui_EnableAlarmsCheckbox, LV_STATE_CHECKED);
        alarmDisabled = true;
    }
    
    // Force LVGL to process all UI updates
    lv_timer_handler();
    
    while (true) {
        // Process LVGL tasks
        lv_timer_handler();
        
        // Check for serial commands
        processSerialCommands();
        
        unsigned long now = millis();
        
        // Update radiation data with mutex protection
        if (xSemaphoreTake(dataMutex, portMAX_DELAY)) {
            // Use the traditional pulse history which tracks a full minute
            // This ensures we have the correct CPM calculation
            int cpm = getRealTimeCPM();
            
            // Calculate time delta since last update
            static unsigned long lastUpdateTime = millis();
            float dtSec = (float)(now - lastUpdateTime) / 1000.0f;
            lastUpdateTime = now;
            
            // Log significant changes in radiation levels
            static float lastCPM = 0;
            if (abs(cpm - lastCPM) > 10) {
                DEBUG_PRINTF("CPM change: %d → %d (%.2f µSv/h)\n", (int)lastCPM, cpm, cpm / CONVERSION_FACTOR);
                lastCPM = cpm;
            }
            
            // Update real-time stats
            updateRealTimeStats((float)cpm, dtSec);
            updateLabels();
            accumulateCharts((float)cpm, dtSec);
            checkAlarms();
            
            xSemaphoreGive(dataMutex);
        }
        
        // Update power management
        managePower();
        
        // Update WiFi info every second if connected
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
                DEBUG_PRINTLN("NTP time sync failed");
            }
        }
        
        // Handle OTA updates
        if (otaInitialized) {
            server.handleClient();
        }
        
        vTaskDelay(xDelay);
    }
}

/*******************************************************************************
 * setup() and loop()
 ******************************************************************************/ 
void setup() {
    initSerial();
    
    // Print startup debug information
    Serial.println("\n\n=== Radiation Detector Startup ===");
    Serial.println("Debug output is enabled");
    DEBUG_PRINTLN("DEBUG macro is working if you see this message");
    
    initTFT();
    
    // Initialize LVGL BEFORE setting the startup screen
    initLVGL();
    
    // IMPORTANT: Set the startup screen IMMEDIATELY to avoid flicker
    // This overrides the default screen set in ui_init()
    lv_scr_load(ui_Startup_screen);
    
    // Create mutex for thread-safe data access
    dataMutex = xSemaphoreCreateMutex();
    
    // Initialize pulse buffer and history
    memset(pulseBuffer, 0, sizeof(pulseBuffer));
    memset(pulseHistory, 0, sizeof(pulseHistory));
    pulseBufferIndex = 0;
    pulseHistoryIndex = 0;
    
    // Initialize and validate charts
    assignChartSeries();
    
    // Additional validation for charts
    if (ui_Chart1 && ui_Chart3) {
        drawChart1();
        drawChart3();
        lv_timer_handler();
        DEBUG_PRINTLN("Charts explicitly zeroed and drawn on screen.");
    } else {
        DEBUG_PRINTLN("WARNING: Charts not properly initialized!");
    }
    
    initBuzzer();

    // Attach UI event callbacks
    lv_obj_add_event_cb(ui_Connect, connect_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_OnStartup, onstartup_checkbox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_EnableAlarmsCheckbox, alarms_checkbox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_CurrentSpinbox, spinbox_changed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_CumulativeSpinbox, spinbox_changed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Set up power management
    setupPowerManagement();

    // Restore saved settings and load alarms 
    preferences.begin("settings", true);
    
    // Load onStartup setting
    bool onStartup = preferences.getBool("onstartup", false);
    
    // Load alarm state explicitly
    bool alarmEnabled = preferences.getBool("alarmEnabled", false); // Default is disabled
    alarmDisabled = !alarmEnabled;
    
    // Log the loaded settings
    DEBUG_PRINTF("Loaded settings: onStartup=%s, alarmEnabled=%s\n", 
                onStartup ? "true" : "false", 
                alarmEnabled ? "true" : "false");
    
    preferences.end();
    
    // Set UI checkbox states (but wait to update the spinbox values until UI task)
    if (onStartup)
        lv_obj_add_state(ui_OnStartup, LV_STATE_CHECKED);
    else
        lv_obj_clear_state(ui_OnStartup, LV_STATE_CHECKED);
    
    // Force LVGL to process UI changes
    lv_timer_handler();

    // Start WiFi timer if auto-connect is enabled
    if (onStartup) {
        DEBUG_PRINTLN("Auto-connect enabled, starting WiFi timer");
        lv_timer_create(wifi_connect_timer_cb, 2000, NULL);
    }

    startTime = millis();
    lastLoop = startTime;
    
    // Create tasks on specific cores
    xTaskCreatePinnedToCore(
        pulseTask,           // Task function
        "PulseTask",         // Task name
        4096,               // Stack size
        NULL,               // Parameters
        1,                  // Priority
        &pulseTaskHandle,   // Task handle
        0                   // Core ID (Core 0)
    );
    
    xTaskCreatePinnedToCore(
        uiTask,             // Task function
        "UITask",           // Task name
        8192,               // Stack size (larger for UI)
        NULL,               // Parameters
        1,                  // Priority
        &uiTaskHandle,      // Task handle
        1                   // Core ID (Core 1)
    );
    
    DEBUG_PRINTLN("Setup completed.");
}

void loop() {
    // Main loop is now empty as tasks handle everything
    vTaskDelay(pdMS_TO_TICKS(1000)); // Prevent watchdog trigger
}

/*******************************************************************************
 * Other Functions (Initialization, UI Setup, etc.)
 ******************************************************************************/ 
void initSerial() {
    Serial.begin(115200);
    delay(50);  // Short delay to allow serial to initialize
    Serial.flush();  // Flush any pending data
    DEBUG_PRINTLN("Initializing Serial...");
}

void initTFT() {
    static TFT_eSPI tft(SCREEN_WIDTH, SCREEN_HEIGHT);
    tft.begin();
    tft.setRotation(1);
    uint16_t calData[5] = {300, 3597, 187, 3579, 6};
    tft.setTouch(calData);
    DEBUG_PRINTLN("TFT initialized.");
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
    DEBUG_PRINTLN("LVGL initialized + UI created.");
}

void updateRealTimeStats(float cpm, float dtSec) {
    // Calculate raw value using the proper conversion factor
    // CONVERSION_FACTOR is 153.8, so we divide CPM by this value to get µSv/h
    float rawCurrentuSvHr = cpm / CONVERSION_FACTOR;
    
    // Add to the smoothing window
    currentReadingsWindow[currentReadingIndex] = rawCurrentuSvHr;
    currentReadingIndex = (currentReadingIndex + 1) % SMOOTHING_WINDOW_SIZE;
    
    // Calculate smoothed value (moving average)
    float sum = 0.0f;
    int validReadings = 0;
    for (int i = 0; i < SMOOTHING_WINDOW_SIZE; i++) {
        if (currentReadingsWindow[i] > 0 || i < currentReadingIndex) {
            sum += currentReadingsWindow[i];
            validReadings++;
        }
    }
    // Avoid division by zero
    currentuSvHr = (validReadings > 0) ? (sum / validReadings) : 0.0f;
    
    // Update average using total counts since start
    unsigned long now = millis();
    float totalTimeMin = (float)(now - startTime) / 60000.0f;
    if (totalTimeMin > 0.0f) {
        float avgCPM = (float)totalCounts / totalTimeMin;
        averageuSvHr = avgCPM / CONVERSION_FACTOR;
    }
    
    // Update maximum if current reading is higher (but only if it seems valid)
    if (currentuSvHr > maxuSvHr && currentuSvHr < 100.0f) { // Sanity check upper limit
        maxuSvHr = currentuSvHr;
    }
    
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
    
    // Update alarm threshold labels on the main screen
    if (ui_CurrentSpinbox && ui_CurrentAlarm) {
        int32_t currentValue = lv_spinbox_get_value(ui_CurrentSpinbox);
        float currentThreshold = (float)currentValue / 10.0f;
        snprintf(buffer, sizeof(buffer), "%.1f", currentThreshold);
        lv_label_set_text(ui_CurrentAlarm, buffer);
    }
    
    if (ui_CumulativeSpinbox && ui_CumulativeAlarm) {
        int32_t cumulativeValue = lv_spinbox_get_value(ui_CumulativeSpinbox);
        float cumulativeThreshold = (float)cumulativeValue / 10.0f;
        snprintf(buffer, sizeof(buffer), "%.1f", cumulativeThreshold);
        lv_label_set_text(ui_CumulativeAlarm, buffer);
    }
}

/*******************************************************************************
 * WiFi/OTA Code
 ******************************************************************************/ 
// Function to serve the OTA warning page
void serveOtaWarningPage() {
    String warningPage = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1'>";
    warningPage += "<title>OTA Update Warning</title>";
    warningPage += "<style>";
    warningPage += "body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 20px; text-align: center; background-color: #0f1317; color: #fff; }";
    warningPage += ".warning-container { max-width: 600px; margin: 0 auto; background-color: #262a36; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.3); }";
    warningPage += "h1 { color: #D8C12C; }";
    warningPage += ".warning-icon { margin-bottom: 20px; }";
    warningPage += ".warning-text { color: #fff; text-align: left; margin: 20px 0; line-height: 1.5; }";
    warningPage += ".btn-container { display: flex; justify-content: center; gap: 20px; margin-top: 20px; }";
    warningPage += ".btn { display: inline-flex; justify-content: center; align-items: center; background-color: #D8C12C; color: #0f1317; padding: 12px 24px; text-decoration: none; border-radius: 5px; font-weight: bold; border: none; cursor: pointer; font-size: 16px; min-width: 200px; height: 50px; box-sizing: border-box; }";
    warningPage += ".btn:hover { background-color: #7274ee; color: #fff; }";
    warningPage += ".back-btn { background-color: #262a36; color: #fff; border: 1px solid #7274ee; }";
    warningPage += ".back-btn:hover { background-color: #7274ee; }";
    warningPage += "form { display: inline-block; margin: 0; }";
    warningPage += "button.btn { width: 100%; }";
    warningPage += "strong { color: #D8C12C; }";
    warningPage += "ul { color: #fff; }";
    warningPage += "</style>";
    warningPage += "</head><body>";
    warningPage += "<div class='warning-container'>";
    warningPage += "<div class='warning-icon'><svg xmlns='http://www.w3.org/2000/svg' width='64' height='64' viewBox='0 0 24 24'><path fill='#D8C12C' d='M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-2h2v2zm0-4h-2V7h2v6z'/></svg></div>";
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
    warningPage += "<form method='post' action='/acknowledge-ota' style='width: 200px;'><button type='submit' class='btn'>I Understand, Proceed</button></form>";
    warningPage += "</div>";
    warningPage += "</div></body></html>";
    
    server.send(200, "text/html", warningPage);
}

// Update the WiFi/OTA initialization in connect_btn_event_cb
static void connect_btn_event_cb(lv_event_t *e) {
    const char* ssid = lv_textarea_get_text(ui_SSID);
    const char* password = lv_textarea_get_text(ui_PASSWORD);
    DEBUG_PRINTF("Attempting to connect to SSID: %s\n", ssid);
    lv_label_set_text(ui_WIFIINFO, "Connecting");
    
    if (connectToWiFi(ssid, password)) {
        DEBUG_PRINTLN("WiFi connected.");
        wifi_ip = WiFi.localIP().toString();
        
        // Initialize mDNS responder
        if (MDNS.begin("radiation")) {
            DEBUG_PRINTLN("mDNS responder started - Device accessible at http://radiation.local");
        } else {
            DEBUG_PRINTLN("Error setting up mDNS responder!");
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
        } else {
            String info = String("IP: ") + wifi_ip + "\nTime: NTP Failed";
            lv_label_set_text(ui_WIFIINFO, info.c_str());
            DEBUG_PRINTLN("NTP time sync failed");
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
            DEBUG_PRINTLN("OTA initialized with warning page.");
            
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
        DEBUG_PRINTLN("WiFi connection failed.");
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
        DEBUG_PRINTF("Found saved credentials. Trying to connect to: %s\n", storedSsid.c_str());
        
        // Update the info text while connecting
        lv_label_set_text(ui_WIFIINFO, "Connecting...");
        
        // Process the UI update immediately
        lv_timer_handler();
        
        if (connectToWiFi(storedSsid.c_str(), storedPassword.c_str())) {
            DEBUG_PRINTLN("Auto WiFi connection successful.");
            wifi_ip = WiFi.localIP().toString();
            
            // Initialize mDNS responder
            if (MDNS.begin("radiation")) {
                DEBUG_PRINTLN("mDNS responder started - Device accessible at http://radiation.local");
            } else {
                DEBUG_PRINTLN("Error setting up mDNS responder!");
            }
            
            configTime(0, 0, "pool.ntp.org", "time.nist.gov");
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                char timeStr[64];
                strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
                String info = String("IP: ") + wifi_ip + "\nTime: " + timeStr + " UTC";
                lv_label_set_text(ui_WIFIINFO, info.c_str());
            } else {
                String info = String("IP: ") + wifi_ip + "\nTime: NTP Failed";
                lv_label_set_text(ui_WIFIINFO, info.c_str());
                DEBUG_PRINTLN("NTP time sync failed");
            }
            
            // Initialize OTA and web server if needed
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
                DEBUG_PRINTLN("OTA initialized with warning page.");
                
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
            DEBUG_PRINTLN("Auto WiFi connection failed.");
            lv_label_set_text(ui_WIFIINFO, "Disconnected\nFailed");
        }
    } else {
        DEBUG_PRINTLN("No stored credentials found.");
        lv_label_set_text(ui_WIFIINFO, "No credentials");
    }
    
    // Only now load the initial screen after all connection attempts
    DEBUG_PRINTLN("Loading InitialScreen after connection attempt");
    lv_scr_load(ui_InitialScreen);
}

static void wifi_connect_timer_cb(lv_timer_t * timer) {
    // Check if auto-connect is enabled
    if(lv_obj_has_state(ui_OnStartup, LV_STATE_CHECKED)) {
        DEBUG_PRINTLN("OnStartup enabled. Attempting auto WiFi connection...");
        tryAutoConnect(); // This function will handle screen transitions
    } else {
        DEBUG_PRINTLN("OnStartup disabled. Loading InitialScreen immediately.");
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
