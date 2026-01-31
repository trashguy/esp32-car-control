#include "ui_screen_main.h"
#include "ui_theme.h"
#include "shared/protocol.h"
#include <Arduino.h>

// =============================================================================
// Layout Constants
// =============================================================================

#define SYNC_DOT_X          12
#define SYNC_DOT_Y          12
#define SYNC_DOT_R          5

#define NAV_BTN_SIZE        40    // Navigation button size in menu bar

// Power steering widget layout - near top, below header
#define PS_WIDGET_Y         38    // Y position of the power steering widget (closer to top)
#define PS_WIDGET_HEIGHT    44    // Height of the oval container
#define PS_WIDGET_WIDTH    220    // Width of the oval container
#define RPM_BTN_SIZE        40    // Arrow button size
#define RPM_BTN_MARGIN      8     // Margin from screen edge

// Water pump widget layout - below power steering
#define WP_WIDGET_Y         (PS_WIDGET_Y + PS_WIDGET_HEIGHT + 24)  // Below PS label
#define WP_WIDGET_HEIGHT    44
#define WP_WIDGET_WIDTH    220

// =============================================================================
// Static UI Objects
// =============================================================================

static lv_obj_t* screen_main = nullptr;

// Labels
static lv_obj_t* lbl_title = nullptr;
static lv_obj_t* lbl_no_signal = nullptr;
static lv_obj_t* lbl_wifi_icon = nullptr;  // WiFi status icon (top right)

// Status indicator
static lv_obj_t* led_status = nullptr;

// RPM row container (tappable for mode toggle)
static lv_obj_t* cont_power_steering = nullptr;  // Outer oval container
static lv_obj_t* cont_rpm_row = nullptr;
static lv_obj_t* lbl_mode = nullptr;      // "AUTO" or "MANUAL" text
static lv_obj_t* lbl_rpm = nullptr;       // RPM value
static lv_obj_t* lbl_ps_label = nullptr;  // "POWER STEERING" label

// Arrow buttons (outside the rpm row container)
static lv_obj_t* btn_rpm_up = nullptr;
static lv_obj_t* btn_rpm_down = nullptr;

// Water pump widget (similar to power steering but static for now)
static lv_obj_t* cont_water_pump = nullptr;
static lv_obj_t* cont_wp_row = nullptr;
static lv_obj_t* lbl_wp_mode = nullptr;
static lv_obj_t* lbl_wp_value = nullptr;
static lv_obj_t* lbl_wp_label = nullptr;

// Menu bar
static lv_obj_t* menu_bar = nullptr;
static lv_obj_t* btn_gear = nullptr;

// Callbacks
static void (*cb_gear)(void) = nullptr;
static void (*cb_mode)(void) = nullptr;
static void (*cb_rpm_up)(void) = nullptr;
static void (*cb_rpm_down)(void) = nullptr;

// State
static bool blink_state = false;
static bool is_connected = false;
static uint8_t current_mode = MODE_AUTO;

// Menu bar auto-hide state
static bool menu_bar_visible = false;
static uint32_t menu_bar_show_time = 0;
static const uint32_t MENU_BAR_TIMEOUT_MS = 3000;  // Hide after 3 seconds

// =============================================================================
// Event Handlers
// =============================================================================

static void gear_btn_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && cb_gear) {
        cb_gear();
    }
}

