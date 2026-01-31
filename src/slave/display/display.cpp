#include "display.h"
#include "display_common.h"
#include "../tasks.h"
#include "slave/spi_slave.h"
#include "../usb_msc.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include <Arduino.h>

// USE_LVGL_UI is defined via platformio.ini build_flags
// LVGL UI includes (when USE_LVGL_UI=1)
#if USE_LVGL_UI
#include "lvgl/lvgl_driver.h"
#include "lvgl/ui_theme.h"
#include "lvgl/ui_screen_main.h"
#include "lvgl/ui_screen_settings.h"
#include "lvgl/ui_screen_filebrowser.h"
#include "lvgl/ui_screen_wifi.h"
#include "lvgl/ui_ota_popup.h"
#endif

// Legacy TFT direct-drawing includes (when USE_LVGL_UI=0)
#if !USE_LVGL_UI
#include "legacy/screen_main.h"
#include "legacy/screen_settings.h"
#include "legacy/screen_filebrowser.h"
#include "legacy/screen_wifi.h"
#include "legacy/screen_ota_popup.h"
#endif

// =============================================================================
// Screen Management
// =============================================================================

static ScreenType currentScreen = SCREEN_MAIN;
static bool lastTouchState = false;
static int16_t lastTouchX = 0;
static int16_t lastTouchY = 0;
static unsigned long lastWifiStatusCheck = 0;
static bool lastWifiConnected = false;

ScreenType getCurrentScreen() {
    return currentScreen;
}

#if !USE_LVGL_UI
void switchToScreen(ScreenType screen) {
    // Reset current screen state before switching
    switch (currentScreen) {
        case SCREEN_MAIN:
            // Main screen has no reset needed
            break;
        case SCREEN_SETTINGS:
            screenSettingsReset();
            break;
        case SCREEN_FILE_BROWSER:
            screenFileBrowserReset();
            break;
        case SCREEN_WIFI:
            screenWifiReset();
            break;
    }

    currentScreen = screen;

    // Draw the new screen
    switch (screen) {
        case SCREEN_MAIN:
            screenMainDraw();
            break;
        case SCREEN_SETTINGS:
            screenSettingsDraw();
            break;
        case SCREEN_FILE_BROWSER:
            screenFileBrowserDraw();
            break;
        case SCREEN_WIFI:
            screenWifiDraw();
            break;
    }
}
#endif

// =============================================================================
// Display Initialization
// =============================================================================

#if USE_LVGL_UI
// Forward declarations for LVGL screen switching
static void lvgl_switch_to_screen(ScreenType screen);

// LVGL button callbacks - Main screen
static void onGearButtonPressed() {
    lvgl_switch_to_screen(SCREEN_SETTINGS);
}

