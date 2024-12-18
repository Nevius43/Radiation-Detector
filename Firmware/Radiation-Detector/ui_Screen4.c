// This file was generated by SquareLine Studio
// SquareLine Studio version: SquareLine Studio 1.5.0
// LVGL version: 8.3.11
// Project name: SquareLine_Project

#include "ui.h"

void ui_Screen4_screen_init(void)
{
    ui_Screen4 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen4, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Screen4, lv_color_hex(0x363383), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen4, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel3 = lv_obj_create(ui_Screen4);
    lv_obj_set_width(ui_Panel3, 480);
    lv_obj_set_height(ui_Panel3, 320);
    lv_obj_set_x(ui_Panel3, -480);
    lv_obj_set_y(ui_Panel3, 0);
    lv_obj_set_align(ui_Panel3, LV_ALIGN_CENTER);
    lv_obj_clear_flag(ui_Panel3, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Panel3, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Container2 = lv_obj_create(ui_Screen4);
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
    lv_obj_set_style_bg_color(ui_Button12, lv_color_hex(0x4A4CD5), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button12, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_Button12, &ui_img_1719020684, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Button13 = lv_btn_create(ui_Container2);
    lv_obj_set_width(ui_Button13, 120);
    lv_obj_set_height(ui_Button13, 50);
    lv_obj_set_x(ui_Button13, 120);
    lv_obj_set_y(ui_Button13, 0);
    lv_obj_set_align(ui_Button13, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_Button13, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button13, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Button13, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Button13, lv_color_hex(0x5255EE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button13, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_Button13, &ui_img_220545925, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Button14 = lv_btn_create(ui_Container2);
    lv_obj_set_width(ui_Button14, 120);
    lv_obj_set_height(ui_Button14, 50);
    lv_obj_set_x(ui_Button14, 240);
    lv_obj_set_y(ui_Button14, 0);
    lv_obj_set_align(ui_Button14, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_Button14, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button14, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Button14, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Button14, lv_color_hex(0x5BD16D), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button14, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_Button14, &ui_img_1666199263, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Button15 = lv_btn_create(ui_Container2);
    lv_obj_set_width(ui_Button15, 120);
    lv_obj_set_height(ui_Button15, 50);
    lv_obj_set_x(ui_Button15, 360);
    lv_obj_set_y(ui_Button15, 0);
    lv_obj_set_align(ui_Button15, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_Button15, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button15, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_Button15, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_Button15, lv_color_hex(0x5255EE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button15, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_img_src(ui_Button15, &ui_img_85097194, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Panel2 = lv_obj_create(ui_Screen4);
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

    ui_Label20 = lv_label_create(ui_Screen4);
    lv_obj_set_width(ui_Label20, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label20, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label20, -119);
    lv_obj_set_y(ui_Label20, -90);
    lv_obj_set_align(ui_Label20, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label20, "Current Voltage");

    ui_Label21 = lv_label_create(ui_Screen4);
    lv_obj_set_width(ui_Label21, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label21, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label21, 125);
    lv_obj_set_y(ui_Label21, -90);
    lv_obj_set_align(ui_Label21, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label21, "Target Voltage");

    ui_Label22 = lv_label_create(ui_Screen4);
    lv_obj_set_width(ui_Label22, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label22, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label22, -115);
    lv_obj_set_y(ui_Label22, -39);
    lv_obj_set_align(ui_Label22, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label22, "399.98 V");
    lv_obj_set_style_text_font(ui_Label22, &ui_font_Pixel, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Button39 = lv_btn_create(ui_Screen4);
    lv_obj_set_width(ui_Button39, 50);
    lv_obj_set_height(ui_Button39, 50);
    lv_obj_set_x(ui_Button39, 180);
    lv_obj_set_y(ui_Button39, 92);
    lv_obj_set_align(ui_Button39, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Button39, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button39, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Button39, lv_color_hex(0x5255EE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button39, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label24 = lv_label_create(ui_Button39);
    lv_obj_set_width(ui_Label24, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label24, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label24, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label24, "+");
    lv_obj_set_style_text_font(ui_Label24, &ui_font_Pixel, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Button38 = lv_btn_create(ui_Screen4);
    lv_obj_set_width(ui_Button38, 50);
    lv_obj_set_height(ui_Button38, 50);
    lv_obj_set_x(ui_Button38, 100);
    lv_obj_set_y(ui_Button38, 92);
    lv_obj_set_align(ui_Button38, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Button38, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button38, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Button38, lv_color_hex(0x5255EE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button38, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label25 = lv_label_create(ui_Button38);
    lv_obj_set_width(ui_Label25, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label25, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label25, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label25, "-");
    lv_obj_set_style_text_font(ui_Label25, &ui_font_Pixel, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label23 = lv_label_create(ui_Screen4);
    lv_obj_set_width(ui_Label23, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label23, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_Label23, 132);
    lv_obj_set_y(ui_Label23, -39);
    lv_obj_set_align(ui_Label23, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label23, "400.00 V");
    lv_obj_set_style_text_font(ui_Label23, &ui_font_Pixel, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Button40 = lv_btn_create(ui_Screen4);
    lv_obj_set_width(ui_Button40, 100);
    lv_obj_set_height(ui_Button40, 50);
    lv_obj_set_x(ui_Button40, -123);
    lv_obj_set_y(ui_Button40, 92);
    lv_obj_set_align(ui_Button40, LV_ALIGN_CENTER);
    lv_obj_add_flag(ui_Button40, LV_OBJ_FLAG_SCROLL_ON_FOCUS);     /// Flags
    lv_obj_clear_flag(ui_Button40, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_Button40, lv_color_hex(0x0EDD33), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Button40, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Label26 = lv_label_create(ui_Button40);
    lv_obj_set_width(ui_Label26, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_Label26, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_Label26, LV_ALIGN_CENTER);
    lv_label_set_text(ui_Label26, "AUTO");

    lv_obj_add_event_cb(ui_Button12, ui_event_Button12, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_Button13, ui_event_Button13, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_Button15, ui_event_Button15, LV_EVENT_ALL, NULL);

}
