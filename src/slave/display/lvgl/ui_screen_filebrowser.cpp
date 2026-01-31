#include "ui_screen_filebrowser.h"
#include "ui_theme.h"
#include "../../sd_card.h"
#include "../../usb_msc.h"
#include <Arduino.h>
#include <SD_MMC.h>

// =============================================================================
// Layout Constants
// =============================================================================

#define FILE_LIST_Y         45
#define FILE_LIST_H         150
#define FILE_LINE_HEIGHT    22
#define MAX_FILES           64
#define BTN_SIZE            36
#define BTN_MARGIN          8

// =============================================================================
// Static UI Objects
// =============================================================================

static lv_obj_t* screen_filebrowser = nullptr;

// Labels
static lv_obj_t* lbl_title = nullptr;

// File list container
static lv_obj_t* list_files = nullptr;

// USB locked overlay
static lv_obj_t* cont_usb_locked = nullptr;

// Buttons
static lv_obj_t* btn_back = nullptr;

// Callbacks
static void (*cb_back)(void) = nullptr;
static void (*cb_file)(const char* filename) = nullptr;

// State
static bool usb_locked = false;

// =============================================================================
// Event Handlers
// =============================================================================

static void back_btn_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && cb_back) {
        cb_back();
    }
}

static void file_item_event_handler(lv_event_t* e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED && cb_file) {
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t* lbl = lv_obj_get_child(btn, 0);
        if (lbl) {
            const char* filename = lv_label_get_text(lbl);
            cb_file(filename);
        }
    }
}

// Wrapper for swipe-back gesture
static void swipe_back_handler() {
    if (cb_back) {
        cb_back();
    }
}

// =============================================================================
// Helper: Populate file list from SD card
// =============================================================================