static void rpm_row_event_handler(lv_event_t* e) {
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
// Menu Bar Show/Hide Logic
// =============================================================================

static void show_menu_bar() {
    if (!menu_bar_visible) {
        lv_obj_clear_flag(menu_bar, LV_OBJ_FLAG_HIDDEN);
        menu_bar_visible = true;
    }
    menu_bar_show_time = lv_tick_get();
}

static void hide_menu_bar() {
    if (menu_bar_visible) {
        lv_obj_add_flag(menu_bar, LV_OBJ_FLAG_HIDDEN);
        menu_bar_visible = false;
    }
}

// Screen touch event - show menu bar when screen background is touched
static void screen_touch_event_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_PRESSED) {
        // Only trigger if the touch target is the screen itself (not a child widget)
        lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
        if (target == screen_main) {
            show_menu_bar();
        }
    }
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
    lv_obj_set_style_text_letter_space(lbl_title, 4, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 6);
    
    // Status LED (top left, vertically aligned with title)
    led_status = lv_led_create(screen_main);
    lv_obj_set_size(led_status, SYNC_DOT_R * 2, SYNC_DOT_R * 2);
    lv_obj_align(led_status, LV_ALIGN_TOP_LEFT, 10, 12);  // Aligned with title baseline
    lv_led_set_color(led_status, UI_COLOR_ERROR);
    lv_led_on(led_status);
    
    // WiFi icon (top right, hidden by default - shown when connected)
    lbl_wifi_icon = lv_label_create(screen_main);
    lv_label_set_text(lbl_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(lbl_wifi_icon, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_wifi_icon, UI_COLOR_SUCCESS, 0);  // Green when connected
    lv_obj_align(lbl_wifi_icon, LV_ALIGN_TOP_RIGHT, -10, 8);
    lv_obj_add_flag(lbl_wifi_icon, LV_OBJ_FLAG_HIDDEN);  // Hidden until WiFi connects
    
    // =========================================================================
    // Power Steering Widget: Oval container with [▲] [AUTO  1234] [▼] inside
    // The center area (mode + rpm) is tappable to toggle mode
    // =========================================================================
    
    // Calculate button Y position (vertically centered in the widget)
    int32_t btn_y = PS_WIDGET_Y + (PS_WIDGET_HEIGHT - RPM_BTN_SIZE) / 2;
    
    // Up arrow button (left side) - matching oval style
    btn_rpm_up = lv_button_create(screen_main);
    lv_obj_set_size(btn_rpm_up, RPM_BTN_SIZE, RPM_BTN_SIZE);
    lv_obj_set_pos(btn_rpm_up, RPM_BTN_MARGIN, btn_y);
    lv_obj_set_style_bg_color(btn_rpm_up, lv_color_make(0x48, 0x48, 0x48), 0);  // Match oval
    lv_obj_set_style_bg_color(btn_rpm_up, lv_color_make(0x58, 0x58, 0x58), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_rpm_up, RPM_BTN_SIZE / 2, 0);  // Circular
    lv_obj_set_style_border_width(btn_rpm_up, 0, 0);
    lv_obj_set_style_shadow_width(btn_rpm_up, 0, 0);
    lv_obj_add_event_cb(btn_rpm_up, rpm_up_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(btn_rpm_up, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    
    lv_obj_t* lbl_up = lv_label_create(btn_rpm_up);
    lv_label_set_text(lbl_up, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_font(lbl_up, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_up, UI_COLOR_ON_SURFACE, 0);
    lv_obj_center(lbl_up);
    
    // Down arrow button (right side) - matching oval style
    btn_rpm_down = lv_button_create(screen_main);
    lv_obj_set_size(btn_rpm_down, RPM_BTN_SIZE, RPM_BTN_SIZE);
    lv_obj_set_pos(btn_rpm_down, UI_SCREEN_WIDTH - RPM_BTN_SIZE - RPM_BTN_MARGIN, btn_y);
    lv_obj_set_style_bg_color(btn_rpm_down, lv_color_make(0x48, 0x48, 0x48), 0);  // Match oval
    lv_obj_set_style_bg_color(btn_rpm_down, lv_color_make(0x58, 0x58, 0x58), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_rpm_down, RPM_BTN_SIZE / 2, 0);  // Circular
    lv_obj_set_style_border_width(btn_rpm_down, 0, 0);
    lv_obj_set_style_shadow_width(btn_rpm_down, 0, 0);
    lv_obj_add_event_cb(btn_rpm_down, rpm_down_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(btn_rpm_down, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    
    lv_obj_t* lbl_down = lv_label_create(btn_rpm_down);
    lv_label_set_text(lbl_down, LV_SYMBOL_MINUS);
    lv_obj_set_style_text_font(lbl_down, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_down, UI_COLOR_ON_SURFACE, 0);
    lv_obj_center(lbl_down);
    
    // Outer oval container (lighter grey background)
    cont_power_steering = lv_obj_create(screen_main);
    lv_obj_set_size(cont_power_steering, PS_WIDGET_WIDTH, PS_WIDGET_HEIGHT);
    lv_obj_align(cont_power_steering, LV_ALIGN_TOP_MID, 0, PS_WIDGET_Y);
    lv_obj_set_style_bg_color(cont_power_steering, lv_color_make(0x48, 0x48, 0x48), 0);  // Lighter grey
    lv_obj_set_style_bg_opa(cont_power_steering, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cont_power_steering, lv_color_make(0x58, 0x58, 0x58), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(cont_power_steering, 0, 0);
    lv_obj_set_style_radius(cont_power_steering, PS_WIDGET_HEIGHT / 2, 0);  // Full oval/pill shape
    lv_obj_set_style_pad_all(cont_power_steering, 0, 0);
    lv_obj_clear_flag(cont_power_steering, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont_power_steering, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(cont_power_steering, rpm_row_event_handler, LV_EVENT_CLICKED, nullptr);
    
    // Inner flex container for mode + RPM (centered in oval)
    cont_rpm_row = lv_obj_create(cont_power_steering);
    lv_obj_set_size(cont_rpm_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(cont_rpm_row);
    lv_obj_set_style_bg_opa(cont_rpm_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_rpm_row, 0, 0);
    lv_obj_set_style_pad_all(cont_rpm_row, 0, 0);
    lv_obj_clear_flag(cont_rpm_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(cont_rpm_row, LV_OBJ_FLAG_CLICKABLE);  // Let clicks pass through to parent
    lv_obj_add_flag(cont_rpm_row, LV_OBJ_FLAG_EVENT_BUBBLE);  // Bubble events to parent
    
    // Use flex layout for horizontal alignment
    lv_obj_set_flex_flow(cont_rpm_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_rpm_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont_rpm_row, 12, 0);  // Gap between mode and RPM
    
    // Mode indicator label (e.g., "AUTO" or "MANUAL")
    lbl_mode = lv_label_create(cont_rpm_row);
    lv_label_set_text(lbl_mode, "AUTO");
    lv_obj_set_style_text_font(lbl_mode, UI_FONT_MEDIUM, 0);  // 20px - readable
    lv_obj_set_style_text_color(lbl_mode, UI_COLOR_SUCCESS, 0);  // Green for Auto
    lv_obj_add_flag(lbl_mode, LV_OBJ_FLAG_EVENT_BUBBLE);  // Bubble clicks to parent
    
    // RPM value label
    lbl_rpm = lv_label_create(cont_rpm_row);
    lv_label_set_text(lbl_rpm, "0");
    lv_obj_set_style_text_font(lbl_rpm, UI_FONT_LARGE, 0);  // 28px - smaller than before
    lv_obj_set_style_text_color(lbl_rpm, UI_COLOR_ON_SURFACE, 0);
    lv_obj_add_flag(lbl_rpm, LV_OBJ_FLAG_EVENT_BUBBLE);  // Bubble clicks to parent
    
    // "POWER STEERING" label below the oval
    lbl_ps_label = lv_label_create(screen_main);
    lv_label_set_text(lbl_ps_label, "POWER STEERING");
    lv_obj_set_style_text_font(lbl_ps_label, &lv_font_montserrat_12, 0);  // Size ~11
    lv_obj_set_style_text_color(lbl_ps_label, UI_COLOR_ON_SURFACE_VAR, 0);  // Slightly dimmer text
    lv_obj_align_to(lbl_ps_label, cont_power_steering, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
    
    // =========================================================================
    // Water Pump Widget: Similar oval container with AUTO and 100%
    // =========================================================================
    
    // Outer oval container (lighter grey background)
    cont_water_pump = lv_obj_create(screen_main);
    lv_obj_set_size(cont_water_pump, WP_WIDGET_WIDTH, WP_WIDGET_HEIGHT);
    lv_obj_align(cont_water_pump, LV_ALIGN_TOP_MID, 0, WP_WIDGET_Y);
    lv_obj_set_style_bg_color(cont_water_pump, lv_color_make(0x48, 0x48, 0x48), 0);  // Lighter grey
    lv_obj_set_style_bg_opa(cont_water_pump, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(cont_water_pump, lv_color_make(0x58, 0x58, 0x58), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(cont_water_pump, 0, 0);
    lv_obj_set_style_radius(cont_water_pump, WP_WIDGET_HEIGHT / 2, 0);  // Full oval/pill shape
    lv_obj_set_style_pad_all(cont_water_pump, 0, 0);
    lv_obj_clear_flag(cont_water_pump, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont_water_pump, LV_OBJ_FLAG_CLICKABLE);
    // TODO: Add event handler for water pump mode toggle when implemented
    
    // Inner flex container for mode + value (centered in oval)
    cont_wp_row = lv_obj_create(cont_water_pump);
    lv_obj_set_size(cont_wp_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(cont_wp_row);
    lv_obj_set_style_bg_opa(cont_wp_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_wp_row, 0, 0);
    lv_obj_set_style_pad_all(cont_wp_row, 0, 0);
    lv_obj_clear_flag(cont_wp_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(cont_wp_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(cont_wp_row, LV_OBJ_FLAG_EVENT_BUBBLE);
    
    // Use flex layout for horizontal alignment
    lv_obj_set_flex_flow(cont_wp_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_wp_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont_wp_row, 12, 0);
    
    // Mode indicator label - always AUTO for now
    lbl_wp_mode = lv_label_create(cont_wp_row);
    lv_label_set_text(lbl_wp_mode, "AUTO");
    lv_obj_set_style_text_font(lbl_wp_mode, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_wp_mode, UI_COLOR_SUCCESS, 0);  // Green for Auto
    lv_obj_add_flag(lbl_wp_mode, LV_OBJ_FLAG_EVENT_BUBBLE);
    
    // Value label - always 100% for now
    lbl_wp_value = lv_label_create(cont_wp_row);
    lv_label_set_text(lbl_wp_value, "100%");
    lv_obj_set_style_text_font(lbl_wp_value, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(lbl_wp_value, UI_COLOR_ON_SURFACE, 0);
    lv_obj_add_flag(lbl_wp_value, LV_OBJ_FLAG_EVENT_BUBBLE);
    
    // "WATER PUMP" label below the oval
    lbl_wp_label = lv_label_create(screen_main);
    lv_label_set_text(lbl_wp_label, "WATER PUMP");
    lv_obj_set_style_text_font(lbl_wp_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_wp_label, UI_COLOR_ON_SURFACE_VAR, 0);
    lv_obj_align_to(lbl_wp_label, cont_water_pump, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
    
    // "NO SIGNAL" label (shown when disconnected, hides both widgets)
    lbl_no_signal = lv_label_create(screen_main);
    lv_label_set_text(lbl_no_signal, "NO SIGNAL");
    lv_obj_set_style_text_font(lbl_no_signal, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(lbl_no_signal, UI_COLOR_ERROR, 0);
    lv_obj_align(lbl_no_signal, LV_ALIGN_TOP_MID, 0, PS_WIDGET_Y + 10);
    
    // =========================================================================
    // Android-style bottom navigation bar (black) - only settings button now
    // =========================================================================
    menu_bar = ui_create_menu_bar(screen_main, UI_MENU_BAR_HEIGHT);
    
    // Settings/Gear button (centered or right side)
    btn_gear = lv_button_create(menu_bar);
    lv_obj_set_size(btn_gear, NAV_BTN_SIZE, NAV_BTN_SIZE);
    lv_obj_add_style(btn_gear, &style_btn_nav, 0);
    lv_obj_add_style(btn_gear, &style_btn_nav_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_gear, gear_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* lbl_gear = lv_label_create(btn_gear);
    lv_label_set_text(lbl_gear, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(lbl_gear, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_gear, lv_color_white(), 0);
    lv_obj_center(lbl_gear);
    
    // Start with menu bar hidden - user touches screen to reveal
    lv_obj_add_flag(menu_bar, LV_OBJ_FLAG_HIDDEN);
    menu_bar_visible = false;
    
    // Add touch event to screen background to show menu bar
    lv_obj_add_event_cb(screen_main, screen_touch_event_handler, LV_EVENT_PRESSED, nullptr);
    
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
        // Show power steering widget
        lv_obj_clear_flag(cont_power_steering, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_ps_label, LV_OBJ_FLAG_HIDDEN);
        // Show water pump widget
        lv_obj_clear_flag(cont_water_pump, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_wp_label, LV_OBJ_FLAG_HIDDEN);
        // Hide no signal
        lv_obj_add_flag(lbl_no_signal, LV_OBJ_FLAG_HIDDEN);
    } else {
        // Hide power steering widget
        lv_obj_add_flag(cont_power_steering, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_ps_label, LV_OBJ_FLAG_HIDDEN);
        // Hide water pump widget
        lv_obj_add_flag(cont_water_pump, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lbl_wp_label, LV_OBJ_FLAG_HIDDEN);
        // Show no signal
        lv_obj_clear_flag(lbl_no_signal, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_screen_main_set_status(bool connected, bool synced) {
    is_connected = connected;
    
    if (connected) {
        if (synced) {
            lv_led_set_color(led_status, UI_COLOR_SUCCESS);
        } else {
            lv_led_set_color(led_status, UI_COLOR_WARNING);
        }
        lv_led_on(led_status);
    } else {
        lv_led_set_color(led_status, UI_COLOR_ERROR);
    }
}

void ui_screen_main_set_mode(uint8_t mode) {
    current_mode = mode;
    
    if (mode == MODE_AUTO) {
        lv_label_set_text(lbl_mode, "AUTO");
        lv_obj_set_style_text_color(lbl_mode, UI_COLOR_SUCCESS, 0);
    } else {
        lv_label_set_text(lbl_mode, "MANUAL");
        lv_obj_set_style_text_color(lbl_mode, UI_COLOR_PRIMARY, 0);  // Theme blue
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

void ui_screen_main_update_menu_bar() {
    // Auto-hide menu bar after timeout
    if (menu_bar_visible) {
        uint32_t elapsed = lv_tick_get() - menu_bar_show_time;
        if (elapsed >= MENU_BAR_TIMEOUT_MS) {
            hide_menu_bar();
        }
    }
}

void ui_screen_main_set_wifi_status(bool connected) {
    if (lbl_wifi_icon == nullptr) return;
    
    if (connected) {
        lv_obj_clear_flag(lbl_wifi_icon, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(lbl_wifi_icon, LV_OBJ_FLAG_HIDDEN);
    }
}
