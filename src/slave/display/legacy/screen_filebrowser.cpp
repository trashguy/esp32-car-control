#include "screen_filebrowser.h"
#include "../display_common.h"
#include "../../sd_card.h"
#include "../../usb_msc.h"
#include <Arduino.h>
#include <SD_MMC.h>

// =============================================================================
// Local State
// =============================================================================

static char* fileList[MAX_FILES];
static int fileCount = 0;
static int scrollOffset = 0;
static TouchState touchState = {false, -1, false};

// Button state
static bool arrowButtonPressed = false;

// USB lock state - tracks if we're showing the locked overlay
static bool usbLockedDisplayed = false;

// =============================================================================
// Helper Functions
// =============================================================================

// Draw USB locked overlay - shows when SD card is mounted via USB
static void drawUsbLockedOverlay() {
    TFT_eSPI& tft = getTft();

    const int16_t boxMargin = 8;
    const int16_t boxX = boxMargin;
    const int16_t boxY = FILE_LIST_Y_START - 4;
    const int16_t boxW = SCREEN_WIDTH - (boxMargin * 2);
    const int16_t boxH = FILE_LIST_Y_END - FILE_LIST_Y_START + 8;

    // Draw dark overlay box
    tft.fillRect(boxX, boxY, boxW, boxH, COLOR_BACKGROUND);
    tft.drawRect(boxX, boxY, boxW, boxH, COLOR_DISCONNECTED);

    // Draw USB icon (simple representation)
    int16_t centerX = SCREEN_WIDTH / 2;
    int16_t centerY = (FILE_LIST_Y_START + FILE_LIST_Y_END) / 2 - 20;

    // USB connector shape
    tft.fillRoundRect(centerX - 15, centerY - 10, 30, 20, 3, COLOR_LABEL);
    tft.fillRect(centerX - 10, centerY + 10, 20, 8, COLOR_LABEL);
    tft.fillRect(centerX - 6, centerY - 4, 4, 8, COLOR_BACKGROUND);
    tft.fillRect(centerX + 2, centerY - 4, 4, 8, COLOR_BACKGROUND);

    // Lock message
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_DISCONNECTED, COLOR_BACKGROUND);
    tft.drawString("SD LOCKED", centerX, centerY + 45);

    tft.setTextSize(1);
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.drawString("Mounted via USB", centerX, centerY + 65);
    tft.drawString("Eject from PC to unlock", centerX, centerY + 80);
}

static void clearFileList() {
    for (int i = 0; i < fileCount; i++) {
        if (fileList[i]) {
            free(fileList[i]);
            fileList[i] = nullptr;
        }
    }
    fileCount = 0;
    scrollOffset = 0;
}

static void loadFileList() {
    clearFileList();

    if (!sdCardPresent()) return;

    fs::File root = SD_MMC.open("/");
    if (!root || !root.isDirectory()) return;

    fs::File file = root.openNextFile();
    while (file && fileCount < MAX_FILES) {
        const char* name = file.name();
        size_t len = strlen(name);
        fileList[fileCount] = (char*)malloc(len + 2);
        if (fileList[fileCount]) {
            if (file.isDirectory()) {
                snprintf(fileList[fileCount], len + 2, "/%s", name);
            } else {
                strncpy(fileList[fileCount], name, len + 1);
            }
            fileCount++;
        }
        file = root.openNextFile();
    }
    root.close();
}

// =============================================================================
// Drawing Functions
// =============================================================================

