// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.5.1
// LVGL version: 8.3.11
// Project name: SquareLine_Project

#include "ui.h"

void ui_VoltageScreen_screen_init(void)
{
    ui_VoltageScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_VoltageScreen, LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Panel3 = lv_obj_create(ui_VoltageScreen);
    lv_obj_set_width(ui_Panel3, 480);
    lv_obj_set_height(ui_Panel3, 320);
    lv_obj_set_x(ui_Panel3, -480);
    lv_obj_set_y(ui_Panel3, 0);
    lv_obj_set_align(ui_Panel3, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Panel3, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Container2 = lv_obj_create(ui_VoltageScreen);
    lv_obj_remove_style_all(ui_Container2);
    lv_obj_set_width(ui_Container2, 480);
    lv_obj_set_height(ui_Container2, 50);
    lv_obj_set_align(ui_Container2, LV_ALIGN_TOP_MID);
    lv_obj_clear_flag(ui_Container2, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Button12 = lv_btn_create(ui_Container2);
    lv_obj_set_width(ui_Button12, 120);
    lv_obj_set_height(ui_Button12, 50);
    lv_obj_set_flex_flow(ui_Button12, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_Button12, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(ui_Button12, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button12, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Button12, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Button12, lv_color_hex(0x5456D5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button12, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_Button12, &ui_img_radiation_png, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Button13 = lv_btn_create(ui_Container2);
    lv_obj_set_width(ui_Button13, 120);
    lv_obj_set_height(ui_Button13, 50);
    lv_obj_set_x(ui_Button13, 120);
    lv_obj_set_y(ui_Button13, 0);
    lv_obj_set_align(ui_Button13, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_Button13, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button13, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Button13, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Button13, lv_color_hex(0x7274ED), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button13, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_Button13, &ui_img_1345346577, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Button14 = lv_btn_create(ui_Container2);
    lv_obj_set_width(ui_Button14, 120);
    lv_obj_set_height(ui_Button14, 50);
    lv_obj_set_x(ui_Button14, 240);
    lv_obj_set_y(ui_Button14, 0);
    lv_obj_set_align(ui_Button14, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_Button14, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button14, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Button14, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Button14, lv_color_hex(0x99D1A2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button14, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_Button14, &ui_img_electricity_png, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Button15 = lv_btn_create(ui_Container2);
    lv_obj_set_width(ui_Button15, 120);
    lv_obj_set_height(ui_Button15, 50);
    lv_obj_set_x(ui_Button15, 360);
    lv_obj_set_y(ui_Button15, 0);
    lv_obj_set_align(ui_Button15, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_Button15, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button15, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Button15, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Button15, lv_color_hex(0x7274ED), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button15, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_Button15, &ui_img_settings_png, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel2 = lv_obj_create(ui_VoltageScreen);
    lv_obj_set_width(ui_Panel2, 490);
    lv_obj_set_height(ui_Panel2, 6);
    lv_obj_set_x(ui_Panel2, 0);
    lv_obj_set_y(ui_Panel2, 30);
    lv_obj_set_align(ui_Panel2, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Panel2, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Panel2, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Panel2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_Panel2, lv_color_hex(0x313083), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(ui_Panel2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_MoreVoltage = lv_btn_create(ui_VoltageScreen);
    lv_obj_set_width(ui_MoreVoltage, 50);
    lv_obj_set_height(ui_MoreVoltage, 50);
    lv_obj_set_x(ui_MoreVoltage, 130);
    lv_obj_set_y(ui_MoreVoltage, 95);
    lv_obj_set_align(ui_MoreVoltage, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_MoreVoltage, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_MoreVoltage, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_MoreVoltage, lv_color_hex(0x6AFF7C), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_MoreVoltage, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label24 = lv_label_create(ui_MoreVoltage);
    lv_obj_set_width(ui_Label24, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label24, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label24, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label24, "+");
    lv_obj_set_style_text_font(ui_Label24, &ui_font_Pixel, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_LessVoltage = lv_btn_create(ui_VoltageScreen);
    lv_obj_set_width(ui_LessVoltage, 50);
    lv_obj_set_height(ui_LessVoltage, 50);
    lv_obj_set_x(ui_LessVoltage, 40);
    lv_obj_set_y(ui_LessVoltage, 95);
    lv_obj_set_align(ui_LessVoltage, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_LessVoltage, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_LessVoltage, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_LessVoltage, lv_color_hex(0xFF5E5E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_LessVoltage, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label25 = lv_label_create(ui_LessVoltage);
    lv_obj_set_width(ui_Label25, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label25, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label25, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label25, "-");
    lv_obj_set_style_text_font(ui_Label25, &ui_font_Pixel, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_AutoCalibrate = lv_btn_create(ui_VoltageScreen);
    lv_obj_set_width(ui_AutoCalibrate, 100);
    lv_obj_set_height(ui_AutoCalibrate, 50);
    lv_obj_set_x(ui_AutoCalibrate, -155);
    lv_obj_set_y(ui_AutoCalibrate, 95);
    lv_obj_set_align(ui_AutoCalibrate, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_AutoCalibrate, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_AutoCalibrate, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_AutoCalibrate, lv_color_hex(0x549CFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_AutoCalibrate, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label26 = lv_label_create(ui_AutoCalibrate);
    lv_obj_set_width(ui_Label26, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label26, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label26, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label26, "AUTO");

    ui_Container13 = lv_obj_create(ui_VoltageScreen);
    lv_obj_remove_style_all(ui_Container13);
    lv_obj_set_width(ui_Container13, 150);
    lv_obj_set_height(ui_Container13, 50);
    lv_obj_set_x(ui_Container13, 20);
    lv_obj_set_y(ui_Container13, -70);
    lv_obj_set_align(ui_Container13, LV_ALIGN_LEFT_MID);
    lv_obj_clear_flag(ui_Container13, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Label20 = lv_label_create(ui_Container13);
    lv_obj_set_width(ui_Label20, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label20, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label20, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_Label20, "Current Voltage");

    ui_CurrentVoltage = lv_label_create(ui_Container13);
    lv_obj_set_width(ui_CurrentVoltage, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_CurrentVoltage, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_CurrentVoltage, LV_ALIGN_BOTTOM_MID);
    lv_label_set_text(ui_CurrentVoltage, "399.98 V");
    lv_obj_set_style_text_font(ui_CurrentVoltage, &ui_font_Pixel, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Container14 = lv_obj_create(ui_VoltageScreen);
    lv_obj_remove_style_all(ui_Container14);
    lv_obj_set_width(ui_Container14, 150);
    lv_obj_set_height(ui_Container14, 50);
    lv_obj_set_x(ui_Container14, 20);
    lv_obj_set_y(ui_Container14, -10);
    lv_obj_set_align(ui_Container14, LV_ALIGN_LEFT_MID);
    lv_obj_clear_flag(ui_Container14, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_TargetVoltage = lv_label_create(ui_Container14);
    lv_obj_set_width(ui_TargetVoltage, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_TargetVoltage, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_TargetVoltage, LV_ALIGN_BOTTOM_MID);
    lv_label_set_text(ui_TargetVoltage, "400.00 V");
    lv_obj_set_style_text_font(ui_TargetVoltage, &ui_font_Pixel, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label21 = lv_label_create(ui_Container14);
    lv_obj_set_width(ui_Label21, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label21, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label21, LV_ALIGN_TOP_MID);
    lv_label_set_text(ui_Label21, "Target Voltage");

    ui_Container15 = lv_obj_create(ui_VoltageScreen);
    lv_obj_remove_style_all(ui_Container15);
    lv_obj_set_width(ui_Container15, 292);
    lv_obj_set_height(ui_Container15, 134);
    lv_obj_set_x(ui_Container15, 81);
    lv_obj_set_y(ui_Container15, -39);
    lv_obj_set_align(ui_Container15, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Container15, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_Chart2 = lv_chart_create(ui_Container15);
    lv_obj_set_width(ui_Chart2, 240);
    lv_obj_set_height(ui_Chart2, 100);
    lv_obj_set_x(ui_Chart2, 5);
    lv_obj_set_y(ui_Chart2, 0);
    lv_obj_set_align(ui_Chart2, LV_ALIGN_CENTER);
    lv_chart_set_type(ui_Chart2, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ui_Chart2, 30);
    lv_chart_set_axis_tick(ui_Chart2, LV_CHART_AXIS_PRIMARY_X, 10, 5, 5, 2, false, 50);
    lv_chart_set_axis_tick(ui_Chart2, LV_CHART_AXIS_PRIMARY_Y, 10, 5, 5, 2, false, 50);
    lv_chart_set_axis_tick(ui_Chart2, LV_CHART_AXIS_SECONDARY_Y, 10, 5, 5, 2, false, 25);
    lv_chart_series_t * ui_Chart2_series_1 = lv_chart_add_series(ui_Chart2, lv_color_hex(0x25FF00),
                                                                 LV_CHART_AXIS_PRIMARY_Y);
    static lv_coord_t ui_Chart2_series_1_array[] = { 0, 13, 24, 34, 42, 50, 56, 62, 67, 71, 75, 78, 81, 83, 86, 87, 89, 90, 92, 93, 94, 94, 95, 96, 96, 97, 97, 98, 98, 98, 100 };
    lv_chart_set_ext_y_array(ui_Chart2, ui_Chart2_series_1, ui_Chart2_series_1_array);
    lv_obj_set_style_bg_color(ui_Chart2, lv_color_hex(0x161616), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Chart2, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui_Chart2, lv_color_hex(0x3D3D3D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui_Chart2, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_style_size(ui_Chart2, 1, LV_PART_INDICATOR | LV_STATE_DEFAULT);

    lv_obj_set_style_line_color(ui_Chart2, lv_color_hex(0x000000), LV_PART_TICKS | LV_STATE_DEFAULT);
    lv_obj_set_style_line_opa(ui_Chart2, 0, LV_PART_TICKS | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_Button12, ui_event_Button12, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_Button13, ui_event_Button13, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_Button15, ui_event_Button15, LV_EVENT_ALL, NULL);

}