static void onModeButtonPressed() {
    uint8_t currentMode = spiSlaveGetRequestedMode();
    uint8_t newMode = (currentMode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
    spiSlaveSetRequestedMode(newMode);
    ui_screen_main_set_mode(newMode);
    // Show/hide RPM buttons based on mode
    ui_screen_main_show_rpm_buttons(newMode == MODE_MANUAL && spiSlaveIsConnected());
}

static void onRpmUpPressed() {
    uint16_t currentRpm = spiSlaveGetRequestedRpm();
    if (currentRpm < 5000) {
        uint16_t newRpm = currentRpm + 100;
        spiSlaveSetRequestedRpm(newRpm);
    }
}

static void onRpmDownPressed() {
    uint16_t currentRpm = spiSlaveGetRequestedRpm();
    if (currentRpm > 500) {
        uint16_t newRpm = currentRpm - 100;
        spiSlaveSetRequestedRpm(newRpm);
    }
}

// LVGL button callbacks - Settings screen
static void onSettingsBackPressed() {
    lvgl_switch_to_screen(SCREEN_MAIN);
}

static void onSettingsSdPressed() {
    lvgl_switch_to_screen(SCREEN_FILE_BROWSER);
}

static void onSettingsWifiPressed() {
    lvgl_switch_to_screen(SCREEN_WIFI);
}

static void onSettingsUsbPressed() {
    // Toggle USB MSC mode
    if (usbMscIsEnabled()) {
        usbMscDisable();
        ui_screen_settings_set_usb_enabled(false);
    } else {
        usbMscEnable();
        ui_screen_settings_set_usb_enabled(true);
    }
}

// LVGL button callbacks - File Browser screen
static void onFileBrowserBackPressed() {
    lvgl_switch_to_screen(SCREEN_SETTINGS);
}

static void onFileBrowserFileSelected(const char* filename) {
    Serial.printf("File selected: %s\n", filename);
    // TODO: Handle file selection (e.g., for OTA updates)
}

// LVGL button callbacks - WiFi screen
static void onWifiBackPressed() {
    lvgl_switch_to_screen(SCREEN_SETTINGS);
}

static void onWifiSavePressed() {
    Serial.println("WiFi settings saved");
    // Settings are saved internally by the WiFi screen
}

// LVGL OTA popup callbacks
static void onOtaInstallPressed() {
    Serial.println("OTA Install pressed from LVGL popup");
    // Install is handled by ui_ota_popup internally via otaStartInstall()
}

static void onOtaAbortPressed() {
    Serial.println("OTA Abort pressed from LVGL popup");
    // Abort is handled by ui_ota_popup internally
}

static void onOtaDismissPressed() {
    Serial.println("OTA Dismiss pressed from LVGL popup");
    // Dismiss is handled by ui_ota_popup internally
}

// LVGL screen switching helper
static void lvgl_switch_to_screen(ScreenType screen) {
    currentScreen = screen;
    
    switch (screen) {
        case SCREEN_MAIN:
            lv_screen_load(ui_screen_main_get());
            break;
        case SCREEN_SETTINGS:
            ui_screen_settings_update();  // Refresh diagnostics
            lv_screen_load(ui_screen_settings_get());
            break;
        case SCREEN_FILE_BROWSER:
            ui_screen_filebrowser_refresh();  // Refresh file list
            lv_screen_load(ui_screen_filebrowser_get());
            break;
        case SCREEN_WIFI:
            ui_screen_wifi_update();  // Refresh WiFi state
            lv_screen_load(ui_screen_wifi_get());
            break;
    }
    
    Serial.printf("LVGL screen switched to: %d\n", screen);
}
#endif

bool displayInit() {
    TFT_eSPI& tft = getTft();

    // Initialize TFT
    tft.init();
    tft.setRotation(1);  // Landscape
    tft.invertDisplay(true);  // ILI9341V requires color inversion
    tft.fillScreen(COLOR_BACKGROUND);

    // Configure backlight
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    Serial.println("ILI9341V display initialized (320x240, Rotation 1)");

    // Initialize touch controller
    if (!touchInit()) {
        Serial.println("WARNING: Touch controller not available");
    }

#if USE_LVGL_UI
    // Initialize LVGL
    lvgl_init();
    ui_theme_init();
    
    // Create all LVGL screens
    ui_screen_main_create();
    ui_screen_settings_create();
    ui_screen_filebrowser_create();
    ui_screen_wifi_create();
    
    // Initialize WiFi screen state from NVS
    ui_screen_wifi_init();
    
    // Set up Main screen callbacks
    ui_screen_main_set_gear_callback(onGearButtonPressed);
    ui_screen_main_set_mode_callback(onModeButtonPressed);
    ui_screen_main_set_rpm_up_callback(onRpmUpPressed);
    ui_screen_main_set_rpm_down_callback(onRpmDownPressed);
    
    // Set up Settings screen callbacks
    ui_screen_settings_set_back_callback(onSettingsBackPressed);
    ui_screen_settings_set_sd_callback(onSettingsSdPressed);
    ui_screen_settings_set_wifi_callback(onSettingsWifiPressed);
    ui_screen_settings_set_usb_callback(onSettingsUsbPressed);
    
    // Set up File Browser screen callbacks
    ui_screen_filebrowser_set_back_callback(onFileBrowserBackPressed);
    ui_screen_filebrowser_set_file_callback(onFileBrowserFileSelected);
    
    // Set up WiFi screen callbacks
    ui_screen_wifi_set_back_callback(onWifiBackPressed);
    ui_screen_wifi_set_save_callback(onWifiSavePressed);
    
    // Create and set up OTA popup
    ui_ota_popup_create();
    ui_ota_popup_set_callbacks(onOtaInstallPressed, onOtaAbortPressed, onOtaDismissPressed);
    
    // Load main screen
    lv_screen_load(ui_screen_main_get());
    
    // Set initial WiFi status
    lastWifiConnected = isWifiConnected();
    ui_screen_main_set_wifi_status(lastWifiConnected);
    Serial.printf("Initial WiFi status: %s\n", lastWifiConnected ? "connected" : "disconnected");
    
    Serial.println("LVGL UI initialized with all screens");
#else
    // Initialize WiFi settings from NVS and apply (legacy)
    screenWifiInit();
    
    // Initialize main screen state (legacy)
    screenMainInit();

    // Draw initial screen (legacy)
    screenMainDraw();
#endif

    Serial.println("Touch controller uses Wire (I2C0), RPM slave uses SPI");

    return true;
}

// =============================================================================
// Public API - Called from main.cpp
// =============================================================================

void displayUpdateRpm(uint16_t rpm) {
#if USE_LVGL_UI
    ui_screen_main_set_rpm(rpm, true);
    // Update sync status
    bool synced = (spiSlaveGetMasterMode() == spiSlaveGetRequestedMode());
    if (spiSlaveGetMasterMode() == MODE_MANUAL) {
        synced = synced && (spiSlaveGetLastRpm() == spiSlaveGetRequestedRpm());
    }
    ui_screen_main_set_status(true, synced);
#else
    screenMainUpdateRpm(rpm, true);
#endif
}

void displaySetConnected(bool connected) {
#if USE_LVGL_UI
    if (connected) {
        bool synced = (spiSlaveGetMasterMode() == spiSlaveGetRequestedMode());
        ui_screen_main_set_status(true, synced);
    } else {
        ui_screen_main_set_rpm(0, false);
        ui_screen_main_set_status(false, false);
    }
#else
    screenMainSetConnected(connected);
#endif
}

bool displayGetTouch(int16_t* x, int16_t* y) {
    return touchGetPoint(x, y);
}

ScreenType displayGetScreen() {
    return currentScreen;
}

// =============================================================================
// Main Display Loop
// =============================================================================

// Blink timer for disconnected state
static unsigned long lastBlinkTime = 0;

void displayLoop() {
    // Check for USB MSC eject from host (clear flag regardless of screen)
    // This prevents the flag from accumulating if user is on a different screen
    usbMscCheckEjected();

#if USE_LVGL_UI
    // LVGL handles touch input internally via the driver callbacks
    // Just call the task handler to process everything
    lvgl_task_handler();
    
    // Handle blink animation for disconnected state (main screen only)
    if (currentScreen == SCREEN_MAIN && !spiSlaveIsConnected() && millis() - lastBlinkTime >= 500) {
        lastBlinkTime = millis();
        ui_screen_main_update_blink();
    }
    
    // Update menu bar auto-hide timer (main screen only)
    if (currentScreen == SCREEN_MAIN) {
        ui_screen_main_update_menu_bar();
    }
    
    // Periodic updates for current screen
    static unsigned long lastPeriodicUpdate = 0;
    if (millis() - lastPeriodicUpdate > 1000) {
        lastPeriodicUpdate = millis();
        
        // Update WiFi status icon on main screen
        bool currentWifiConnected = isWifiConnected();
        if (currentWifiConnected != lastWifiConnected) {
            lastWifiConnected = currentWifiConnected;
            ui_screen_main_set_wifi_status(currentWifiConnected);
            Serial.printf("WiFi status changed: %s\n", currentWifiConnected ? "connected" : "disconnected");
        }
        
        switch (currentScreen) {
            case SCREEN_SETTINGS:
                ui_screen_settings_update();
                break;
            case SCREEN_FILE_BROWSER:
                ui_screen_filebrowser_update();
                break;
            default:
                break;
        }
    }
    
    // Update OTA popup state (checks for new packages, progress, etc.)
    ui_ota_popup_update();

#else  // Legacy UI
    
    // Periodic WiFi status check (not during keyboard input)
    if (!screenWifiKeyboardVisible() && millis() - lastWifiStatusCheck > 1000) {
        lastWifiStatusCheck = millis();
        bool currentWifiConnected = isWifiConnected();
        if (currentWifiConnected != lastWifiConnected) {
            lastWifiConnected = currentWifiConnected;
            drawWifiStatusIndicator();
        }
    }

    // Get touch input
    int16_t touchX, touchY;
    bool currentTouch = touchGetPoint(&touchX, &touchY);
    
    // Preserve last valid touch position for release events
    // When touch is released, we need to know WHERE the finger was lifted
    if (currentTouch) {
        lastTouchX = touchX;
        lastTouchY = touchY;
    } else {
        // Use last known position for release event
        touchX = lastTouchX;
        touchY = lastTouchY;
    }

    // Dispatch to current screen
    switch (currentScreen) {
        case SCREEN_MAIN:
            screenMainUpdate();
            if (currentTouch != lastTouchState || currentTouch) {
                screenMainHandleTouch(touchX, touchY, currentTouch);
            }
            break;

        case SCREEN_SETTINGS:
            screenSettingsUpdate();
            if (currentTouch != lastTouchState || currentTouch) {
                screenSettingsHandleTouch(touchX, touchY, currentTouch);
            }
            break;

        case SCREEN_FILE_BROWSER:
            screenFileBrowserUpdate();
            if (currentTouch != lastTouchState || currentTouch) {
                screenFileBrowserHandleTouch(touchX, touchY, currentTouch);
            }
            break;

        case SCREEN_WIFI:
            screenWifiUpdate();
            // WiFi screen handles its own lastTouchState for keyboard
            screenWifiHandleTouch(touchX, touchY, currentTouch);
            break;
    }

    lastTouchState = currentTouch;
#endif
}

// =============================================================================
// Thread-Safe UI Command Sending
// =============================================================================

bool displaySendModeRequest(uint8_t mode) {
    if (queueDisplayToSpi == nullptr) {
        // Queue not initialized - fall back to direct call
        spiSlaveSetRequestedMode(mode);
        return true;
    }

    DisplayToSpiMsg msg;
    msg.requestedMode = mode;
    msg.requestedRpm = spiSlaveGetRequestedRpm();

    return xQueueSend(queueDisplayToSpi, &msg, pdMS_TO_TICKS(10)) == pdTRUE;
}

bool displaySendRpmRequest(uint16_t rpm) {
    if (queueDisplayToSpi == nullptr) {
        // Queue not initialized - fall back to direct call
        spiSlaveSetRequestedRpm(rpm);
        return true;
    }

    DisplayToSpiMsg msg;
    msg.requestedMode = spiSlaveGetRequestedMode();
    msg.requestedRpm = rpm;

    return xQueueSend(queueDisplayToSpi, &msg, pdMS_TO_TICKS(10)) == pdTRUE;
}

bool displaySendRequest(uint8_t mode, uint16_t rpm) {
    if (queueDisplayToSpi == nullptr) {
        // Queue not initialized - fall back to direct call
        spiSlaveSetRequest(mode, rpm);
        return true;
    }

    DisplayToSpiMsg msg;
    msg.requestedMode = mode;
    msg.requestedRpm = rpm;

    return xQueueSend(queueDisplayToSpi, &msg, pdMS_TO_TICKS(10)) == pdTRUE;
}
