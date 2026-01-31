#include "ui_theme.h"
#include <Arduino.h>

// =============================================================================
// Style Definitions
// =============================================================================

// Screen styles
lv_style_t style_screen;

// Button styles
lv_style_t style_btn;
lv_style_t style_btn_pressed;
lv_style_t style_btn_success;
lv_style_t style_btn_danger;

// Label styles
lv_style_t style_label;
lv_style_t style_label_large;
lv_style_t style_label_title;
lv_style_t style_label_success;
lv_style_t style_label_danger;
lv_style_t style_label_warning;

// Container styles
lv_style_t style_container;
lv_style_t style_list_item;

// Progress bar styles
lv_style_t style_bar_bg;
lv_style_t style_bar_indicator;

// =============================================================================
// Color Conversions
// =============================================================================

// Pre-computed LVGL colors (converted from RGB565)
static lv_color_t color_background;
static lv_color_t color_text;
static lv_color_t color_connected;
static lv_color_t color_disconnected;
static lv_color_t color_warning;
static lv_color_t color_btn_normal;
static lv_color_t color_btn_pressed;
static lv_color_t color_btn_text;

static void init_colors() {
    // Pure black background
    color_background = lv_color_black();
    
    // Pure white text
    color_text = lv_color_white();
    
    // Muted green (#2D9D4A) - from 0x2DC9
    color_connected = lv_color_make(0x2D, 0x9D, 0x4A);
    
    // Muted red (#DC3545) - from 0xD8A3
    color_disconnected = lv_color_make(0xDC, 0x35, 0x45);
    
    // Muted amber (#FFC107) - from 0xFD20
    color_warning = lv_color_make(0xFF, 0xC1, 0x07);
    
    // Dark blue (#293A4A) - from 0x2945
    color_btn_normal = lv_color_make(0x29, 0x3A, 0x4A);
    
    // Lighter blue (#3D5A73) - from 0x3B8F
    color_btn_pressed = lv_color_make(0x3D, 0x5A, 0x73);
    
    // Soft white (#DEE2E6) - from 0xDEFB
    color_btn_text = lv_color_make(0xDE, 0xE2, 0xE6);
}

// =============================================================================
// Theme Initialization
// =============================================================================

