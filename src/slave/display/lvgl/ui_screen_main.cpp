#include "ui_screen_main.h"
#include "ui_theme.h"
#include "shared/protocol.h"
#include <Arduino.h>

// =============================================================================
// Layout Constants (matching original screen_main.h)
// =============================================================================

#define RPM_Y_POS           80
#define LOGO_Y_POS          (240 - 30)
#define SYNC_DOT_X          12
#define SYNC_DOT_Y          12
#define SYNC_DOT_R          5

#define GEAR_BTN_SIZE       36
#define GEAR_BTN_X          (320 - GEAR_BTN_SIZE - 8)
#define GEAR_BTN_Y          (240 - GEAR_BTN_SIZE - 8)

#define MODE_BTN_SIZE       36
#define MODE_BTN_X          8
#define MODE_BTN_Y          (240 - MODE_BTN_SIZE - 8)

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
static lv_obj_t* lbl_logo = nullptr;
static lv_obj_t* lbl_logo_shadow = nullptr;

// Status indicator
static lv_obj_t* led_status = nullptr;

// Buttons
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
    // Create the screen with dark background
    screen_main = ui_create_screen();
    
    // Title: "POWER STEERING"
    lbl_title = lv_label_create(screen_main);
    lv_label_set_text(lbl_title, "POWER STEERING");
    lv_obj_set_style_text_font(lbl_title, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Status LED (top left)
    led_status = lv_led_create(screen_main);
    lv_obj_set_size(led_status, SYNC_DOT_R * 2, SYNC_DOT_R * 2);
    lv_obj_set_pos(led_status, SYNC_DOT_X - SYNC_DOT_R, SYNC_DOT_Y - SYNC_DOT_R);
    lv_led_set_color(led_status, lv_color_make(0xDC, 0x35, 0x45));  // Red by default
    lv_led_on(led_status);
    
    // RPM value label (large, centered)
    lbl_rpm = lv_label_create(screen_main);
    lv_label_set_text(lbl_rpm, "0");
    lv_obj_set_style_text_font(lbl_rpm, UI_FONT_XLARGE, 0);
    lv_obj_set_style_text_color(lbl_rpm, lv_color_white(), 0);
    lv_obj_align(lbl_rpm, LV_ALIGN_CENTER, 0, RPM_Y_POS - 120);
    lv_obj_add_flag(lbl_rpm, LV_OBJ_FLAG_HIDDEN);  // Hidden until connected
    
    // "NO SIGNAL" label
    lbl_no_signal = lv_label_create(screen_main);
    lv_label_set_text(lbl_no_signal, "NO SIGNAL");
    lv_obj_set_style_text_font(lbl_no_signal, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(lbl_no_signal, lv_color_make(0xDC, 0x35, 0x45), 0);
    lv_obj_align(lbl_no_signal, LV_ALIGN_CENTER, 0, RPM_Y_POS - 120);
    
    // Logo shadow (for 3D effect)
    lbl_logo_shadow = lv_label_create(screen_main);
    lv_label_set_text(lbl_logo_shadow, "Vonderwagen");
    lv_obj_set_style_text_font(lbl_logo_shadow, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl_logo_shadow, lv_color_make(0x42, 0x42, 0x42), 0);
    lv_obj_align(lbl_logo_shadow, LV_ALIGN_BOTTOM_MID, 2, -28);
    
    // Logo text
    lbl_logo = lv_label_create(screen_main);
    lv_label_set_text(lbl_logo, "Vonderwagen");
    lv_obj_set_style_text_font(lbl_logo, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl_logo, lv_color_white(), 0);
    lv_obj_align(lbl_logo, LV_ALIGN_BOTTOM_MID, 0, -30);
    
    // Gear button (bottom right)
    btn_gear = lv_button_create(screen_main);
    lv_obj_set_size(btn_gear, GEAR_BTN_SIZE, GEAR_BTN_SIZE);
    lv_obj_set_pos(btn_gear, GEAR_BTN_X, GEAR_BTN_Y);
    lv_obj_add_style(btn_gear, &style_btn, 0);
    lv_obj_add_style(btn_gear, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_gear, gear_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    create_gear_icon(btn_gear);
    
    // Mode button (bottom left)
    btn_mode = lv_button_create(screen_main);
    lv_obj_set_size(btn_mode, MODE_BTN_SIZE, MODE_BTN_SIZE);
    lv_obj_set_pos(btn_mode, MODE_BTN_X, MODE_BTN_Y);
    lv_obj_add_style(btn_mode, &style_btn, 0);
    lv_obj_add_style(btn_mode, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_mode, mode_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    lbl_mode = lv_label_create(btn_mode);
    lv_label_set_text(lbl_mode, "A");
    lv_obj_set_style_text_font(lbl_mode, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(lbl_mode, lv_color_make(0x2D, 0x9D, 0x4A), 0);  // Green for Auto
    lv_obj_center(lbl_mode);
    
    // RPM up/down buttons (hidden by default)
    btn_rpm_up = create_arrow_button(screen_main, true, RPM_UP_BTN_X, RPM_BTN_Y);
    lv_obj_add_event_cb(btn_rpm_up, rpm_up_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    btn_rpm_down = create_arrow_button(screen_main, false, RPM_DOWN_BTN_X, RPM_BTN_Y);
    lv_obj_add_event_cb(btn_rpm_down, rpm_down_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
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
            lv_led_set_color(led_status, lv_color_make(0x2D, 0x9D, 0x4A));  // Green
        } else {
            lv_led_set_color(led_status, lv_color_make(0xFF, 0xC1, 0x07));  // Amber
        }
        lv_led_on(led_status);
    } else {
        // Blinking handled by update_blink
        lv_led_set_color(led_status, lv_color_make(0xDC, 0x35, 0x45));  // Red
    }
}

void ui_screen_main_set_mode(uint8_t mode) {
    if (mode == MODE_AUTO) {
        lv_label_set_text(lbl_mode, "A");
        lv_obj_set_style_text_color(lbl_mode, lv_color_make(0x2D, 0x9D, 0x4A), 0);  // Green
    } else {
        lv_label_set_text(lbl_mode, "M");
        lv_obj_set_style_text_color(lbl_mode, lv_color_make(0xFF, 0xC1, 0x07), 0);  // Amber
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
