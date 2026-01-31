#include "slave/ota_popup.h"
#include "slave/ota_handler.h"
#include "slave/spi_ota.h"
#include "shared/ota_protocol.h"
#include "display_common.h"
#include <Arduino.h>

// =============================================================================
// Local State
// =============================================================================

static OtaPopupState popupState = OTA_POPUP_HIDDEN;
static uint8_t installProgress = 0;
static char popupErrorMessage[64] = "";
static bool installButtonPressed = false;   // Also used for verify button
static bool abortButtonPressed = false;
static bool okButtonPressed = false;

// =============================================================================
// Drawing Helpers
// =============================================================================

static void drawDimmedBackground() {
    TFT_eSPI& tft = getTft();
    
    // Draw semi-transparent overlay by drawing a pattern of dark pixels
    // This creates a dimming effect over the main screen
    for (int y = 0; y < SCREEN_HEIGHT; y += 2) {
        for (int x = (y / 2) % 2; x < SCREEN_WIDTH; x += 2) {
            tft.drawPixel(x, y, COLOR_BACKGROUND);
        }
    }
}

static void drawPopupFrame(const char* title) {
    TFT_eSPI& tft = getTft();
    
    // Draw popup background
    tft.fillRoundRect(OTA_POPUP_X, OTA_POPUP_Y, OTA_POPUP_W, OTA_POPUP_HEIGHT, 8, COLOR_BTN_NORMAL);
    tft.drawRoundRect(OTA_POPUP_X, OTA_POPUP_Y, OTA_POPUP_W, OTA_POPUP_HEIGHT, 8, COLOR_BTN_TEXT);
    
    // Draw title
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_RPM_TEXT, COLOR_BTN_NORMAL);
    tft.setTextSize(2);
    tft.drawString(title, SCREEN_WIDTH / 2, OTA_POPUP_Y + 12);
}

static void drawInstallButton(bool pressed) {
    TFT_eSPI& tft = getTft();
    uint16_t btnColor = pressed ? COLOR_CONNECTED : COLOR_BTN_PRESSED;
    
    tft.fillRoundRect(OTA_POPUP_INSTALL_X, OTA_POPUP_BTN_Y, 
                      OTA_POPUP_BTN_W, OTA_POPUP_BTN_H, 6, btnColor);
    tft.drawRoundRect(OTA_POPUP_INSTALL_X, OTA_POPUP_BTN_Y, 
                      OTA_POPUP_BTN_W, OTA_POPUP_BTN_H, 6, COLOR_BTN_TEXT);
    
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_RPM_TEXT, btnColor);
    tft.setTextSize(2);
    tft.drawString("INSTALL", OTA_POPUP_INSTALL_X + OTA_POPUP_BTN_W / 2, 
                   OTA_POPUP_BTN_Y + OTA_POPUP_BTN_H / 2);
}

static void drawVerifyButton(bool pressed) {
    TFT_eSPI& tft = getTft();
    uint16_t btnColor = pressed ? COLOR_CONNECTED : COLOR_BTN_PRESSED;
    
    tft.fillRoundRect(OTA_POPUP_INSTALL_X, OTA_POPUP_BTN_Y, 
                      OTA_POPUP_BTN_W, OTA_POPUP_BTN_H, 6, btnColor);
    tft.drawRoundRect(OTA_POPUP_INSTALL_X, OTA_POPUP_BTN_Y, 
                      OTA_POPUP_BTN_W, OTA_POPUP_BTN_H, 6, COLOR_BTN_TEXT);
    
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_RPM_TEXT, btnColor);
    tft.setTextSize(2);
    tft.drawString("VERIFY", OTA_POPUP_INSTALL_X + OTA_POPUP_BTN_W / 2, 
                   OTA_POPUP_BTN_Y + OTA_POPUP_BTN_H / 2);
}

