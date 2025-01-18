#include <lvgl.h>
#include <TFT_eSPI.h>
#include "ui.h" // Include your UI header
#define USE_DMA_TO_TFT
// Define screen resolution
static const uint16_t screenWidth  = 480;
static const uint16_t screenHeight = 320;

// LVGL display buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[ screenWidth * screenHeight / 10 ];

// TFT instance
TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);

// Variables for LVGL tick handling
unsigned long last_tick = 0;

// Optional: Enable LVGL logging for debugging
#if LV_USE_LOG != 0
/* Serial debugging */
void my_print(const char * buf)
{
    Serial.printf(buf);
    Serial.flush();
}
#endif

/* Display flushing function */
void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp_drv);
}

/* Touchpad reading function */
void my_touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    uint16_t touchX, touchY;
    bool touched = tft.getTouch(&touchY, &touchX, 600);

    if (!touched)
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touchX;
        data->point.y = touchY;
    }
}

/* Setup function */
void setup()
{
    // Initialize Serial for debugging
    Serial.begin(115200);
    Serial.println("Initializing...");

    // Initialize LVGL
    lv_init();

    // Initialize TFT
    tft.begin();
    tft.setRotation(1); // Adjust rotation as needed

    // Optional: Set touch calibration data
    uint16_t calData[5] = { 267, 3646, 198, 3669, 6 };
    tft.setTouch(calData);

    // Initialize LVGL draw buffer
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * screenHeight / 10);

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

    Serial.println("LVGL and TFT initialized.");

    // Initialize UI from ui.h
    ui_init(); // Call the UI initialization function

    Serial.println("UI initialized.");
}

/* Main loop */
void loop()
{

    // Handle LVGL tasks
    lv_timer_handler();
    
    // Small delay to prevent overloading the CPU
    delay(1);
}