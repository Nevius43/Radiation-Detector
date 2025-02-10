/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "ui.h"  // Must declare/extern: ui_Chart1, ui_Chart3, ui_CurrentRad, etc.
#include <vector>

/*******************************************************************************
 * Definitions & Constants
 ******************************************************************************/
static const uint16_t SCREEN_WIDTH  = 480;
static const uint16_t SCREEN_HEIGHT = 320;

/* LVGL draw buffer */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ SCREEN_WIDTH * SCREEN_HEIGHT / 10 ];

/* Schmitt trigger digital output pin */
static const uint8_t GEIGER_PULSE_PIN = 9;

/* Buzzer pin and beep settings */
static const uint8_t BUZZER_PIN       = 38;
static const unsigned int BEEP_FREQ   = 40;  // 40 Hz is typically inaudible
static const unsigned long BEEP_MS    = 50;  // beep duration (ms)

/* Conversion factor for CPM -> µSv/h */
static const float CONVERSION_FACTOR = 153.8f;

/* Rolling 1-minute list of pulse timestamps */
static std::vector<unsigned long> events;

/* All-time counts & timing */
static unsigned long totalCounts = 0;   
static unsigned long startTime   = 0;   
static unsigned long lastLoop    = 0;   

/* Real-time variables for label display */
static float currentuSvHr      = 0.0f;
static float averageuSvHr      = 0.0f;
static float maxuSvHr          = 0.0f;
static float cumulativeDosemSv = 0.0f;

/*******************************************************************************
 * Chart Configuration
 *  - ui_Chart1: 20 bars -> 3 min each -> 60 min total
 *  - ui_Chart3: 24 bars -> 1 hour each -> 24 hours total
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

/* LVGL Objects (from your UI) */
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
static volatile unsigned long lastInterruptMs = 0; // optional lockout
static const unsigned long PULSE_LOCKOUT_MS  = 5;  // ignore pulses within 5ms

/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/
void initSerial();
void initTFT();
void initLVGL();
void assignChartSeries();

void initPulsePin();
void IRAM_ATTR geigerPulseISR(); // the ISR

void initBuzzer();
void handleNewPulses();  // main-loop logic that processes ISR pulses

void removeOldEvents();
float getRealTimeCPM();
void updateRealTimeStats(float cpm, float dtSec);
void updateLabels();

void updateChartsEveryMinute();
void updateChart1RealTime();
void updateChart3RealTime();

void shiftChart1Left();
void shiftChart3Left();

/*******************************************************************************
 * setup()
 ******************************************************************************/
void setup()
{
    initSerial();
    initTFT();
    initLVGL();          
    assignChartSeries(); 

    initPulsePin();
    initBuzzer();

    startTime = millis();
    lastLoop  = startTime;

    Serial.println("Setup completed.");
}

/*******************************************************************************
 * loop()
 ******************************************************************************/
void loop()
{
    // 1) LVGL tasks
    lv_timer_handler();

    // 2) Check if ISR flagged new pulses
    handleNewPulses();

    // 3) Remove old events from 1-min window
    removeOldEvents();

    // 4) Time step for dose integration
    unsigned long now = millis();
    float dtSec = (float)(now - lastLoop) / 1000.0f;
    lastLoop = now;

    // 5) Real-time CPM from events => dose stats
    float cpm = getRealTimeCPM();
    updateRealTimeStats(cpm, dtSec);

    // 6) Update numeric labels
    updateLabels();

    // 7) Charts once per minute
    updateChartsEveryMinute();

    // 8) Update last bar in real time
    updateChart1RealTime();
    updateChart3RealTime();

    // optional delay
    // delay(5);
}

/*******************************************************************************
 * Implementation - Initialization
 ******************************************************************************/

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

    // Example calibration
    uint16_t calData[5] = {267, 3646, 198, 3669, 6};
    tft.setTouch(calData);

    Serial.println("TFT initialized.");
}