static void drawAbortButton(bool pressed, bool disabled) {
    TFT_eSPI& tft = getTft();
    uint16_t btnColor;
    uint16_t textColor;
    
    if (disabled) {
        btnColor = COLOR_BACKGROUND;  // Dimmed
        textColor = 0x7BEF;           // Gray text
    } else {
        btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
        textColor = COLOR_BTN_TEXT;
    }
    
    tft.fillRoundRect(OTA_POPUP_LATER_X, OTA_POPUP_BTN_Y, 
                      OTA_POPUP_BTN_W, OTA_POPUP_BTN_H, 6, btnColor);
    tft.drawRoundRect(OTA_POPUP_LATER_X, OTA_POPUP_BTN_Y, 
                      OTA_POPUP_BTN_W, OTA_POPUP_BTN_H, 6, disabled ? 0x7BEF : COLOR_BTN_TEXT);
    
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(textColor, btnColor);
    tft.setTextSize(2);
    tft.drawString("ABORT", OTA_POPUP_LATER_X + OTA_POPUP_BTN_W / 2, 
                   OTA_POPUP_BTN_Y + OTA_POPUP_BTN_H / 2);
}

static void drawOkButton(bool pressed) {
    TFT_eSPI& tft = getTft();
    
    // Center the OK button
    int16_t btnX = (SCREEN_WIDTH - OTA_POPUP_BTN_W) / 2;
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_CONNECTED;
    
    tft.fillRoundRect(btnX, OTA_POPUP_BTN_Y, 
                      OTA_POPUP_BTN_W, OTA_POPUP_BTN_H, 6, btnColor);
    tft.drawRoundRect(btnX, OTA_POPUP_BTN_Y, 
                      OTA_POPUP_BTN_W, OTA_POPUP_BTN_H, 6, COLOR_BTN_TEXT);
    
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_RPM_TEXT, btnColor);
    tft.setTextSize(2);
    tft.drawString("OK", btnX + OTA_POPUP_BTN_W / 2, 
                   OTA_POPUP_BTN_Y + OTA_POPUP_BTN_H / 2);
}

static void drawProgressBar(uint8_t progress) {
    TFT_eSPI& tft = getTft();
    
    int16_t barX = OTA_POPUP_X + 20;
    int16_t barY = OTA_POPUP_Y + 70;
    int16_t barW = OTA_POPUP_W - 40;
    int16_t barH = 25;
    
    // Background
    tft.fillRect(barX, barY, barW, barH, COLOR_BACKGROUND);
    tft.drawRect(barX, barY, barW, barH, COLOR_BTN_TEXT);
    
    // Fill
    int16_t fillW = (barW - 4) * progress / 100;
    if (fillW > 0) {
        tft.fillRect(barX + 2, barY + 2, fillW, barH - 4, COLOR_CONNECTED);
    }
    
    // Percentage text
    char pctStr[8];
    snprintf(pctStr, sizeof(pctStr), "%d%%", progress);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_RPM_TEXT, (progress > 50) ? COLOR_CONNECTED : COLOR_BACKGROUND);
    tft.setTextSize(2);
    tft.drawString(pctStr, SCREEN_WIDTH / 2, barY + barH / 2);
}

// =============================================================================
// Public Functions
// =============================================================================

void otaPopupShow() {
    if (popupState == OTA_POPUP_HIDDEN) {
        popupState = OTA_POPUP_CONFIRM;
        installProgress = 0;
        popupErrorMessage[0] = '\0';
        installButtonPressed = false;
        abortButtonPressed = false;
        okButtonPressed = false;
        spiOtaClearVerifyState();  // Clear any previous verification state
        Serial.println("[OTA Popup] Showing update popup");
    }
}

void otaPopupHide() {
    popupState = OTA_POPUP_HIDDEN;
    Serial.println("[OTA Popup] Hidden");
}

bool otaPopupIsVisible() {
    return popupState != OTA_POPUP_HIDDEN;
}

OtaPopupState otaPopupGetState() {
    return popupState;
}

