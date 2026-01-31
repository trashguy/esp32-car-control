#ifndef UI_THEME_H
#define UI_THEME_H

#include <lvgl.h>

// =============================================================================
// Color Definitions (matching original display_common.h)
// =============================================================================

// Convert RGB565 to LVGL color
#define RGB565_TO_LV(c) lv_color_make(((c >> 11) & 0x1F) << 3, ((c >> 5) & 0x3F) << 2, (c & 0x1F) << 3)

// Original RGB565 colors from display_common.h
#define COLOR_BACKGROUND_565   0x0000    // Pure black
#define COLOR_RPM_TEXT_565     0xFFFF    // Pure white
#define COLOR_LABEL_565        0xFFFF    // Pure white
#define COLOR_CONNECTED_565    0x2DC9    // Muted green (#2D9D4A)
#define COLOR_DISCONNECTED_565 0xD8A3    // Muted red (#DC3545)
#define COLOR_WARNING_565      0xFD20    // Muted amber (#FFC107)
#define COLOR_BTN_NORMAL_565   0x2945    // Dark blue (#293A4A)
#define COLOR_BTN_PRESSED_565  0x3B8F    // Lighter blue (#3D5A73)
#define COLOR_BTN_TEXT_565     0xDEFB    // Soft white (#DEE2E6)
#define COLOR_KB_BG_565        0x1082    // Keyboard background

// LVGL color macros (direct lv_color_make)
#define UI_COLOR_BACKGROUND    lv_color_black()
#define UI_COLOR_PRIMARY       lv_color_make(0x29, 0x3A, 0x4A)   // Dark blue
#define UI_COLOR_SURFACE       lv_color_make(0x29, 0x3A, 0x4A)   // Same as primary
#define UI_COLOR_SECONDARY     lv_color_make(0x6C, 0x75, 0x7D)   // Gray
#define UI_COLOR_SUCCESS       lv_color_make(0x2D, 0x9D, 0x4A)   // Green
#define UI_COLOR_ERROR         lv_color_make(0xDC, 0x35, 0x45)   // Red
#define UI_COLOR_WARNING       lv_color_make(0xFF, 0xC1, 0x07)   // Amber

// =============================================================================
// LVGL Style Declarations
// =============================================================================

// Screen styles
extern lv_style_t style_screen;          // Black background for all screens

// Button styles
extern lv_style_t style_btn;             // Default button (dark blue)
extern lv_style_t style_btn_pressed;     // Pressed button state
extern lv_style_t style_btn_success;     // Green/success button
extern lv_style_t style_btn_danger;      // Red/danger button

// Label styles
extern lv_style_t style_label;           // Normal white text
extern lv_style_t style_label_large;     // Large text (for RPM)
extern lv_style_t style_label_title;     // Screen titles
extern lv_style_t style_label_success;   // Green text
extern lv_style_t style_label_danger;    // Red text
extern lv_style_t style_label_warning;   // Amber/warning text

// Container styles
extern lv_style_t style_container;       // Dark container with border
extern lv_style_t style_list_item;       // List item style

// Progress bar style
extern lv_style_t style_bar_bg;          // Progress bar background
extern lv_style_t style_bar_indicator;   // Progress bar fill

// =============================================================================
// Font Definitions
// =============================================================================

// Font sizes mapped to original TFT_eSPI sizes
#define UI_FONT_SMALL     &lv_font_montserrat_10   // Size 1 (~8px)
#define UI_FONT_NORMAL    &lv_font_montserrat_14   // Size 2 (~16px)
#define UI_FONT_MEDIUM    &lv_font_montserrat_20   // Size 3
#define UI_FONT_LARGE     &lv_font_montserrat_28   // Size 4 (~26px)
#define UI_FONT_XLARGE    &lv_font_montserrat_48   // Size 6 (~48px) for RPM

// =============================================================================
// Theme Initialization
// =============================================================================

/**
 * Initialize all UI styles. Call this after lvgl_init().
 */
void ui_theme_init();

/**
 * Apply dark theme to the default display.
 */
void ui_theme_apply();

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Create a styled button with text.
 * @param parent Parent object
 * @param text Button label text
 * @param width Button width (0 for content-fit)
 * @param height Button height (0 for content-fit)
 * @return The created button object
 */
lv_obj_t* ui_create_button(lv_obj_t* parent, const char* text, int32_t width, int32_t height);

/**
 * Create a styled label.
 * @param parent Parent object
 * @param text Label text
 * @param font Font to use (UI_FONT_*)
 * @return The created label object
 */
lv_obj_t* ui_create_label(lv_obj_t* parent, const char* text, const lv_font_t* font);

/**
 * Create a screen with standard dark background.
 * @return The created screen object
 */
lv_obj_t* ui_create_screen();

/**
 * Add swipe-right-to-go-back gesture to a screen.
 * When the user swipes right, the callback will be invoked.
 * @param screen The screen object to add gesture to
 * @param back_callback Function to call when swipe-right is detected
 */
void ui_add_swipe_back_gesture(lv_obj_t* screen, void (*back_callback)(void));

#endif // UI_THEME_H
