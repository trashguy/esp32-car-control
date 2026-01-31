#include "ui_ota_popup.h"
#include "ui_theme.h"
#include "slave/ota_handler.h"
#include "slave/spi_ota.h"
#include "shared/ota_protocol.h"
#include <Arduino.h>

// =============================================================================
// Layout Constants
// =============================================================================

#define POPUP_WIDTH         280
#define POPUP_HEIGHT        160
#define POPUP_RADIUS        12
#define BTN_WIDTH           100
#define BTN_HEIGHT          38
#define BTN_MARGIN          20
#define PROGRESS_BAR_HEIGHT 25

// =============================================================================
// Static Objects
// =============================================================================

static lv_obj_t* popup_container = nullptr;
static lv_obj_t* popup_bg = nullptr;

// Title and content labels
static lv_obj_t* lbl_title = nullptr;
static lv_obj_t* lbl_content = nullptr;
static lv_obj_t* lbl_content2 = nullptr;
static lv_obj_t* lbl_warning = nullptr;

// Buttons
static lv_obj_t* btn_primary = nullptr;   // VERIFY/INSTALL/OK
static lv_obj_t* lbl_btn_primary = nullptr;
static lv_obj_t* btn_secondary = nullptr; // ABORT
static lv_obj_t* lbl_btn_secondary = nullptr;

// Progress bar
static lv_obj_t* bar_progress = nullptr;
static lv_obj_t* lbl_progress = nullptr;

// State
static UiOtaPopupState popup_state = UI_OTA_POPUP_HIDDEN;
static uint8_t install_progress = 0;

// Callbacks
static void (*cb_install)(void) = nullptr;
static void (*cb_abort)(void) = nullptr;
static void (*cb_dismiss)(void) = nullptr;

// =============================================================================
// Forward Declarations
// =============================================================================

static void update_popup_content();
static void primary_btn_handler(lv_event_t* e);
static void secondary_btn_handler(lv_event_t* e);

// =============================================================================
// Button Event Handlers
// =============================================================================

static void primary_btn_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    
    switch (popup_state) {
        case UI_OTA_POPUP_CONFIRM:
#if OTA_ENABLE_TEST_MODE
            // VERIFY button pressed
            Serial.println("[UI OTA] Verify button pressed");
            popup_state = UI_OTA_POPUP_VERIFYING;
            spiOtaClearVerifyState();
            spiOtaRequestVerify();
            update_popup_content();
#else
            // INSTALL button pressed (no verify mode)
            Serial.println("[UI OTA] Install button pressed (no verify)");
            popup_state = UI_OTA_POPUP_INSTALLING;
            update_popup_content();
            if (cb_install) cb_install();
#endif
            break;
            
        case UI_OTA_POPUP_VERIFIED:
            // INSTALL button pressed after verification
            Serial.println("[UI OTA] Install button pressed (after verify)");
            popup_state = UI_OTA_POPUP_INSTALLING;
            update_popup_content();
            if (cb_install) cb_install();
            break;
            
        case UI_OTA_POPUP_COMPLETE:
        case UI_OTA_POPUP_ERROR:
            // OK/DISMISS button pressed
            Serial.println("[UI OTA] Dismiss button pressed");
            if (popup_state == UI_OTA_POPUP_ERROR) {
                spiOtaExitMode();
                otaDismissUpdate();
            }
            ui_ota_popup_hide();
            if (cb_dismiss) cb_dismiss();
            break;
            
        default:
            break;
    }
}

static void secondary_btn_handler(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    
    // ABORT button
    if (popup_state == UI_OTA_POPUP_CONFIRM || popup_state == UI_OTA_POPUP_VERIFIED) {
        Serial.println("[UI OTA] Abort button pressed");
        spiOtaExitMode();
        otaDismissUpdate();
        ui_ota_popup_hide();
        if (cb_abort) cb_abort();
    }
}

// =============================================================================
// UI Update Helpers
// =============================================================================

