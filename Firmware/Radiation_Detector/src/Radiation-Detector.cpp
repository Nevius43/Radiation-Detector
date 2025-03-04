/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "ui.h"  // Must declare/extern: ui_Chart1, ui_Chart3, ui_CurrentRad, ui_AverageRad, ui_MaximumRad, ui_CumulativeRad
               // Also: ui_SSID, ui_PASSWORD, ui_WIFIINFO, ui_Connect
#include <vector>
#include <WiFi.h>
#include <time.h>
#include <EEPROM.h>  // For storing WiFi credentials

/*******************************************************************************
 * Definitions & Constants
 ******************************************************************************/
static const uint16_t SCREEN_WIDTH  = 480;
static const uint16_t SCREEN_HEIGHT = 320;

/* LVGL draw buffer */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ SCREEN_WIDTH * SCREEN_HEIGHT / 10 ];

/* Geiger counter pulse input and buzzer settings */
static const uint8_t GEIGER_PULSE_PIN = 9;
static const uint8_t BUZZER_PIN       = 38;
static const unsigned int BEEP_FREQ   = 40;  // 40 Hz (typically inaudible)
static const unsigned long BEEP_MS    = 50;   // beep duration (ms)

/* Conversion factor for CPM -> µSv/h */
static const float CONVERSION_FACTOR = 153.8f;

/* Rolling 1-minute list of pulse timestamps */
static std::vector<unsigned long> events;

/* All-time counts & timing */
static unsigned long totalCounts = 0;
static unsigned long startTime   = 0;
static unsigned long lastLoop    = 0;

/* Real-time radiation variables for label display */
static float currentuSvHr      = 0.0f;
static float averageuSvHr      = 0.0f;
static float maxuSvHr          = 0.0f;
static float cumulativeDosemSv = 0.0f;

/*******************************************************************************
 * Chart Configuration
 *   - ui_Chart1: 20 bars → 3 min each → 60 min total
 *   - ui_Chart3: 24 bars → 1 hour each → 24 hours total
 ******************************************************************************/
static const int CHART1_SEGMENTS    = 20;
static const int CHART1_SEGMENT_MIN = 3;   // 3-minute segments
static const int CHART3_SEGMENTS    = 24;
static const int CHART3_SEGMENT_HR  = 1;   // 1-hour segments

/* Arrays for final bar values */
static float chart1Data[CHART1_SEGMENTS] = {0.0f};
static float chart3Data[CHART3_SEGMENTS] = {0.0f};

/* Partial sums for building each bar segment */
static float partialSum3Min = 0.0f;
static int   minuteCount3   = 0;
static float partialSum60Min = 0.0f;
static int   hourCount       = 0;

/* LVGL Objects for charts and radiation values (from your UI) */
extern lv_obj_t* ui_CurrentRad;
extern lv_obj_t* ui_AverageRad;
extern lv_obj_t* ui_MaximumRad;
extern lv_obj_t* ui_CumulativeRad;
extern lv_obj_t* ui_Chart1;
extern lv_obj_t* ui_Chart3;

/* Single data series per chart */
static lv_chart_series_t* chart1Series = nullptr;
static lv_chart_series_t* chart3Series = nullptr;

/*******************************************************************************
 * Interrupt-Related Variables
 ******************************************************************************/
static volatile bool newPulseFlag = false;         // set by ISR, cleared in loop
static volatile unsigned long lastInterruptMs = 0;   // for lockout
static const unsigned long PULSE_LOCKOUT_MS  = 5;    // ignore pulses within 5ms

/*******************************************************************************
 * EEPROM & WiFi Credentials
 ******************************************************************************/
#define EEPROM_SIZE 512
// Maximum lengths (including terminating null) for credentials
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64

struct WifiCredentials {
    char ssid[MAX_SSID_LEN];
    char password[MAX_PASS_LEN];
};

// Global instance to hold credentials
WifiCredentials wifiCred;

// Global variable for WiFi IP address
static String wifi_ip = "";

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

// WiFi connection event callback
static void connect_btn_event_cb(lv_event_t *e);

// Function to try connecting with saved credentials
void tryAutoConnect();

