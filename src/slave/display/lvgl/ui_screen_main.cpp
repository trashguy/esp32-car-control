#include "ui_screen_main.h"
#include "ui_theme.h"
#include "shared/protocol.h"
#include <Arduino.h>

// =============================================================================
// Layout Constants (matching original screen_main.h)
// =============================================================================

#define RPM_Y_POS           70    // Adjusted for menu bar
#define LOGO_Y_POS          (UI_CONTENT_HEIGHT - 20)  // Above menu bar
#define SYNC_DOT_X          12
#define SYNC_DOT_Y          12
#define SYNC_DOT_R          5

#define NAV_BTN_SIZE        40    // Navigation button size in menu bar

#define RPM_BTN_SIZE        40
#define RPM_BTN_Y           (RPM_Y_POS - RPM_BTN_SIZE / 2)
#define RPM_UP_BTN_X        20
#define RPM_DOWN_BTN_X      (320 - RPM_BTN_SIZE - 20)

// =============================================================================
// Static UI Objects
// =============================================================================

static lv_obj_t* screen_main = nullptr;

// Labels
static lv_obj_t* lbl_title = nullptr;
static lv_obj_t* lbl_rpm = nullptr;
static lv_obj_t* lbl_no_signal = nullptr;

// Status indicator
static lv_obj_t* led_status = nullptr;

// Buttons
static lv_obj_t* menu_bar = nullptr;
static lv_obj_t* btn_gear = nullptr;
static lv_obj_t* btn_mode = nullptr;
static lv_obj_t* lbl_mode = nullptr;  // Label inside mode button
static lv_obj_t* btn_rpm_up = nullptr;
static lv_obj_t* btn_rpm_down = nullptr;

// Callbacks
static void (*cb_gear)(void) = nullptr;
static void (*cb_mode)(void) = nullptr;
static void (*cb_rpm_up)(void) = nullptr;
static void (*cb_rpm_down)(void) = nullptr;

// State
static bool blink_state = false;
static bool is_connected = false;

// =============================================================================
// Event Handlers
// =============================================================================

static void gear_btn_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && cb_gear) {
        cb_gear();
    }
}

static void mode_btn_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && cb_mode) {
        cb_mode();
    }
}

static void rpm_up_btn_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && cb_rpm_up) {
        cb_rpm_up();
    }
}

static void rpm_down_btn_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && cb_rpm_down) {
        cb_rpm_down();
    }
}

// =============================================================================
// Helper: Draw Gear Icon on Canvas (simplified version)
// =============================================================================

static void create_gear_icon(lv_obj_t* parent) {
    // For now, just use a text label - can be replaced with canvas/image later
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl);
}

// =============================================================================
// Helper: Create Arrow Button
// =============================================================================

static lv_obj_t* create_arrow_button(lv_obj_t* parent, bool up, int32_t x, int32_t y) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, RPM_BTN_SIZE, RPM_BTN_SIZE);
    lv_obj_set_pos(btn, x, y);
    lv_obj_add_style(btn, &style_btn, 0);
    lv_obj_add_style(btn, &style_btn_pressed, LV_STATE_PRESSED);
    
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, up ? LV_SYMBOL_UP : LV_SYMBOL_DOWN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(lbl);
    
    // Hidden by default
    lv_obj_add_flag(btn, LV_OBJ_FLAG_HIDDEN);
    
    return btn;
}

// =============================================================================
// Public API
// =============================================================================

