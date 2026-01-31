#ifndef UI_OTA_POPUP_H
#define UI_OTA_POPUP_H

#include <lvgl.h>
#include <stdint.h>

// =============================================================================
// OTA Popup States (matches legacy ota_popup.h)
// =============================================================================

enum UiOtaPopupState {
    UI_OTA_POPUP_HIDDEN,           // Popup not visible
    UI_OTA_POPUP_CONFIRM,          // Showing VERIFY/ABORT buttons (initial state)
    UI_OTA_POPUP_VERIFYING,        // Running SPI verification test
    UI_OTA_POPUP_VERIFIED,         // Test passed, showing INSTALL/ABORT buttons
    UI_OTA_POPUP_INSTALLING,       // Showing progress bar (display firmware)
    UI_OTA_POPUP_CONTROLLER,       // Installing controller firmware
    UI_OTA_POPUP_COMPLETE,         // Update complete, show OK
    UI_OTA_POPUP_ERROR             // Error occurred, show DISMISS
};

// =============================================================================
// OTA Popup Public Interface
// =============================================================================

/**
 * Create the OTA popup objects. Call once after LVGL init.
 */
void ui_ota_popup_create();

/**
 * Show the OTA popup with update confirmation.
 */
void ui_ota_popup_show();

/**
 * Hide the OTA popup.
 */
void ui_ota_popup_hide();

/**
 * Check if popup is currently visible.
 */
bool ui_ota_popup_is_visible();

/**
 * Get current popup state.
 */
UiOtaPopupState ui_ota_popup_get_state();

/**
 * Update popup state (call periodically from display loop).
 * Checks OTA handler state and updates UI accordingly.
 */
void ui_ota_popup_update();

/**
 * Set progress for installing state (0-100).
 */
void ui_ota_popup_set_progress(uint8_t progress);

/**
 * Set error message and switch to error state.
 */
void ui_ota_popup_set_error(const char* message);

/**
 * Set complete state.
 */
void ui_ota_popup_set_complete();

/**
 * Set callbacks for popup buttons.
 * @param on_install Called when install is confirmed
 * @param on_abort Called when update is aborted
 * @param on_dismiss Called when popup is dismissed (OK on complete/error)
 */
void ui_ota_popup_set_callbacks(
    void (*on_install)(void),
    void (*on_abort)(void),
    void (*on_dismiss)(void)
);

#endif // UI_OTA_POPUP_H
