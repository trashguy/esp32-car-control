#ifndef LVGL_DRIVER_H
#define LVGL_DRIVER_H

#include <lvgl.h>

/**
 * Initialize LVGL library, display driver, and touch input driver.
 * Must be called after TFT and touch are initialized (after displayInit()).
 */
void lvgl_init();

/**
 * LVGL task handler - call this periodically from the display task.
 * Handles rendering, animations, and input processing.
 */
void lvgl_task_handler();

/**
 * Get the LVGL display handle.
 */
lv_display_t* lvgl_get_display();

/**
 * Get the LVGL touch input device handle.
 */
lv_indev_t* lvgl_get_indev();

/**
 * Check if LVGL is initialized.
 */
bool lvgl_is_initialized();

#endif // LVGL_DRIVER_H