void ui_screen_main_create() {
    // Create the screen with grey background
    screen_main = ui_create_screen();
    
    // Brand logo at top: "VONDERWAGEN" - stylized with letter spacing
    lbl_title = lv_label_create(screen_main);
    lv_label_set_text(lbl_title, "VONDERWAGEN");
    lv_obj_set_style_text_font(lbl_title, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_set_style_text_letter_space(lbl_title, 4, 0);  // Wider letter spacing for style
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 12);
    
    // Status LED (top left)
    led_status = lv_led_create(screen_main);
    lv_obj_set_size(led_status, SYNC_DOT_R * 2, SYNC_DOT_R * 2);
    lv_obj_set_pos(led_status, SYNC_DOT_X - SYNC_DOT_R, SYNC_DOT_Y - SYNC_DOT_R);
    lv_led_set_color(led_status, UI_COLOR_ERROR);  // MD3 error color by default
    lv_led_on(led_status);
    
    // RPM value label (large, centered in content area)
    lbl_rpm = lv_label_create(screen_main);
    lv_label_set_text(lbl_rpm, "0");
    lv_obj_set_style_text_font(lbl_rpm, UI_FONT_XLARGE, 0);
    lv_obj_set_style_text_color(lbl_rpm, UI_COLOR_ON_SURFACE, 0);
    lv_obj_align(lbl_rpm, LV_ALIGN_TOP_MID, 0, RPM_Y_POS);
    lv_obj_add_flag(lbl_rpm, LV_OBJ_FLAG_HIDDEN);  // Hidden until connected
    
    // "NO SIGNAL" label
    lbl_no_signal = lv_label_create(screen_main);
    lv_label_set_text(lbl_no_signal, "NO SIGNAL");
    lv_obj_set_style_text_font(lbl_no_signal, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(lbl_no_signal, UI_COLOR_ERROR, 0);
    lv_obj_align(lbl_no_signal, LV_ALIGN_TOP_MID, 0, RPM_Y_POS);
    
    // RPM up/down buttons (hidden by default, in content area)
    btn_rpm_up = create_arrow_button(screen_main, true, RPM_UP_BTN_X, RPM_BTN_Y);
    lv_obj_add_event_cb(btn_rpm_up, rpm_up_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    btn_rpm_down = create_arrow_button(screen_main, false, RPM_DOWN_BTN_X, RPM_BTN_Y);
    lv_obj_add_event_cb(btn_rpm_down, rpm_down_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    // =========================================================================
    // Android-style bottom navigation bar (black)
    // =========================================================================
    menu_bar = ui_create_menu_bar(screen_main, UI_MENU_BAR_HEIGHT);
    
    // Mode button (left side of menu bar) - Android dark style
    btn_mode = lv_button_create(menu_bar);
    lv_obj_set_size(btn_mode, NAV_BTN_SIZE, NAV_BTN_SIZE);
    lv_obj_add_style(btn_mode, &style_btn_nav, 0);
    lv_obj_add_style(btn_mode, &style_btn_nav_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_mode, mode_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    lbl_mode = lv_label_create(btn_mode);
    lv_label_set_text(lbl_mode, "A");
    lv_obj_set_style_text_font(lbl_mode, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(lbl_mode, UI_COLOR_SUCCESS, 0);  // Green for Auto
    lv_obj_center(lbl_mode);
    
    // Settings/Gear button (right side of menu bar) - Android dark style
    btn_gear = lv_button_create(menu_bar);
    lv_obj_set_size(btn_gear, NAV_BTN_SIZE, NAV_BTN_SIZE);
    lv_obj_add_style(btn_gear, &style_btn_nav, 0);
    lv_obj_add_style(btn_gear, &style_btn_nav_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_gear, gear_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    // Gear icon (white)
    lv_obj_t* lbl_gear = lv_label_create(btn_gear);
    lv_label_set_text(lbl_gear, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(lbl_gear, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_gear, lv_color_white(), 0);
    lv_obj_center(lbl_gear);
    
    Serial.println("UI Main screen created");
}

lv_obj_t* ui_screen_main_get() {
    return screen_main;
}

void ui_screen_main_set_rpm(uint16_t rpm, bool connected) {
    is_connected = connected;
    
    if (connected) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%u", rpm);
        lv_label_set_text(lbl_rpm, buf);
        lv_obj_clear_flag(lbl_rpm, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_no_signal, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(lbl_rpm, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_no_signal, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_screen_main_set_status(bool connected, bool synced) {
    is_connected = connected;
    
    if (connected) {
        if (synced) {
            lv_led_set_color(led_status, UI_COLOR_SUCCESS);  // MD3 green
        } else {
            lv_led_set_color(led_status, UI_COLOR_WARNING);  // MD3 amber
        }
        lv_led_on(led_status);
    } else {
        // Blinking handled by update_blink
        lv_led_set_color(led_status, UI_COLOR_ERROR);  // MD3 error
    }
}

void ui_screen_main_set_mode(uint8_t mode) {
    if (mode == MODE_AUTO) {
        lv_label_set_text(lbl_mode, "A");
        lv_obj_set_style_text_color(lbl_mode, UI_COLOR_SUCCESS, 0);  // MD3 green
    } else {
        lv_label_set_text(lbl_mode, "M");
        lv_obj_set_style_text_color(lbl_mode, UI_COLOR_WARNING, 0);  // MD3 amber
    }
}

void ui_screen_main_show_rpm_buttons(bool show) {
    if (show) {
        lv_obj_clear_flag(btn_rpm_up, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(btn_rpm_down, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(btn_rpm_up, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(btn_rpm_down, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_screen_main_set_gear_callback(void (*callback)(void)) {
    cb_gear = callback;
}

void ui_screen_main_set_mode_callback(void (*callback)(void)) {
    cb_mode = callback;
}

void ui_screen_main_set_rpm_up_callback(void (*callback)(void)) {
    cb_rpm_up = callback;
}

void ui_screen_main_set_rpm_down_callback(void (*callback)(void)) {
    cb_rpm_down = callback;
}

void ui_screen_main_update_blink() {
    if (!is_connected) {
        blink_state = !blink_state;
        if (blink_state) {
            lv_led_on(led_status);
        } else {
            lv_led_off(led_status);
        }
    }
}
