#include "display.h"
#include "slave/spi_slave.h"
#include "sd_card.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <Preferences.h>
#include <math.h>

// FT6336G Touch Controller (FocalTech FT6x36 family)
// Register-compatible with FT6206/FT6236, uses same I2C protocol
#define FT6336G_ADDR        TOUCH_I2C_ADDR  // 0x38
#define FT6336G_REG_MODE    0x00
#define FT6336G_REG_GEST    0x01
#define FT6336G_REG_STATUS  0x02
#define FT6336G_REG_XH      0x03
#define FT6336G_REG_XL      0x04
#define FT6336G_REG_YH      0x05
#define FT6336G_REG_YL      0x06
#define FT6336G_REG_CHIPID  0xA3
#define FT6336G_REG_FWVERS  0xA6

static TFT_eSPI tft = TFT_eSPI();
static bool touchInitialized = false;

static DisplayState currentState = DISPLAY_NO_SIGNAL;
static uint16_t displayedRpm = 0;
static uint16_t manualRpm = 3000;  // Adjustable RPM for manual mode
static bool needsFullRedraw = true;
static unsigned long lastBlinkTime = 0;
static bool blinkState = false;

// Dark mode color scheme
#define COLOR_BACKGROUND   0x0000    // Pure black
#define COLOR_RPM_TEXT     0xFFFF    // Pure white
#define COLOR_LABEL        0xFFFF    // Pure white
#define COLOR_CONNECTED    0x2DC9    // Muted green (#2D9D4A)
#define COLOR_DISCONNECTED 0xD8A3    // Muted red (#DC3545)
#define COLOR_WARNING      0xFD20    // Muted amber (#FFC107)
#define COLOR_BTN_NORMAL   0x2945    // Dark blue (#293A4A)
#define COLOR_BTN_PRESSED  0x3B8F    // Lighter blue (#3D5A73)
#define COLOR_BTN_TEXT     0xDEFB    // Soft white (#DEE2E6)

// Screen layout
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240
#define RPM_Y_POS     80
#define LABEL_Y_POS   140
#define STATUS_Y_POS  165

// Logo layout
#define LOGO_Y_POS    (SCREEN_HEIGHT - 30)
#define LOGO_3D_OFFSET 2

// Button styling
#define BTN_RADIUS    8

// Gear button layout (bottom right)
#define GEAR_BTN_SIZE   36
#define GEAR_BTN_X      (SCREEN_WIDTH - GEAR_BTN_SIZE - 8)
#define GEAR_BTN_Y      (SCREEN_HEIGHT - GEAR_BTN_SIZE - 8)

// Mode button layout (bottom left, opposite of gear button)
#define MODE_BTN_SIZE   36
#define MODE_BTN_X      8
#define MODE_BTN_Y      (SCREEN_HEIGHT - MODE_BTN_SIZE - 8)

// Back button layout (diagnostics screen - center)
#define BACK_BTN_WIDTH  80
#define BACK_BTN_HEIGHT 36
#define BACK_BTN_X      ((SCREEN_WIDTH - BACK_BTN_WIDTH) / 2)
#define BACK_BTN_Y      (SCREEN_HEIGHT - BACK_BTN_HEIGHT - 10)

// SD card button layout (diagnostics screen - bottom right)
#define SD_BTN_SIZE     36
#define SD_BTN_X        (SCREEN_WIDTH - SD_BTN_SIZE - 8)
#define SD_BTN_Y        (SCREEN_HEIGHT - SD_BTN_SIZE - 8)

// Back arrow button layout (file browser - bottom left)
#define ARROW_BTN_SIZE  36
#define ARROW_BTN_X     8
#define ARROW_BTN_Y     (SCREEN_HEIGHT - ARROW_BTN_SIZE - 8)

// File browser layout
#define FILE_LIST_Y_START   45
#define FILE_LIST_Y_END     (SCREEN_HEIGHT - 50)
#define FILE_LINE_HEIGHT    20
#define MAX_VISIBLE_FILES   ((FILE_LIST_Y_END - FILE_LIST_Y_START) / FILE_LINE_HEIGHT)
#define MAX_FILES           64

// WiFi button layout (diagnostics screen - bottom left)
#define WIFI_BTN_SIZE       36
#define WIFI_BTN_X          8
#define WIFI_BTN_Y          (SCREEN_HEIGHT - WIFI_BTN_SIZE - 8)

// RPM up/down buttons layout (manual mode only, positioned beside RPM value)
#define RPM_BTN_SIZE        40
#define RPM_BTN_Y           (RPM_Y_POS - RPM_BTN_SIZE / 2)  // Vertically centered with RPM
#define RPM_UP_BTN_X        20                               // Left of RPM number
#define RPM_DOWN_BTN_X      (SCREEN_WIDTH - RPM_BTN_SIZE - 20) // Right of RPM number

// WiFi screen layout (scrollable content area)
#define WIFI_CONTENT_Y      40
#define WIFI_CONTENT_H      (SCREEN_HEIGHT - WIFI_CONTENT_Y - 10)
#define WIFI_MODE_BTN_X     10
#define WIFI_MODE_BTN_Y     10   // Relative to scroll offset
#define WIFI_MODE_BTN_W     300
#define WIFI_MODE_BTN_H     30
#define WIFI_SSID_Y         50   // Relative to scroll offset
#define WIFI_PASS_Y         90   // Relative to scroll offset
#define WIFI_INPUT_X        10
#define WIFI_INPUT_W        300
#define WIFI_INPUT_H        28
#define WIFI_LIST_Y         150  // Relative to scroll offset (below scan button)
#define WIFI_LIST_H         22
#define MAX_WIFI_NETWORKS   5
#define MAX_SSID_LEN        32
#define MAX_PASS_LEN        64
#define WIFI_SCROLL_MAX     50   // Max scroll offset

// Full-screen QWERTY Keyboard layout
#define KB_HEADER_H         45   // Height for text entry display
#define KB_KEY_H            36   // Key height
#define KB_KEY_W            30   // Standard key width
#define KB_WIDE_KEY_W       45   // Wide key width (shift, del, etc)
#define KB_SPACE_W          120  // Space bar width
#define KB_SPACING          2
#define KB_START_Y          (KB_HEADER_H + 5)

// Screen state
static ScreenType currentScreen = SCREEN_MAIN;

// Button state
static bool gearButtonPressed = false;
static bool backButtonPressed = false;
static bool sdButtonPressed = false;
static bool arrowButtonPressed = false;
static bool wifiButtonPressed = false;
static bool wifiModeButtonPressed = false;
static bool wifiBackButtonPressed = false;
static bool wifiScanButtonPressed = false;
static bool modeButtonPressed = false;
static bool rpmUpButtonPressed = false;
static bool rpmDownButtonPressed = false;
static bool lastTouchState = false;
static bool lastSyncState = false;

// File browser state
static char* fileList[MAX_FILES];
static int fileCount = 0;
static int scrollOffset = 0;
static int16_t lastTouchY = -1;
static bool isDragging = false;

// Diagnostics screen state
static int diagScrollOffset = 0;
#define DIAG_CONTENT_Y      40
#define DIAG_CONTENT_H      (SCREEN_HEIGHT - DIAG_CONTENT_Y - 50)
#define DIAG_TOTAL_HEIGHT   220  // Total content height (will be calculated)

// WiFi state
static int wifiMode = 0;  // 0 = disabled, 1 = client, 2 = AP
static char wifiSsid[MAX_SSID_LEN + 1] = "";
static char wifiPassword[MAX_PASS_LEN + 1] = "";
static int activeInput = 0;  // 0 = none, 1 = SSID, 2 = password
static unsigned long lastWifiStatusCheck = 0;
static bool lastWifiConnected = false;

// WiFi scan results
struct WifiNetwork {
    char ssid[MAX_SSID_LEN + 1];
    int32_t rssi;
};
static WifiNetwork wifiNetworks[MAX_WIFI_NETWORKS];
static int wifiNetworkCount = 0;
static unsigned long lastWifiScan = 0;
static bool wifiScanInProgress = false;

// Preferences for persistent storage
static Preferences prefs;

// Full-screen keyboard state
static bool keyboardVisible = false;
static bool keyboardShift = false;
static bool keyboardSymbols = false;  // Show symbols/numbers instead of letters
static char* keyboardTarget = nullptr;  // Pointer to string being edited
static int keyboardTargetMax = 0;       // Max length of target string
static const char* keyboardLabel = "";  // Label to show (e.g., "SSID:" or "Password:")
static bool keyboardIsPassword = false; // Whether to mask input

// QWERTY keyboard layouts
static const char* kbRowLetters[] = {
    "qwertyuiop",
    "asdfghjkl",
    "zxcvbnm"
};
static const char* kbRowNumbers = "1234567890";
static const char* kbRowSymbols1 = "!@#$%^&*()";
static const char* kbRowSymbols2 = "-_=+[]{}";
static const char* kbRowSymbols3 = ";:'\",.?/";

// Forward declarations
static void switchToScreen(ScreenType screen);
static void drawWifiScanButton(bool pressed);
static void drawModeButton(bool pressed);
static void drawRpmUpButton(bool pressed);
static void drawRpmDownButton(bool pressed);