void ui_theme_init() {
    init_colors();
    
    // -------------------------------------------------------------------------
    // Screen style (black background)
    // -------------------------------------------------------------------------
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, color_background);
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    
    // -------------------------------------------------------------------------
    // Default button style
    // -------------------------------------------------------------------------
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, color_btn_normal);
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_border_color(&style_btn, color_btn_text);
    lv_style_set_border_width(&style_btn, 1);
    lv_style_set_border_opa(&style_btn, LV_OPA_50);
    lv_style_set_radius(&style_btn, 8);
    lv_style_set_pad_all(&style_btn, 8);
    lv_style_set_text_color(&style_btn, color_btn_text);
    lv_style_set_text_font(&style_btn, UI_FONT_NORMAL);
    
    // Pressed button state
    lv_style_init(&style_btn_pressed);
    lv_style_set_bg_color(&style_btn_pressed, color_btn_pressed);
    
    // Success button (green)
    lv_style_init(&style_btn_success);
    lv_style_set_bg_color(&style_btn_success, color_connected);
    
    // Danger button (red)
    lv_style_init(&style_btn_danger);
    lv_style_set_bg_color(&style_btn_danger, color_disconnected);
    
    // -------------------------------------------------------------------------
    // Label styles
    // -------------------------------------------------------------------------
    lv_style_init(&style_label);
    lv_style_set_text_color(&style_label, color_text);
    lv_style_set_text_font(&style_label, UI_FONT_NORMAL);
    
    lv_style_init(&style_label_large);
    lv_style_set_text_color(&style_label_large, color_text);
    lv_style_set_text_font(&style_label_large, UI_FONT_XLARGE);
    
    lv_style_init(&style_label_title);
    lv_style_set_text_color(&style_label_title, color_text);
    lv_style_set_text_font(&style_label_title, UI_FONT_MEDIUM);
    
    lv_style_init(&style_label_success);
    lv_style_set_text_color(&style_label_success, color_connected);
    lv_style_set_text_font(&style_label_success, UI_FONT_NORMAL);
    
    lv_style_init(&style_label_danger);
    lv_style_set_text_color(&style_label_danger, color_disconnected);
    lv_style_set_text_font(&style_label_danger, UI_FONT_NORMAL);
    
    lv_style_init(&style_label_warning);
    lv_style_set_text_color(&style_label_warning, color_warning);
    lv_style_set_text_font(&style_label_warning, UI_FONT_NORMAL);
    
    // -------------------------------------------------------------------------
    // Container style
    // -------------------------------------------------------------------------
    lv_style_init(&style_container);
    lv_style_set_bg_color(&style_container, lv_color_make(0x10, 0x10, 0x10));
    lv_style_set_bg_opa(&style_container, LV_OPA_COVER);
    lv_style_set_border_color(&style_container, lv_color_make(0x30, 0x30, 0x30));
    lv_style_set_border_width(&style_container, 1);
    lv_style_set_radius(&style_container, 4);
    lv_style_set_pad_all(&style_container, 8);
    
    // List item style
    lv_style_init(&style_list_item);
    lv_style_set_bg_opa(&style_list_item, LV_OPA_TRANSP);
    lv_style_set_pad_ver(&style_list_item, 8);
    lv_style_set_border_color(&style_list_item, lv_color_make(0x30, 0x30, 0x30));
    lv_style_set_border_width(&style_list_item, 1);
    lv_style_set_border_side(&style_list_item, LV_BORDER_SIDE_BOTTOM);
    
    // -------------------------------------------------------------------------
    // Progress bar styles
    // -------------------------------------------------------------------------
    lv_style_init(&style_bar_bg);
    lv_style_set_bg_color(&style_bar_bg, lv_color_make(0x20, 0x20, 0x20));
    lv_style_set_bg_opa(&style_bar_bg, LV_OPA_COVER);
    lv_style_set_radius(&style_bar_bg, 4);
    
    lv_style_init(&style_bar_indicator);
    lv_style_set_bg_color(&style_bar_indicator, color_connected);
    lv_style_set_bg_opa(&style_bar_indicator, LV_OPA_COVER);
    lv_style_set_radius(&style_bar_indicator, 4);
    
    Serial.println("UI theme initialized");
}

void ui_theme_apply() {
    // Apply dark theme settings to default display
    lv_disp_t* disp = lv_display_get_default();
    if (disp) {
        lv_obj_t* scr = lv_display_get_screen_active(disp);
        if (scr) {
            lv_obj_add_style(scr, &style_screen, 0);
        }
    }
}

// =============================================================================
// Helper Functions
// =============================================================================

lv_obj_t* ui_create_button(lv_obj_t* parent, const char* text, int32_t width, int32_t height) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_add_style(btn, &style_btn, 0);
    lv_obj_add_style(btn, &style_btn_pressed, LV_STATE_PRESSED);
    
    if (width > 0) {
        lv_obj_set_width(btn, width);
    }
    if (height > 0) {
        lv_obj_set_height(btn, height);
    }
    
    // Add label
    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    
    return btn;
}

lv_obj_t* ui_create_label(lv_obj_t* parent, const char* text, const lv_font_t* font) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_add_style(label, &style_label, 0);
    
    if (font) {
        lv_obj_set_style_text_font(label, font, 0);
    }
    
    return label;
}

lv_obj_t* ui_create_screen() {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_add_style(scr, &style_screen, 0);
    return scr;
}

// =============================================================================
// Swipe Gesture Helper
// =============================================================================

static void swipe_back_gesture_handler(lv_event_t* e) {
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_active());
    if (dir == LV_DIR_RIGHT) {
        void (*callback)(void) = (void (*)(void))lv_event_get_user_data(e);
        if (callback) {
            callback();
        }
    }
}

void ui_add_swipe_back_gesture(lv_obj_t* screen, void (*back_callback)(void)) {
    if (!screen || !back_callback) return;
    lv_obj_add_event_cb(screen, swipe_back_gesture_handler, LV_EVENT_GESTURE, (void*)back_callback);
}