static void drawArrowButton(bool pressed) {
    TFT_eSPI& tft = getTft();
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t cx = ARROW_BTN_X + ARROW_BTN_SIZE / 2;
    int16_t cy = ARROW_BTN_Y + ARROW_BTN_SIZE / 2;

    tft.fillRoundRect(ARROW_BTN_X, ARROW_BTN_Y, ARROW_BTN_SIZE, ARROW_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(ARROW_BTN_X, ARROW_BTN_Y, ARROW_BTN_SIZE, ARROW_BTN_SIZE, 6, COLOR_BTN_TEXT);

    drawBackArrowIcon(cx, cy, ARROW_BTN_SIZE - 8, COLOR_BTN_TEXT);
}

static void drawFileListArea() {
    TFT_eSPI& tft = getTft();

    const int16_t boxMargin = 8;
    const int16_t boxX = boxMargin;
    const int16_t boxY = FILE_LIST_Y_START - 4;
    const int16_t boxW = SCREEN_WIDTH - (boxMargin * 2);
    const int16_t boxH = FILE_LIST_Y_END - FILE_LIST_Y_START + 8;

    // Clear and draw box
    tft.fillRect(boxX, boxY, boxW, boxH, COLOR_BACKGROUND);
    tft.drawRect(boxX, boxY, boxW, boxH, COLOR_BTN_TEXT);

    if (!sdCardPresent()) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_DISCONNECTED, COLOR_BACKGROUND);
        tft.drawString("No SD Card", SCREEN_WIDTH / 2, (FILE_LIST_Y_START + FILE_LIST_Y_END) / 2);
        return;
    }

    if (fileCount == 0) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(2);
        tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
        tft.drawString("Empty", SCREEN_WIDTH / 2, (FILE_LIST_Y_START + FILE_LIST_Y_END) / 2);
        return;
    }

    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);

    int16_t y = FILE_LIST_Y_START;
    for (int i = scrollOffset; i < fileCount && i < scrollOffset + MAX_VISIBLE_FILES; i++) {
        if (fileList[i]) {
            // Directory entries start with /
            if (fileList[i][0] == '/') {
                tft.setTextColor(COLOR_CONNECTED, COLOR_BACKGROUND);
            } else {
                tft.setTextColor(COLOR_RPM_TEXT, COLOR_BACKGROUND);
            }
            tft.drawString(fileList[i], boxX + 6, y);
        }
        y += FILE_LINE_HEIGHT;
    }

    // Draw scroll indicator if needed
    if (fileCount > MAX_VISIBLE_FILES) {
        int16_t scrollBarX = boxX + boxW - 8;
        int16_t scrollBarHeight = boxH - 8;
        int16_t thumbHeight = (MAX_VISIBLE_FILES * scrollBarHeight) / fileCount;
        if (thumbHeight < 10) thumbHeight = 10;
        int16_t thumbY = boxY + 4 + (scrollOffset * (scrollBarHeight - thumbHeight)) / (fileCount - MAX_VISIBLE_FILES);

        tft.fillRect(scrollBarX, boxY + 4, 4, scrollBarHeight, COLOR_BTN_NORMAL);
        tft.fillRect(scrollBarX, thumbY, 4, thumbHeight, COLOR_BTN_TEXT);
    }
}

// =============================================================================
// Public Interface
// =============================================================================

void screenFileBrowserDraw() {
    TFT_eSPI& tft = getTft();
    tft.fillScreen(COLOR_BACKGROUND);

    // Title
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.drawString("FILE BROWSER", SCREEN_WIDTH / 2, 10);

    // Horizontal line
    tft.drawLine(20, 35, SCREEN_WIDTH - 20, 35, COLOR_LABEL);

    // Check if SD card is mounted via USB
    if (usbMscMounted()) {
        usbLockedDisplayed = true;
        drawUsbLockedOverlay();
    } else {
        usbLockedDisplayed = false;
        // Load and draw file list
        loadFileList();
        drawFileListArea();
    }

    // Draw back arrow button
    drawArrowButton(false);

    drawWifiStatusIndicator();
}

void screenFileBrowserHandleTouch(int16_t x, int16_t y, bool pressed) {
    if (pressed) {
        // Check back arrow button (always allow going back)
        if (pointInRect(x, y, ARROW_BTN_X, ARROW_BTN_Y, ARROW_BTN_SIZE, ARROW_BTN_SIZE) &&
            !arrowButtonPressed) {
            arrowButtonPressed = true;
            drawArrowButton(true);
            Serial.println("ARROW button pressed");
        }

        // Block file list interactions when USB mounted
        if (usbLockedDisplayed) {
            return;
        }

        // Handle scrolling in file list area
        if (y >= FILE_LIST_Y_START && y <= FILE_LIST_Y_END) {
            if (!touchState.isDragging) {
                touchState.isDragging = true;
                touchState.lastTouchY = y;
            } else if (touchState.lastTouchY >= 0) {
                int16_t delta = touchState.lastTouchY - y;
                if (abs(delta) > 5) {
                    int maxScroll = fileCount - MAX_VISIBLE_FILES;
                    if (maxScroll < 0) maxScroll = 0;
                    scrollOffset += (delta > 0) ? 1 : -1;
                    if (scrollOffset < 0) scrollOffset = 0;
                    if (scrollOffset > maxScroll) scrollOffset = maxScroll;
                    drawFileListArea();
                    touchState.lastTouchY = y;
                }
            }
        }
    } else {
        // Handle releases
        if (arrowButtonPressed) {
            arrowButtonPressed = false;
            switchToScreen(SCREEN_SETTINGS);
            Serial.println("Switching to settings screen");
        }

        touchState.isDragging = false;
        touchState.lastTouchY = -1;
    }
}

void screenFileBrowserUpdate() {
    // Check if USB mount state changed and refresh display
    bool currentlyMounted = usbMscMounted();
    if (currentlyMounted != usbLockedDisplayed) {
        // State changed - redraw the file list area
        if (currentlyMounted) {
            usbLockedDisplayed = true;
            drawUsbLockedOverlay();
            Serial.println("USB mounted - file browser locked");
        } else {
            usbLockedDisplayed = false;
            loadFileList();
            drawFileListArea();
            Serial.println("USB ejected - file browser unlocked");
        }
    }
}

void screenFileBrowserReset() {
    clearFileList();
    scrollOffset = 0;
    touchState = {false, -1, false};
    arrowButtonPressed = false;
    usbLockedDisplayed = false;
}
