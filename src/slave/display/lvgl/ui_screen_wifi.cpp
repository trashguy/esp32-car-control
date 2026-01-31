#include "ui_screen_wifi.h"
#include "ui_theme.h"
#include "ui_keyboard.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <string.h>

// =============================================================================
// Layout Constants
// =============================================================================

#define CONTENT_Y           50
#define BTN_SIZE            36
#define BTN_MARGIN          8
#define INPUT_HEIGHT        36
#define INPUT_WIDTH         200
#define LIST_ITEM_HEIGHT    32

// =============================================================================
// Static State
// =============================================================================

static lv_obj_t* screen_wifi = nullptr;

// Title
static lv_obj_t* lbl_title = nullptr;

// Mode toggle
static lv_obj_t* btn_mode = nullptr;
static lv_obj_t* lbl_mode = nullptr;

// SSID input
static lv_obj_t* cont_ssid = nullptr;
static lv_obj_t* lbl_ssid_label = nullptr;
static lv_obj_t* ta_ssid = nullptr;

// Password input
static lv_obj_t* cont_pass = nullptr;
static lv_obj_t* lbl_pass_label = nullptr;
static lv_obj_t* ta_pass = nullptr;

// Scan button and network list
static lv_obj_t* btn_scan = nullptr;
static lv_obj_t* list_networks = nullptr;

// Keyboard (shared for both inputs)
static lv_obj_t* keyboard = nullptr;

// Back button
static lv_obj_t* btn_back = nullptr;

// Callbacks
static void (*cb_back)(void) = nullptr;
static void (*cb_save)(void) = nullptr;

// WiFi state
static int wifiMode = 0;  // 0 = disabled, 1 = client
static char ssidBuffer[UI_MAX_SSID_LEN + 1] = "";
static char passBuffer[UI_MAX_PASS_LEN + 1] = "";

// Scan results
struct WifiNetwork {
    char ssid[UI_MAX_SSID_LEN + 1];
    int32_t rssi;
};
static WifiNetwork scannedNetworks[UI_MAX_WIFI_NETWORKS];
static int networkCount = 0;
static bool scanInProgress = false;

// NVS
static Preferences prefs;

// =============================================================================
// Forward Declarations
// =============================================================================

static void updateModeButton();
static void updateInputsVisibility();
static void saveSettings();
static void connectWifi();
static void populateNetworkList();

// =============================================================================
// Event Handlers
// =============================================================================

// Wrapper for swipe-back that also hides keyboard
static void swipe_back_handler() {
    // Hide keyboard if visible before navigating back
    ui_keyboard_hide(keyboard);
    if (cb_back) {
        cb_back();
    }
}

static void back_btn_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Hide keyboard if visible
        ui_keyboard_hide(keyboard);
        if (cb_back) {
            cb_back();
        }
    }
}

static void mode_btn_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        // Toggle mode
        wifiMode = (wifiMode == 0) ? 1 : 0;
        updateModeButton();
        updateInputsVisibility();
        saveSettings();
        
        if (wifiMode == 0) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            networkCount = 0;
            populateNetworkList();
        } else {
            WiFi.mode(WIFI_STA);
            if (strlen(ssidBuffer) > 0) {
                connectWifi();
            }
        }
        
        Serial.printf("WiFi mode changed to: %s\n", wifiMode == 0 ? "Disabled" : "Client");
    }
}