static void populate_file_list() {
    if (!list_files) return;
    
    // Clear existing items
    lv_obj_clean(list_files);
    
    if (!sdCardPresent()) {
        // Show "No SD Card" message
        lv_obj_t* lbl = lv_label_create(list_files);
        lv_label_set_text(lbl, "No SD Card");
        lv_obj_set_style_text_font(lbl, UI_FONT_NORMAL, 0);
        lv_obj_set_style_text_color(lbl, UI_COLOR_ERROR, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
        return;
    }
    
    // Open root directory
    fs::File root = SD_MMC.open("/");
    if (!root || !root.isDirectory()) {
        lv_obj_t* lbl = lv_label_create(list_files);
        lv_label_set_text(lbl, "Cannot read SD");
        lv_obj_set_style_text_font(lbl, UI_FONT_NORMAL, 0);
        lv_obj_set_style_text_color(lbl, UI_COLOR_ERROR, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
        return;
    }
    
    int fileCount = 0;
    fs::File file = root.openNextFile();
    
    while (file && fileCount < MAX_FILES) {
        const char* name = file.name();
        bool isDir = file.isDirectory();
        
        // Create list button for this file/directory
        lv_obj_t* btn = lv_list_add_button(list_files, 
            isDir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE, 
            name);
        
        // Style the button
        lv_obj_set_style_bg_color(btn, UI_COLOR_SURFACE, 0);
        lv_obj_set_style_bg_color(btn, lv_color_make(0x3D, 0x5A, 0x73), LV_STATE_PRESSED);
        lv_obj_set_style_pad_ver(btn, 4, 0);
        
        // Get the label and style it
        lv_obj_t* lbl = lv_obj_get_child(btn, 0);  // Icon
        if (lbl) {
            lv_obj_set_style_text_color(lbl, isDir ? UI_COLOR_SUCCESS : lv_color_white(), 0);
        }
        lbl = lv_obj_get_child(btn, 1);  // Text
        if (lbl) {
            lv_obj_set_style_text_font(lbl, UI_FONT_SMALL, 0);
            lv_obj_set_style_text_color(lbl, isDir ? UI_COLOR_SUCCESS : lv_color_white(), 0);
        }
        
        // Add click handler
        lv_obj_add_event_cb(btn, file_item_event_handler, LV_EVENT_CLICKED, nullptr);
        
        fileCount++;
        file = root.openNextFile();
    }
    
    root.close();
    
    // Show "Empty" if no files
    if (fileCount == 0) {
        lv_obj_t* lbl = lv_label_create(list_files);
        lv_label_set_text(lbl, "Empty");
        lv_obj_set_style_text_font(lbl, UI_FONT_NORMAL, 0);
        lv_obj_set_style_text_color(lbl, UI_COLOR_SECONDARY, 0);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    }
}

// =============================================================================
// Helper: Create USB locked overlay
// =============================================================================

static void create_usb_locked_overlay() {
    cont_usb_locked = lv_obj_create(screen_filebrowser);
    lv_obj_set_size(cont_usb_locked, 304, FILE_LIST_H + 10);
    lv_obj_set_pos(cont_usb_locked, 8, FILE_LIST_Y - 5);
    lv_obj_set_style_bg_color(cont_usb_locked, UI_COLOR_BACKGROUND, 0);
    lv_obj_set_style_border_color(cont_usb_locked, UI_COLOR_ERROR, 0);
    lv_obj_set_style_border_width(cont_usb_locked, 2, 0);
    lv_obj_set_style_radius(cont_usb_locked, 4, 0);
    lv_obj_clear_flag(cont_usb_locked, LV_OBJ_FLAG_SCROLLABLE);
    
    // USB icon (using symbol)
    lv_obj_t* lbl_icon = lv_label_create(cont_usb_locked);
    lv_label_set_text(lbl_icon, LV_SYMBOL_USB);
    lv_obj_set_style_text_font(lbl_icon, UI_FONT_LARGE, 0);
    lv_obj_set_style_text_color(lbl_icon, lv_color_white(), 0);
    lv_obj_align(lbl_icon, LV_ALIGN_CENTER, 0, -30);
    
    // "SD LOCKED" text
    lv_obj_t* lbl_locked = lv_label_create(cont_usb_locked);
    lv_label_set_text(lbl_locked, "SD LOCKED");
    lv_obj_set_style_text_font(lbl_locked, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl_locked, UI_COLOR_ERROR, 0);
    lv_obj_align(lbl_locked, LV_ALIGN_CENTER, 0, 10);
    
    // Instructions
    lv_obj_t* lbl_info1 = lv_label_create(cont_usb_locked);
    lv_label_set_text(lbl_info1, "Mounted via USB");
    lv_obj_set_style_text_font(lbl_info1, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_info1, UI_COLOR_SECONDARY, 0);
    lv_obj_align(lbl_info1, LV_ALIGN_CENTER, 0, 35);
    
    lv_obj_t* lbl_info2 = lv_label_create(cont_usb_locked);
    lv_label_set_text(lbl_info2, "Eject from PC to unlock");
    lv_obj_set_style_text_font(lbl_info2, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_info2, UI_COLOR_SECONDARY, 0);
    lv_obj_align(lbl_info2, LV_ALIGN_CENTER, 0, 50);
    
    // Hidden by default
    lv_obj_add_flag(cont_usb_locked, LV_OBJ_FLAG_HIDDEN);
}

// =============================================================================
// Public API
// =============================================================================

void ui_screen_filebrowser_create() {
    // Create the screen
    screen_filebrowser = ui_create_screen();
    
    // Title
    lbl_title = lv_label_create(screen_filebrowser);
    lv_label_set_text(lbl_title, "FILE BROWSER");
    lv_obj_set_style_text_font(lbl_title, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Horizontal line under title
    lv_obj_t* line_title = lv_obj_create(screen_filebrowser);
    lv_obj_set_size(line_title, 280, 2);
    lv_obj_set_pos(line_title, 20, 35);
    lv_obj_set_style_bg_color(line_title, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_border_width(line_title, 0, 0);
    lv_obj_set_style_radius(line_title, 0, 0);
    
    // File list (LVGL list widget)
    list_files = lv_list_create(screen_filebrowser);
    lv_obj_set_size(list_files, 304, FILE_LIST_H);
    lv_obj_set_pos(list_files, 8, FILE_LIST_Y);
    lv_obj_set_style_bg_color(list_files, UI_COLOR_BACKGROUND, 0);
    lv_obj_set_style_border_color(list_files, UI_COLOR_SECONDARY, 0);
    lv_obj_set_style_border_width(list_files, 1, 0);
    lv_obj_set_style_pad_all(list_files, 4, 0);
    lv_obj_set_style_pad_row(list_files, 2, 0);
    
    // Create USB locked overlay (hidden initially)
    create_usb_locked_overlay();
    
    // Back button (bottom left)
    btn_back = lv_button_create(screen_filebrowser);
    lv_obj_set_size(btn_back, BTN_SIZE, BTN_SIZE);
    lv_obj_set_pos(btn_back, BTN_MARGIN, 240 - BTN_SIZE - BTN_MARGIN);
    lv_obj_add_style(btn_back, &style_btn, 0);
    lv_obj_add_style(btn_back, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_back, back_btn_event_handler, LV_EVENT_CLICKED, nullptr);
    
    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(lbl_back, UI_FONT_NORMAL, 0);
    lv_obj_center(lbl_back);
    
    // Add swipe gesture (swipe right to go back)
    ui_add_swipe_back_gesture(screen_filebrowser, swipe_back_handler);
    
    Serial.println("UI FileBrowser screen created");
}

lv_obj_t* ui_screen_filebrowser_get() {
    return screen_filebrowser;
}

void ui_screen_filebrowser_refresh() {
    if (usb_locked) return;  // Don't refresh while USB mounted
    populate_file_list();
}

void ui_screen_filebrowser_update() {
    // Check if USB mount state changed
    bool currentlyMounted = usbMscMounted();
    if (currentlyMounted != usb_locked) {
        ui_screen_filebrowser_set_usb_locked(currentlyMounted);
        
        if (!currentlyMounted) {
            // USB was ejected - refresh file list
            populate_file_list();
            Serial.println("USB ejected - file browser unlocked");
        } else {
            Serial.println("USB mounted - file browser locked");
        }
    }
}

void ui_screen_filebrowser_set_back_callback(void (*callback)(void)) {
    cb_back = callback;
}

void ui_screen_filebrowser_set_file_callback(void (*callback)(const char* filename)) {
    cb_file = callback;
}

void ui_screen_filebrowser_set_usb_locked(bool locked) {
    usb_locked = locked;
    
    if (cont_usb_locked) {
        if (locked) {
            lv_obj_clear_flag(cont_usb_locked, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(cont_usb_locked, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ui_screen_filebrowser_reset() {
    usb_locked = false;
    if (cont_usb_locked) {
        lv_obj_add_flag(cont_usb_locked, LV_OBJ_FLAG_HIDDEN);
    }
    // Refresh file list when screen is entered
    populate_file_list();
}
