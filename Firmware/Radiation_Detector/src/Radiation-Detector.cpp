/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "ui.h"  // Must declare/extern:
               //   ui_Chart1, ui_Chart3, ui_CurrentRad, ui_AverageRad, ui_MaximumRad, ui_CumulativeRad,
               //   ui_SSID, ui_PASSWORD, ui_WIFIINFO, ui_Connect,
               //   ui_Startup, ui_InitialScreen, ui_OnStartup,
               //   ui_CurrentAlarmSlider, ui_CumulativeAlarmSlider,
               //   ui_CurrentAlarmLabel, ui_CumulativeAlarmLabel
#include <vector>
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>
#include "dashboard.h"
// OTA includes
#include <WebServer.h>
#include <ElegantOTA.h>

/*******************************************************************************
 * Definitions & Constants
 ******************************************************************************/
static const uint16_t SCREEN_WIDTH  = 480;
static const uint16_t SCREEN_HEIGHT = 320;

static const uint8_t GEIGER_PULSE_PIN = 9;
static const uint8_t BUZZER_PIN       = 38; // Use LEDC output pin

static const float CONVERSION_FACTOR = 153.8f;

static const int CHART1_SEGMENTS    = 20;
static const int CHART1_SEGMENT_MIN = 3;   // 3-minute segments
static const int CHART3_SEGMENTS    = 24;
static const int CHART3_SEGMENT_HR  = 1;   // 1-hour segments

// Alarm tone parameters (in Hz and ms)
#define ALARM_FREQ1 1000
#define ALARM_FREQ2 1500
#define ALARM_TONE_DURATION 200   // tone duration (ms)
#define ALARM_PAUSE_DURATION 50  // pause between tones (ms)

/*******************************************************************************
 * Global Variables
 ******************************************************************************/
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ SCREEN_WIDTH * SCREEN_HEIGHT / 10 ];

static std::vector<unsigned long> events;
static unsigned long totalCounts = 0;
static unsigned long startTime   = 0;
static unsigned long lastLoop    = 0;

float currentuSvHr      = 0.0f;
float averageuSvHr      = 0.0f;
float maxuSvHr          = 0.0f;
float cumulativeDosemSv = 0.0f;

static float chart1Data[CHART1_SEGMENTS] = {0.0f};
static float chart3Data[CHART3_SEGMENTS] = {0.0f};

static float partialSum3Min = 0.0f;
static int   minuteCount3   = 0;
static float partialSum60Min = 0.0f;
static int   hourCount       = 0;

// UI objects declared in ui.h:
extern lv_obj_t* ui_CurrentRad;
extern lv_obj_t* ui_AverageRad;
extern lv_obj_t* ui_MaximumRad;
extern lv_obj_t* ui_CumulativeRad;
extern lv_obj_t* ui_Chart1;
extern lv_obj_t* ui_Chart3;

extern lv_obj_t* ui_Startup;       // Startup screen (shows while connecting)
extern lv_obj_t* ui_InitialScreen; // Screen shown after connection attempt
extern lv_obj_t* ui_OnStartup;     // Checkbox for auto–connect on startup
extern lv_obj_t* ui_CurrentAlarmSlider;
extern lv_obj_t* ui_CumulativeAlarmSlider;
extern lv_obj_t* ui_CurrentAlarmLabel;
extern lv_obj_t* ui_CumulativeAlarmLabel;

static lv_chart_series_t* chart1Series = nullptr;
static lv_chart_series_t* chart3Series = nullptr;

// For the LEDC–based alarm tone
bool alarmToneToggle = false;
unsigned long lastAlarmMillis = 0;

// Interrupt variables for the Geiger counter pulse input
static volatile bool newPulseFlag = false;
static volatile unsigned long lastInterruptMs = 0;
static const unsigned long PULSE_LOCKOUT_MS  = 5;

// Preferences (for storing WiFi credentials and settings)
Preferences preferences;
static String wifi_ip = "";

// OTA support global variables
WebServer server(80);
bool otaInitialized = false;