void otaPopupDraw() {
    if (popupState == OTA_POPUP_HIDDEN) return;
    
    TFT_eSPI& tft = getTft();
    
    // Draw dimmed background
    drawDimmedBackground();
    
    switch (popupState) {
        case OTA_POPUP_CONFIRM: {
            // Initial state: show VERIFY/INSTALL and ABORT buttons
            drawPopupFrame("FIRMWARE UPDATE");
            
            // Get package info
            const OtaPackageInfo* info = otaGetPackageInfo();
            
            // Version
            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(1);
            tft.setTextColor(COLOR_BTN_TEXT, COLOR_BTN_NORMAL);
            
            char versionStr[48];
            if (info && info->valid) {
                snprintf(versionStr, sizeof(versionStr), "Version: %s", info->version);
            } else {
                snprintf(versionStr, sizeof(versionStr), "New update available");
            }
            tft.drawString(versionStr, SCREEN_WIDTH / 2, OTA_POPUP_Y + 45);
            
            // Firmware sizes
            if (info && info->valid) {
                char sizeStr[64];
                float displayMB = info->displaySize / 1048576.0f;
                float controllerMB = info->controllerSize / 1048576.0f;
                snprintf(sizeStr, sizeof(sizeStr), "Display: %.2f MB  Controller: %.2f MB", 
                         displayMB, controllerMB);
                tft.drawString(sizeStr, SCREEN_WIDTH / 2, OTA_POPUP_Y + 65);
            }
            
#if OTA_ENABLE_TEST_MODE
            // Buttons: VERIFY (left) and ABORT (right, enabled)
            drawVerifyButton(installButtonPressed);
#else
            // Test mode disabled: show INSTALL button directly
            drawInstallButton(installButtonPressed);
#endif
            drawAbortButton(abortButtonPressed, false);  // Not disabled
            break;
        }
        
        case OTA_POPUP_VERIFYING: {
            // Running SPI verification test
            drawPopupFrame("VERIFYING SPI");
            
            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(1);
            tft.setTextColor(COLOR_WARNING, COLOR_BTN_NORMAL);
            tft.drawString("Testing connection...", SCREEN_WIDTH / 2, OTA_POPUP_Y + 60);
            
            // ABORT button disabled during verification
            drawAbortButton(false, true);  // Disabled
            break;
        }
        
        case OTA_POPUP_VERIFIED: {
            // Verification passed: show INSTALL and ABORT buttons
            drawPopupFrame("VERIFIED - READY");
            
            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(1);
            tft.setTextColor(COLOR_CONNECTED, COLOR_BTN_NORMAL);
            tft.drawString("SPI connection verified!", SCREEN_WIDTH / 2, OTA_POPUP_Y + 60);
            
            // Buttons: INSTALL (left) and ABORT (right, enabled)
            drawInstallButton(installButtonPressed);
            drawAbortButton(abortButtonPressed, false);  // Not disabled
            break;
        }
        
        case OTA_POPUP_INSTALLING:
            drawPopupFrame("UPDATING DISPLAY");
            drawProgressBar(installProgress);
            
            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(1);
            tft.setTextColor(COLOR_WARNING, COLOR_BTN_NORMAL);
            tft.drawString("Do not power off!", SCREEN_WIDTH / 2, OTA_POPUP_Y + OTA_POPUP_HEIGHT - 25);
            // No buttons during installation - progress bar uses that space
            break;
            
        case OTA_POPUP_CONTROLLER:
            drawPopupFrame("UPDATING CONTROLLER");
            drawProgressBar(installProgress);
            
            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(1);
            tft.setTextColor(COLOR_WARNING, COLOR_BTN_NORMAL);
            tft.drawString("Do not power off!", SCREEN_WIDTH / 2, OTA_POPUP_Y + OTA_POPUP_HEIGHT - 25);
            // No buttons during installation - progress bar uses that space
            break;
            
        case OTA_POPUP_COMPLETE:
            drawPopupFrame("UPDATE COMPLETE");
            
            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(1);
            tft.setTextColor(COLOR_CONNECTED, COLOR_BTN_NORMAL);
            tft.drawString("Firmware updated successfully!", SCREEN_WIDTH / 2, OTA_POPUP_Y + 60);
            
            drawOkButton(okButtonPressed);
            break;
            
        case OTA_POPUP_ERROR:
            drawPopupFrame("UPDATE FAILED");
            
            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(1);
            tft.setTextColor(COLOR_DISCONNECTED, COLOR_BTN_NORMAL);
            tft.drawString(popupErrorMessage, SCREEN_WIDTH / 2, OTA_POPUP_Y + 60);
            
            drawOkButton(okButtonPressed);
            break;
            
        default:
            break;
    }
}

