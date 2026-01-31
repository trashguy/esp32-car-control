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
lv_style_t style_btn_nav;
lv_style_t style_btn_nav_pressed;

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
lv_style_t style_menu_bar;

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
    // Material Design 3 Dark Theme colors
    
    // Surface (dark background)
    color_background = UI_COLOR_SURFACE_DIM;
    
    // On Surface (primary text)
    color_text = UI_COLOR_ON_SURFACE;
    
    // Success - MD3 green (softer)
    color_connected = UI_COLOR_SUCCESS;
    
    // Error - MD3 error color
    color_disconnected = UI_COLOR_ERROR;
    
    // Warning - amber
    color_warning = UI_COLOR_WARNING;
    
    // Primary Container for buttons
    color_btn_normal = UI_COLOR_PRIMARY_CONT;
    
    // Surface Container High for pressed state
    color_btn_pressed = UI_COLOR_SURFACE_HIGH;
    
    // Primary color for button text
    color_btn_text = UI_COLOR_PRIMARY;
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
    // Default button style (MD3 Filled Tonal Button)
    // -------------------------------------------------------------------------
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, color_btn_normal);
    lv_style_set_bg_opa(&style_btn, LV_OPA_COVER);
    lv_style_set_border_width(&style_btn, 0);  // MD3 tonal buttons have no border
    lv_style_set_radius(&style_btn, 20);  // MD3 uses pill-shaped buttons
    lv_style_set_pad_all(&style_btn, 10);
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
    // Navigation bar button style (Android dark theme - transparent with white icons)
    // -------------------------------------------------------------------------
    lv_style_init(&style_btn_nav);
    lv_style_set_bg_opa(&style_btn_nav, LV_OPA_TRANSP);  // Transparent background
    lv_style_set_border_width(&style_btn_nav, 0);
    lv_style_set_radius(&style_btn_nav, 8);
    lv_style_set_pad_all(&style_btn_nav, 8);
    lv_style_set_text_color(&style_btn_nav, lv_color_white());  // White icons/text
    lv_style_set_text_font(&style_btn_nav, UI_FONT_NORMAL);
    
    // Nav button pressed state (subtle white highlight)
    lv_style_init(&style_btn_nav_pressed);
    lv_style_set_bg_color(&style_btn_nav_pressed, lv_color_make(0x40, 0x40, 0x40));
    lv_style_set_bg_opa(&style_btn_nav_pressed, LV_OPA_COVER);
    
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
    // Container style (MD3 Surface Container)
    // -------------------------------------------------------------------------
    lv_style_init(&style_container);
    lv_style_set_bg_color(&style_container, UI_COLOR_SURFACE_CONT);
    lv_style_set_bg_opa(&style_container, LV_OPA_COVER);
    lv_style_set_border_color(&style_container, UI_COLOR_OUTLINE_VAR);
    lv_style_set_border_width(&style_container, 1);
    lv_style_set_radius(&style_container, 12);  // MD3 uses larger radius
    lv_style_set_pad_all(&style_container, 12);
    
    // List item style
    lv_style_init(&style_list_item);
    lv_style_set_bg_opa(&style_list_item, LV_OPA_TRANSP);
    lv_style_set_pad_ver(&style_list_item, 8);
    lv_style_set_border_color(&style_list_item, UI_COLOR_OUTLINE_VAR);
    lv_style_set_border_width(&style_list_item, 1);
    lv_style_set_border_side(&style_list_item, LV_BORDER_SIDE_BOTTOM);
    
    // Menu bar style (black bar at bottom of screen)
    lv_style_init(&style_menu_bar);
    lv_style_set_bg_color(&style_menu_bar, UI_COLOR_MENU_BAR);
    lv_style_set_bg_opa(&style_menu_bar, LV_OPA_COVER);
    lv_style_set_border_width(&style_menu_bar, 0);
    lv_style_set_radius(&style_menu_bar, 0);
    lv_style_set_pad_all(&style_menu_bar, 4);
    
    // -------------------------------------------------------------------------
    // Progress bar styles (MD3 style)
    // -------------------------------------------------------------------------
    lv_style_init(&style_bar_bg);
    lv_style_set_bg_color(&style_bar_bg, UI_COLOR_SURFACE_CONT);
    lv_style_set_bg_opa(&style_bar_bg, LV_OPA_COVER);
    lv_style_set_radius(&style_bar_bg, 8);
    
    lv_style_init(&style_bar_indicator);
    lv_style_set_bg_color(&style_bar_indicator, UI_COLOR_PRIMARY);
    lv_style_set_bg_opa(&style_bar_indicator, LV_OPA_COVER);
    lv_style_set_radius(&style_bar_indicator, 8);
    
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

lv_obj_t* ui_create_menu_bar(lv_obj_t* parent, int32_t height) {
    // Android-style bottom navigation bar
    // - Black background
    // - Full width, fixed height at bottom
    // - Uses flex layout for evenly spaced items
    
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 320, height);  // Full width
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_style(bar, &style_menu_bar, 0);
    
    // Use flex layout for even spacing of nav items
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Disable scrolling on menu bar
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    
    return bar;
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
