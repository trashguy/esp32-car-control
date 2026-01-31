#include "ui_screen_settings.h"
#include "ui_theme.h"
#include "ui_screen_wifi.h"
#include "shared/version.h"
#include "../../sd_card.h"
#include "../../usb_msc.h"
#include <Arduino.h>
#include <WiFi.h>

// =============================================================================
// Layout Constants
// =============================================================================

#define CONTENT_Y           45
#define CONTENT_H           (UI_CONTENT_HEIGHT - CONTENT_Y - 5)  // Adjust for menu bar
#define NAV_BTN_SIZE        40

// =============================================================================
// Static UI Objects
// =============================================================================

static lv_obj_t* screen_settings = nullptr;

// Labels
static lv_obj_t* lbl_title = nullptr;

// Scrollable content container
static lv_obj_t* cont_diag = nullptr;

// Diagnostic labels (updated periodically)
static lv_obj_t* lbl_fw_version = nullptr;
static lv_obj_t* lbl_fw_built = nullptr;
static lv_obj_t* lbl_sd_status = nullptr;
static lv_obj_t* lbl_sd_total = nullptr;
static lv_obj_t* lbl_sd_used = nullptr;
static lv_obj_t* lbl_wifi_mode = nullptr;
static lv_obj_t* lbl_wifi_status = nullptr;
static lv_obj_t* lbl_wifi_ssid = nullptr;
static lv_obj_t* lbl_wifi_ip = nullptr;
static lv_obj_t* lbl_wifi_rssi = nullptr;

// Buttons
static lv_obj_t* menu_bar = nullptr;
static lv_obj_t* btn_back = nullptr;
static lv_obj_t* btn_sd = nullptr;
static lv_obj_t* btn_wifi = nullptr;
static lv_obj_t* btn_usb = nullptr;

// Callbacks
static void (*cb_back)(void) = nullptr;
static void (*cb_sd)(void) = nullptr;
static void (*cb_wifi)(void) = nullptr;
static void (*cb_usb)(void) = nullptr;

// =============================================================================
// Event Handlers
// =============================================================================

static void back_btn_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && cb_back) {
        cb_back();
    }
}

static void sd_btn_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && cb_sd) {
        cb_sd();
    }
}

static void wifi_btn_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && cb_wifi) {
        cb_wifi();
    }
}

static void usb_btn_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && cb_usb) {
        cb_usb();
    }
}

// Wrapper for swipe-back gesture
static void swipe_back_handler() {
    if (cb_back) {
        cb_back();
    }
}

// =============================================================================
// Helper: Create info row (label: value)
// =============================================================================

static lv_obj_t* create_info_row(lv_obj_t* parent, const char* label) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // Label part
    lv_obj_t* lbl_key = lv_label_create(cont);
    lv_label_set_text(lbl_key, label);
    lv_obj_set_style_text_font(lbl_key, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_key, UI_COLOR_ON_SURFACE_VAR, 0);  // MD3 secondary text
    lv_obj_set_width(lbl_key, 100);
    
    // Value part (returned for updating)
    lv_obj_t* lbl_val = lv_label_create(cont);
    lv_label_set_text(lbl_val, "-");
    lv_obj_set_style_text_font(lbl_val, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_val, UI_COLOR_ON_SURFACE, 0);  // MD3 primary text
    lv_obj_set_flex_grow(lbl_val, 1);
    
    return lbl_val;
}

// =============================================================================
// Helper: Create separator line
// =============================================================================

