/*******************************************************************************
 * Includes
 ******************************************************************************/
#include <lvgl.h>
#include <TFT_eSPI.h>
#include "ui.h"   // Must declare/extern lv_obj_t *ui_Chart1, ui_Chart3, labels, etc.
#include <vector> // For storing timestamps of button presses

/*******************************************************************************
 * Definitions & Constants
 ******************************************************************************/
static const uint16_t SCREEN_WIDTH  = 480;
static const uint16_t SCREEN_HEIGHT = 320;

/* LVGL draw buffer */
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ SCREEN_WIDTH * SCREEN_HEIGHT / 10 ];

/* Pins */
static const uint8_t BUTTON_PIN = 37;
static const uint8_t BUZZER_PIN = 38;

/* Conversion factor for CPM → µSv/h. Adjust for your tube or simulation. */
static const float CONVERSION_FACTOR = 1.0f; //Conversion factor currently 1 for testing, depends of the tube, the idea is that it can be set up via the settings tab

/* Rolling list of timestamps (1-minute window) */
static std::vector<unsigned long> events;

/* Lifetime counts & timing */
static unsigned long totalCounts = 0;  // all-time button presses
static unsigned long startTime   = 0;  // for average calculations
static unsigned long lastLoop    = 0;  // for dose integration
static bool LastButtonState      = HIGH;

/* Real-time variables for label display */
static float currentuSvHr      = 0.0f;
static float averageuSvHr      = 0.0f;
static float maxuSvHr          = 0.0f;
static float cumulativeDosemSv = 0.0f;

/*******************************************************************************
 * Chart Configuration
 *  - ui_Chart1 has 20 bars => each bar = 3 min => total 60 min
 *  - ui_Chart3 has 24 bars => each bar = 1 hour => total 24 hours
 ******************************************************************************/
static const int CHART1_SEGMENTS    = 20;
static const int CHART1_SEGMENT_MIN = 3;   // 3 minutes per bar => 60 min total

static const int CHART3_SEGMENTS    = 24;
static const int CHART3_SEGMENT_HR  = 1;   // 1 hour per bar => 24 hours total

/* Data arrays for each chart (storing the final bar values) */
static float chart1Data[CHART1_SEGMENTS] = {0.0f};
static float chart3Data[CHART3_SEGMENTS] = {0.0f};

/* Partial sums to build each bar in real time */
static float partialSum3Min = 0.0f;   // accumulates partial data for Chart1
static int   minuteCount3   = 0;      // how many minutes have passed in the 3-min segment

static float partialSum60Min = 0.0f;  // accumulates partial data for Chart3
static int   hourCount       = 0;     // how many minutes have passed in the 1-hour segment

/* We shift data left once the segment completes. So the "last bar" is always
 * chartXData[CHARTX_SEGMENTS - 1]. */
 
/*******************************************************************************
 * LVGL Objects (from ui.h / SquareLine)
 *  - Already defined as bar charts with correct point counts
 ******************************************************************************/
extern lv_obj_t* ui_CurrentRad;
extern lv_obj_t* ui_AverageRad;
extern lv_obj_t* ui_MaximumRad;
extern lv_obj_t* ui_CumulativeRad;
extern lv_obj_t* ui_Chart1;
extern lv_obj_t* ui_Chart3;

/* We'll get the chart series after ui_init() */
static lv_chart_series_t* chart1Series = nullptr;
static lv_chart_series_t* chart3Series = nullptr;

/*******************************************************************************
 * Function Prototypes
 ******************************************************************************/
void initSerial();
void initTFT();
void initLVGL();
void assignChartSeries(); // retrieve the single series from ui_Chart1 & ui_Chart3
void initButton();
void initBuzzer();

void handleButton();
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
    initLVGL();       // calls ui_init() inside
    assignChartSeries();
    initButton();
    initBuzzer();

    // Initialize timers
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

    // 2) Button logic (falling edges + beep)
    handleButton();

    // 3) Maintain 1-min rolling timestamps for CPM
    removeOldEvents();

    // 4) dtSec for dose integration
    unsigned long now = millis();
    float dtSec = (float)(now - lastLoop) / 1000.0f;
    lastLoop = now;

    // 5) Compute real-time CPM and update stats
    float cpm = getRealTimeCPM();
    updateRealTimeStats(cpm, dtSec);

    // 6) Update numeric labels
    updateLabels();

    // 7) Update chart data once per minute
    updateChartsEveryMinute();

    // 8) Continuously update the "last bar" in real time
    updateChart1RealTime(); 
    updateChart3RealTime();

    // Optional small delay
    // delay(5);
}

/*******************************************************************************
 * Implementation - Initialization
 ******************************************************************************/

/**
 * @brief Initialize Serial
 */
void initSerial()
{
    Serial.begin(115200);
    Serial.println("Initializing Serial...");
}