/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/
void initSerial();
void initTFT();
void initLVGL();
void assignChartSeries();
void initPulsePin();
void IRAM_ATTR geigerPulseISR();
void initBuzzer();
void playAlarmTone(uint32_t freq);
void stopAlarmTone();
void handleNewPulses();
void removeOldEvents();
float getRealTimeCPM();
void updateRealTimeStats(float cpm, float dtSec);
void updateLabels();
void updateChartsEveryMinute();
void updateChart1RealTime();
void updateChart3RealTime();
void shiftChart1Left();
void shiftChart3Left();
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
    // Initialize LEDC channel 0 (8-bit resolution) on BUZZER_PIN.
    ledcSetup(0, 2000, 8);  // default frequency of 2000Hz (will be reconfigured before each tone)
    ledcAttachPin(BUZZER_PIN, 0);
    ledcWrite(0, 0);        // start with no sound
}

void playAlarmTone(uint32_t freq) {
    // Reconfigure channel 0 with the given frequency
    ledcSetup(0, freq, 8);
    // Set 50% duty cycle (range 0–255) for an audible tone
    ledcWrite(0, 128);
}

void stopAlarmTone() {
    ledcWrite(0, 0);
}

/*******************************************************************************
 * Slider Event Callbacks (to update alarm threshold labels)
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
 * This function compares the measured radiation values with the thresholds
 * set via the sliders. If either threshold is exceeded, it alternates between
 * two tones using LEDC.
 ******************************************************************************/
void checkAlarms() {
    int currentThreshold = lv_slider_get_value(ui_CurrentAlarmSlider);
    int cumulativeThreshold = lv_slider_get_value(ui_CumulativeAlarmSlider);

    // (Ensure labels reflect the slider values)
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", currentThreshold);
    lv_label_set_text(ui_CurrentAlarmLabel, buf);
    snprintf(buf, sizeof(buf), "%d", cumulativeThreshold);
    lv_label_set_text(ui_CumulativeAlarmLabel, buf);

    // Check if either measurement exceeds its alarm threshold.
    bool condition = (currentuSvHr > currentThreshold) || (cumulativeDosemSv > cumulativeThreshold);
    unsigned long now = millis();
    if (condition) {
        if (now - lastAlarmMillis >= (ALARM_TONE_DURATION + ALARM_PAUSE_DURATION)) {
            // Stop tone briefly to smooth transition
            stopAlarmTone();
            // Alternate between the two alarm frequencies.
            if (alarmToneToggle)
                playAlarmTone(ALARM_FREQ1);
            else
                playAlarmTone(ALARM_FREQ2);
            alarmToneToggle = !alarmToneToggle;
            lastAlarmMillis = now;
        }
    } else {
        stopAlarmTone();
    }
}

/*******************************************************************************
 * setup()
 ******************************************************************************/
