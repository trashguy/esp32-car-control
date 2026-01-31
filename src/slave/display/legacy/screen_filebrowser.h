#ifndef SCREEN_FILEBROWSER_H
#define SCREEN_FILEBROWSER_H

#include "../display_common.h"

// =============================================================================
// File Browser Layout Constants
// =============================================================================

#define FILE_LIST_Y_START   45
#define FILE_LIST_Y_END     (SCREEN_HEIGHT - 50)
#define FILE_LINE_HEIGHT    20
#define MAX_VISIBLE_FILES   ((FILE_LIST_Y_END - FILE_LIST_Y_START) / FILE_LINE_HEIGHT)
#define MAX_FILES           64

// Back arrow button (bottom left)
#define ARROW_BTN_SIZE  36
#define ARROW_BTN_X     8
#define ARROW_BTN_Y     (SCREEN_HEIGHT - ARROW_BTN_SIZE - 8)

// =============================================================================
// File Browser Functions
// =============================================================================

// Draw the complete file browser screen
void screenFileBrowserDraw();

// Handle touch events
void screenFileBrowserHandleTouch(int16_t x, int16_t y, bool pressed);

// Update (for scroll animations, etc)
void screenFileBrowserUpdate();

// Reset state (called when entering screen)
void screenFileBrowserReset();

#endif // SCREEN_FILEBROWSER_H
