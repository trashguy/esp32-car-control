#ifndef LEGACY_KEYBOARD_H
#define LEGACY_KEYBOARD_H

#include "../display_common.h"

// =============================================================================
// Keyboard Layout Constants
// =============================================================================

#define KB_HEADER_H     45
#define KB_KEY_H        36
#define KB_KEY_W        30
#define KB_WIDE_KEY_W   45
#define KB_SPACE_W      120
#define KB_SPACING      2
#define KB_START_Y      (KB_HEADER_H + 5)

// =============================================================================
// Keyboard Callback Types
// =============================================================================

// Called when OK is pressed (save = true) or BACK is pressed (save = false)
typedef void (*KeyboardCallback)(bool save);

// =============================================================================
// Keyboard Public Interface
// =============================================================================

// Show the keyboard for text input
// label: Header label (e.g., "SSID:", "Password:")
// target: Buffer to edit
// maxLen: Maximum length of input
// isPassword: If true, shows asterisks instead of actual characters
// callback: Function to call when keyboard is closed (can be nullptr)
void keyboardShow(const char* label, char* target, int maxLen, bool isPassword, KeyboardCallback callback = nullptr);

// Hide the keyboard
// save: If true, the callback will be called with save=true
void keyboardHide(bool save);

// Draw the keyboard (call when keyboard is visible)
void keyboardDraw();

// Handle touch input for the keyboard
// Returns true if the keyboard handled the touch (and caller should not process further)
bool keyboardHandleTouch(int16_t x, int16_t y);

// Check if the keyboard is currently visible
bool keyboardIsVisible();

// Get the current target buffer (for external access if needed)
char* keyboardGetTarget();

#endif // LEGACY_KEYBOARD_H