void setup() {
    initSerial();
    initTFT();
    initLVGL();
    assignChartSeries();
    initPulsePin();
    initBuzzer();

    // Attach event callbacks
    lv_obj_add_event_cb(ui_Connect, connect_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_OnStartup, onstartup_checkbox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_CurrentAlarmSlider, current_alarm_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_CumulativeAlarmSlider, cumulative_alarm_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Restore saved state for OnStartup checkbox
    preferences.begin("settings", true);
    bool onStartup = preferences.getBool("onstartup", false);
    preferences.end();
    if(onStartup)
        lv_obj_add_state(ui_OnStartup, LV_STATE_CHECKED);
    else
        lv_obj_clear_state(ui_OnStartup, LV_STATE_CHECKED);

    // Load the Startup screen by default.
    lv_scr_load(ui_Startup);
    // Delay auto–WiFi connection attempt until after the UI loads.
    lv_timer_create(wifi_connect_timer_cb, 2000, NULL);

    startTime = millis();
    lastLoop  = startTime;
    Serial.println("Setup completed.");
}

/*******************************************************************************
 * loop()
 ******************************************************************************/
void loop() {
    lv_timer_handler();

    handleNewPulses();
    removeOldEvents();

    unsigned long now = millis();
    float dtSec = (float)(now - lastLoop) / 1000.0f;
    lastLoop = now;

    float cpm = getRealTimeCPM();
    updateRealTimeStats(cpm, dtSec);
    updateLabels();
    updateChartsEveryMinute();
    updateChart1RealTime();
    updateChart3RealTime();

    // Check if alarm thresholds are exceeded and play tone if needed.
    checkAlarms();

    // Update WiFi info once per second if connected.
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

    // Handle OTA update requests.
    if (otaInitialized) {
        server.handleClient();
    }
}

/*******************************************************************************
 * Other Functions (Initialization, Sensor Processing, UI Updates)
 ******************************************************************************/
void initSerial() {
    Serial.begin(115200);
    Serial.println("Initializing Serial...");
}

void initTFT() {
    static TFT_eSPI tft(SCREEN_WIDTH, SCREEN_HEIGHT);
    tft.begin();
    tft.setRotation(1);
    uint16_t calData[5] = { 300, 3597, 187, 3579, 6 };
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
        if(!initted) {
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
        if(!inittedTouch) {
            tftTouch.begin();
            tftTouch.setRotation(1);
            uint16_t cData[5] = {267, 3646, 198, 3669, 6};
            tftTouch.setTouch(cData);
            inittedTouch = true;
        }
        uint16_t x, y;
        bool touched = tftTouch.getTouch(&y, &x, 600);
        if(!touched)
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
    if(!chart1Series)
        Serial.println("Warning: ui_Chart1 has no series!");
    if(!chart3Series)
        Serial.println("Warning: ui_Chart3 has no series!");
    for (int i = 0; i < CHART1_SEGMENTS; i++)
        lv_chart_set_value_by_id(ui_Chart1, chart1Series, i, 0);
    lv_chart_refresh(ui_Chart1);
    for (int i = 0; i < CHART3_SEGMENTS; i++)
        lv_chart_set_value_by_id(ui_Chart3, chart3Series, i, 0);
    lv_chart_refresh(ui_Chart3);
    Serial.println("Chart series assigned.");
}

void initPulsePin() {
    pinMode(GEIGER_PULSE_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(GEIGER_PULSE_PIN), geigerPulseISR, RISING);
    Serial.println("Geiger pulse pin set for interrupt on RISING edge.");
}

void IRAM_ATTR geigerPulseISR() {
    unsigned long nowMs = millis();
    if(nowMs - lastInterruptMs > PULSE_LOCKOUT_MS) {
        newPulseFlag = true;
        lastInterruptMs = nowMs;
    }
}

void handleNewPulses() {
    if(newPulseFlag) {
        newPulseFlag = false;
        unsigned long now = millis();
        events.push_back(now);
        totalCounts++;
    }
}

void removeOldEvents() {
    unsigned long now = millis();
    while(!events.empty() && (now - events.front()) > 60000UL)
        events.erase(events.begin());
}

float getRealTimeCPM() {
    return (float)events.size();
}

void updateRealTimeStats(float cpm, float dtSec) {
    currentuSvHr = cpm / CONVERSION_FACTOR;
    unsigned long now = millis();
    float totalTimeMin = (float)(now - startTime) / 60000.0f;
    if(totalTimeMin > 0.0f) {
        float avgCPM = (float)totalCounts / totalTimeMin;
        averageuSvHr = avgCPM / CONVERSION_FACTOR;
    }
    if(currentuSvHr > maxuSvHr)
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

void updateChartsEveryMinute() {
    static unsigned long lastMinuteMark = 0;
    unsigned long now = millis();
    if((now - lastMinuteMark) < 60000UL)
        return;
    lastMinuteMark = now;
    partialSum3Min  += currentuSvHr;
    partialSum60Min += currentuSvHr;
    minuteCount3++;
    hourCount++;
    if(minuteCount3 >= CHART1_SEGMENT_MIN) {
        float finalAvg = (minuteCount3 > 0) ? (partialSum3Min / (float)minuteCount3) : 0.0f;
        shiftChart1Left();
        chart1Data[CHART1_SEGMENTS - 1] = finalAvg;
        for (int i = 0; i < CHART1_SEGMENTS; i++)
            lv_chart_set_value_by_id(ui_Chart1, chart1Series, i, (lv_coord_t)chart1Data[i]);
        lv_chart_refresh(ui_Chart1);
        partialSum3Min = 0.0f;
        minuteCount3   = 0;
    }
    if(hourCount >= 60) {
        float finalAvg = (hourCount > 0) ? (partialSum60Min / (float)hourCount) : 0.0f;
        shiftChart3Left();
        chart3Data[CHART3_SEGMENTS - 1] = finalAvg;
        for (int i = 0; i < CHART3_SEGMENTS; i++)
            lv_chart_set_value_by_id(ui_Chart3, chart3Series, i, (lv_coord_t)chart3Data[i]);
        lv_chart_refresh(ui_Chart3);
        partialSum60Min = 0.0f;
        hourCount       = 0;
    }
}

void updateChart1RealTime() {
    float partialAvg = (minuteCount3 > 0) ? (partialSum3Min / (float)minuteCount3) : 0.0f;
    chart1Data[CHART1_SEGMENTS - 1] = partialAvg;
    lv_chart_set_value_by_id(ui_Chart1, chart1Series, CHART1_SEGMENTS - 1, (lv_coord_t)partialAvg);
    lv_chart_refresh(ui_Chart1);
}

void updateChart3RealTime() {
    float partialAvg = (hourCount > 0) ? (partialSum60Min / (float)hourCount) : 0.0f;
    chart3Data[CHART3_SEGMENTS - 1] = partialAvg;
    lv_chart_set_value_by_id(ui_Chart3, chart3Series, CHART3_SEGMENTS - 1, (lv_coord_t)partialAvg);
    lv_chart_refresh(ui_Chart3);
}

void shiftChart1Left() {
    for (int i = 0; i < CHART1_SEGMENTS - 1; i++)
        chart1Data[i] = chart1Data[i + 1];
    chart1Data[CHART1_SEGMENTS - 1] = 0.0f;
}

void shiftChart3Left() {
    for (int i = 0; i < CHART3_SEGMENTS - 1; i++)
        chart3Data[i] = chart3Data[i + 1];
    chart3Data[CHART3_SEGMENTS - 1] = 0.0f;
}

static void connect_btn_event_cb(lv_event_t *e) {
    const char* ssid = lv_textarea_get_text(ui_SSID);
    const char* password = lv_textarea_get_text(ui_PASSWORD);
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    lv_label_set_text(ui_WIFIINFO, "Connecting");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    int max_attempts = 20;
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
        delay(500);
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
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
        // Initialize OTA if not already done
        if (!otaInitialized) {
            server.begin();
            ElegantOTA.begin(&server);
            otaInitialized = true;
            Serial.println("OTA initialized.");
            
            // Define the route for the dashboard.
            server.on("/", [](){
                String dashboard = getDashboardPage();
                server.send(200, "text/html", dashboard);
            });
        }  // <-- Missing bracket fixed here.
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
        WiFi.mode(WIFI_STA);
        WiFi.begin(storedSsid.c_str(), storedPassword.c_str());
        int max_attempts = 20;
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
            delay(500);
            attempts++;
        }
        if (WiFi.status() == WL_CONNECTED) {
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
            // Initialize OTA if not already done
            if (!otaInitialized) {
                server.begin();
                ElegantOTA.begin(&server);
                otaInitialized = true;
                Serial.println("OTA initialized.");
                
                // Define the route for the dashboard.
                server.on("/", [](){
                    String dashboard = getDashboardPage();
                    server.send(200, "text/html", dashboard);
                });
            }
        } else {
            Serial.println("Auto WiFi connection failed.");
            lv_label_set_text(ui_WIFIINFO, "Disconnected\nFailed");
        }
    }
    else {
        Serial.println("No stored credentials found.");
    }
    lv_scr_load(ui_InitialScreen);
}

static void wifi_connect_timer_cb(lv_timer_t * timer) {
    if(lv_obj_has_state(ui_OnStartup, LV_STATE_CHECKED)) {
        Serial.println("OnStartup enabled. Attempting auto WiFi connection...");
        tryAutoConnect();
    }
    else {
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