/**
 * @brief Initialize TFT if needed (optional if your code already handles it)
 */
void initTFT()
{
    static TFT_eSPI tft(SCREEN_WIDTH, SCREEN_HEIGHT);
    tft.begin();
    tft.setRotation(1);

    uint16_t calData[5] = {267, 3646, 198, 3669, 6};
    tft.setTouch(calData);

    Serial.println("TFT initialized.");
}

/**
 * @brief Initialize LVGL and create UI (ui_init()).
 *        We'll retrieve chart series afterwards in assignChartSeries().
 */
void initLVGL()
{
    lv_init();

    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * SCREEN_HEIGHT / 10);

    // Display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = [](lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
    {
        static TFT_eSPI tftTemp(SCREEN_WIDTH, SCREEN_HEIGHT);
        static bool initted = false;
        if(!initted){
            tftTemp.begin();
            tftTemp.setRotation(1);
            initted = true;
        }
        uint32_t w = (area->x2 - area->x1 + 1);
        uint32_t h = (area->y2 - area->y1 + 1);

        tftTemp.startWrite();
        tftTemp.setAddrWindow(area->x1, area->y1, w, h);
        tftTemp.pushColors((uint16_t *)&color_p->full, w * h, true);
        tftTemp.endWrite();

        lv_disp_flush_ready(drv);
    };
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Input driver (touch) - optional
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

    // Create all UI elements, including ui_Chart1 and ui_Chart3
    ui_init();

    Serial.println("LVGL initialized + UI created.");
}

/**
 * @brief Fetch the single series from ui_Chart1 and ui_Chart3
 */
void assignChartSeries()
{
    // We assume each chart has exactly one series, created in ui.c
    chart1Series = lv_chart_get_series_next(ui_Chart1, NULL);
    chart3Series = lv_chart_get_series_next(ui_Chart3, NULL);

    if(!chart1Series) {
        Serial.println("Warning: ui_Chart1 has no series!");
    }
    if(!chart3Series) {
        Serial.println("Warning: ui_Chart3 has no series!");
    }

    // (Optionally ensure the point count is correct for each chart, e.g. 20, 24)
    // lv_chart_set_point_count(ui_Chart1, CHART1_SEGMENTS);
    // lv_chart_set_point_count(ui_Chart3, CHART3_SEGMENTS);

    // Initialize the data in both charts to zero
    for(int i=0; i<CHART1_SEGMENTS; i++)
    {
        lv_chart_set_value_by_id(ui_Chart1, chart1Series, i, 0);
    }
    for(int i=0; i<CHART3_SEGMENTS; i++)
    {
        lv_chart_set_value_by_id(ui_Chart3, chart3Series, i, 0);
    }
    lv_chart_refresh(ui_Chart1);
    lv_chart_refresh(ui_Chart3);

    Serial.println("Chart series assigned.");
}

/**
 * @brief Initialize a button pin with pull-up
 */
void initButton()
{
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    LastButtonState = digitalRead(BUTTON_PIN);
    Serial.println("Button initialized (INPUT_PULLUP).");
}

/**
 * @brief Initialize the buzzer
 */
void initBuzzer()
{
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    Serial.println("Buzzer initialized.");
}

/*******************************************************************************
 * Implementation - Button & CPM
 ******************************************************************************/

/**
 * @brief Detect falling edge. Each press => beep at 40 Hz, record event.
 */
void handleButton()
{
    bool currentState = digitalRead(BUTTON_PIN);
    if(LastButtonState == HIGH && currentState == LOW)
    {
        // New event
        events.push_back(millis());
        // All-time count
        totalCounts++;

        // 40 Hz beep for 50 ms
        tone(BUZZER_PIN, 40, 50);
    }
    LastButtonState = currentState;
}

/**
 * @brief Remove events older than 60 seconds
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
 * @brief Real-time CPM = number of events in the last 60 seconds
 */
float getRealTimeCPM()
{
    return (float)events.size();
}

/*******************************************************************************
 * Implementation - Stats & Labels
 ******************************************************************************/

/**
 * @brief Update current, average, max, cumulative dose
 * @param cpm   real-time counts per minute
 * @param dtSec time since last loop, seconds
 */
void updateRealTimeStats(float cpm, float dtSec)
{
    // 1) Current reading
    currentuSvHr = cpm / CONVERSION_FACTOR;

    // 2) Average reading
    unsigned long now = millis();
    float totalTimeMin = (float)(now - startTime) / 60000.0f;
    if(totalTimeMin > 0.0f)
    {
        float avgCPM = (float)totalCounts / totalTimeMin;
        averageuSvHr = avgCPM / CONVERSION_FACTOR;
    }

    // 3) Max reading
    if(currentuSvHr > maxuSvHr)
    {
        maxuSvHr = currentuSvHr;
    }

    // 4) Cumulative dose (mSv)
    float doseIncrement_mSv = (currentuSvHr / 3600.0f) * dtSec / 1000.0f;
    cumulativeDosemSv += doseIncrement_mSv;
}

