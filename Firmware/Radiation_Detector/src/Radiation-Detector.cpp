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
#include "driver/pcnt.h"  // ESP32 pulse counter driver

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
static const int CHART1_SEGMENTS         = 20;
static const int CHART1_INTERVAL_SECONDS = 180; // 3 minutes = 180 s

// Chart3: 24 segments, each representing a 1-hour average (24 hours total)
static const int CHART3_SEGMENTS         = 24;
static const int CHART3_INTERVAL_SECONDS = 3600; // 1 hour = 3600 s

// Alarm tone parameters (in Hz and ms)
#define ALARM_FREQ1 1000
#define ALARM_FREQ2 1500
#define ALARM_TONE_DURATION 200   ///< Tone duration in ms
#define ALARM_PAUSE_DURATION 50   ///< Pause between tones in ms

// PCNT parameters – note the PCNT hardware counter is 16-bit
static const pcnt_unit_t PCNT_UNIT = PCNT_UNIT_0;
static const int PULSE_HISTORY_SIZE = 60;   ///< 60 one-second intervals in a ring buffer

/*******************************************************************************
 * Global Variables
 ******************************************************************************/ 

// LVGL display buffer and UI chart series
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ SCREEN_WIDTH * SCREEN_HEIGHT / 10 ];

// Ring buffers for chart data
static float chart1Buffer[CHART1_SEGMENTS] = {0.0f}; ///< 3‑minute averages for Chart1
static float chart3Buffer[CHART3_SEGMENTS] = {0.0f}; ///< 1‑hour averages for Chart3
static int   chart1Index = 0;  ///< Write index for Chart1
static int   chart3Index = 0;  ///< Write index for Chart3

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

extern lv_obj_t* ui_Startup;       ///< Startup screen (shown while connecting)
extern lv_obj_t* ui_InitialScreen; ///< Screen shown after connection attempt
extern lv_obj_t* ui_OnStartup;     ///< Checkbox for auto–connect on startup
extern lv_obj_t* ui_CurrentAlarmSlider;
extern lv_obj_t* ui_CumulativeAlarmSlider;
extern lv_obj_t* ui_CurrentAlarmLabel;
extern lv_obj_t* ui_CumulativeAlarmLabel;
extern lv_obj_t* ui_SSID;
extern lv_obj_t* ui_PASSWORD;
extern lv_obj_t* ui_WIFIINFO;
extern lv_obj_t* ui_Connect;

// Radiation measurement variables
float currentuSvHr      = 0.0f; ///< Instantaneous dose rate (µSv/h)
float averageuSvHr      = 0.0f; ///< Average dose rate (µSv/h)
float maxuSvHr          = 0.0f; ///< Maximum dose rate (µSv/h)
float cumulativeDosemSv = 0.0f; ///< Cumulative dose (mSv)

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
void updateChart1(float average);
void updateChart3(float average);
void drawChart1();
void drawChart3();
bool connectToWiFi(const char* ssid, const char* password);
static void connect_btn_event_cb(lv_event_t *e);
void tryAutoConnect();
static void wifi_connect_timer_cb(lv_timer_t * timer);
static void onstartup_checkbox_event_cb(lv_event_t * e);
static void current_alarm_slider_event_cb(lv_event_t * e);
static void cumulative_alarm_slider_event_cb(lv_event_t * e);
void checkAlarms();

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
 * Slider Event Callbacks
 ******************************************************************************/ 
static void current_alarm_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", value);
    lv_label_set_text(ui_CurrentAlarmLabel, buf);
}