static void update_popup_content() {
    if (!popup_container) return;
    
    switch (popup_state) {
        case UI_OTA_POPUP_HIDDEN:
            lv_obj_add_flag(popup_container, LV_OBJ_FLAG_HIDDEN);
            break;
            
        case UI_OTA_POPUP_CONFIRM: {
            lv_obj_clear_flag(popup_container, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(lbl_title, "FIRMWARE UPDATE");
            
            // Get package info
            const OtaPackageInfo* info = otaGetPackageInfo();
            if (info && info->valid) {
                char buf[48];
                snprintf(buf, sizeof(buf), "Version: %s", info->version);
                lv_label_set_text(lbl_content, buf);
                
                float displayMB = info->displaySize / 1048576.0f;
                float controllerMB = info->controllerSize / 1048576.0f;
                snprintf(buf, sizeof(buf), "Display: %.2fMB  Controller: %.2fMB", 
                         displayMB, controllerMB);
                lv_label_set_text(lbl_content2, buf);
            } else {
                lv_label_set_text(lbl_content, "New update available");
                lv_label_set_text(lbl_content2, "");
            }
            
            lv_obj_add_flag(lbl_warning, LV_OBJ_FLAG_HIDDEN);
            
            // Show buttons
#if OTA_ENABLE_TEST_MODE
            lv_label_set_text(lbl_btn_primary, "VERIFY");
#else
            lv_label_set_text(lbl_btn_primary, "INSTALL");
#endif
            lv_obj_clear_flag(btn_primary, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_state(btn_primary, LV_STATE_DISABLED);
            
            lv_label_set_text(lbl_btn_secondary, "ABORT");
            lv_obj_clear_flag(btn_secondary, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_state(btn_secondary, LV_STATE_DISABLED);
            
            // Hide progress bar
            lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_progress, LV_OBJ_FLAG_HIDDEN);
            break;
        }
            
        case UI_OTA_POPUP_VERIFYING:
            lv_label_set_text(lbl_title, "VERIFYING SPI");
            lv_label_set_text(lbl_content, "Testing connection...");
            lv_label_set_text(lbl_content2, "");
            lv_obj_set_style_text_color(lbl_content, UI_COLOR_WARNING, 0);
            
            lv_obj_add_flag(lbl_warning, LV_OBJ_FLAG_HIDDEN);
            
            // Hide primary button, disable abort
            lv_obj_add_flag(btn_primary, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_state(btn_secondary, LV_STATE_DISABLED);
            
            lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_progress, LV_OBJ_FLAG_HIDDEN);
            break;
            
        case UI_OTA_POPUP_VERIFIED:
            lv_label_set_text(lbl_title, "VERIFIED - READY");
            lv_label_set_text(lbl_content, "SPI connection verified!");
            lv_label_set_text(lbl_content2, "");
            lv_obj_set_style_text_color(lbl_content, UI_COLOR_SUCCESS, 0);
            
            lv_obj_add_flag(lbl_warning, LV_OBJ_FLAG_HIDDEN);
            
            lv_label_set_text(lbl_btn_primary, "INSTALL");
            lv_obj_clear_flag(btn_primary, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_state(btn_primary, LV_STATE_DISABLED);
            
            lv_obj_clear_state(btn_secondary, LV_STATE_DISABLED);
            
            lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_progress, LV_OBJ_FLAG_HIDDEN);
            break;
            
        case UI_OTA_POPUP_INSTALLING:
            lv_label_set_text(lbl_title, "UPDATING DISPLAY");
            lv_label_set_text(lbl_content, "");
            lv_label_set_text(lbl_content2, "");
            
            lv_label_set_text(lbl_warning, "Do not power off!");
            lv_obj_clear_flag(lbl_warning, LV_OBJ_FLAG_HIDDEN);
            
            // Hide buttons, show progress
            lv_obj_add_flag(btn_primary, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(btn_secondary, LV_OBJ_FLAG_HIDDEN);
            
            lv_obj_clear_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(lbl_progress, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(bar_progress, install_progress, LV_ANIM_ON);
            
            char pct[8];
            snprintf(pct, sizeof(pct), "%d%%", install_progress);
            lv_label_set_text(lbl_progress, pct);
            break;
            
        case UI_OTA_POPUP_CONTROLLER:
            lv_label_set_text(lbl_title, "UPDATING CONTROLLER");
            lv_label_set_text(lbl_content, "");
            lv_label_set_text(lbl_content2, "");
            
            lv_label_set_text(lbl_warning, "Do not power off!");
            lv_obj_clear_flag(lbl_warning, LV_OBJ_FLAG_HIDDEN);
            
            lv_obj_add_flag(btn_primary, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(btn_secondary, LV_OBJ_FLAG_HIDDEN);
            
            lv_obj_clear_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(lbl_progress, LV_OBJ_FLAG_HIDDEN);
            break;
            
        case UI_OTA_POPUP_COMPLETE:
            lv_label_set_text(lbl_title, "UPDATE COMPLETE");
            lv_label_set_text(lbl_content, "Firmware updated successfully!");
            lv_label_set_text(lbl_content2, "");
            lv_obj_set_style_text_color(lbl_content, UI_COLOR_SUCCESS, 0);
            
            lv_obj_add_flag(lbl_warning, LV_OBJ_FLAG_HIDDEN);
            
            lv_label_set_text(lbl_btn_primary, "OK");
            lv_obj_clear_flag(btn_primary, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_state(btn_primary, LV_STATE_DISABLED);
            lv_obj_add_style(btn_primary, &style_btn_success, 0);
            
            lv_obj_add_flag(btn_secondary, LV_OBJ_FLAG_HIDDEN);
            
            lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_progress, LV_OBJ_FLAG_HIDDEN);
            break;
            
        case UI_OTA_POPUP_ERROR:
            lv_label_set_text(lbl_title, "UPDATE FAILED");
            // lbl_content is set by ui_ota_popup_set_error
            lv_label_set_text(lbl_content2, "");
            lv_obj_set_style_text_color(lbl_content, UI_COLOR_ERROR, 0);
            
            lv_obj_add_flag(lbl_warning, LV_OBJ_FLAG_HIDDEN);
            
            lv_label_set_text(lbl_btn_primary, "DISMISS");
            lv_obj_clear_flag(btn_primary, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_state(btn_primary, LV_STATE_DISABLED);
            
            lv_obj_add_flag(btn_secondary, LV_OBJ_FLAG_HIDDEN);
            
            lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(lbl_progress, LV_OBJ_FLAG_HIDDEN);
            break;
    }
}

// =============================================================================
// Public API
// =============================================================================

void ui_ota_popup_create() {
    // Create fullscreen container for dimming effect
    popup_container = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(popup_container);
    lv_obj_set_size(popup_container, 320, 240);
    lv_obj_set_style_bg_color(popup_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(popup_container, LV_OPA_50, 0);
    lv_obj_add_flag(popup_container, LV_OBJ_FLAG_HIDDEN);
    
    // Create popup background
    popup_bg = lv_obj_create(popup_container);
    lv_obj_set_size(popup_bg, POPUP_WIDTH, POPUP_HEIGHT);
    lv_obj_center(popup_bg);
    lv_obj_set_style_bg_color(popup_bg, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(popup_bg, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(popup_bg, POPUP_RADIUS, 0);
    lv_obj_set_style_border_color(popup_bg, lv_color_white(), 0);
    lv_obj_set_style_border_width(popup_bg, 2, 0);
    lv_obj_set_style_pad_all(popup_bg, 10, 0);
    lv_obj_clear_flag(popup_bg, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title label
    lbl_title = lv_label_create(popup_bg);
    lv_label_set_text(lbl_title, "FIRMWARE UPDATE");
    lv_obj_set_style_text_font(lbl_title, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 5);
    
    // Content labels
    lbl_content = lv_label_create(popup_bg);
    lv_label_set_text(lbl_content, "");
    lv_obj_set_style_text_font(lbl_content, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_content, lv_color_make(0xDE, 0xE2, 0xE6), 0);
    lv_obj_align(lbl_content, LV_ALIGN_TOP_MID, 0, 35);
    
    lbl_content2 = lv_label_create(popup_bg);
    lv_label_set_text(lbl_content2, "");
    lv_obj_set_style_text_font(lbl_content2, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_content2, lv_color_make(0xDE, 0xE2, 0xE6), 0);
    lv_obj_align(lbl_content2, LV_ALIGN_TOP_MID, 0, 55);
    
    // Warning label
    lbl_warning = lv_label_create(popup_bg);
    lv_label_set_text(lbl_warning, "Do not power off!");
    lv_obj_set_style_text_font(lbl_warning, UI_FONT_SMALL, 0);
    lv_obj_set_style_text_color(lbl_warning, UI_COLOR_WARNING, 0);
    lv_obj_align(lbl_warning, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_add_flag(lbl_warning, LV_OBJ_FLAG_HIDDEN);
    
    // Progress bar
    bar_progress = lv_bar_create(popup_bg);
    lv_obj_set_size(bar_progress, POPUP_WIDTH - 60, PROGRESS_BAR_HEIGHT);
    lv_obj_align(bar_progress, LV_ALIGN_CENTER, 0, 10);
    lv_bar_set_range(bar_progress, 0, 100);
    lv_bar_set_value(bar_progress, 0, LV_ANIM_OFF);
    lv_obj_add_style(bar_progress, &style_bar_bg, LV_PART_MAIN);
    lv_obj_add_style(bar_progress, &style_bar_indicator, LV_PART_INDICATOR);
    lv_obj_add_flag(bar_progress, LV_OBJ_FLAG_HIDDEN);
    
    // Progress percentage label
    lbl_progress = lv_label_create(popup_bg);
    lv_label_set_text(lbl_progress, "0%");
    lv_obj_set_style_text_font(lbl_progress, UI_FONT_NORMAL, 0);
    lv_obj_set_style_text_color(lbl_progress, lv_color_white(), 0);
    lv_obj_align_to(lbl_progress, bar_progress, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(lbl_progress, LV_OBJ_FLAG_HIDDEN);
    
    // Primary button (VERIFY/INSTALL/OK)
    btn_primary = lv_button_create(popup_bg);
    lv_obj_set_size(btn_primary, BTN_WIDTH, BTN_HEIGHT);
    lv_obj_align(btn_primary, LV_ALIGN_BOTTOM_LEFT, BTN_MARGIN, -25);
    lv_obj_add_style(btn_primary, &style_btn, 0);
    lv_obj_add_style(btn_primary, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_primary, primary_btn_handler, LV_EVENT_CLICKED, nullptr);
    
    lbl_btn_primary = lv_label_create(btn_primary);
    lv_label_set_text(lbl_btn_primary, "INSTALL");
    lv_obj_center(lbl_btn_primary);
    
    // Secondary button (ABORT)
    btn_secondary = lv_button_create(popup_bg);
    lv_obj_set_size(btn_secondary, BTN_WIDTH, BTN_HEIGHT);
    lv_obj_align(btn_secondary, LV_ALIGN_BOTTOM_RIGHT, -BTN_MARGIN, -25);
    lv_obj_add_style(btn_secondary, &style_btn, 0);
    lv_obj_add_style(btn_secondary, &style_btn_pressed, LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn_secondary, secondary_btn_handler, LV_EVENT_CLICKED, nullptr);
    
    lbl_btn_secondary = lv_label_create(btn_secondary);
    lv_label_set_text(lbl_btn_secondary, "ABORT");
    lv_obj_center(lbl_btn_secondary);
    
    Serial.println("[UI OTA] Popup created");
}

void ui_ota_popup_show() {
    if (popup_state == UI_OTA_POPUP_HIDDEN) {
        popup_state = UI_OTA_POPUP_CONFIRM;
        install_progress = 0;
        spiOtaClearVerifyState();
        
        // Reset content color
        lv_obj_set_style_text_color(lbl_content, lv_color_make(0xDE, 0xE2, 0xE6), 0);
        
        // Reset button style
        lv_obj_remove_style(btn_primary, &style_btn_success, 0);
        lv_obj_add_style(btn_primary, &style_btn, 0);
        
        update_popup_content();
        Serial.println("[UI OTA] Popup shown");
    }
}

void ui_ota_popup_hide() {
    popup_state = UI_OTA_POPUP_HIDDEN;
    lv_obj_add_flag(popup_container, LV_OBJ_FLAG_HIDDEN);
    Serial.println("[UI OTA] Popup hidden");
}

bool ui_ota_popup_is_visible() {
    return popup_state != UI_OTA_POPUP_HIDDEN;
}

UiOtaPopupState ui_ota_popup_get_state() {
    return popup_state;
}

void ui_ota_popup_update() {
    if (popup_state == UI_OTA_POPUP_HIDDEN) return;
    
    // Check for verification result from master
    if (popup_state == UI_OTA_POPUP_VERIFYING) {
        uint8_t verifyState = spiOtaGetVerifyState();
        if (verifyState == 2) {
            // Verification passed
            popup_state = UI_OTA_POPUP_VERIFIED;
            update_popup_content();
            Serial.println("[UI OTA] Verification passed");
        } else if (verifyState == 3) {
            // Verification failed
            ui_ota_popup_set_error("SPI verification failed");
            Serial.println("[UI OTA] Verification failed");
        }
    }
    
    // Check OTA handler state
    OtaState otaState = otaGetState();
    
    switch (otaState) {
        case OTA_STATE_PACKAGE_READY:
            if (popup_state == UI_OTA_POPUP_HIDDEN) {
                ui_ota_popup_show();
            }
            break;
            
        case OTA_STATE_PENDING_CONTROLLER:
            if (popup_state != UI_OTA_POPUP_CONTROLLER) {
                popup_state = UI_OTA_POPUP_CONTROLLER;
                install_progress = 0;
                update_popup_content();
                otaStartControllerUpdate();
            }
            break;
            
        case OTA_STATE_COMPLETE:
            if (popup_state != UI_OTA_POPUP_COMPLETE) {
                popup_state = UI_OTA_POPUP_COMPLETE;
                update_popup_content();
            }
            break;
            
        case OTA_STATE_ERROR:
            if (popup_state != UI_OTA_POPUP_ERROR) {
                ui_ota_popup_set_error(otaGetErrorMessage());
            }
            break;
            
        default:
            break;
    }
    
    // Update progress bar if installing
    if (popup_state == UI_OTA_POPUP_INSTALLING || popup_state == UI_OTA_POPUP_CONTROLLER) {
        uint8_t progress = otaGetProgress();
        if (progress != install_progress) {
            install_progress = progress;
            lv_bar_set_value(bar_progress, install_progress, LV_ANIM_ON);
            
            char pct[8];
            snprintf(pct, sizeof(pct), "%d%%", install_progress);
            lv_label_set_text(lbl_progress, pct);
        }
    }
}

void ui_ota_popup_set_progress(uint8_t progress) {
    install_progress = progress;
    if (popup_state == UI_OTA_POPUP_INSTALLING || popup_state == UI_OTA_POPUP_CONTROLLER) {
        lv_bar_set_value(bar_progress, install_progress, LV_ANIM_ON);
        
        char pct[8];
        snprintf(pct, sizeof(pct), "%d%%", install_progress);
        lv_label_set_text(lbl_progress, pct);
    }
}

void ui_ota_popup_set_error(const char* message) {
    popup_state = UI_OTA_POPUP_ERROR;
    lv_label_set_text(lbl_content, message);
    update_popup_content();
}

void ui_ota_popup_set_complete() {
    popup_state = UI_OTA_POPUP_COMPLETE;
    update_popup_content();
}

void ui_ota_popup_set_callbacks(
    void (*on_install)(void),
    void (*on_abort)(void),
    void (*on_dismiss)(void)
) {
    cb_install = on_install;
    cb_abort = on_abort;
    cb_dismiss = on_dismiss;
}
