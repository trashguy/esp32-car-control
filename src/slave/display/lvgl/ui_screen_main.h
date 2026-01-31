#ifndef UI_SCREEN_MAIN_H
#define UI_SCREEN_MAIN_H

#include <lvgl.h>

// =============================================================================
// Main Screen LVGL Interface
// =============================================================================

/**
 * Create and initialize the main screen.
 * Call this once after LVGL and theme are initialized.
 */
void ui_screen_main_create();

/**
 * Get the main screen object.
 */
lv_obj_t* ui_screen_main_get();

/**
 * Update the RPM display value.
 * @param rpm The RPM value to display
 * @param connected Whether the device is connected
 */
void ui_screen_main_set_rpm(uint16_t rpm, bool connected);

/**
 * Update the connection status indicator.
 * @param connected Whether connected
 * @param synced Whether synced (only used when connected)
 */
void ui_screen_main_set_status(bool connected, bool synced);

/**
 * Update the mode indicator (Auto/Manual).
 * @param mode MODE_AUTO or MODE_MANUAL
 */
void ui_screen_main_set_mode(uint8_t mode);

/**
 * Show/hide the RPM adjustment buttons.
 * @param show True to show buttons (manual mode)
 */
void ui_screen_main_show_rpm_buttons(bool show);

/**
 * Set callback for gear button press.
 */
void ui_screen_main_set_gear_callback(void (*callback)(void));

/**
 * Set callback for mode button press.
 */
void ui_screen_main_set_mode_callback(void (*callback)(void));

/**
 * Set callback for RPM up button press.
 */
void ui_screen_main_set_rpm_up_callback(void (*callback)(void));

/**
 * Set callback for RPM down button press.
 */
void ui_screen_main_set_rpm_down_callback(void (*callback)(void));

/**
 * Trigger the disconnected blink animation.
 * Call this periodically when disconnected.
 */
void ui_screen_main_update_blink();

#endif // UI_SCREEN_MAIN_H