static void scan_btn_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        if (scanInProgress || wifiMode == 0) return;
        
        scanInProgress = true;
        lv_label_set_text(lv_obj_get_child(btn_scan, 0), "...");
        
        // Clear and show scanning message
        lv_obj_clean(list_networks);
        lv_obj_t* lbl = lv_list_add_text(list_networks, "Scanning...");
        lv_obj_set_style_text_color(lbl, UI_COLOR_SECONDARY, 0);
        
        // Force UI refresh
        lv_refr_now(NULL);
        
        // Perform scan
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
        
        int n = WiFi.scanNetworks();
        networkCount = 0;
        
        // Sort by signal strength and take top networks
        for (int i = 0; i < n && networkCount < UI_MAX_WIFI_NETWORKS; i++) {
            int bestIdx = -1;
            int32_t bestRssi = -999;
            for (int j = 0; j < n; j++) {
                bool alreadyAdded = false;
                for (int k = 0; k < networkCount; k++) {
                    if (strcmp(scannedNetworks[k].ssid, WiFi.SSID(j).c_str()) == 0) {
                        alreadyAdded = true;
                        break;
                    }
                }
                if (!alreadyAdded && WiFi.RSSI(j) > bestRssi) {
                    bestRssi = WiFi.RSSI(j);
                    bestIdx = j;
                }
            }
            if (bestIdx >= 0) {
                strncpy(scannedNetworks[networkCount].ssid, WiFi.SSID(bestIdx).c_str(), UI_MAX_SSID_LEN);
                scannedNetworks[networkCount].ssid[UI_MAX_SSID_LEN] = '\0';
                scannedNetworks[networkCount].rssi = bestRssi;
                networkCount++;
            }
        }
        
        WiFi.scanDelete();
        scanInProgress = false;
        
        lv_label_set_text(lv_obj_get_child(btn_scan, 0), "SCAN");
        populateNetworkList();
        
        Serial.printf("WiFi scan complete: %d networks found\n", networkCount);
    }
}

static void network_item_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        int idx = (int)(intptr_t)lv_event_get_user_data(e);
        
        if (idx >= 0 && idx < networkCount) {
            // Copy SSID to buffer and text area
            strncpy(ssidBuffer, scannedNetworks[idx].ssid, UI_MAX_SSID_LEN);
            ssidBuffer[UI_MAX_SSID_LEN] = '\0';
            lv_textarea_set_text(ta_ssid, ssidBuffer);
            
            // Focus password input and show keyboard
            lv_obj_clear_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
            lv_keyboard_set_textarea(keyboard, ta_pass);
            lv_obj_scroll_to_view(ta_pass, LV_ANIM_ON);
            
            Serial.printf("Selected network: %s\n", ssidBuffer);
        }
    }
}

static void ta_event_handler(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t* ta = (lv_obj_t*)lv_event_get_target(e);
    
    if (code == LV_EVENT_FOCUSED) {
        // Show keyboard when text area is focused
        if (keyboard) {
            ui_keyboard_show(keyboard, ta, nullptr, nullptr);
        }
    } else if (code == LV_EVENT_DEFOCUSED) {
        // Update buffer when defocused
        if (ta == ta_ssid) {
            strncpy(ssidBuffer, lv_textarea_get_text(ta_ssid), UI_MAX_SSID_LEN);
            ssidBuffer[UI_MAX_SSID_LEN] = '\0';
        } else if (ta == ta_pass) {
            strncpy(passBuffer, lv_textarea_get_text(ta_pass), UI_MAX_PASS_LEN);
            passBuffer[UI_MAX_PASS_LEN] = '\0';
        }
    }
}

// Keyboard ready callback - called when Enter/OK is pressed
static void on_keyboard_ready(const char* text) {
    // Save both text areas
    strncpy(ssidBuffer, lv_textarea_get_text(ta_ssid), UI_MAX_SSID_LEN);
    ssidBuffer[UI_MAX_SSID_LEN] = '\0';
    strncpy(passBuffer, lv_textarea_get_text(ta_pass), UI_MAX_PASS_LEN);
    passBuffer[UI_MAX_PASS_LEN] = '\0';
    
    saveSettings();
    connectWifi();
    
    if (cb_save) {
        cb_save();
    }
    
    Serial.println("WiFi credentials saved");
}

// Keyboard cancel callback - called when Cancel/Back is pressed  
static void on_keyboard_cancel() {
    // Just hide, nothing special to do
}

// =============================================================================
// Helper Functions
// =============================================================================

