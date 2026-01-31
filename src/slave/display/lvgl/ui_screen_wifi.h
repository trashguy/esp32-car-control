#ifndef UI_SCREEN_WIFI_H
#define UI_SCREEN_WIFI_H

#include "lvgl.h"

// Max lengths (matching original)
#define UI_MAX_SSID_LEN     32
#define UI_MAX_PASS_LEN     64
#define UI_MAX_WIFI_NETWORKS 5

/**
 * @brief Create the WiFi screen with LVGL
 */
void ui_screen_wifi_create();

/**
 * @brief Get the WiFi screen object
 */
lv_obj_t* ui_screen_wifi_get();

/**
 * @brief Initialize WiFi settings from NVS (call at startup)
 */
void ui_screen_wifi_init();

/**
 * @brief Update screen state (connection status, etc)
 */
void ui_screen_wifi_update();

/**
 * @brief Refresh network list after scan
 */
void ui_screen_wifi_refresh_networks();

/**
 * @brief Set callback for back button
 */
void ui_screen_wifi_set_back_callback(void (*callback)(void));

/**
 * @brief Set callback for settings changed (mode, SSID, password)
 */
void ui_screen_wifi_set_save_callback(void (*callback)(void));

/**
 * @brief Check if keyboard is currently visible
 */
bool ui_screen_wifi_keyboard_visible();

/**
 * @brief Reset screen state
 */
void ui_screen_wifi_reset();

/**
 * @brief Get current WiFi mode (0=disabled, 1=client)
 */
int ui_screen_wifi_get_mode();

/**
 * @brief Get SSID string
 */
const char* ui_screen_wifi_get_ssid();

/**
 * @brief Get password string
 */
const char* ui_screen_wifi_get_password();

#endif // UI_SCREEN_WIFI_H