static void cumulative_alarm_slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int value = lv_slider_get_value(slider);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", value);
    lv_label_set_text(ui_CumulativeAlarmLabel, buf);
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

    int currentThreshold = lv_slider_get_value(ui_CurrentAlarmSlider);
    int cumulativeThreshold = lv_slider_get_value(ui_CumulativeAlarmSlider);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", currentThreshold);
    lv_label_set_text(ui_CurrentAlarmLabel, buf);
    snprintf(buf, sizeof(buf), "%d", cumulativeThreshold);
    lv_label_set_text(ui_CumulativeAlarmLabel, buf);

    bool condition = (currentuSvHr > currentThreshold) || (cumulativeDosemSv > cumulativeThreshold);
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
    for (int i = 0; i < CHART1_SEGMENTS; i++) {
        chart1Buffer[i] = 0.0f;
    }
    chart1Index = 0;
    chart1AccumulatedCpm = 0.0f;
    chart1ElapsedSec = 0.0f;

    for (int i = 0; i < CHART3_SEGMENTS; i++) {
        chart3Buffer[i] = 0.0f;
    }
    chart3Index = 0;
    chart3AccumulatedCpm = 0.0f;
    chart3ElapsedSec = 0.0f;

    drawChart1();
    drawChart3();
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
    // Accumulate for Chart1
    chart1AccumulatedCpm += (cpm * dtSec);
    chart1ElapsedSec     += dtSec;
    if (chart1ElapsedSec >= CHART1_INTERVAL_SECONDS) {
        float avgCpm = chart1AccumulatedCpm / chart1ElapsedSec;
        updateChart1(avgCpm);
        chart1AccumulatedCpm = 0.0f;
        chart1ElapsedSec = 0.0f;
    }

    // Accumulate for Chart3
    chart3AccumulatedCpm += (cpm * dtSec);
    chart3ElapsedSec     += dtSec;
    if (chart3ElapsedSec >= CHART3_INTERVAL_SECONDS) {
        float avgCpm = chart3AccumulatedCpm / chart3ElapsedSec;
        updateChart3(avgCpm);
        chart3AccumulatedCpm = 0.0f;
        chart3ElapsedSec = 0.0f;
    }
}

/**
 * @brief Updates Chart1 ring buffer with the computed average and redraws.
 *
 * @param average The computed average CPM for the interval.
 */
void updateChart1(float average) {
    float value = average / CONVERSION_FACTOR; // Convert CPM to µSv/h
    if (chart1Index < CHART1_SEGMENTS) {
        chart1Buffer[chart1Index] = value;
        chart1Index++;
    } else {
        for (int i = 0; i < CHART1_SEGMENTS - 1; i++) {
            chart1Buffer[i] = chart1Buffer[i + 1];
        }
        chart1Buffer[CHART1_SEGMENTS - 1] = value;
    }
    drawChart1();
}

/**
 * @brief Updates Chart3 ring buffer with the computed average and redraws.
 *
 * @param average The computed average CPM for the interval.
 */
void updateChart3(float average) {
    float value = average / CONVERSION_FACTOR;
    if (chart3Index < CHART3_SEGMENTS) {
        chart3Buffer[chart3Index] = value;
        chart3Index++;
    } else {
        for (int i = 0; i < CHART3_SEGMENTS - 1; i++) {
            chart3Buffer[i] = chart3Buffer[i + 1];
        }
        chart3Buffer[CHART3_SEGMENTS - 1] = value;
    }
    drawChart3();
}

/**
 * @brief Draws Chart1 using the current ring buffer values.
 */
void drawChart1() {
    if (!chart1Series) return;
    for (int i = 0; i < CHART1_SEGMENTS; i++) {
        lv_chart_set_value_by_id(ui_Chart1, chart1Series, i, (lv_coord_t)chart1Buffer[i]);
    }
    lv_chart_refresh(ui_Chart1);
}

/**
 * @brief Draws Chart3 using the current ring buffer values.
 */