bool otaPopupHandleTouch(int16_t x, int16_t y, bool pressed) {
    if (popupState == OTA_POPUP_HIDDEN) return false;
    
    // Debug: log all touches when popup is visible
    Serial.printf("[OTA Popup] Touch: x=%d, y=%d, pressed=%d, state=%d\n", 
                  x, y, pressed, popupState);
    
    // During installation or verification, ignore all touches (ABORT is disabled)
    if (popupState == OTA_POPUP_INSTALLING || popupState == OTA_POPUP_CONTROLLER ||
        popupState == OTA_POPUP_VERIFYING) {
        return true;  // Consume touch but do nothing
    }
    
    if (popupState == OTA_POPUP_CONFIRM) {
#if OTA_ENABLE_TEST_MODE
        // VERIFY button (left side)
        if (pointInRect(x, y, OTA_POPUP_INSTALL_X, OTA_POPUP_BTN_Y, 
                        OTA_POPUP_BTN_W, OTA_POPUP_BTN_H)) {
            if (pressed && !installButtonPressed) {
                installButtonPressed = true;
                drawVerifyButton(true);
            } else if (!pressed && installButtonPressed) {
                installButtonPressed = false;
                Serial.println("[OTA Popup] Verify button pressed");
                popupState = OTA_POPUP_VERIFYING;
                otaPopupDraw();
                
                // Request verification from master via SPI
                spiOtaRequestVerify();
            }
            return true;
        }
#else
        // INSTALL button (left side) - test mode disabled, go straight to install
        if (pointInRect(x, y, OTA_POPUP_INSTALL_X, OTA_POPUP_BTN_Y, 
                        OTA_POPUP_BTN_W, OTA_POPUP_BTN_H)) {
            if (pressed && !installButtonPressed) {
                installButtonPressed = true;
                drawInstallButton(true);
            } else if (!pressed && installButtonPressed) {
                installButtonPressed = false;
                Serial.println("[OTA Popup] Install button pressed (no verify)");
                popupState = OTA_POPUP_INSTALLING;
                otaPopupDraw();
                
                // Start the update directly
                if (!otaStartInstall()) {
                    // If start fails, error will be shown
                    otaPopupSetError(otaGetErrorMessage());
                }
            }
            return true;
        }
#endif
        
        // ABORT button (right side, enabled in CONFIRM state)
        if (pointInRect(x, y, OTA_POPUP_LATER_X, OTA_POPUP_BTN_Y, 
                        OTA_POPUP_BTN_W, OTA_POPUP_BTN_H)) {
            if (pressed && !abortButtonPressed) {
                abortButtonPressed = true;
                drawAbortButton(true, false);
            } else if (!pressed && abortButtonPressed) {
                abortButtonPressed = false;
                Serial.println("[OTA Popup] Abort button pressed");
                spiOtaExitMode();  // Exit OTA mode - master will resume normal SPI
                otaDismissUpdate();
                otaPopupHide();
            }
            return true;
        }
    }
    
    if (popupState == OTA_POPUP_VERIFIED) {
        // INSTALL button (left side, after verification passed)
        if (pointInRect(x, y, OTA_POPUP_INSTALL_X, OTA_POPUP_BTN_Y, 
                        OTA_POPUP_BTN_W, OTA_POPUP_BTN_H)) {
            if (pressed && !installButtonPressed) {
                installButtonPressed = true;
                drawInstallButton(true);
            } else if (!pressed && installButtonPressed) {
                installButtonPressed = false;
                Serial.println("[OTA Popup] Install button pressed (after verify)");
                popupState = OTA_POPUP_INSTALLING;
                otaPopupDraw();
                
                // Start the update
                if (!otaStartInstall()) {
                    // If start fails, error will be shown
                    otaPopupSetError(otaGetErrorMessage());
                }
            }
            return true;
        }
        
        // ABORT button (right side, enabled in VERIFIED state)
        if (pointInRect(x, y, OTA_POPUP_LATER_X, OTA_POPUP_BTN_Y, 
                        OTA_POPUP_BTN_W, OTA_POPUP_BTN_H)) {
            if (pressed && !abortButtonPressed) {
                abortButtonPressed = true;
                drawAbortButton(true, false);
            } else if (!pressed && abortButtonPressed) {
                abortButtonPressed = false;
                Serial.println("[OTA Popup] Abort button pressed (after verify)");
                spiOtaExitMode();  // Exit OTA mode - master will resume normal SPI
                otaDismissUpdate();
                otaPopupHide();
            }
            return true;
        }
    }
    
    if (popupState == OTA_POPUP_COMPLETE || popupState == OTA_POPUP_ERROR) {
        // OK button (centered)
        int16_t btnX = (SCREEN_WIDTH - OTA_POPUP_BTN_W) / 2;
        if (pointInRect(x, y, btnX, OTA_POPUP_BTN_Y, OTA_POPUP_BTN_W, OTA_POPUP_BTN_H)) {
            if (pressed && !okButtonPressed) {
                okButtonPressed = true;
                drawOkButton(true);
            } else if (!pressed && okButtonPressed) {
                okButtonPressed = false;
                Serial.println("[OTA Popup] OK button pressed");
                
                if (popupState == OTA_POPUP_ERROR) {
                    spiOtaExitMode();  // Exit OTA mode on error dismiss
                    otaDismissUpdate();
                }
                otaPopupHide();
            }
            return true;
        }
    }
    
    // Consume all touches when popup is visible (block main screen)
    return true;
}