static void drawBackground() {
    tft.fillScreen(COLOR_BACKGROUND);

    // Draw title
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.drawString("POWER STEERING", SCREEN_WIDTH / 2, 10);

    // Draw RPM label
    tft.setTextSize(2);
    tft.drawString("RPM", SCREEN_WIDTH / 2, LABEL_Y_POS);
}

static void drawRpmValue(uint16_t rpm, bool connected) {
    // Clear RPM area
    tft.fillRect(0, RPM_Y_POS - 40, SCREEN_WIDTH, 80, COLOR_BACKGROUND);

    tft.setTextDatum(MC_DATUM);

    if (!connected) {
        // Show "NO SIGNAL" in red
        tft.setTextColor(COLOR_DISCONNECTED, COLOR_BACKGROUND);
        tft.setTextSize(3);
        tft.drawString("NO SIGNAL", SCREEN_WIDTH / 2, RPM_Y_POS);
    } else {
        // Show RPM value
        tft.setTextColor(COLOR_RPM_TEXT, COLOR_BACKGROUND);
        tft.setTextSize(6);

        char rpmStr[8];
        snprintf(rpmStr, sizeof(rpmStr), "%u", rpm);
        tft.drawString(rpmStr, SCREEN_WIDTH / 2, RPM_Y_POS);
    }

    // Draw up/down buttons if in manual mode and connected
    if (spiSlaveGetMasterMode() == MODE_MANUAL && connected) {
        drawRpmUpButton(rpmUpButtonPressed);
        drawRpmDownButton(rpmDownButtonPressed);
    }
}

// Check if master and slave are in sync (master has acknowledged our requests)
static bool isSynced() {
    if (!spiSlaveIsConnected()) return false;

    // Check if master's state matches our requested state
    uint8_t masterMode = spiSlaveGetMasterMode();
    uint8_t requestedMode = spiSlaveGetRequestedMode();

    if (masterMode != requestedMode) {
        // Mode mismatch - waiting for master to adopt our mode request
        return false;
    }

    if (masterMode == MODE_MANUAL) {
        // In manual mode, also check if RPM matches
        uint16_t masterRpm = spiSlaveGetLastRpm();
        uint16_t requestedRpm = spiSlaveGetRequestedRpm();
        return masterRpm == requestedRpm;
    }

    // In auto mode, synced if mode matches
    return true;
}

static void drawStatusIndicator(bool connected) {
    // Clear status area
    tft.fillRect(0, STATUS_Y_POS - 10, SCREEN_WIDTH, 20, COLOR_BACKGROUND);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);

    if (connected) {
        bool synced = isSynced();
        if (synced) {
            tft.setTextColor(COLOR_CONNECTED, COLOR_BACKGROUND);
            tft.fillCircle(SCREEN_WIDTH / 2 - 50, STATUS_Y_POS, 5, COLOR_CONNECTED);
            tft.drawString("SYNCED", SCREEN_WIDTH / 2 + 10, STATUS_Y_POS);
        } else {
            // Connected but not yet synced (waiting for master to adopt manual RPM)
            tft.setTextColor(COLOR_WARNING, COLOR_BACKGROUND);
            tft.fillCircle(SCREEN_WIDTH / 2 - 50, STATUS_Y_POS, 5, COLOR_WARNING);
            tft.drawString("SYNCING", SCREEN_WIDTH / 2 + 10, STATUS_Y_POS);
        }
    } else {
        // Blink the disconnected indicator
        uint16_t indicatorColor = blinkState ? COLOR_DISCONNECTED : COLOR_BACKGROUND;
        tft.setTextColor(COLOR_DISCONNECTED, COLOR_BACKGROUND);
        tft.fillCircle(SCREEN_WIDTH / 2 - 50, STATUS_Y_POS, 5, indicatorColor);
        tft.drawString("DISCONNECTED", SCREEN_WIDTH / 2 + 10, STATUS_Y_POS);
    }
}

static void drawVonderwagen3DLogo() {
    // 3D styled logo "Vonderwagen" at bottom center
    const char* logoText = "Vonderwagen";
    int16_t centerX = SCREEN_WIDTH / 2;

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);

    // Draw shadow for 3D effect
    tft.setTextColor(0x4228, COLOR_BACKGROUND);
    tft.drawString(logoText, centerX + 2, LOGO_Y_POS + 2);

    // Main text
    tft.setTextColor(0xFFFF, COLOR_BACKGROUND);
    tft.drawString(logoText, centerX, LOGO_Y_POS);
}

static bool isTouchInGearButton(int16_t x, int16_t y) {
    return (x >= GEAR_BTN_X && x <= GEAR_BTN_X + GEAR_BTN_SIZE &&
            y >= GEAR_BTN_Y && y <= GEAR_BTN_Y + GEAR_BTN_SIZE);
}

static bool isTouchInBackButton(int16_t x, int16_t y) {
    return (x >= BACK_BTN_X && x <= BACK_BTN_X + BACK_BTN_WIDTH &&
            y >= BACK_BTN_Y && y <= BACK_BTN_Y + BACK_BTN_HEIGHT);
}

static bool isTouchInSdButton(int16_t x, int16_t y) {
    return (x >= SD_BTN_X && x <= SD_BTN_X + SD_BTN_SIZE &&
            y >= SD_BTN_Y && y <= SD_BTN_Y + SD_BTN_SIZE);
}

static bool isTouchInArrowButton(int16_t x, int16_t y) {
    return (x >= ARROW_BTN_X && x <= ARROW_BTN_X + ARROW_BTN_SIZE &&
            y >= ARROW_BTN_Y && y <= ARROW_BTN_Y + ARROW_BTN_SIZE);
}

static bool isTouchInFileList(int16_t x, int16_t y) {
    return (y >= FILE_LIST_Y_START && y <= FILE_LIST_Y_END);
}

static void drawGearIcon(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
    // Draw a simple gear icon - scaled to fit inside button
    int16_t outerR = size / 3;
    int16_t holeR = outerR / 3;

    // Draw outer circle (gear body)
    tft.fillCircle(cx, cy, outerR, color);

    // Draw gear teeth (6 teeth)
    for (int i = 0; i < 6; i++) {
        float angle = i * 3.14159 / 3;
        int16_t tx = cx + cos(angle) * (outerR + 2);
        int16_t ty = cy + sin(angle) * (outerR + 2);
        tft.fillCircle(tx, ty, 3, color);
    }

    // Draw center hole
    tft.fillCircle(cx, cy, holeR, COLOR_BACKGROUND);
}