void initLVGL()
{
    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * SCREEN_HEIGHT / 10);

    // Minimal flush callback
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

    // (Optional) Simple touch driver
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

    // Create UI
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
    for(int i=0; i<CHART1_SEGMENTS; i++){
        lv_chart_set_value_by_id(ui_Chart1, chart1Series, i, 0);
    }
    lv_chart_refresh(ui_Chart1);

    for(int i=0; i<CHART3_SEGMENTS; i++){
        lv_chart_set_value_by_id(ui_Chart3, chart3Series, i, 0);
    }
    lv_chart_refresh(ui_Chart3);

    Serial.println("Chart series assigned.");
}

/*******************************************************************************
 * ISR-based Pulse Detection
 ******************************************************************************/

/**
 * @brief Configure the digital pin and attach rising-edge interrupt.
 */
void initPulsePin()
{
    pinMode(GEIGER_PULSE_PIN, INPUT); // or INPUT_PULLDOWN if your hardware uses internal pull-down
    attachInterrupt(digitalPinToInterrupt(GEIGER_PULSE_PIN), geigerPulseISR, RISING);
    Serial.println("Geiger pulse pin set for interrupt on RISING edge.");
}

/**
 * @brief The ISR for geiger pulses.
 *        We only set a flag (and possibly a brief lockout) to avoid double counting.
 */
void IRAM_ATTR geigerPulseISR()
{
    unsigned long nowMs = millis();
    if(nowMs - lastInterruptMs > PULSE_LOCKOUT_MS)
    {
        newPulseFlag = true;
        lastInterruptMs = nowMs;
    }
}

/*******************************************************************************
 * Buzzer
 ******************************************************************************/
void initBuzzer()
{
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("Buzzer initialized.");
}

/*******************************************************************************
 * Main-Loop Handling of New Pulses
 ******************************************************************************/

/**
 * @brief If the ISR indicated new pulses, we record them in 'events' and beep.
 *        Because we can't safely call tone() in the ISR, we do it here.
 */
void handleNewPulses()
{
    if(newPulseFlag)
    {
        // Clear the flag first to handle multiple pulses
        newPulseFlag = false;

        // Record the event
        unsigned long now = millis();
        events.push_back(now);
        totalCounts++;

        // Buzzer beep
        //tone(BUZZER_PIN, BEEP_FREQ, BEEP_MS);
    }
}

/*******************************************************************************
 * Implementation - Rolling CPM & Stats
 ******************************************************************************/

/**
 * @brief Remove pulses older than 60,000 ms to maintain 1-min window
 */
void removeOldEvents()
{
    unsigned long now = millis();
    while(!events.empty() && (now - events.front()) > 60000UL)
    {
        events.erase(events.begin());
    }
}

/**
 * @brief Return how many pulses in the last 60 seconds
 */
float getRealTimeCPM()
{
    return (float)events.size();
}

/**
 * @brief Compute current, average, max, cumulative dose
 * @param cpm   counts per minute from getRealTimeCPM()
 * @param dtSec time since last loop
 */
void updateRealTimeStats(float cpm, float dtSec)
{
    // 1) Current reading
    currentuSvHr = cpm / CONVERSION_FACTOR;

    // 2) Average
    unsigned long now = millis();
    float totalTimeMin = (float)(now - startTime) / 60000.0f;
    if(totalTimeMin > 0.0f)
    {
        float avgCPM = (float)totalCounts / totalTimeMin;
        averageuSvHr = avgCPM / CONVERSION_FACTOR;
    }

    // 3) Max
    if(currentuSvHr > maxuSvHr)
    {
        maxuSvHr = currentuSvHr;
    }

    // 4) Cumulative
    // currentuSvHr => µSv/h => /3600 => µSv/s => * dtSec => µSv => /1000 => mSv
    float doseIncrement_mSv = (currentuSvHr / 3600.0f) * dtSec / 1000.0f;
    cumulativeDosemSv += doseIncrement_mSv;
}

/*******************************************************************************
 * Implementation - Label Updates
 ******************************************************************************/

/**
 * @brief Convert floats to text and set them in LVGL labels
 */