//////////////////////////////////////////////////////////////////////////////
// setup()
//////////////////////////////////////////////////////////////////////////////
void setup()
{
    initSerial();
    EEPROM.begin(EEPROM_SIZE);  // Initialize EEPROM storage
    initTFT();
    initLVGL();
    assignChartSeries();

    // Initialize Geiger counter hardware
    initPulsePin();
    initBuzzer();

    // Attach WiFi Connect callback to the existing ui_Connect button
    lv_obj_add_event_cb(ui_Connect, connect_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // Try auto-connecting if credentials are saved
    tryAutoConnect();

    startTime = millis();
    lastLoop  = startTime;

    Serial.println("Setup completed.");
}

//////////////////////////////////////////////////////////////////////////////
// loop()
//////////////////////////////////////////////////////////////////////////////
void loop()
{
    // Process LVGL tasks
    lv_timer_handler();

    // Process Geiger pulses and update radiation stats
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

    // --- Update WiFi time every second if connected ---
    static unsigned long lastTimeUpdate = 0;
    if (WiFi.status() == WL_CONNECTED && (now - lastTimeUpdate >= 1000)) {
        lastTimeUpdate = now;
        struct tm timeinfo;
        if (getLocalTime(&timeinfo)) {
            char timeStr[64];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
            // Append " UTC" after the time
            String info = String("IP: ") + wifi_ip + "\nTime: " + timeStr + " UTC";
            lv_label_set_text(ui_WIFIINFO, info.c_str());
        } else {
            String info = String("IP: ") + wifi_ip + "\nTime: NTP Failed";
            lv_label_set_text(ui_WIFIINFO, info.c_str());
        }
    }
    // Optional short delay for stability
    // delay(5);
}

//////////////////////////////////////////////////////////////////////////////
// Initialization Functions
//////////////////////////////////////////////////////////////////////////////
void initSerial()
{
    Serial.begin(115200);
    Serial.println("Initializing Serial...");
}

void initTFT()
{
    static TFT_eSPI tft(SCREEN_WIDTH, SCREEN_HEIGHT);
    tft.begin();
    tft.setRotation(1);

    // Example touch calibration (adjust as needed)
    uint16_t calData[5] = {267, 3646, 198, 3669, 6};
    tft.setTouch(calData);

    Serial.println("TFT initialized.");
}

void initLVGL()
{
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * SCREEN_HEIGHT / 10);

    // Minimal flush callback for LVGL
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = [](lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
    {
        static TFT_eSPI tftDisp(SCREEN_WIDTH, SCREEN_HEIGHT);
        static bool initted = false;
        if(!initted){
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

    // (Optional) Simple touch driver for LVGL
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = [](lv_indev_drv_t *drv, lv_indev_data_t *data)
    {
        static TFT_eSPI tftTouch(SCREEN_WIDTH, SCREEN_HEIGHT);
        static bool inittedTouch = false;
        if(!inittedTouch){
            tftTouch.begin();
            tftTouch.setRotation(1);
            uint16_t cData[5] = {267, 3646, 198, 3669, 6};
            tftTouch.setTouch(cData);
            inittedTouch = true;
        }
        uint16_t x, y;
        bool touched = tftTouch.getTouch(&y, &x, 600);
        if(!touched){
            data->state = LV_INDEV_STATE_REL;
        } else {
            data->state = LV_INDEV_STATE_PR;
            data->point.x = x;
            data->point.y = y;
        }
    };
    lv_indev_drv_register(&indev_drv);

    // Create UI elements (your ui_init() function defined in ui.h)
    ui_init();

    Serial.println("LVGL initialized + UI created.");
}

void assignChartSeries()
{
    chart1Series = lv_chart_get_series_next(ui_Chart1, NULL);
    chart3Series = lv_chart_get_series_next(ui_Chart3, NULL);

    if(!chart1Series){
        Serial.println("Warning: ui_Chart1 has no series!");
    }
    if(!chart3Series){
        Serial.println("Warning: ui_Chart3 has no series!");
    }

    // Initialize chart data to zero
    for (int i = 0; i < CHART1_SEGMENTS; i++){
        lv_chart_set_value_by_id(ui_Chart1, chart1Series, i, 0);
    }
    lv_chart_refresh(ui_Chart1);

    for (int i = 0; i < CHART3_SEGMENTS; i++){
        lv_chart_set_value_by_id(ui_Chart3, chart3Series, i, 0);
    }
    lv_chart_refresh(ui_Chart3);

    Serial.println("Chart series assigned.");
}

//////////////////////////////////////////////////////////////////////////////
// Geiger Pulse & Buzzer Functions
//////////////////////////////////////////////////////////////////////////////
void initPulsePin()
{
    pinMode(GEIGER_PULSE_PIN, INPUT); // or use INPUT_PULLDOWN if needed
    attachInterrupt(digitalPinToInterrupt(GEIGER_PULSE_PIN), geigerPulseISR, RISING);
    Serial.println("Geiger pulse pin set for interrupt on RISING edge.");
}

void IRAM_ATTR geigerPulseISR()
{
    unsigned long nowMs = millis();
    if(nowMs - lastInterruptMs > PULSE_LOCKOUT_MS)
    {
        newPulseFlag = true;
        lastInterruptMs = nowMs;
    }
}

void initBuzzer()
{
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("Buzzer initialized.");
}

void handleNewPulses()
{
    if(newPulseFlag)
    {
        newPulseFlag = false;
        unsigned long now = millis();
        events.push_back(now);
        totalCounts++;

        // Uncomment if tone() is available to produce a beep:
        // tone(BUZZER_PIN, BEEP_FREQ, BEEP_MS);
    }
}

//////////////////////////////////////////////////////////////////////////////
// Rolling CPM & Dose Statistics
//////////////////////////////////////////////////////////////////////////////
void removeOldEvents()
{
    unsigned long now = millis();
    while(!events.empty() && (now - events.front()) > 60000UL)
    {
        events.erase(events.begin());
    }
}

float getRealTimeCPM()
{
    return (float)events.size();
}

void updateRealTimeStats(float cpm, float dtSec)
{
    // Current reading
    currentuSvHr = cpm / CONVERSION_FACTOR;

    // Average reading
    unsigned long now = millis();
    float totalTimeMin = (float)(now - startTime) / 60000.0f;
    if(totalTimeMin > 0.0f)
    {
        float avgCPM = (float)totalCounts / totalTimeMin;
        averageuSvHr = avgCPM / CONVERSION_FACTOR;
    }

    // Maximum reading
    if(currentuSvHr > maxuSvHr)
    {
        maxuSvHr = currentuSvHr;
    }

    // Cumulative dose (µSv/h → mSv)
    float doseIncrement_mSv = (currentuSvHr / 3600.0f) * dtSec / 1000.0f;
    cumulativeDosemSv += doseIncrement_mSv;
}

//////////////////////////////////////////////////////////////////////////////
// Radiation Label Updates
//////////////////////////////////////////////////////////////////////////////
void updateLabels()
{
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

//////////////////////////////////////////////////////////////////////////////
// Chart Updates
//////////////////////////////////////////////////////////////////////////////
void updateChartsEveryMinute()
{
    static unsigned long lastMinuteMark = 0;
    unsigned long now = millis();

    if((now - lastMinuteMark) < 60000UL)
        return; // wait for a full minute

    lastMinuteMark = now;

    partialSum3Min  += currentuSvHr;
    partialSum60Min += currentuSvHr;
    minuteCount3++;
    hourCount++;

    // Finalize a 3-minute segment for Chart1
    if(minuteCount3 >= CHART1_SEGMENT_MIN)
    {
        float finalAvg = (minuteCount3 > 0) ? (partialSum3Min / (float)minuteCount3) : 0.0f;
        shiftChart1Left();
        chart1Data[CHART1_SEGMENTS - 1] = finalAvg;
        for (int i = 0; i < CHART1_SEGMENTS; i++){
            lv_chart_set_value_by_id(ui_Chart1, chart1Series, i, (lv_coord_t)chart1Data[i]);
        }
        lv_chart_refresh(ui_Chart1);
        partialSum3Min = 0.0f;
        minuteCount3   = 0;
    }

    // Finalize a 60-minute segment for Chart3
    if(hourCount >= 60)
    {
        float finalAvg = (hourCount > 0) ? (partialSum60Min / (float)hourCount) : 0.0f;
        shiftChart3Left();
        chart3Data[CHART3_SEGMENTS - 1] = finalAvg;
        for (int i = 0; i < CHART3_SEGMENTS; i++){
            lv_chart_set_value_by_id(ui_Chart3, chart3Series, i, (lv_coord_t)chart3Data[i]);
        }
        lv_chart_refresh(ui_Chart3);
        partialSum60Min = 0.0f;
        hourCount       = 0;
    }
}

void updateChart1RealTime()
{
    float partialAvg = (minuteCount3 > 0) ? (partialSum3Min / (float)minuteCount3) : 0.0f;
    chart1Data[CHART1_SEGMENTS - 1] = partialAvg;
    lv_chart_set_value_by_id(ui_Chart1, chart1Series, CHART1_SEGMENTS - 1, (lv_coord_t)partialAvg);
    lv_chart_refresh(ui_Chart1);
}

void updateChart3RealTime()
{
    float partialAvg = (hourCount > 0) ? (partialSum60Min / (float)hourCount) : 0.0f;
    chart3Data[CHART3_SEGMENTS - 1] = partialAvg;
    lv_chart_set_value_by_id(ui_Chart3, chart3Series, CHART3_SEGMENTS - 1, (lv_coord_t)partialAvg);
    lv_chart_refresh(ui_Chart3);
}

//////////////////////////////////////////////////////////////////////////////
// Data Shifting for Charts
//////////////////////////////////////////////////////////////////////////////
void shiftChart1Left()
{
    for (int i = 0; i < CHART1_SEGMENTS - 1; i++){
        chart1Data[i] = chart1Data[i + 1];
    }
    chart1Data[CHART1_SEGMENTS - 1] = 0.0f;
}

void shiftChart3Left()
{
    for (int i = 0; i < CHART3_SEGMENTS - 1; i++){
        chart3Data[i] = chart3Data[i + 1];
    }
    chart3Data[CHART3_SEGMENTS - 1] = 0.0f;
}

//////////////////////////////////////////////////////////////////////////////
// WiFi Connect Button Event Callback
// Reads SSID and Password from ui_SSID and ui_PASSWORD, attempts a WiFi connection,
// stores the credentials in EEPROM if successful, and updates ui_WIFIINFO.
//////////////////////////////////////////////////////////////////////////////
static void connect_btn_event_cb(lv_event_t *e) {
    const char* ssid = lv_textarea_get_text(ui_SSID);
    const char* password = lv_textarea_get_text(ui_PASSWORD);
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);

    // Update UI to indicate connection attempt
    lv_label_set_text(ui_WIFIINFO, "Connecting...");

    // Start WiFi connection
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    // Wait (up to 10 seconds) for connection
    int max_attempts = 20;  // 20 x 500ms = 10 seconds
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
        delay(500);
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected.");
        wifi_ip = WiFi.localIP().toString(); // Store IP for continuous updates

        // Save credentials to EEPROM
        strncpy(wifiCred.ssid, ssid, MAX_SSID_LEN);
        strncpy(wifiCred.password, password, MAX_PASS_LEN);
        EEPROM.put(0, wifiCred);
        EEPROM.commit();

        // Initialize NTP time
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
    } else {
        Serial.println("WiFi connection failed.");
        lv_label_set_text(ui_WIFIINFO, "Disconnected\nFailed");
    }
}

//////////////////////////////////////////////////////////////////////////////
// tryAutoConnect()
// Reads stored credentials from EEPROM and, if valid, tries to connect.
//////////////////////////////////////////////////////////////////////////////
void tryAutoConnect() {
    // Read the stored credentials from EEPROM
    EEPROM.get(0, wifiCred);

    // Check if the stored SSID is non-empty
    if (wifiCred.ssid[0] != '\0') {
        Serial.print("Found saved credentials. Trying to connect to: ");
        Serial.println(wifiCred.ssid);

        WiFi.mode(WIFI_STA);
        WiFi.begin(wifiCred.ssid, wifiCred.password);

        // Wait (up to 10 seconds) for connection
        int max_attempts = 20;
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < max_attempts) {
            delay(500);
            attempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("Auto WiFi connection successful.");
            wifi_ip = WiFi.localIP().toString();

            // Initialize NTP time
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
        } else {
            Serial.println("Auto WiFi connection failed.");
            lv_label_set_text(ui_WIFIINFO, "Disconnected\nFailed");
        }
    }
}