static void create_separator(lv_obj_t* parent) {
    lv_obj_t* line = lv_obj_create(parent);
    lv_obj_set_size(line, lv_pct(90), 1);
    lv_obj_set_style_bg_color(line, UI_COLOR_OUTLINE_VAR, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_50, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
}

// =============================================================================
// Public API
// =============================================================================

void ui_screen_settings_create() {
    // Create the screen
    screen_settings = ui_create_screen();
    
    // Title
    lbl_title = lv_label_create(screen_settings);
    lv_label_set_text(lbl_title, "SETTINGS");
    lv_obj_set_style_text_font(lbl_title, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_ON_SURFACE, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Horizontal line under title
    lv_obj_t* line_title = lv_obj_create(screen_settings);
    lv_obj_set_size(line_title, 280, 2);
    lv_obj_set_pos(line_title, 20, 35);
    lv_obj_set_style_bg_color(line_title, UI_COLOR_OUTLINE_VAR, 0);
    lv_obj_set_style_border_width(line_title, 0, 0);
    lv_obj_set_style_radius(line_title, 0, 0);
    
    // Scrollable diagnostics container
    cont_diag = lv_obj_create(screen_settings);
    lv_obj_set_size(cont_diag, 300, CONTENT_H);
    lv_obj_set_pos(cont_diag, 10, CONTENT_Y);
    lv_obj_set_style_bg_opa(cont_diag, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_diag, 0, 0);
    lv_obj_set_style_pad_all(cont_diag, 5, 0);
    lv_obj_set_flex_flow(cont_diag, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont_diag, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(cont_diag, LV_DIR_VER);
    lv_obj_add_flag(cont_diag, LV_OBJ_FLAG_SCROLLABLE);
    
    // Firmware section
    lbl_fw_version = create_info_row(cont_diag, "Firmware:");
    lv_label_set_text(lbl_fw_version, FIRMWARE_VERSION);
    
    lbl_fw_built = create_info_row(cont_diag, "Built:");
    lv_label_set_text(lbl_fw_built, BUILD_TIMESTAMP);
    
    create_separator(cont_diag);
    
    // SD Card section
    lbl_sd_status = create_info_row(cont_diag, "SD Card:");
    lbl_sd_total = create_info_row(cont_diag, "Total:");
    lbl_sd_used = create_info_row(cont_diag, "Used:");
    
    create_separator(cont_diag);
    
    // WiFi section
    lbl_wifi_mode = create_info_row(cont_diag, "WiFi Mode:");
    lbl_wifi_status = create_info_row(cont_diag, "Status:");
    lbl_wifi_ssid = create_info_row(cont_diag, "SSID:");
    lbl_wifi_ip = create_info_row(cont_diag, "IP:");
    lbl_wifi_rssi = create_info_row(cont_diag, "Signal:");
    
    // =========================================================================
    // Android-style bottom navigation bar (black)
    // =========================================================================
    menu_bar = ui_create_menu_bar(screen_settings, UI_MENU_BAR_HEIGHT);
    
    // Back button (far left)
    btn_back = lv_button_create(menu_bar);
    lv_obj_set_size(btn_back, NAV_BTN_SIZE, NAV_BTN_SIZE);
    lv_obj_add_style(btn_back, &style_btn_nav, 0);
    lv_obj_add_style(btn_back, &style_btn_nav_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_back, back_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(lbl_back, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_back, lv_color_white(), 0);
    lv_obj_center(lbl_back);
    
    // Spacer to push remaining buttons to the right
    lv_obj_t* spacer = lv_obj_create(menu_bar);
    lv_obj_set_size(spacer, 1, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_flex_grow(spacer, 1);
    
    // USB button - production build only (before WiFi)
#if PRODUCTION_BUILD
    btn_usb = lv_button_create(menu_bar);
    lv_obj_set_size(btn_usb, NAV_BTN_SIZE, NAV_BTN_SIZE);
    lv_obj_add_style(btn_usb, &style_btn_nav, 0);
    lv_obj_add_style(btn_usb, &style_btn_nav_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_usb, usb_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* lbl_usb = lv_label_create(btn_usb);
    lv_label_set_text(lbl_usb, LV_SYMBOL_USB);
    lv_obj_set_style_text_font(lbl_usb, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_usb, lv_color_white(), 0);
    lv_obj_center(lbl_usb);
#endif
    
    // WiFi button (right side)
    btn_wifi = lv_button_create(menu_bar);
    lv_obj_set_size(btn_wifi, NAV_BTN_SIZE, NAV_BTN_SIZE);
    lv_obj_add_style(btn_wifi, &style_btn_nav, 0);
    lv_obj_add_style(btn_wifi, &style_btn_nav_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_wifi, wifi_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* lbl_wifi = lv_label_create(btn_wifi);
    lv_label_set_text(lbl_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(lbl_wifi, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_wifi, lv_color_white(), 0);
    lv_obj_center(lbl_wifi);
    
    // SD button (far right)
    btn_sd = lv_button_create(menu_bar);
    lv_obj_set_size(btn_sd, NAV_BTN_SIZE, NAV_BTN_SIZE);
    lv_obj_add_style(btn_sd, &style_btn_nav, 0);
    lv_obj_add_style(btn_sd, &style_btn_nav_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_sd, sd_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* lbl_sd = lv_label_create(btn_sd);
    lv_label_set_text(lbl_sd, LV_SYMBOL_SD_CARD);
    lv_obj_set_style_text_font(lbl_sd, UI_FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(lbl_sd, lv_color_white(), 0);
    lv_obj_center(lbl_sd);

    // Initial update
    ui_screen_settings_update();
    
    // Add swipe gesture (swipe right to go back)
    ui_add_swipe_back_gesture(screen_settings, swipe_back_handler);
    
    Serial.println("UI Settings screen created");
}

lv_obj_t* ui_screen_settings_get() {
    return screen_settings;
}

void ui_screen_settings_update() {
    if (!screen_settings) return;
    
    char buf[64];
    
    // SD Card info
    if (sdCardPresent()) {
        lv_label_set_text(lbl_sd_status, sdCardType());
        lv_obj_set_style_text_color(lbl_sd_status, UI_COLOR_SUCCESS, 0);
        
        snprintf(buf, sizeof(buf), "%llu MB", sdCardTotalBytes() / (1024 * 1024));
        lv_label_set_text(lbl_sd_total, buf);
        
        snprintf(buf, sizeof(buf), "%llu MB", sdCardUsedBytes() / (1024 * 1024));
        lv_label_set_text(lbl_sd_used, buf);
    } else {
        lv_label_set_text(lbl_sd_status, "Not Present");
        lv_obj_set_style_text_color(lbl_sd_status, UI_COLOR_ERROR, 0);
        lv_label_set_text(lbl_sd_total, "-");
        lv_label_set_text(lbl_sd_used, "-");
    }
    
    // WiFi info
    int wifiMode = ui_screen_wifi_get_mode();
    if (wifiMode == 0) {
        lv_label_set_text(lbl_wifi_mode, "Disabled");
        lv_obj_set_style_text_color(lbl_wifi_mode, UI_COLOR_ERROR, 0);
        lv_label_set_text(lbl_wifi_status, "-");
        lv_label_set_text(lbl_wifi_ssid, "-");
        lv_label_set_text(lbl_wifi_ip, "-");
        lv_label_set_text(lbl_wifi_rssi, "-");
    } else {
        lv_label_set_text(lbl_wifi_mode, "Client");
        lv_obj_set_style_text_color(lbl_wifi_mode, UI_COLOR_SUCCESS, 0);
        
        bool connected = (WiFi.status() == WL_CONNECTED);
        const char* ssid = ui_screen_wifi_get_ssid();
        if (connected) {
            lv_label_set_text(lbl_wifi_status, "Connected");
            lv_obj_set_style_text_color(lbl_wifi_status, UI_COLOR_SUCCESS, 0);
            
            lv_label_set_text(lbl_wifi_ssid, ssid);
            
            IPAddress ip = WiFi.localIP();
            snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            lv_label_set_text(lbl_wifi_ip, buf);
            
            snprintf(buf, sizeof(buf), "%d dBm", WiFi.RSSI());
            lv_label_set_text(lbl_wifi_rssi, buf);
        } else {
            lv_label_set_text(lbl_wifi_status, strlen(ssid) > 0 ? "Connecting..." : "Disconnected");
            lv_obj_set_style_text_color(lbl_wifi_status, UI_COLOR_WARNING, 0);
            
            if (strlen(ssid) > 0) {
                lv_label_set_text(lbl_wifi_ssid, ssid);
            } else {
                lv_label_set_text(lbl_wifi_ssid, "-");
            }
            lv_label_set_text(lbl_wifi_ip, "-");
            lv_label_set_text(lbl_wifi_rssi, "-");
        }
    }
}

void ui_screen_settings_set_back_callback(void (*callback)(void)) {
    cb_back = callback;
}

void ui_screen_settings_set_sd_callback(void (*callback)(void)) {
    cb_sd = callback;
}

void ui_screen_settings_set_wifi_callback(void (*callback)(void)) {
    cb_wifi = callback;
}

void ui_screen_settings_set_usb_callback(void (*callback)(void)) {
    cb_usb = callback;
}

void ui_screen_settings_set_usb_enabled(bool enabled) {
#if PRODUCTION_BUILD
    if (!btn_usb) return;
    
    if (enabled) {
        // Green background when enabled
        lv_obj_set_style_bg_color(btn_usb, UI_COLOR_SUCCESS, 0);
    } else {
        // Normal button style
        lv_obj_set_style_bg_color(btn_usb, UI_COLOR_SURFACE, 0);
    }
#else
    (void)enabled;  // Suppress unused warning
#endif
}

void ui_screen_settings_set_sd_present(bool present) {
    if (btn_sd) {
        if (present) {
            lv_obj_clear_flag(btn_sd, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(btn_sd, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
#if PRODUCTION_BUILD
    if (btn_usb) {
        if (present) {
            lv_obj_clear_flag(btn_usb, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(btn_usb, LV_OBJ_FLAG_HIDDEN);
        }
    }
#endif
}
