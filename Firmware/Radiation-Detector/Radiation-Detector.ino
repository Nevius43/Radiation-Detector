#include <lvgl.h>
#include <TFT_eSPI.h>
#include "ui.h" // Include your UI header
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

// Define screen resolution
static const uint16_t screenWidth  = 480;
static const uint16_t screenHeight = 320;

// LVGL display buffer
#define BUF_LINES 60 // Adjust based on available RAM
static lv_color_t buf1[screenWidth * BUF_LINES];
static lv_color_t buf2[screenWidth * BUF_LINES];
static lv_disp_draw_buf_t draw_buf;

// TFT instance
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

// Timer handle for LVGL ticks
TimerHandle_t lv_tick_timer;

// Debug logging
#define ENABLE_DEBUG_LOGS 0

#if ENABLE_DEBUG_LOGS
  #define DEBUG_PRINT(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
#endif

/* Display flushing function */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true); // Ensure 16-bit color
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

/* Touchpad reading function */
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
    static uint16_t lastX = 0, lastY = 0;
    static bool lastState = false;

    uint16_t touchX, touchY;
    bool touched = tft.getTouch(&touchX, &touchY, 600);

    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
    } else {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;

        // Simple debouncing
        if (touchX != lastX || touchY != lastY || !lastState) {
            lastX = touchX;
            lastY = touchY;
            lastState = true;

            DEBUG_PRINT("Touch X: " + String(touchX));
            DEBUG_PRINT("Touch Y: " + String(touchY));
        }
    }

    if (!touched) {
        lastState = false;
    }
}

/* LVGL tick task with corrected signature */
void lv_tick_task(TimerHandle_t xTimer) {
    lv_tick_inc(1);
}

/* Initialize display buffer */
void setup_display_buffer() {
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, screenWidth * BUF_LINES);
}

/* Memory monitor task */
void memory_monitor_task(void *pvParameters) {
    while (1) {
        #if ENABLE_DEBUG_LOGS
        Serial.print("Free heap: ");
        Serial.println(xPortGetFreeHeapSize());
        #endif
        vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second
    }
}

void setup() {
    // Initialize Serial if debugging is enabled
    #if ENABLE_DEBUG_LOGS
    Serial.begin(115200);
    Serial.println("Initializing...");
    #endif

    // Initialize LVGL
    lv_init();

    // Initialize TFT
    tft.begin();
    tft.setRotation(1); // Adjust rotation as needed

    // Optional: Set touch calibration data
    uint16_t calData[5] = {198, 3732, 226, 3557, 7};
    tft.setTouch(calData);

    // Initialize display buffer
    setup_display_buffer();

    // Initialize and register display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth;
    disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Initialize and register input device driver (touch)
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    // Initialize UI from ui.h
    ui_init();

    // Create a FreeRTOS timer for LVGL ticks
    lv_tick_timer = xTimerCreate(
        "LV_Tick",                     // Timer name
        pdMS_TO_TICKS(1),             // Timer period (1 ms)
        pdTRUE,                        // Auto-reload
        NULL,                          // Timer ID
        lv_tick_task                   // Callback function
    );

    if (lv_tick_timer != NULL) {
        if (xTimerStart(lv_tick_timer, 0) != pdPASS) {
            #if ENABLE_DEBUG_LOGS
            Serial.println("Failed to start LVGL tick timer.");
            #endif
        }
    } else {
        #if ENABLE_DEBUG_LOGS
        Serial.println("Failed to create LVGL tick timer.");
        #endif
    }

    #if ENABLE_DEBUG_LOGS
    Serial.println("LVGL and TFT initialized.");
    Serial.println("UI initialized.");
    #endif

    // Create LVGL task with high priority on core 0
    xTaskCreatePinnedToCore(
        [] (void * parameter) -> void {
            while (1) {
                lv_timer_handler();
                vTaskDelay(pdMS_TO_TICKS(1)); // Yield to other tasks
            }
        },
        "LVGL_Task",        // Task name
        4096,               // Stack size (in words)
        NULL,               // Task input parameter
        2,                  // Task priority
        NULL,               // Task handle
        0                   // Core 0
    );

    // Create memory monitor task on core 1 with low priority
    xTaskCreatePinnedToCore(
        memory_monitor_task,
        "Memory_Monitor",   // Task name
        2048,               // Stack size (in words)
        NULL,               // Task input parameter
        0,                  // Task priority
        NULL,               // Task handle
        1                   // Core 1
    );
}

void loop() {
    // Empty because tasks are handled by FreeRTOS
}
