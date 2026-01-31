#include "lvgl_driver.h"
#include "../display_common.h"
#include <Arduino.h>
#include <TFT_eSPI.h>

// =============================================================================
// Configuration
// =============================================================================

#define LVGL_BUF_LINES      40      // Number of lines per buffer (40 lines = 25.6KB per buffer)
#define LVGL_BUF_SIZE       (SCREEN_WIDTH * LVGL_BUF_LINES)

// =============================================================================
// Static Variables
// =============================================================================

static lv_display_t* disp = nullptr;
static lv_indev_t* indev_touch = nullptr;
static bool initialized = false;

// Double buffer for smooth rendering (DMA-friendly)
static lv_color_t buf1[LVGL_BUF_SIZE];
static lv_color_t buf2[LVGL_BUF_SIZE];

// =============================================================================
// Display Flush Callback
// =============================================================================

static void disp_flush_cb(lv_display_t* display, const lv_area_t* area, uint8_t* px_map) {
    TFT_eSPI& tft = getTft();
    
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    // LV_COLOR_16_SWAP=1 already swaps bytes, so don't swap again in pushColors
    tft.pushColors((uint16_t*)px_map, w * h, false);
    tft.endWrite();
    
    lv_display_flush_ready(display);
}

// =============================================================================
// Touch Input Callback
// =============================================================================

static void touch_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    int16_t x, y;
    bool touched = touchGetPoint(&x, &y);
    
    if (touched) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// =============================================================================
// Tick Callback for LVGL 9.x
// =============================================================================

static uint32_t tick_get_cb(void) {
    return millis();
}

// =============================================================================
// Public API
// =============================================================================

void lvgl_init() {
    if (initialized) {
        return;
    }
    
    Serial.println("Initializing LVGL...");
    
    // Initialize LVGL library
    lv_init();
    
    // Set tick callback for LVGL 9.x
    lv_tick_set_cb(tick_get_cb);
    
    // Create display with double buffering
    disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, disp_flush_cb);
    lv_display_set_buffers(disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // Create touch input device
    indev_touch = lv_indev_create();
    lv_indev_set_type(indev_touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touch, touch_read_cb);
    
    initialized = true;
    Serial.println("LVGL initialized successfully");
    Serial.printf("  Display: %dx%d\n", SCREEN_WIDTH, SCREEN_HEIGHT);
    Serial.printf("  Buffer: %d lines (%d bytes x2)\n", LVGL_BUF_LINES, sizeof(buf1));
}

void lvgl_task_handler() {
    if (!initialized) {
        return;
    }
    
    // Let LVGL handle timers, animations, and rendering
    lv_timer_handler();
}

lv_display_t* lvgl_get_display() {
    return disp;
}

lv_indev_t* lvgl_get_indev() {
    return indev_touch;
}

bool lvgl_is_initialized() {
    return initialized;
}
