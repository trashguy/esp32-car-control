#ifndef UI_SCREEN_FILEBROWSER_H
#define UI_SCREEN_FILEBROWSER_H

#include "lvgl.h"

/**
 * @brief Create the file browser screen with LVGL
 */
void ui_screen_filebrowser_create();

/**
 * @brief Get the file browser screen object
 */
lv_obj_t* ui_screen_filebrowser_get();

/**
 * @brief Refresh the file list from SD card
 */
void ui_screen_filebrowser_refresh();

/**
 * @brief Update screen state (check USB mount status)
 */
void ui_screen_filebrowser_update();

/**
 * @brief Set callback for back button
 */
void ui_screen_filebrowser_set_back_callback(void (*callback)(void));

/**
 * @brief Set callback for file selection
 * @param callback Function called with filename when a file is tapped
 */
void ui_screen_filebrowser_set_file_callback(void (*callback)(const char* filename));

/**
 * @brief Show/hide USB locked overlay
 */
void ui_screen_filebrowser_set_usb_locked(bool locked);

/**
 * @brief Reset screen state
 */
void ui_screen_filebrowser_reset();

#endif // UI_SCREEN_FILEBROWSER_H