/**
 * @brief Show numeric values (no units) in LVGL labels
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
 * Implementation - Chart Updates
 ******************************************************************************/

/**
 * @brief Every minute, add currentuSvHr to partial sums.
 *        - After 3 minutes => shift Chart1 left, finalize last bar = partial average
 *        - After 60 minutes => shift Chart3 left, finalize last bar = partial average
 */
void updateChartsEveryMinute()
{
    static unsigned long lastMinuteMark = 0;
    unsigned long now = millis();

    // If less than 60,000 ms have passed, skip
    if((now - lastMinuteMark) < 60000UL) return;

    // Mark the new minute
    lastMinuteMark = now;

    // Accumulate partial sums
    partialSum3Min  += currentuSvHr; 
    partialSum60Min += currentuSvHr;

    minuteCount3++;
    hourCount++;

    // Check if we've hit 3 min for ui_Chart1
    if(minuteCount3 >= CHART1_SEGMENT_MIN)
    {
        // Compute a final average over the 3-minute block
        float finalAvg = 0.0f;
        if(minuteCount3 > 0) {
            finalAvg = partialSum3Min / (float)minuteCount3;
        }

        // Shift chart1 left by 1, store 'finalAvg' in the last bar
        shiftChart1Left();
        chart1Data[CHART1_SEGMENTS - 1] = finalAvg;

        // Update the chart
        for(int i=0; i<CHART1_SEGMENTS; i++)
        {
            lv_chart_set_value_by_id(ui_Chart1, chart1Series, i, (lv_coord_t)chart1Data[i]);
        }
        lv_chart_refresh(ui_Chart1);

        // Reset partial sum/counter
        partialSum3Min = 0.0f;
        minuteCount3   = 0;
    }

    // Check if we've hit 60 min for ui_Chart3
    if(hourCount >= 60)
    {
        float finalAvg = 0.0f;
        if(hourCount > 0) {
            finalAvg = partialSum60Min / (float)hourCount;
        }

        // Shift chart3 left by 1, store finalAvg in the last bar
        shiftChart3Left();
        chart3Data[CHART3_SEGMENTS - 1] = finalAvg;

        // Update the chart
        for(int i=0; i<CHART3_SEGMENTS; i++)
        {
            lv_chart_set_value_by_id(ui_Chart3, chart3Series, i, (lv_coord_t)chart3Data[i]);
        }
        lv_chart_refresh(ui_Chart3);

        // Reset partial sum/counter
        partialSum60Min = 0.0f;
        hourCount       = 0;
    }
}

/**
 * @brief Continuously update the "last bar" in ui_Chart1 with the partial average 
 *        so it changes in real time. 
 */
void updateChart1RealTime()
{
    // If we haven't started counting, it's 0
    float partialAvg = 0.0f;
    if(minuteCount3 > 0) {
        partialAvg = partialSum3Min / (float)minuteCount3;
    }

    // Place this partial average in the last index [CHART1_SEGMENTS-1]
    chart1Data[CHART1_SEGMENTS - 1] = partialAvg;

    // Update that one bar in LVGL
    lv_chart_set_value_by_id(ui_Chart1, chart1Series, CHART1_SEGMENTS - 1, (lv_coord_t)partialAvg);
    lv_chart_refresh(ui_Chart1);
}

/**
 * @brief Continuously update the "last bar" in ui_Chart3 with the partial average
 *        so it changes in real time.
 */
void updateChart3RealTime()
{
    float partialAvg = 0.0f;
    if(hourCount > 0) {
        partialAvg = partialSum60Min / (float)hourCount;
    }

    chart3Data[CHART3_SEGMENTS - 1] = partialAvg;

    lv_chart_set_value_by_id(ui_Chart3, chart3Series, CHART3_SEGMENTS - 1, (lv_coord_t)partialAvg);
    lv_chart_refresh(ui_Chart3);
}

/*******************************************************************************
 * Implementation - Chart Shifting
 ******************************************************************************/

/**
 * @brief Shift the chart1Data array left by one position
 *        so we open a new slot at the right end for the next partial average.
 */
void shiftChart1Left()
{
    for(int i=0; i < CHART1_SEGMENTS - 1; i++)
    {
        chart1Data[i] = chart1Data[i + 1];
    }
    chart1Data[CHART1_SEGMENTS - 1] = 0.0f; // clear the last slot
}

/**
 * @brief Shift the chart3Data array left by one position
 *        so we open a new slot at the right end for the next partial average.
 */
void shiftChart3Left()
{
    for(int i=0; i < CHART3_SEGMENTS - 1; i++)
    {
        chart3Data[i] = chart3Data[i + 1];
    }
    chart3Data[CHART3_SEGMENTS - 1] = 0.0f; // clear last slot
}