static void drawGearButton(bool pressed) {
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t cx = GEAR_BTN_X + GEAR_BTN_SIZE / 2;
    int16_t cy = GEAR_BTN_Y + GEAR_BTN_SIZE / 2;

    // Draw button background
    tft.fillRoundRect(GEAR_BTN_X, GEAR_BTN_Y, GEAR_BTN_SIZE, GEAR_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(GEAR_BTN_X, GEAR_BTN_Y, GEAR_BTN_SIZE, GEAR_BTN_SIZE, 6, COLOR_BTN_TEXT);

    // Draw gear icon (size parameter controls overall gear size)
    drawGearIcon(cx, cy, GEAR_BTN_SIZE, COLOR_BTN_TEXT);
}

static void drawModeButton(bool pressed) {
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    uint8_t mode = spiSlaveGetRequestedMode();  // Display the mode user has requested

    // Draw button background
    tft.fillRoundRect(MODE_BTN_X, MODE_BTN_Y, MODE_BTN_SIZE, MODE_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(MODE_BTN_X, MODE_BTN_Y, MODE_BTN_SIZE, MODE_BTN_SIZE, 6, COLOR_BTN_TEXT);

    // Draw A or M
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(mode == MODE_AUTO ? COLOR_CONNECTED : COLOR_WARNING, btnColor);
    tft.drawString(mode == MODE_AUTO ? "A" : "M", MODE_BTN_X + MODE_BTN_SIZE / 2, MODE_BTN_Y + MODE_BTN_SIZE / 2);
}

static bool isTouchInModeButton(int16_t x, int16_t y) {
    return (x >= MODE_BTN_X && x <= MODE_BTN_X + MODE_BTN_SIZE &&
            y >= MODE_BTN_Y && y <= MODE_BTN_Y + MODE_BTN_SIZE);
}

static bool isTouchInRpmUpButton(int16_t x, int16_t y) {
    return (x >= RPM_UP_BTN_X && x <= RPM_UP_BTN_X + RPM_BTN_SIZE &&
            y >= RPM_BTN_Y && y <= RPM_BTN_Y + RPM_BTN_SIZE);
}

static bool isTouchInRpmDownButton(int16_t x, int16_t y) {
    return (x >= RPM_DOWN_BTN_X && x <= RPM_DOWN_BTN_X + RPM_BTN_SIZE &&
            y >= RPM_BTN_Y && y <= RPM_BTN_Y + RPM_BTN_SIZE);
}

static void drawRpmUpButton(bool pressed) {
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t cx = RPM_UP_BTN_X + RPM_BTN_SIZE / 2;
    int16_t cy = RPM_BTN_Y + RPM_BTN_SIZE / 2;

    // Draw button background
    tft.fillRoundRect(RPM_UP_BTN_X, RPM_BTN_Y, RPM_BTN_SIZE, RPM_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(RPM_UP_BTN_X, RPM_BTN_Y, RPM_BTN_SIZE, RPM_BTN_SIZE, 6, COLOR_BTN_TEXT);

    // Draw up arrow triangle
    int16_t arrowSize = 12;
    tft.fillTriangle(
        cx, cy - arrowSize,                    // Top point
        cx - arrowSize, cy + arrowSize / 2,    // Bottom left
        cx + arrowSize, cy + arrowSize / 2,    // Bottom right
        COLOR_BTN_TEXT
    );
}

static void drawRpmDownButton(bool pressed) {
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t cx = RPM_DOWN_BTN_X + RPM_BTN_SIZE / 2;
    int16_t cy = RPM_BTN_Y + RPM_BTN_SIZE / 2;

    // Draw button background
    tft.fillRoundRect(RPM_DOWN_BTN_X, RPM_BTN_Y, RPM_BTN_SIZE, RPM_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(RPM_DOWN_BTN_X, RPM_BTN_Y, RPM_BTN_SIZE, RPM_BTN_SIZE, 6, COLOR_BTN_TEXT);

    // Draw down arrow triangle
    int16_t arrowSize = 12;
    tft.fillTriangle(
        cx, cy + arrowSize,                    // Bottom point
        cx - arrowSize, cy - arrowSize / 2,    // Top left
        cx + arrowSize, cy - arrowSize / 2,    // Top right
        COLOR_BTN_TEXT
    );
}

static void drawBackButton(bool pressed) {
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;

    tft.fillRoundRect(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_WIDTH, BACK_BTN_HEIGHT, BTN_RADIUS, btnColor);
    tft.drawRoundRect(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_WIDTH, BACK_BTN_HEIGHT, BTN_RADIUS, COLOR_BTN_TEXT);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_BTN_TEXT, btnColor);
    tft.drawString("BACK", BACK_BTN_X + BACK_BTN_WIDTH / 2, BACK_BTN_Y + BACK_BTN_HEIGHT / 2);
}

static void drawSdCardIcon(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
    // Draw SD card shape
    int16_t w = size * 2 / 3;
    int16_t h = size - 4;
    int16_t x = cx - w / 2;
    int16_t y = cy - h / 2;
    int16_t notch = w / 3;

    // Main body
    tft.fillRect(x, y + notch, w, h - notch, color);
    // Top part with corner cut
    tft.fillRect(x + notch, y, w - notch, notch, color);
    // Diagonal notch
    tft.fillTriangle(x, y + notch, x + notch, y + notch, x + notch, y, color);

    // Draw lines on card to show contacts
    tft.drawLine(x + 3, cy, x + 3, cy + h/3, COLOR_BACKGROUND);
    tft.drawLine(x + 6, cy, x + 6, cy + h/3, COLOR_BACKGROUND);
    tft.drawLine(x + 9, cy, x + 9, cy + h/3, COLOR_BACKGROUND);
}

static void drawSdButton(bool pressed) {
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t cx = SD_BTN_X + SD_BTN_SIZE / 2;
    int16_t cy = SD_BTN_Y + SD_BTN_SIZE / 2;

    // Draw button background
    tft.fillRoundRect(SD_BTN_X, SD_BTN_Y, SD_BTN_SIZE, SD_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(SD_BTN_X, SD_BTN_Y, SD_BTN_SIZE, SD_BTN_SIZE, 6, COLOR_BTN_TEXT);

    // Draw SD card icon
    drawSdCardIcon(cx, cy, SD_BTN_SIZE - 8, COLOR_BTN_TEXT);
}

static void drawBackArrowIcon(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
    // Draw left-pointing arrow
    int16_t arrowW = size / 2;
    int16_t arrowH = size / 2;

    // Arrow head (triangle pointing left)
    tft.fillTriangle(
        cx - arrowW/2, cy,           // Left point
        cx + arrowW/4, cy - arrowH/2, // Top right
        cx + arrowW/4, cy + arrowH/2, // Bottom right
        color
    );

    // Arrow shaft
    tft.fillRect(cx - arrowW/4, cy - 3, arrowW/2 + 2, 6, color);
}

static void drawArrowButton(bool pressed) {
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t cx = ARROW_BTN_X + ARROW_BTN_SIZE / 2;
    int16_t cy = ARROW_BTN_Y + ARROW_BTN_SIZE / 2;

    // Draw button background
    tft.fillRoundRect(ARROW_BTN_X, ARROW_BTN_Y, ARROW_BTN_SIZE, ARROW_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(ARROW_BTN_X, ARROW_BTN_Y, ARROW_BTN_SIZE, ARROW_BTN_SIZE, 6, COLOR_BTN_TEXT);

    // Draw back arrow icon
    drawBackArrowIcon(cx, cy, ARROW_BTN_SIZE - 8, COLOR_BTN_TEXT);
}

static void drawWifiIcon(int16_t cx, int16_t cy, int16_t size, uint16_t color) {
    // Draw WiFi signal arcs
    int16_t baseY = cy + size / 4;

    // Draw three arcs (signal strength indicators)
    for (int i = 0; i < 3; i++) {
        int16_t r = (i + 1) * size / 4;
        // Draw arc using multiple small lines
        for (int a = -45; a <= 45; a += 10) {
            float rad = a * 3.14159 / 180.0;
            int16_t x1 = cx + cos(rad - 3.14159/2) * r;
            int16_t y1 = baseY + sin(rad - 3.14159/2) * r;
            int16_t x2 = cx + cos(rad - 3.14159/2 + 0.17) * r;
            int16_t y2 = baseY + sin(rad - 3.14159/2 + 0.17) * r;
            tft.drawLine(x1, y1, x2, y2, color);
        }
    }

    // Draw center dot
    tft.fillCircle(cx, baseY, 2, color);
}

static void drawWifiButton(bool pressed) {
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t cx = WIFI_BTN_X + WIFI_BTN_SIZE / 2;
    int16_t cy = WIFI_BTN_Y + WIFI_BTN_SIZE / 2;

    // Draw button background
    tft.fillRoundRect(WIFI_BTN_X, WIFI_BTN_Y, WIFI_BTN_SIZE, WIFI_BTN_SIZE, 6, btnColor);
    tft.drawRoundRect(WIFI_BTN_X, WIFI_BTN_Y, WIFI_BTN_SIZE, WIFI_BTN_SIZE, 6, COLOR_BTN_TEXT);

    // Draw WiFi icon
    drawWifiIcon(cx, cy, WIFI_BTN_SIZE - 12, COLOR_BTN_TEXT);
}

static bool isTouchInWifiButton(int16_t x, int16_t y) {
    return (x >= WIFI_BTN_X && x <= WIFI_BTN_X + WIFI_BTN_SIZE &&
            y >= WIFI_BTN_Y && y <= WIFI_BTN_Y + WIFI_BTN_SIZE);
}

static bool isTouchInWifiModeButton(int16_t x, int16_t y) {
    int16_t btnY = WIFI_CONTENT_Y + WIFI_MODE_BTN_Y;
    return (x >= WIFI_MODE_BTN_X && x <= WIFI_MODE_BTN_X + WIFI_MODE_BTN_W &&
            y >= btnY && y <= btnY + WIFI_MODE_BTN_H);
}

// WiFi connection status indicator (top right of screen)
#define WIFI_STATUS_X   (SCREEN_WIDTH - 25)
#define WIFI_STATUS_Y   12
#define WIFI_STATUS_SIZE 16

static void drawWifiStatusIndicator() {
    // Check if WiFi is connected
    bool connected = (wifiMode == 1 && WiFi.status() == WL_CONNECTED);

    if (connected) {
        drawWifiIcon(WIFI_STATUS_X, WIFI_STATUS_Y, WIFI_STATUS_SIZE, COLOR_CONNECTED);
    } else {
        // Clear the area if not connected
        tft.fillRect(WIFI_STATUS_X - WIFI_STATUS_SIZE/2 - 2, WIFI_STATUS_Y - WIFI_STATUS_SIZE/2 - 2,
                     WIFI_STATUS_SIZE + 4, WIFI_STATUS_SIZE + 4, COLOR_BACKGROUND);
    }
}

static bool isTouchInSsidInput(int16_t x, int16_t y) {
    int16_t inputY = WIFI_CONTENT_Y + WIFI_SSID_Y;
    return (x >= WIFI_INPUT_X && x <= WIFI_INPUT_X + WIFI_INPUT_W &&
            y >= inputY && y <= inputY + WIFI_INPUT_H);
}

static bool isTouchInPassInput(int16_t x, int16_t y) {
    int16_t inputY = WIFI_CONTENT_Y + WIFI_PASS_Y;
    return (x >= WIFI_INPUT_X && x <= WIFI_INPUT_X + WIFI_INPUT_W &&
            y >= inputY && y <= inputY + WIFI_INPUT_H);
}

static bool isTouchInWifiNetwork(int16_t x, int16_t y, int* networkIndex) {
    int16_t listY = WIFI_CONTENT_Y + WIFI_LIST_Y;
    if (x >= WIFI_INPUT_X && x <= WIFI_INPUT_X + WIFI_INPUT_W &&
        y >= listY && y < listY + (MAX_WIFI_NETWORKS * WIFI_LIST_H)) {
        *networkIndex = (y - listY) / WIFI_LIST_H;
        return (*networkIndex < wifiNetworkCount);
    }
    return false;
}

static void scanWifiNetworks() {
    if (wifiScanInProgress || wifiMode == 0) return;  // Don't scan if disabled

    wifiScanInProgress = true;
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    delay(100);

    int n = WiFi.scanNetworks();
    wifiNetworkCount = 0;

    // Sort by signal strength and take top 5
    for (int i = 0; i < n && wifiNetworkCount < MAX_WIFI_NETWORKS; i++) {
        // Find strongest remaining network
        int bestIdx = -1;
        int32_t bestRssi = -999;
        for (int j = 0; j < n; j++) {
            bool alreadyAdded = false;
            for (int k = 0; k < wifiNetworkCount; k++) {
                if (strcmp(wifiNetworks[k].ssid, WiFi.SSID(j).c_str()) == 0) {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded && WiFi.RSSI(j) > bestRssi) {
                bestRssi = WiFi.RSSI(j);
                bestIdx = j;
            }
        }
        if (bestIdx >= 0) {
            strncpy(wifiNetworks[wifiNetworkCount].ssid, WiFi.SSID(bestIdx).c_str(), MAX_SSID_LEN);
            wifiNetworks[wifiNetworkCount].ssid[MAX_SSID_LEN] = '\0';
            wifiNetworks[wifiNetworkCount].rssi = bestRssi;
            wifiNetworkCount++;
        }
    }

    WiFi.scanDelete();
    wifiScanInProgress = false;
    lastWifiScan = millis();
}

static void saveWifiSettings() {
    prefs.begin("wifi", false);
    prefs.putInt("mode", wifiMode);
    prefs.putString("ssid", wifiSsid);
    prefs.putString("pass", wifiPassword);
    prefs.end();
}

static void loadWifiSettings() {
    prefs.begin("wifi", true);
    wifiMode = prefs.getInt("mode", 0);  // Default: disabled
    String ssid = prefs.getString("ssid", "");
    String pass = prefs.getString("pass", "");
    strncpy(wifiSsid, ssid.c_str(), MAX_SSID_LEN);
    strncpy(wifiPassword, pass.c_str(), MAX_PASS_LEN);
    prefs.end();
}

// Initialize requested RPM state (master is source of truth, we just send requests)
static void initRequestedRpm() {
    manualRpm = 3000;  // Default starting value for UI
    spiSlaveSetRequestedRpm(manualRpm);
    spiSlaveSetRequestedMode(MODE_AUTO);  // Start in auto mode
}

static const char* getWifiModeString() {
    switch (wifiMode) {
        case 0: return "Mode: Disabled";
        case 1: return "Mode: Client";
        default: return "Mode: Unknown";
    }
}

static void drawWifiModeButton() {
    uint16_t btnColor = wifiModeButtonPressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t y = WIFI_CONTENT_Y + WIFI_MODE_BTN_Y;

    tft.fillRoundRect(WIFI_MODE_BTN_X, y, WIFI_MODE_BTN_W, WIFI_MODE_BTN_H, 6, btnColor);
    tft.drawRoundRect(WIFI_MODE_BTN_X, y, WIFI_MODE_BTN_W, WIFI_MODE_BTN_H, 6, COLOR_BTN_TEXT);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_BTN_TEXT, btnColor);
    tft.drawString(getWifiModeString(), WIFI_MODE_BTN_X + WIFI_MODE_BTN_W / 2, y + WIFI_MODE_BTN_H / 2);
}

// Full-screen QWERTY Keyboard functions
static void drawKeyboardKey(int16_t x, int16_t y, int16_t w, const char* label, uint16_t color) {
    tft.fillRoundRect(x, y, w, KB_KEY_H, 4, color);
    tft.drawRoundRect(x, y, w, KB_KEY_H, 4, COLOR_BTN_TEXT);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_BTN_TEXT, color);
    tft.drawString(label, x + w / 2, y + KB_KEY_H / 2);
}

static void drawKeyboard() {
    if (!keyboardVisible) return;

    // Fill entire screen with keyboard background
    tft.fillScreen(0x1082);

    // Draw header with label and current text
    tft.fillRect(0, 0, SCREEN_WIDTH, KB_HEADER_H, COLOR_BACKGROUND);
    tft.drawLine(0, KB_HEADER_H, SCREEN_WIDTH, KB_HEADER_H, COLOR_BTN_TEXT);

    // Draw label
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.drawString(keyboardLabel, 10, 5);

    // Draw current text in input box
    tft.fillRoundRect(5, 18, SCREEN_WIDTH - 10, 24, 4, COLOR_BTN_NORMAL);
    tft.drawRoundRect(5, 18, SCREEN_WIDTH - 10, 24, 4, COLOR_CONNECTED);

    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_RPM_TEXT, COLOR_BTN_NORMAL);

    if (keyboardTarget) {
        if (keyboardIsPassword && strlen(keyboardTarget) > 0) {
            // Show asterisks for password
            char masked[MAX_PASS_LEN + 1];
            int len = strlen(keyboardTarget);
            for (int i = 0; i < len && i < MAX_PASS_LEN; i++) masked[i] = '*';
            masked[len] = '\0';
            tft.drawString(masked, 12, 30);
        } else {
            tft.drawString(keyboardTarget, 12, 30);
        }
    }

    int16_t y = KB_START_Y;
    int16_t x;
    char keyStr[2] = {0, 0};

    // Row 1: Numbers or symbols row 1
    const char* row1 = keyboardSymbols ? kbRowSymbols1 : kbRowNumbers;
    x = 2;
    for (int i = 0; i < 10; i++) {
        keyStr[0] = row1[i];
        drawKeyboardKey(x, y, KB_KEY_W, keyStr, COLOR_BTN_NORMAL);
        x += KB_KEY_W + KB_SPACING;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 2: QWERTYUIOP or symbols row 2
    const char* row2 = keyboardSymbols ? kbRowSymbols2 : kbRowLetters[0];
    int row2len = strlen(row2);
    x = (SCREEN_WIDTH - (row2len * (KB_KEY_W + KB_SPACING) - KB_SPACING)) / 2;
    for (int i = 0; i < row2len; i++) {
        keyStr[0] = row2[i];
        if (!keyboardSymbols && keyboardShift) keyStr[0] = toupper(keyStr[0]);
        drawKeyboardKey(x, y, KB_KEY_W, keyStr, COLOR_BTN_NORMAL);
        x += KB_KEY_W + KB_SPACING;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 3: ASDFGHJKL or symbols row 3
    const char* row3 = keyboardSymbols ? kbRowSymbols3 : kbRowLetters[1];
    int row3len = strlen(row3);
    x = (SCREEN_WIDTH - (row3len * (KB_KEY_W + KB_SPACING) - KB_SPACING)) / 2;
    for (int i = 0; i < row3len; i++) {
        keyStr[0] = row3[i];
        if (!keyboardSymbols && keyboardShift) keyStr[0] = toupper(keyStr[0]);
        drawKeyboardKey(x, y, KB_KEY_W, keyStr, COLOR_BTN_NORMAL);
        x += KB_KEY_W + KB_SPACING;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 4: SHIFT + ZXCVBNM + DEL
    x = 2;
    drawKeyboardKey(x, y, KB_WIDE_KEY_W, "SHIFT", keyboardShift ? COLOR_CONNECTED : COLOR_BTN_NORMAL);
    x += KB_WIDE_KEY_W + KB_SPACING;

    const char* row4 = kbRowLetters[2];
    for (int i = 0; i < 7; i++) {
        keyStr[0] = row4[i];
        if (keyboardShift) keyStr[0] = toupper(keyStr[0]);
        drawKeyboardKey(x, y, KB_KEY_W, keyStr, COLOR_BTN_NORMAL);
        x += KB_KEY_W + KB_SPACING;
    }
    drawKeyboardKey(x, y, KB_WIDE_KEY_W, "DEL", COLOR_DISCONNECTED);
    y += KB_KEY_H + KB_SPACING;

    // Row 5: ?123 + SPACE + . + OK + BACK
    x = 2;
    drawKeyboardKey(x, y, KB_WIDE_KEY_W, keyboardSymbols ? "ABC" : "?123", COLOR_BTN_NORMAL);
    x += KB_WIDE_KEY_W + KB_SPACING;

    drawKeyboardKey(x, y, KB_SPACE_W, "SPACE", COLOR_BTN_NORMAL);
    x += KB_SPACE_W + KB_SPACING;

    drawKeyboardKey(x, y, KB_KEY_W, ".", COLOR_BTN_NORMAL);
    x += KB_KEY_W + KB_SPACING;

    drawKeyboardKey(x, y, KB_WIDE_KEY_W, "OK", COLOR_CONNECTED);
    x += KB_WIDE_KEY_W + KB_SPACING;

    drawKeyboardKey(x, y, KB_WIDE_KEY_W, "BACK", COLOR_WARNING);
}

static void showKeyboard(const char* label, char* target, int maxLen, bool isPassword) {
    keyboardLabel = label;
    keyboardTarget = target;
    keyboardTargetMax = maxLen;
    keyboardIsPassword = isPassword;
    keyboardShift = false;
    keyboardSymbols = false;
    keyboardVisible = true;
    drawKeyboard();
}

static void connectToWifi() {
    if (wifiMode == 1 && strlen(wifiSsid) > 0) {
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        WiFi.begin(wifiSsid, wifiPassword);
        Serial.printf("Connecting to WiFi: %s\n", wifiSsid);
    }
}

static void hideKeyboard(bool save) {
    keyboardVisible = false;
    keyboardTarget = nullptr;
    if (save) {
        saveWifiSettings();
        connectToWifi();
    }
}

static void keyboardAddChar(char c) {
    if (!keyboardTarget) return;
    int len = strlen(keyboardTarget);
    if (len < keyboardTargetMax) {
        keyboardTarget[len] = c;
        keyboardTarget[len + 1] = '\0';
    }
}

static void keyboardDeleteChar() {
    if (!keyboardTarget) return;
    int len = strlen(keyboardTarget);
    if (len > 0) {
        keyboardTarget[len - 1] = '\0';
    }
}

static void handleKeyboardTouch(int16_t touchX, int16_t touchY) {
    if (!keyboardVisible) return;

    int16_t y = KB_START_Y;
    char keyStr[2] = {0, 0};

    // Row 1: Numbers/symbols
    if (touchY >= y && touchY < y + KB_KEY_H) {
        const char* row1 = keyboardSymbols ? kbRowSymbols1 : kbRowNumbers;
        int col = (touchX - 2) / (KB_KEY_W + KB_SPACING);
        if (col >= 0 && col < 10) {
            keyboardAddChar(row1[col]);
            drawKeyboard();
        }
        return;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 2: QWERTYUIOP or symbols
    if (touchY >= y && touchY < y + KB_KEY_H) {
        const char* row2 = keyboardSymbols ? kbRowSymbols2 : kbRowLetters[0];
        int row2len = strlen(row2);
        int16_t startX = (SCREEN_WIDTH - (row2len * (KB_KEY_W + KB_SPACING) - KB_SPACING)) / 2;
        int col = (touchX - startX) / (KB_KEY_W + KB_SPACING);
        if (col >= 0 && col < row2len) {
            char c = row2[col];
            if (!keyboardSymbols && keyboardShift) c = toupper(c);
            keyboardAddChar(c);
            if (keyboardShift) keyboardShift = false;  // Auto-unshift
            drawKeyboard();
        }
        return;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 3: ASDFGHJKL or symbols
    if (touchY >= y && touchY < y + KB_KEY_H) {
        const char* row3 = keyboardSymbols ? kbRowSymbols3 : kbRowLetters[1];
        int row3len = strlen(row3);
        int16_t startX = (SCREEN_WIDTH - (row3len * (KB_KEY_W + KB_SPACING) - KB_SPACING)) / 2;
        int col = (touchX - startX) / (KB_KEY_W + KB_SPACING);
        if (col >= 0 && col < row3len) {
            char c = row3[col];
            if (!keyboardSymbols && keyboardShift) c = toupper(c);
            keyboardAddChar(c);
            if (keyboardShift) keyboardShift = false;
            drawKeyboard();
        }
        return;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 4: SHIFT + ZXCVBNM + DEL
    if (touchY >= y && touchY < y + KB_KEY_H) {
        int16_t x = 2;
        // SHIFT
        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            keyboardShift = !keyboardShift;
            drawKeyboard();
            return;
        }
        x += KB_WIDE_KEY_W + KB_SPACING;

        // ZXCVBNM
        for (int i = 0; i < 7; i++) {
            if (touchX >= x && touchX < x + KB_KEY_W) {
                char c = kbRowLetters[2][i];
                if (keyboardShift) c = toupper(c);
                keyboardAddChar(c);
                if (keyboardShift) keyboardShift = false;
                drawKeyboard();
                return;
            }
            x += KB_KEY_W + KB_SPACING;
        }

        // DEL
        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            keyboardDeleteChar();
            drawKeyboard();
            return;
        }
        return;
    }
    y += KB_KEY_H + KB_SPACING;

    // Row 5: ?123 + SPACE + . + OK + BACK
    if (touchY >= y && touchY < y + KB_KEY_H) {
        int16_t x = 2;

        // ?123 / ABC
        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            keyboardSymbols = !keyboardSymbols;
            drawKeyboard();
            return;
        }
        x += KB_WIDE_KEY_W + KB_SPACING;

        // SPACE
        if (touchX >= x && touchX < x + KB_SPACE_W) {
            keyboardAddChar(' ');
            drawKeyboard();
            return;
        }
        x += KB_SPACE_W + KB_SPACING;

        // .
        if (touchX >= x && touchX < x + KB_KEY_W) {
            keyboardAddChar('.');
            drawKeyboard();
            return;
        }
        x += KB_KEY_W + KB_SPACING;

        // OK
        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            hideKeyboard(true);
            switchToScreen(SCREEN_WIFI);
            return;
        }
        x += KB_WIDE_KEY_W + KB_SPACING;

        // BACK
        if (touchX >= x && touchX < x + KB_WIDE_KEY_W) {
            // Restore original value? For now just close without saving
            hideKeyboard(false);
            switchToScreen(SCREEN_WIFI);
            return;
        }
    }
}

static void drawWifiInput(int16_t y, const char* label, const char* value, bool active) {
    // Label
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.drawString(label, WIFI_INPUT_X, y - 12);

    // Input box
    uint16_t boxColor = active ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    tft.fillRoundRect(WIFI_INPUT_X, y, WIFI_INPUT_W, WIFI_INPUT_H, 4, boxColor);
    tft.drawRoundRect(WIFI_INPUT_X, y, WIFI_INPUT_W, WIFI_INPUT_H, 4,
                      active ? COLOR_CONNECTED : COLOR_BTN_TEXT);

    // Text value (show plain text, no masking)
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_RPM_TEXT, boxColor);
    tft.drawString(value, WIFI_INPUT_X + 6, y + WIFI_INPUT_H / 2);

    // Draw cursor if active
    if (active) {
        int textWidth = tft.textWidth(value);
        tft.fillRect(WIFI_INPUT_X + 6 + textWidth + 2, y + 6, 2, WIFI_INPUT_H - 12, COLOR_RPM_TEXT);
    }
}

static void drawWifiNetworkList() {
    int16_t baseY = WIFI_CONTENT_Y + WIFI_LIST_Y;
    int16_t listH = MAX_WIFI_NETWORKS * WIFI_LIST_H;

    // Draw list box
    tft.fillRect(WIFI_INPUT_X, baseY, WIFI_INPUT_W, listH, COLOR_BACKGROUND);
    tft.drawRect(WIFI_INPUT_X, baseY, WIFI_INPUT_W, listH, COLOR_BTN_TEXT);

    if (wifiNetworkCount == 0) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(COLOR_BTN_TEXT, COLOR_BACKGROUND);
        const char* msg;
        if (wifiMode == 0) {
            msg = "WiFi Disabled";
        } else if (wifiScanInProgress) {
            msg = "Scanning...";
        } else {
            msg = "Press SCAN to search";
        }
        tft.drawString(msg, WIFI_INPUT_X + WIFI_INPUT_W / 2, baseY + listH / 2);
        return;
    }

    for (int i = 0; i < wifiNetworkCount; i++) {
        int16_t itemY = baseY + i * WIFI_LIST_H;

        // Alternating background
        if (i % 2 == 1) {
            tft.fillRect(WIFI_INPUT_X + 1, itemY, WIFI_INPUT_W - 2, WIFI_LIST_H, 0x1082);
        }

        // SSID
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(COLOR_RPM_TEXT, COLOR_BACKGROUND);
        tft.drawString(wifiNetworks[i].ssid, WIFI_INPUT_X + 6, itemY + WIFI_LIST_H / 2);

        // Signal strength indicator
        int bars = 0;
        if (wifiNetworks[i].rssi > -50) bars = 4;
        else if (wifiNetworks[i].rssi > -60) bars = 3;
        else if (wifiNetworks[i].rssi > -70) bars = 2;
        else bars = 1;

        int16_t barX = WIFI_INPUT_X + WIFI_INPUT_W - 30;
        for (int b = 0; b < 4; b++) {
            int16_t barH = 4 + b * 3;
            uint16_t barColor = (b < bars) ? COLOR_CONNECTED : COLOR_BTN_NORMAL;
            tft.fillRect(barX + b * 6, itemY + WIFI_LIST_H - barH - 2, 4, barH, barColor);
        }
    }
}

static void drawWifiScreenContent() {
    // Clear content area (not title)
    int16_t contentEndY = SCREEN_HEIGHT - 50;
    tft.fillRect(0, WIFI_CONTENT_Y, SCREEN_WIDTH, contentEndY - WIFI_CONTENT_Y, COLOR_BACKGROUND);

    // Mode toggle button
    drawWifiModeButton();

    // SSID input
    drawWifiInput(WIFI_CONTENT_Y + WIFI_SSID_Y, "SSID:", wifiSsid, activeInput == 1);

    // Password input (show plain text)
    drawWifiInput(WIFI_CONTENT_Y + WIFI_PASS_Y, "Password:", wifiPassword, activeInput == 2);

    // Scan button and network list (only if not in AP mode and wifi enabled)
    if (wifiMode == 1) {
        drawWifiScanButton(wifiScanButtonPressed);
        drawWifiNetworkList();
    }
}

// WiFi screen header back button (top left, header-sized)
#define WIFI_BACK_BTN_X     5
#define WIFI_BACK_BTN_Y     5
#define WIFI_BACK_BTN_W     50
#define WIFI_BACK_BTN_H     26

// WiFi scan button (above network list)
#define WIFI_SCAN_BTN_X     10
#define WIFI_SCAN_BTN_Y     118   // Relative to WIFI_CONTENT_Y
#define WIFI_SCAN_BTN_W     100
#define WIFI_SCAN_BTN_H     26

static void drawWifiBackButton(bool pressed) {
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;

    tft.fillRoundRect(WIFI_BACK_BTN_X, WIFI_BACK_BTN_Y, WIFI_BACK_BTN_W, WIFI_BACK_BTN_H, 4, btnColor);
    tft.drawRoundRect(WIFI_BACK_BTN_X, WIFI_BACK_BTN_Y, WIFI_BACK_BTN_W, WIFI_BACK_BTN_H, 4, COLOR_BTN_TEXT);

    // Draw back arrow and text
    int16_t cx = WIFI_BACK_BTN_X + 12;
    int16_t cy = WIFI_BACK_BTN_Y + WIFI_BACK_BTN_H / 2;

    // Small arrow
    tft.fillTriangle(cx - 4, cy, cx + 2, cy - 5, cx + 2, cy + 5, COLOR_BTN_TEXT);

    // "Back" text
    tft.setTextDatum(ML_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(COLOR_BTN_TEXT, btnColor);
    tft.drawString("Back", cx + 6, cy);
}

static bool isTouchInWifiBackButton(int16_t x, int16_t y) {
    return (x >= WIFI_BACK_BTN_X && x <= WIFI_BACK_BTN_X + WIFI_BACK_BTN_W &&
            y >= WIFI_BACK_BTN_Y && y <= WIFI_BACK_BTN_Y + WIFI_BACK_BTN_H);
}

static void drawWifiScanButton(bool pressed) {
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;
    int16_t y = WIFI_CONTENT_Y + WIFI_SCAN_BTN_Y;

    tft.fillRoundRect(WIFI_SCAN_BTN_X, y, WIFI_SCAN_BTN_W, WIFI_SCAN_BTN_H, 4, btnColor);
    tft.drawRoundRect(WIFI_SCAN_BTN_X, y, WIFI_SCAN_BTN_W, WIFI_SCAN_BTN_H, 4, COLOR_BTN_TEXT);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_BTN_TEXT, btnColor);
    tft.drawString("SCAN", WIFI_SCAN_BTN_X + WIFI_SCAN_BTN_W / 2, y + WIFI_SCAN_BTN_H / 2);
}

static bool isTouchInWifiScanButton(int16_t x, int16_t y) {
    int16_t btnY = WIFI_CONTENT_Y + WIFI_SCAN_BTN_Y;
    return (x >= WIFI_SCAN_BTN_X && x <= WIFI_SCAN_BTN_X + WIFI_SCAN_BTN_W &&
            y >= btnY && y <= btnY + WIFI_SCAN_BTN_H);
}

static void drawWifiScreen() {
    tft.fillScreen(COLOR_BACKGROUND);

    // Back button (top left, header-sized)
    drawWifiBackButton(false);

    // Title (centered, accounting for back button)
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.drawString("WIFI SETTINGS", SCREEN_WIDTH / 2 + 20, 10);

    // Draw horizontal line
    tft.drawLine(20, 35, SCREEN_WIDTH - 20, 35, COLOR_LABEL);

    // Draw content
    drawWifiScreenContent();

    // WiFi status indicator
    drawWifiStatusIndicator();
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
        // Allocate and copy filename
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

static void drawFileListArea() {
    // Define box margins
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
                tft.drawString(fileList[i], boxX + 6, y);
            } else {
                tft.setTextColor(COLOR_RPM_TEXT, COLOR_BACKGROUND);
                tft.drawString(fileList[i], boxX + 6, y);
            }
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

static void drawFileBrowserScreen() {
    tft.fillScreen(COLOR_BACKGROUND);

    // Title
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.drawString("FILE BROWSER", SCREEN_WIDTH / 2, 10);

    // Draw horizontal line
    tft.drawLine(20, 35, SCREEN_WIDTH - 20, 35, COLOR_LABEL);

    // Load and draw file list
    loadFileList();
    drawFileListArea();

    // Draw back arrow button
    drawArrowButton(false);

    // WiFi status indicator
    drawWifiStatusIndicator();
}

static void drawDiagnosticsContent() {
    // Clear content area
    tft.fillRect(0, DIAG_CONTENT_Y, SCREEN_WIDTH, DIAG_CONTENT_H, COLOR_BACKGROUND);

    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    int16_t baseY = DIAG_CONTENT_Y + 5;
    int16_t lineHeight = 18;
    char buf[64];

    // Calculate visible Y position with scroll offset
    #define DRAW_LINE(label, value, valueColor) do { \
        int16_t screenY = baseY - diagScrollOffset; \
        if (screenY >= DIAG_CONTENT_Y - lineHeight && screenY < DIAG_CONTENT_Y + DIAG_CONTENT_H) { \
            tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND); \
            tft.drawString(label, 10, screenY); \
            tft.setTextColor(valueColor, COLOR_BACKGROUND); \
            tft.drawString(value, 120, screenY); \
        } \
        baseY += lineHeight; \
    } while(0)

    #define DRAW_SEPARATOR() do { \
        int16_t screenY = baseY - diagScrollOffset; \
        if (screenY >= DIAG_CONTENT_Y && screenY < DIAG_CONTENT_Y + DIAG_CONTENT_H) { \
            tft.drawLine(20, screenY, SCREEN_WIDTH - 20, screenY, COLOR_LABEL); \
        } \
        baseY += 8; \
    } while(0)

    // SPI Status
    DRAW_LINE("SPI Status:", spiSlaveIsConnected() ? "Connected" : "Disconnected",
              spiSlaveIsConnected() ? COLOR_CONNECTED : COLOR_DISCONNECTED);

    // Valid packets
    snprintf(buf, sizeof(buf), "%lu", spiSlaveGetValidPacketCount());
    DRAW_LINE("Valid Packets:", buf, COLOR_RPM_TEXT);

    // Invalid packets
    snprintf(buf, sizeof(buf), "%lu", spiSlaveGetInvalidPacketCount());
    DRAW_LINE("Invalid Packets:", buf, COLOR_RPM_TEXT);

    // Last RPM
    snprintf(buf, sizeof(buf), "%u", spiSlaveGetLastRpm());
    DRAW_LINE("Last RPM:", buf, COLOR_RPM_TEXT);

    baseY += 5;
    DRAW_SEPARATOR();

    // SD Card section
    if (sdCardPresent()) {
        DRAW_LINE("SD Card:", sdCardType(), COLOR_CONNECTED);

        snprintf(buf, sizeof(buf), "%llu MB", sdCardTotalBytes() / (1024 * 1024));
        DRAW_LINE("Total:", buf, COLOR_RPM_TEXT);

        snprintf(buf, sizeof(buf), "%llu MB", sdCardUsedBytes() / (1024 * 1024));
        DRAW_LINE("Used:", buf, COLOR_RPM_TEXT);
    } else {
        DRAW_LINE("SD Card:", "Not Present", COLOR_DISCONNECTED);
    }

    baseY += 5;
    DRAW_SEPARATOR();

    // WiFi section
    const char* modeStr = (wifiMode == 0) ? "Disabled" : "Client";
    uint16_t modeColor = (wifiMode == 0) ? COLOR_DISCONNECTED : COLOR_CONNECTED;
    DRAW_LINE("WiFi Mode:", modeStr, modeColor);

    if (wifiMode == 1) {
        // Client mode - show connection status
        bool connected = (WiFi.status() == WL_CONNECTED);
        DRAW_LINE("Status:", connected ? "Connected" : "Disconnected",
                  connected ? COLOR_CONNECTED : COLOR_DISCONNECTED);

        if (connected) {
            DRAW_LINE("SSID:", wifiSsid, COLOR_RPM_TEXT);

            IPAddress ip = WiFi.localIP();
            snprintf(buf, sizeof(buf), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
            DRAW_LINE("IP Address:", buf, COLOR_RPM_TEXT);

            snprintf(buf, sizeof(buf), "%d dBm", WiFi.RSSI());
            DRAW_LINE("Signal:", buf, COLOR_RPM_TEXT);
        } else if (strlen(wifiSsid) > 0) {
            DRAW_LINE("SSID:", wifiSsid, COLOR_WARNING);
            DRAW_LINE("Status:", "Connecting...", COLOR_WARNING);
        }
    }

    #undef DRAW_LINE
    #undef DRAW_SEPARATOR

    // Draw scroll indicator if content overflows
    int totalHeight = baseY - (DIAG_CONTENT_Y + 5);
    int maxScroll = totalHeight - DIAG_CONTENT_H;
    if (maxScroll > 0) {
        int16_t scrollBarX = SCREEN_WIDTH - 8;
        int16_t scrollBarY = DIAG_CONTENT_Y;
        int16_t scrollBarH = DIAG_CONTENT_H;
        int16_t thumbH = (DIAG_CONTENT_H * DIAG_CONTENT_H) / totalHeight;
        if (thumbH < 20) thumbH = 20;
        int16_t thumbY = scrollBarY + (diagScrollOffset * (scrollBarH - thumbH)) / maxScroll;

        tft.fillRect(scrollBarX, scrollBarY, 4, scrollBarH, COLOR_BTN_NORMAL);
        tft.fillRect(scrollBarX, thumbY, 4, thumbH, COLOR_BTN_TEXT);
    }
}

static void drawDiagnosticsScreen() {
    tft.fillScreen(COLOR_BACKGROUND);

    // Title
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.drawString("DIAGNOSTICS", SCREEN_WIDTH / 2, 10);

    // Draw horizontal line
    tft.drawLine(20, 35, SCREEN_WIDTH - 20, 35, COLOR_LABEL);

    // Draw scrollable content
    drawDiagnosticsContent();

    // Draw back button (center)
    drawBackButton(false);

    // Draw WiFi button (bottom left)
    drawWifiButton(false);

    // Draw SD card button (bottom right) - only if SD card present
    if (sdCardPresent()) {
        drawSdButton(false);
    }

    // WiFi status indicator
    drawWifiStatusIndicator();
}

static void drawMainScreen() {
    drawBackground();
    drawRpmValue(displayedRpm, currentState == DISPLAY_CONNECTED);
    drawStatusIndicator(currentState == DISPLAY_CONNECTED);
    drawVonderwagen3DLogo();
    drawGearButton(gearButtonPressed);
    drawModeButton(modeButtonPressed);
    drawWifiStatusIndicator();
}

static void switchToScreen(ScreenType screen) {
    currentScreen = screen;
    if (screen == SCREEN_MAIN) {
        drawMainScreen();
    } else if (screen == SCREEN_DIAGNOSTICS) {
        drawDiagnosticsScreen();
    } else if (screen == SCREEN_FILE_BROWSER) {
        drawFileBrowserScreen();
    } else if (screen == SCREEN_WIFI) {
        drawWifiScreen();
    }
}

bool displayInit() {
    // Enable backlight
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    // Initialize TFT (ILI9341/ILI9341V)
    tft.init();
    tft.setRotation(1);  // Landscape mode

    // ILI9341V color inversion fix
    tft.invertDisplay(true);

    tft.fillScreen(COLOR_BACKGROUND);

    Serial.println("ILI9341V display initialized");
    Serial.printf("Display size: %dx%d\n", tft.width(), tft.height());

    // Initialize FT6336G touch controller over I2C
    // Reset touch controller
    pinMode(TOUCH_RST_PIN, OUTPUT);
    digitalWrite(TOUCH_RST_PIN, LOW);
    delay(10);
    digitalWrite(TOUCH_RST_PIN, HIGH);
    delay(300);  // Wait for touch controller to boot

    // Wire (I2C0) is dedicated to touch controller on GPIO 16/15
    Wire.begin(I2C_TOUCH_SDA_PIN, I2C_TOUCH_SCL_PIN);
    Wire.setClock(I2C_FREQUENCY);

    // Check if FT6336G is present by reading chip ID
    Wire.beginTransmission((uint8_t)FT6336G_ADDR);
    Wire.write(FT6336G_REG_CHIPID);
    if (Wire.endTransmission() == 0) {
        Wire.requestFrom((uint8_t)FT6336G_ADDR, (uint8_t)1);
        if (Wire.available()) {
            uint8_t chipId = Wire.read();
            Serial.printf("FT6336G touch controller found (Chip ID: 0x%02X)\n", chipId);

            // Read firmware version
            Wire.beginTransmission((uint8_t)FT6336G_ADDR);
            Wire.write(FT6336G_REG_FWVERS);
            Wire.endTransmission();
            Wire.requestFrom((uint8_t)FT6336G_ADDR, (uint8_t)1);
            if (Wire.available()) {
                uint8_t fwVer = Wire.read();
                Serial.printf("FT6336G firmware version: 0x%02X\n", fwVer);
            }
            touchInitialized = true;
        }
    } else {
        Serial.println("FT6336G touch controller not found");
        // Continue without touch - not critical
    }

    // Load WiFi settings from NVS
    loadWifiSettings();
    Serial.printf("WiFi mode loaded: %s\n", getWifiModeString());

    // Initialize requested state (master is source of truth)
    initRequestedRpm();
    Serial.println("Slave initialized - master is source of truth");

    // Initialize WiFi based on saved mode
    if (wifiMode == 0) {
        WiFi.mode(WIFI_OFF);
    } else if (wifiMode == 1) {
        WiFi.mode(WIFI_STA);
        if (strlen(wifiSsid) > 0) {
            WiFi.begin(wifiSsid, wifiPassword);
        }
    }

    // Draw initial screen
    drawBackground();
    drawRpmValue(0, false);
    drawStatusIndicator(false);
    drawVonderwagen3DLogo();
    drawGearButton(false);
    drawModeButton(false);
    needsFullRedraw = false;

    // Note: SPI slave for RPM is initialized separately in spiSlaveInit()
    Serial.println("Touch controller uses Wire (I2C0), RPM slave uses SPI");

    return true;
}

void displayUpdateRpm(uint16_t rpm) {
    // Master is source of truth - display whatever it sends
    if (rpm != displayedRpm || currentState != DISPLAY_CONNECTED) {
        displayedRpm = rpm;
        currentState = DISPLAY_CONNECTED;
        // Only update display if on main screen
        if (currentScreen == SCREEN_MAIN) {
            drawRpmValue(rpm, true);
            drawStatusIndicator(true);
        }
    }
}

void displaySetConnected(bool connected) {
    DisplayState newState = connected ? DISPLAY_CONNECTED : DISPLAY_NO_SIGNAL;
    bool currentSyncState = connected ? isSynced() : false;

    bool stateChanged = (newState != currentState);
    bool syncChanged = (currentSyncState != lastSyncState);

    if (stateChanged || syncChanged) {
        currentState = newState;
        lastSyncState = currentSyncState;
        // Only update display if on main screen
        if (currentScreen == SCREEN_MAIN) {
            if (!connected) {
                drawRpmValue(0, false);
            }
            drawStatusIndicator(connected);
        }
    }
}

static uint8_t touchFailCount = 0;

bool displayGetTouch(int16_t* x, int16_t* y) {
    if (!touchInitialized) return false;

    // Wire is dedicated to touch controller - no mode switching needed

    // Read touch data starting from register 0x02 (status)
    Wire.beginTransmission((uint8_t)FT6336G_ADDR);
    Wire.write(FT6336G_REG_STATUS);
    if (Wire.endTransmission() != 0) {
        touchFailCount++;
        return false;
    }

    Wire.requestFrom((uint8_t)FT6336G_ADDR, (uint8_t)5);  // Status + XH, XL, YH, YL
    if (Wire.available() < 5) {
        touchFailCount++;
        return false;
    }

    touchFailCount = 0;  // Reset on success

    uint8_t status = Wire.read();
    uint8_t touches = status & 0x0F;

    if (touches == 0 || touches > 2) {
        return false;
    }

    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();

    // Extract 12-bit coordinates
    int16_t rawX = ((xh & 0x0F) << 8) | xl;
    int16_t rawY = ((yh & 0x0F) << 8) | yl;

    // Map to screen coordinates for rotation 1 (landscape)
    // Touch panel is 240x320, display is rotated to 320x240
    *x = rawY;              // Swap X and Y
    *y = 240 - rawX;        // Invert the new Y

    return true;
}

void displayLoop() {
    // Check WiFi status periodically and update indicator if changed
    if (!keyboardVisible && millis() - lastWifiStatusCheck > 1000) {
        lastWifiStatusCheck = millis();
        bool currentWifiConnected = (wifiMode == 1 && WiFi.status() == WL_CONNECTED);
        if (currentWifiConnected != lastWifiConnected) {
            lastWifiConnected = currentWifiConnected;
            drawWifiStatusIndicator();
        }
    }

    // Handle touch input
    int16_t touchX, touchY;
    bool currentTouch = displayGetTouch(&touchX, &touchY);

    if (currentScreen == SCREEN_MAIN) {
        // Handle blink animation for disconnected state
        if (currentState == DISPLAY_NO_SIGNAL) {
            unsigned long now = millis();
            if (now - lastBlinkTime >= 500) {
                lastBlinkTime = now;
                blinkState = !blinkState;
                drawStatusIndicator(false);
            }
        }

        if (currentTouch) {
            // Check gear button
            if (isTouchInGearButton(touchX, touchY) && !gearButtonPressed) {
                gearButtonPressed = true;
                drawGearButton(true);
                Serial.println("GEAR button pressed");
            }
            // Check mode button
            if (isTouchInModeButton(touchX, touchY) && !modeButtonPressed) {
                modeButtonPressed = true;
                drawModeButton(true);
                Serial.println("MODE button pressed");
            }
            // Check RPM up/down buttons (only in manual mode)
            if (spiSlaveGetMasterMode() == MODE_MANUAL && currentState == DISPLAY_CONNECTED) {
                if (isTouchInRpmUpButton(touchX, touchY) && !rpmUpButtonPressed) {
                    rpmUpButtonPressed = true;
                    drawRpmUpButton(true);
                    Serial.println("RPM UP button pressed");
                }
                if (isTouchInRpmDownButton(touchX, touchY) && !rpmDownButtonPressed) {
                    rpmDownButtonPressed = true;
                    drawRpmDownButton(true);
                    Serial.println("RPM DOWN button pressed");
                }
            }
        } else {
            if (gearButtonPressed) {
                gearButtonPressed = false;
                // Switch to diagnostics screen on release
                switchToScreen(SCREEN_DIAGNOSTICS);
                Serial.println("Switching to diagnostics screen");
            }
            if (modeButtonPressed) {
                modeButtonPressed = false;
                // Request mode toggle - master will decide and persist
                uint8_t currentMode = spiSlaveGetRequestedMode();
                uint8_t newMode = (currentMode == MODE_AUTO) ? MODE_MANUAL : MODE_AUTO;
                spiSlaveSetRequestedMode(newMode);
                drawModeButton(false);
                // Redraw RPM area to show/hide buttons
                drawRpmValue(displayedRpm, currentState == DISPLAY_CONNECTED);
                Serial.printf("Requested mode change to %s\n", newMode == MODE_AUTO ? "AUTO" : "MANUAL");
            }
            // Handle RPM button releases
            if (rpmUpButtonPressed) {
                rpmUpButtonPressed = false;
                // Request RPM increase - master will decide and persist
                uint16_t currentRpm = spiSlaveGetRequestedRpm();
                if (currentRpm < 5000) {
                    uint16_t newRpm = currentRpm + 100;
                    manualRpm = newRpm;  // Update local for display
                    spiSlaveSetRequestedRpm(newRpm);
                    Serial.printf("Requested RPM increase to %u\n", newRpm);
                } else {
                    drawRpmUpButton(false);
                }
            }
            if (rpmDownButtonPressed) {
                rpmDownButtonPressed = false;
                // Request RPM decrease - master will decide and persist
                uint16_t currentRpm = spiSlaveGetRequestedRpm();
                if (currentRpm > 500) {
                    uint16_t newRpm = currentRpm - 100;
                    manualRpm = newRpm;  // Update local for display
                    spiSlaveSetRequestedRpm(newRpm);
                    Serial.printf("Requested RPM decrease to %u\n", newRpm);
                } else {
                    drawRpmDownButton(false);
                }
            }
        }

        // Handle full redraw if needed
        if (needsFullRedraw) {
            drawMainScreen();
            needsFullRedraw = false;
        }
    } else if (currentScreen == SCREEN_DIAGNOSTICS) {
        if (currentTouch) {
            // Check back button
            if (isTouchInBackButton(touchX, touchY) && !backButtonPressed) {
                backButtonPressed = true;
                drawBackButton(true);
                Serial.println("BACK button pressed");
            }
            // Check SD card button
            if (sdCardPresent() && isTouchInSdButton(touchX, touchY) && !sdButtonPressed) {
                sdButtonPressed = true;
                drawSdButton(true);
                Serial.println("SD button pressed");
            }
            // Check WiFi button
            if (isTouchInWifiButton(touchX, touchY) && !wifiButtonPressed) {
                wifiButtonPressed = true;
                drawWifiButton(true);
                Serial.println("WIFI button pressed");
            }
            // Handle scrolling in content area
            if (touchY >= DIAG_CONTENT_Y && touchY < DIAG_CONTENT_Y + DIAG_CONTENT_H) {
                if (!isDragging) {
                    isDragging = true;
                    lastTouchY = touchY;
                } else if (lastTouchY >= 0) {
                    int16_t delta = lastTouchY - touchY;
                    if (abs(delta) > 3) {
                        int maxScroll = 80;  // Approximate max scroll for WiFi content
                        diagScrollOffset += delta;
                        if (diagScrollOffset < 0) diagScrollOffset = 0;
                        if (diagScrollOffset > maxScroll) diagScrollOffset = maxScroll;
                        drawDiagnosticsContent();
                        lastTouchY = touchY;
                    }
                }
            }
        } else {
            if (backButtonPressed) {
                backButtonPressed = false;
                diagScrollOffset = 0;  // Reset scroll on exit
                // Switch back to main screen on release
                switchToScreen(SCREEN_MAIN);
                Serial.println("Switching to main screen");
            }
            if (sdButtonPressed) {
                sdButtonPressed = false;
                // Switch to file browser screen on release
                switchToScreen(SCREEN_FILE_BROWSER);
                Serial.println("Switching to file browser screen");
            }
            if (wifiButtonPressed) {
                wifiButtonPressed = false;
                // Switch to WiFi screen on release
                switchToScreen(SCREEN_WIFI);
                Serial.println("Switching to WiFi screen");
            }
            isDragging = false;
            lastTouchY = -1;
        }
    } else if (currentScreen == SCREEN_FILE_BROWSER) {
        if (currentTouch) {
            // Check back arrow button
            if (isTouchInArrowButton(touchX, touchY) && !arrowButtonPressed) {
                arrowButtonPressed = true;
                drawArrowButton(true);
                Serial.println("ARROW button pressed");
            }
            // Handle scrolling in file list area
            if (isTouchInFileList(touchX, touchY)) {
                if (!isDragging) {
                    isDragging = true;
                    lastTouchY = touchY;
                } else if (lastTouchY >= 0) {
                    int16_t delta = lastTouchY - touchY;
                    if (abs(delta) > 5) {  // Threshold to avoid jitter
                        int maxScroll = fileCount - MAX_VISIBLE_FILES;
                        if (maxScroll < 0) maxScroll = 0;
                        scrollOffset += (delta > 0) ? 1 : -1;
                        if (scrollOffset < 0) scrollOffset = 0;
                        if (scrollOffset > maxScroll) scrollOffset = maxScroll;
                        drawFileListArea();
                        lastTouchY = touchY;
                    }
                }
            }
        } else {
            if (arrowButtonPressed) {
                arrowButtonPressed = false;
                // Switch back to diagnostics screen on release
                switchToScreen(SCREEN_DIAGNOSTICS);
                Serial.println("Switching to diagnostics screen");
            }
            isDragging = false;
            lastTouchY = -1;
        }
    } else if (currentScreen == SCREEN_WIFI) {
        // If full-screen keyboard is visible, handle keyboard touch
        if (keyboardVisible) {
            if (currentTouch && !lastTouchState) {
                handleKeyboardTouch(touchX, touchY);
            }
        } else {
            if (currentTouch) {
                // Check back button (top left)
                if (isTouchInWifiBackButton(touchX, touchY) && !wifiBackButtonPressed) {
                    wifiBackButtonPressed = true;
                    drawWifiBackButton(true);
                    Serial.println("WIFI BACK button pressed");
                }

                // Check scan button (in client mode)
                if (wifiMode == 1 && isTouchInWifiScanButton(touchX, touchY) && !wifiScanButtonPressed) {
                    wifiScanButtonPressed = true;
                    drawWifiScanButton(true);
                    Serial.println("SCAN button pressed");
                }

                // Check mode toggle button
                if (isTouchInWifiModeButton(touchX, touchY) && !wifiModeButtonPressed) {
                    wifiModeButtonPressed = true;
                    drawWifiModeButton();
                    Serial.println("MODE button pressed");
                }

                // Check SSID input - opens full-screen keyboard
                if (isTouchInSsidInput(touchX, touchY) && !lastTouchState) {
                    activeInput = 1;
                    showKeyboard("SSID:", wifiSsid, MAX_SSID_LEN, false);
                    Serial.println("SSID input selected");
                }

                // Check password input - opens full-screen keyboard
                if (isTouchInPassInput(touchX, touchY) && !lastTouchState) {
                    activeInput = 2;
                    showKeyboard("Password:", wifiPassword, MAX_PASS_LEN, true);
                    Serial.println("Password input selected");
                }

                // Check network list selection (in client mode)
                if (wifiMode == 1) {
                    int networkIdx;
                    if (isTouchInWifiNetwork(touchX, touchY, &networkIdx) && !lastTouchState) {
                        strncpy(wifiSsid, wifiNetworks[networkIdx].ssid, MAX_SSID_LEN);
                        wifiSsid[MAX_SSID_LEN] = '\0';
                        activeInput = 2;
                        showKeyboard("Password:", wifiPassword, MAX_PASS_LEN, true);
                        Serial.printf("Selected network: %s\n", wifiSsid);
                    }
                }
            } else {
                if (wifiBackButtonPressed) {
                    wifiBackButtonPressed = false;
                    activeInput = 0;
                    switchToScreen(SCREEN_MAIN);
                    Serial.println("Switching to main screen");
                }
                if (wifiScanButtonPressed) {
                    wifiScanButtonPressed = false;
                    drawWifiScanButton(false);
                    // Perform scan
                    scanWifiNetworks();
                    drawWifiNetworkList();
                    drawWifiBackButton(false);  // Ensure back button is visible
                    Serial.println("WiFi scan complete");
                }
                if (wifiModeButtonPressed) {
                    wifiModeButtonPressed = false;
                    // Toggle WiFi mode: 0 (disabled) <-> 1 (client)
                    wifiMode = (wifiMode == 0) ? 1 : 0;
                    saveWifiSettings();

                    // Apply WiFi mode
                    if (wifiMode == 0) {
                        WiFi.disconnect(true);
                        WiFi.mode(WIFI_OFF);
                        wifiNetworkCount = 0;
                    } else if (wifiMode == 1) {
                        wifiNetworkCount = 0;
                    }

                    drawWifiScreenContent();
                    drawWifiBackButton(false);  // Ensure back button is visible
                    Serial.printf("WiFi mode: %s\n", getWifiModeString());
                }
            }
        }
    }

    lastTouchState = currentTouch;
}

ScreenType displayGetScreen() {
    return currentScreen;
}
