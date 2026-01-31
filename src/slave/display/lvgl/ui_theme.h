#ifndef UI_THEME_H
#define UI_THEME_H

#include <lvgl.h>

// =============================================================================
// Layout Constants
// =============================================================================

#define UI_MENU_BAR_HEIGHT     48   // Android-style bottom nav bar height
#define UI_SCREEN_WIDTH       320
#define UI_SCREEN_HEIGHT      240
#define UI_CONTENT_HEIGHT     (UI_SCREEN_HEIGHT - UI_MENU_BAR_HEIGHT)  // 192px

// =============================================================================
// Color Definitions - Material Design 3 Dark Theme
// =============================================================================

// Convert RGB565 to LVGL color
#define RGB565_TO_LV(c) lv_color_make(((c >> 11) & 0x1F) << 3, ((c >> 5) & 0x3F) << 2, (c & 0x1F) << 3)

// Original RGB565 colors from display_common.h (kept for compatibility)
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

// =============================================================================
// Material Design 3 Color System (Dark Theme)
// Based on Material You / Material Design 3 guidelines
// =============================================================================

// Surface colors (dark theme uses tonal surfaces)
#define UI_COLOR_BACKGROUND    lv_color_make(0x30, 0x30, 0x30)   // Grey background
#define UI_COLOR_SURFACE       lv_color_make(0x30, 0x30, 0x30)   // Surface (grey)
#define UI_COLOR_SURFACE_DIM   lv_color_make(0x20, 0x20, 0x20)   // Surface Dim (darker grey)
#define UI_COLOR_MENU_BAR      lv_color_make(0x00, 0x00, 0x00)   // Black menu bar
#define UI_COLOR_SURFACE_CONT  lv_color_make(0x2B, 0x2A, 0x30)   // Surface Container
#define UI_COLOR_SURFACE_HIGH  lv_color_make(0x36, 0x34, 0x3B)   // Surface Container High
#define UI_COLOR_SURFACE_HIGHEST lv_color_make(0x41, 0x3F, 0x46) // Surface Container Highest

// Primary colors (teal-ish primary for automotive feel)
#define UI_COLOR_PRIMARY       lv_color_make(0xA0, 0xCA, 0xFD)   // Primary (light blue)
#define UI_COLOR_PRIMARY_CONT  lv_color_make(0x00, 0x4A, 0x77)   // Primary Container
#define UI_COLOR_ON_PRIMARY    lv_color_make(0x00, 0x32, 0x58)   // On Primary

// Secondary colors
#define UI_COLOR_SECONDARY     lv_color_make(0xBB, 0xC7, 0xDB)   // Secondary
#define UI_COLOR_SECONDARY_CONT lv_color_make(0x3B, 0x47, 0x57)  // Secondary Container

// Tertiary (accent)
#define UI_COLOR_TERTIARY      lv_color_make(0xD5, 0xBE, 0xE5)   // Tertiary
#define UI_COLOR_TERTIARY_CONT lv_color_make(0x50, 0x3F, 0x5E)   // Tertiary Container

// Semantic colors
#define UI_COLOR_SUCCESS       lv_color_make(0x6D, 0xD5, 0x8C)   // MD3 Green (softer)
#define UI_COLOR_ERROR         lv_color_make(0xF2, 0xB8, 0xB5)   // MD3 Error (on dark)
#define UI_COLOR_ERROR_CONT    lv_color_make(0x8C, 0x1D, 0x18)   // Error Container
#define UI_COLOR_WARNING       lv_color_make(0xFF, 0xB9, 0x45)   // Warning/Amber

// Text colors
#define UI_COLOR_ON_SURFACE    lv_color_make(0xE6, 0xE1, 0xE5)   // On Surface (primary text)
#define UI_COLOR_ON_SURFACE_VAR lv_color_make(0xCA, 0xC4, 0xCF)  // On Surface Variant (secondary text)
#define UI_COLOR_OUTLINE       lv_color_make(0x93, 0x90, 0x94)   // Outline
#define UI_COLOR_OUTLINE_VAR   lv_color_make(0x49, 0x45, 0x4E)   // Outline Variant

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
extern lv_style_t style_btn_nav;         // Navigation bar button (transparent, white icon)
extern lv_style_t style_btn_nav_pressed; // Nav button pressed state

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
extern lv_style_t style_menu_bar;        // Bottom menu bar (black)

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
 * Create a bottom menu bar on the given screen.
 * @param parent The screen to add the menu bar to
 * @param height Height of the menu bar (default 44px)
 * @return The created menu bar object (buttons should be added as children)
 */
lv_obj_t* ui_create_menu_bar(lv_obj_t* parent, int32_t height);

/**
 * Add swipe-right-to-go-back gesture to a screen.
 * When the user swipes right, the callback will be invoked.
 * @param screen The screen object to add gesture to
 * @param back_callback Function to call when swipe-right is detected
 */
void ui_add_swipe_back_gesture(lv_obj_t* screen, void (*back_callback)(void));

#endif // UI_THEME_H