static void updateModeButton() {
    if (!btn_mode || !lbl_mode) return;
    
    if (wifiMode == 0) {
        lv_label_set_text(lbl_mode, "Mode: Disabled");
        lv_obj_set_style_bg_color(btn_mode, UI_COLOR_SURFACE, 0);
    } else {
        lv_label_set_text(lbl_mode, "Mode: Client");
        lv_obj_set_style_bg_color(btn_mode, UI_COLOR_SUCCESS, 0);
    }
}

static void updateInputsVisibility() {
    bool visible = (wifiMode == 1);
    
    if (cont_ssid) {
        if (visible) lv_obj_clear_flag(cont_ssid, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(cont_ssid, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (cont_pass) {
        if (visible) lv_obj_clear_flag(cont_pass, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(cont_pass, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (btn_scan) {
        if (visible) lv_obj_clear_flag(btn_scan, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(btn_scan, LV_OBJ_FLAG_HIDDEN);
    }
    
    if (list_networks) {
        if (visible) lv_obj_clear_flag(list_networks, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(list_networks, LV_OBJ_FLAG_HIDDEN);
    }
}

static void saveSettings() {
    prefs.begin("wifi", false);
    prefs.putInt("mode", wifiMode);
    prefs.putString("ssid", ssidBuffer);
    prefs.putString("pass", passBuffer);
    prefs.end();
}

static void connectWifi() {
    if (wifiMode == 1 && strlen(ssidBuffer) > 0) {
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssidBuffer, passBuffer);
        Serial.printf("Connecting to WiFi: %s\n", ssidBuffer);
    }
}

static const char* getSignalIcon(int32_t rssi) {
    // Return a text representation of signal strength
    if (rssi > -50) return "[||||]";
    else if (rssi > -60) return "[||| ]";
    else if (rssi > -70) return "[||  ]";
    else return "[|   ]";
}

static void populateNetworkList() {
    if (!list_networks) return;
    
    lv_obj_clean(list_networks);
    
    if (wifiMode == 0) {
        lv_obj_t* lbl = lv_list_add_text(list_networks, "WiFi Disabled");
        lv_obj_set_style_text_color(lbl, UI_COLOR_SECONDARY, 0);
        return;
    }
    
    if (networkCount == 0) {
        lv_obj_t* lbl = lv_list_add_text(list_networks, "Press SCAN to search");
        lv_obj_set_style_text_color(lbl, UI_COLOR_SECONDARY, 0);
        return;
    }
    
    for (int i = 0; i < networkCount; i++) {
        char itemText[UI_MAX_SSID_LEN + 16];
        snprintf(itemText, sizeof(itemText), "%s %s", 
                 getSignalIcon(scannedNetworks[i].rssi),
                 scannedNetworks[i].ssid);
        
        lv_obj_t* btn = lv_list_add_button(list_networks, NULL, itemText);
        lv_obj_add_event_cb(btn, network_item_event_handler, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_set_style_bg_color(btn, UI_COLOR_SURFACE, 0);
        lv_obj_set_style_bg_color(btn, UI_COLOR_PRIMARY, LV_STATE_PRESSED);
        
        // Style the label inside the button
        lv_obj_t* lbl = lv_obj_get_child(btn, 0);
        if (lbl) {
            lv_obj_set_style_text_font(lbl, UI_FONT_SMALL, 0);
        }
    }
}

// =============================================================================
// Public API
// =============================================================================

void ui_screen_wifi_create() {
    // Create the screen
    screen_wifi = ui_create_screen();
    
    // Back button (arrow only, in header) - larger touch target
    btn_back = lv_button_create(screen_wifi);
    lv_obj_set_size(btn_back, 40, 32);
    lv_obj_set_pos(btn_back, 5, 4);
    lv_obj_add_style(btn_back, &style_btn, 0);
    lv_obj_add_style(btn_back, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_back, back_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(lbl_back, UI_FONT_NORMAL, 0);
    lv_obj_center(lbl_back);
    
    // Title (centered, accounting for back button)
    lbl_title = lv_label_create(screen_wifi);
    lv_label_set_text(lbl_title, "WIFI SETTINGS");
    lv_obj_set_style_text_font(lbl_title, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Horizontal line under title (more space above)
    lv_obj_t* line_title = lv_obj_create(screen_wifi);
    lv_obj_set_size(line_title, 280, 2);
    lv_obj_set_pos(line_title, 20, 40);
    lv_obj_set_style_bg_color(line_title, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_border_width(line_title, 0, 0);
    lv_obj_set_style_radius(line_title, 0, 0);
    
    // Mode toggle button
    btn_mode = lv_button_create(screen_wifi);
    lv_obj_set_size(btn_mode, 200, 36);
    lv_obj_set_pos(btn_mode, 60, CONTENT_Y);
    lv_obj_add_style(btn_mode, &style_btn, 0);
    lv_obj_add_style(btn_mode, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_mode, mode_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    lbl_mode = lv_label_create(btn_mode);
    lv_label_set_text(lbl_mode, "Mode: Disabled");
    lv_obj_set_style_text_font(lbl_mode, UI_FONT_NORMAL, 0);
    lv_obj_center(lbl_mode);
    
    // SSID row: label + input + scan button
    cont_ssid = lv_obj_create(screen_wifi);
    lv_obj_set_size(cont_ssid, 310, 40);
    lv_obj_set_pos(cont_ssid, 5, CONTENT_Y + 42);
    lv_obj_set_style_bg_opa(cont_ssid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_ssid, 0, 0);
    lv_obj_set_style_pad_all(cont_ssid, 0, 0);
    lv_obj_set_flex_flow(cont_ssid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_ssid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont_ssid, 5, 0);
    
    lbl_ssid_label = lv_label_create(cont_ssid);
    lv_label_set_text(lbl_ssid_label, "SSID:");
    lv_obj_set_style_text_font(lbl_ssid_label, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_ssid_label, UI_COLOR_SECONDARY, 0);
    lv_obj_set_width(lbl_ssid_label, 45);
    
    ta_ssid = lv_textarea_create(cont_ssid);
    lv_textarea_set_one_line(ta_ssid, true);
    lv_textarea_set_max_length(ta_ssid, UI_MAX_SSID_LEN);
    lv_textarea_set_placeholder_text(ta_ssid, "Enter SSID");
    lv_obj_set_width(ta_ssid, 190);
    lv_obj_set_style_text_font(ta_ssid, UI_FONT_SMALL, 0);
    lv_obj_set_style_bg_color(ta_ssid, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_color(ta_ssid, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_border_color(ta_ssid, UI_COLOR_SUCCESS, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(ta_ssid, ta_event_handler, LV_EVENT_ALL, nullptr);
    
    // Scan button (in SSID row)
    btn_scan = lv_button_create(cont_ssid);
    lv_obj_set_size(btn_scan, 55, 32);
    lv_obj_add_style(btn_scan, &style_btn, 0);
    lv_obj_add_style(btn_scan, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_scan, scan_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* lbl_scan = lv_label_create(btn_scan);
    lv_label_set_text(lbl_scan, "SCAN");
    lv_obj_set_style_text_font(lbl_scan, UI_FONT_SMALL, 0);
    lv_obj_center(lbl_scan);
    
    // Password input container
    cont_pass = lv_obj_create(screen_wifi);
    lv_obj_set_size(cont_pass, 310, 40);
    lv_obj_set_pos(cont_pass, 5, CONTENT_Y + 82);
    lv_obj_set_style_bg_opa(cont_pass, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_pass, 0, 0);
    lv_obj_set_style_pad_all(cont_pass, 0, 0);
    lv_obj_set_flex_flow(cont_pass, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_pass, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cont_pass, 5, 0);
    
    lbl_pass_label = lv_label_create(cont_pass);
    lv_label_set_text(lbl_pass_label, "Pass:");
    lv_obj_set_style_text_font(lbl_pass_label, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_pass_label, UI_COLOR_SECONDARY, 0);
    lv_obj_set_width(lbl_pass_label, 45);
    
    ta_pass = lv_textarea_create(cont_pass);
    lv_textarea_set_one_line(ta_pass, true);
    lv_textarea_set_max_length(ta_pass, UI_MAX_PASS_LEN);
    lv_textarea_set_placeholder_text(ta_pass, "Enter password");
    lv_textarea_set_password_mode(ta_pass, true);
    lv_obj_set_width(ta_pass, 250);
    lv_obj_set_style_text_font(ta_pass, UI_FONT_SMALL, 0);
    lv_obj_set_style_bg_color(ta_pass, UI_COLOR_SURFACE, 0);
    lv_obj_set_style_border_color(ta_pass, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_border_color(ta_pass, UI_COLOR_SUCCESS, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(ta_pass, ta_event_handler, LV_EVENT_ALL, nullptr);
    
    // Network list
    list_networks = lv_list_create(screen_wifi);
    lv_obj_set_size(list_networks, 310, 90);
    lv_obj_set_pos(list_networks, 5, CONTENT_Y + 125);
    lv_obj_set_style_bg_color(list_networks, lv_color_black(), 0);
    lv_obj_set_style_border_color(list_networks, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_border_width(list_networks, 1, 0);
    lv_obj_set_style_pad_all(list_networks, 2, 0);
    
    // Keyboard (hidden by default) - using reusable keyboard module
    keyboard = ui_keyboard_create(screen_wifi);
    ui_keyboard_set_callbacks(keyboard, on_keyboard_ready, on_keyboard_cancel);
    
    // Add swipe gesture to screen (swipe right to go back)
    ui_add_swipe_back_gesture(screen_wifi, swipe_back_handler);
    
    // Initialize visibility based on mode
    updateModeButton();
    updateInputsVisibility();
    populateNetworkList();
    
    Serial.println("UI WiFi screen created");
}

lv_obj_t* ui_screen_wifi_get() {
    return screen_wifi;
}

void ui_screen_wifi_init() {
    // Load settings from NVS
    prefs.begin("wifi", true);
    wifiMode = prefs.getInt("mode", 0);
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    strncpy(ssidBuffer, ssid.c_str(), UI_MAX_SSID_LEN);
    ssidBuffer[UI_MAX_SSID_LEN] = '\0';
    strncpy(passBuffer, pass.c_str(), UI_MAX_PASS_LEN);
    passBuffer[UI_MAX_PASS_LEN] = '\0';
    prefs.end();
    
    // Apply WiFi mode
    if (wifiMode == 0) {
        WiFi.mode(WIFI_OFF);
    } else if (wifiMode == 1) {
        WiFi.mode(WIFI_STA);
        if (strlen(ssidBuffer) > 0) {
            WiFi.begin(ssidBuffer, passBuffer);
        }
    }
    
    Serial.printf("WiFi init: mode=%d, ssid=%s\n", wifiMode, ssidBuffer);
}

void ui_screen_wifi_update() {
    if (!screen_wifi) return;
    
    // Update text areas with current values
    if (ta_ssid) {
        lv_textarea_set_text(ta_ssid, ssidBuffer);
    }
    if (ta_pass) {
        lv_textarea_set_text(ta_pass, passBuffer);
    }
    
    updateModeButton();
    updateInputsVisibility();
}

void ui_screen_wifi_refresh_networks() {
    populateNetworkList();
}

void ui_screen_wifi_set_back_callback(void (*callback)(void)) {
    cb_back = callback;
}

void ui_screen_wifi_set_save_callback(void (*callback)(void)) {
    cb_save = callback;
}

bool ui_screen_wifi_keyboard_visible() {
    return ui_keyboard_is_visible(keyboard);
}

void ui_screen_wifi_reset() {
    ui_keyboard_hide(keyboard);
    networkCount = 0;
    scanInProgress = false;
}

int ui_screen_wifi_get_mode() {
    return wifiMode;
}

const char* ui_screen_wifi_get_ssid() {
    return ssidBuffer;
}

const char* ui_screen_wifi_get_password() {
    return passBuffer;
}