void updateLabels()
{
    char buffer[16];

    // Current
    snprintf(buffer, sizeof(buffer), "%.2f", currentuSvHr);
    lv_label_set_text(ui_CurrentRad, buffer);

    // Average
    snprintf(buffer, sizeof(buffer), "%.2f", averageuSvHr);
    lv_label_set_text(ui_AverageRad, buffer);

    // Max
    snprintf(buffer, sizeof(buffer), "%.2f", maxuSvHr);
    lv_label_set_text(ui_MaximumRad, buffer);

    // Cumulative
    snprintf(buffer, sizeof(buffer), "%.2f", cumulativeDosemSv);
    lv_label_set_text(ui_CumulativeRad, buffer);
}

/*******************************************************************************
 * Implementation - Chart Updates (Single Series)
 ******************************************************************************/

/**
 * @brief Called once per minute. Accumulates partial sums for each chart
 *        and finalizes the last bar after 3 min (Chart1) or 60 min (Chart3).
 */
void updateChartsEveryMinute()
{
    static unsigned long lastMinuteMark = 0;
    unsigned long now = millis();

    if((now - lastMinuteMark) < 60000UL)
        return; // not a new minute yet

    lastMinuteMark = now;

    // Accumulate partial sums
    partialSum3Min  += currentuSvHr;
    partialSum60Min += currentuSvHr;

    minuteCount3++;
    hourCount++;

    // 3-minute segment done?
    if(minuteCount3 >= CHART1_SEGMENT_MIN)
    {
        float finalAvg = (minuteCount3 > 0) ? (partialSum3Min / (float)minuteCount3) : 0.0f;

        shiftChart1Left();
        chart1Data[CHART1_SEGMENTS - 1] = finalAvg;

        // Update ui_Chart1
        for(int i=0; i<CHART1_SEGMENTS; i++){
            lv_chart_set_value_by_id(ui_Chart1, chart1Series, i, (lv_coord_t)chart1Data[i]);
        }
        lv_chart_refresh(ui_Chart1);

        partialSum3Min = 0.0f;
        minuteCount3   = 0;
    }

    // 60-minute segment done?
    if(hourCount >= 60)
    {
        float finalAvg = (hourCount > 0) ? (partialSum60Min / (float)hourCount) : 0.0f;

        shiftChart3Left();
        chart3Data[CHART3_SEGMENTS - 1] = finalAvg;

        // Update ui_Chart3
        for(int i=0; i<CHART3_SEGMENTS; i++){
            lv_chart_set_value_by_id(ui_Chart3, chart3Series, i, (lv_coord_t)chart3Data[i]);
        }
        lv_chart_refresh(ui_Chart3);

        partialSum60Min = 0.0f;
        hourCount       = 0;
    }
}

/**
 * @brief Continuously update the last bar with a partial average
 *        for the in-progress segment (3 min or 60 min).
 */
void updateChart1RealTime()
{
    float partialAvg = 0.0f;
    if(minuteCount3 > 0){
        partialAvg = partialSum3Min / (float)minuteCount3;
    }
    chart1Data[CHART1_SEGMENTS - 1] = partialAvg;

    lv_chart_set_value_by_id(ui_Chart1, chart1Series, CHART1_SEGMENTS - 1, (lv_coord_t)partialAvg);
    lv_chart_refresh(ui_Chart1);
}

void updateChart3RealTime()
{
    float partialAvg = 0.0f;
    if(hourCount > 0){
        partialAvg = partialSum60Min / (float)hourCount;
    }
    chart3Data[CHART3_SEGMENTS - 1] = partialAvg;

    lv_chart_set_value_by_id(ui_Chart3, chart3Series, CHART3_SEGMENTS - 1, (lv_coord_t)partialAvg);
    lv_chart_refresh(ui_Chart3);
}

/*******************************************************************************
 * Implementation - Shift Data
 ******************************************************************************/
void shiftChart1Left()
{
    for(int i=0; i < CHART1_SEGMENTS-1; i++){
        chart1Data[i] = chart1Data[i+1];
    }
    chart1Data[CHART1_SEGMENTS - 1] = 0.0f;
}

void shiftChart3Left()
{
    for(int i=0; i < CHART3_SEGMENTS-1; i++){
        chart3Data[i] = chart3Data[i+1];
    }
    chart3Data[CHART3_SEGMENTS - 1] = 0.0f;
}