void otaPopupUpdate() {
    if (popupState == OTA_POPUP_HIDDEN) return;
    
    // Check for verification result from master
    if (popupState == OTA_POPUP_VERIFYING) {
        uint8_t verifyState = spiOtaGetVerifyState();
        if (verifyState == 2) {
            // Verification passed
            popupState = OTA_POPUP_VERIFIED;
            installButtonPressed = false;
            abortButtonPressed = false;
            otaPopupDraw();
            Serial.println("[OTA Popup] Verification passed, showing INSTALL button");
        } else if (verifyState == 3) {
            // Verification failed
            otaPopupSetError("SPI verification failed");
            Serial.println("[OTA Popup] Verification failed");
        }
        // verifyState == 1 means still waiting
    }
    
    // Check OTA handler state
    OtaState otaState = otaGetState();
    
    switch (otaState) {
        case OTA_STATE_PACKAGE_READY:
            if (popupState == OTA_POPUP_HIDDEN) {
                otaPopupShow();
            }
            break;
            
        case OTA_STATE_PENDING_CONTROLLER:
            if (popupState != OTA_POPUP_CONTROLLER) {
                popupState = OTA_POPUP_CONTROLLER;
                installProgress = 0;
                otaPopupDraw();
                
                // Start controller update
                otaStartControllerUpdate();
            }
            break;
            
        case OTA_STATE_COMPLETE:
            if (popupState != OTA_POPUP_COMPLETE) {
                popupState = OTA_POPUP_COMPLETE;
                otaPopupDraw();
            }
            break;
            
        case OTA_STATE_ERROR:
            if (popupState != OTA_POPUP_ERROR) {
                otaPopupSetError(otaGetErrorMessage());
            }
            break;
            
        default:
            break;
    }
    
    // Update progress display if installing
    if (popupState == OTA_POPUP_INSTALLING || popupState == OTA_POPUP_CONTROLLER) {
        uint8_t progress = otaGetProgress();
        if (progress != installProgress) {
            installProgress = progress;
            drawProgressBar(installProgress);
        }
    }
}

void otaPopupSetProgress(uint8_t progress) {
    installProgress = progress;
    if (popupState == OTA_POPUP_INSTALLING || popupState == OTA_POPUP_CONTROLLER) {
        drawProgressBar(installProgress);
    }
}

void otaPopupSetError(const char* message) {
    popupState = OTA_POPUP_ERROR;
    strncpy(popupErrorMessage, message, sizeof(popupErrorMessage) - 1);
    popupErrorMessage[sizeof(popupErrorMessage) - 1] = '\0';
    otaPopupDraw();
}

void otaPopupSetComplete() {
    popupState = OTA_POPUP_COMPLETE;
    otaPopupDraw();
}