void drawChart3() {
    if (!chart3Series) return;
    for (int i = 0; i < CHART3_SEGMENTS; i++) {
        lv_chart_set_value_by_id(ui_Chart3, chart3Series, i, (lv_coord_t)chart3Buffer[i]);
    }
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
    assignChartSeries();
    clearCharts(); // Zero out chart data

    initPulseCounter();
    initBuzzer();

    // Attach UI event callbacks
    lv_obj_add_event_cb(ui_Connect, connect_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_OnStartup, onstartup_checkbox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_CurrentAlarmSlider, current_alarm_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_CumulativeAlarmSlider, cumulative_alarm_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Restore saved startup state
    preferences.begin("settings", true);
    bool onStartup = preferences.getBool("onstartup", false);
    preferences.end();
    if(onStartup)
        lv_obj_add_state(ui_OnStartup, LV_STATE_CHECKED);
    else
        lv_obj_clear_state(ui_OnStartup, LV_STATE_CHECKED);

    // Load startup screen and start WiFi timer
    lv_scr_load(ui_Startup);
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

void assignChartSeries() {
    chart1Series = lv_chart_get_series_next(ui_Chart1, NULL);
    chart3Series = lv_chart_get_series_next(ui_Chart3, NULL);
    if (!chart1Series)
        Serial.println("Warning: ui_Chart1 has no series!");
    if (!chart3Series)
        Serial.println("Warning: ui_Chart3 has no series!");
    for (int i = 0; i < CHART1_SEGMENTS; i++) {
        lv_chart_set_value_by_id(ui_Chart1, chart1Series, i, 0);
    }
    lv_chart_refresh(ui_Chart1);
    for (int i = 0; i < CHART3_SEGMENTS; i++) {
        lv_chart_set_value_by_id(ui_Chart3, chart3Series, i, 0);
    }
    lv_chart_refresh(ui_Chart3);
    Serial.println("Chart series assigned.");
}

void updateRealTimeStats(float cpm, float dtSec) {
    currentuSvHr = cpm / CONVERSION_FACTOR;
    unsigned long now = millis();
    float totalTimeMin = (float)(now - startTime) / 60000.0f;
    if (totalTimeMin > 0.0f) {
        float avgCPM = (float)totalCounts / totalTimeMin;
        averageuSvHr = avgCPM / CONVERSION_FACTOR;
    }
    if (currentuSvHr > maxuSvHr)
        maxuSvHr = currentuSvHr;
    float doseIncrement_mSv = (currentuSvHr / 3600.0f) * dtSec / 1000.0f;
    cumulativeDosemSv += doseIncrement_mSv;
}

void updateLabels() {
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.2f", currentuSvHr);
    lv_label_set_text(ui_CurrentRad, buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", averageuSvHr);
    lv_label_set_text(ui_AverageRad, buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", maxuSvHr);
    lv_label_set_text(ui_MaximumRad, buffer);
    snprintf(buffer, sizeof(buffer), "%.2f", cumulativeDosemSv);
    lv_label_set_text(ui_CumulativeRad, buffer);
}

/*******************************************************************************
 * WiFi/OTA Code
 ******************************************************************************/ 
static void connect_btn_event_cb(lv_event_t *e) {
    const char* ssid = lv_textarea_get_text(ui_SSID);
    const char* password = lv_textarea_get_text(ui_PASSWORD);
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    lv_label_set_text(ui_WIFIINFO, "Connecting");
    
    if (connectToWiFi(ssid, password)) {
        Serial.println("WiFi connected.");
        wifi_ip = WiFi.localIP().toString();
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
            ElegantOTA.begin(&server);
            otaInitialized = true;
            Serial.println("OTA initialized.");
            server.on("/", [](){
                String dashboard = getDashboardPage();
                server.send(200, "text/html", dashboard);
            });
            
            // Add JSON API endpoint
            server.on("/api/data", [](){
                server.send(200, "application/json", getRadiationDataJson());
            });
        }
    } else {
        Serial.println("WiFi connection failed.");
        lv_label_set_text(ui_WIFIINFO, "Disconnected\nFailed");
    }
}

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
                ElegantOTA.begin(&server);
                otaInitialized = true;
                Serial.println("OTA initialized.");
                server.on("/", [](){
                    String dashboard = getDashboardPage();
                    server.send(200, "text/html", dashboard);
                });
                
                // Add JSON API endpoint
                server.on("/api/data", [](){
                    server.send(200, "application/json", getRadiationDataJson());
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
