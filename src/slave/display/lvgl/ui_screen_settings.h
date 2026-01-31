#ifndef UI_SCREEN_SETTINGS_H
#define UI_SCREEN_SETTINGS_H

#include "lvgl.h"

/**
 * @brief Create the settings screen with LVGL
 */
void ui_screen_settings_create();

/**
 * @brief Get the settings screen object
 */
lv_obj_t* ui_screen_settings_get();

/**
 * @brief Update the diagnostics content (call periodically)
 */
void ui_screen_settings_update();

/**
 * @brief Set callback for back button
 */
void ui_screen_settings_set_back_callback(void (*callback)(void));

/**
 * @brief Set callback for SD card button
 */
void ui_screen_settings_set_sd_callback(void (*callback)(void));

/**
 * @brief Set callback for WiFi button
 */
void ui_screen_settings_set_wifi_callback(void (*callback)(void));

/**
 * @brief Set callback for USB button
 */
void ui_screen_settings_set_usb_callback(void (*callback)(void));

/**
 * @brief Update USB button state (enabled/disabled visual)
 */
void ui_screen_settings_set_usb_enabled(bool enabled);

/**
 * @brief Show/hide SD and USB buttons based on SD card presence
 */
void ui_screen_settings_set_sd_present(bool present);

#endif // UI_SCREEN_SETTINGS_H
