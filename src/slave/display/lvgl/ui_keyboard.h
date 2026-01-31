#ifndef UI_KEYBOARD_H
#define UI_KEYBOARD_H

#include <lvgl.h>

// =============================================================================
// LVGL Keyboard Wrapper
// =============================================================================
// Provides a reusable keyboard component that can be attached to any screen
// and used with any textarea widget.

// Callback types
typedef void (*UiKeyboardReadyCallback)(const char* text);
typedef void (*UiKeyboardCancelCallback)(void);

// =============================================================================
// Public Interface
// =============================================================================

/**
 * @brief Create a keyboard attached to a screen
 * @param parent The screen to attach the keyboard to
 * @return The created keyboard object
 */
lv_obj_t* ui_keyboard_create(lv_obj_t* parent);

/**
 * @brief Show the keyboard for a textarea
 * @param keyboard The keyboard object
 * @param textarea The textarea to edit
 * @param on_ready Callback when Enter/OK is pressed (can be nullptr)
 * @param on_cancel Callback when Cancel/Back is pressed (can be nullptr)
 */
void ui_keyboard_show(lv_obj_t* keyboard, lv_obj_t* textarea,
                      UiKeyboardReadyCallback on_ready,
                      UiKeyboardCancelCallback on_cancel);

/**
 * @brief Hide the keyboard
 * @param keyboard The keyboard object
 */
void ui_keyboard_hide(lv_obj_t* keyboard);

/**
 * @brief Check if keyboard is visible
 * @param keyboard The keyboard object
 * @return true if visible, false if hidden
 */
bool ui_keyboard_is_visible(lv_obj_t* keyboard);

/**
 * @brief Set keyboard callbacks
 * @param keyboard The keyboard object  
 * @param on_ready Callback when Enter/OK is pressed
 * @param on_cancel Callback when Cancel/Back is pressed
 */
void ui_keyboard_set_callbacks(lv_obj_t* keyboard,
                               UiKeyboardReadyCallback on_ready,
                               UiKeyboardCancelCallback on_cancel);

#endif // UI_KEYBOARD_H
