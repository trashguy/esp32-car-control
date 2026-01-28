#include "display.h"
#include "i2c_slave.h"
#include "sd_card.h"
#include "shared/config.h"
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <SD_MMC.h>
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

// Test button layout
#define BTN_WIDTH     100
#define BTN_HEIGHT    40
#define BTN_X         ((SCREEN_WIDTH - BTN_WIDTH) / 2)
#define BTN_Y         (SCREEN_HEIGHT - BTN_HEIGHT - 10)
#define BTN_RADIUS    8

// Gear button layout (bottom right)
#define GEAR_BTN_SIZE   36
#define GEAR_BTN_X      (SCREEN_WIDTH - GEAR_BTN_SIZE - 8)
#define GEAR_BTN_Y      (SCREEN_HEIGHT - GEAR_BTN_SIZE - 8)

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

// Screen state
static ScreenType currentScreen = SCREEN_MAIN;

// Button state
static bool testButtonPressed = false;
static bool gearButtonPressed = false;
static bool backButtonPressed = false;
static bool sdButtonPressed = false;
static bool arrowButtonPressed = false;
static bool lastTouchState = false;

// File browser state
static char* fileList[MAX_FILES];
static int fileCount = 0;
static int scrollOffset = 0;
static int16_t lastTouchY = -1;
static bool isDragging = false;

static void drawBackground() {
    tft.fillScreen(COLOR_BACKGROUND);

    // Draw title
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.drawString("VOLVO C60 RPM", SCREEN_WIDTH / 2, 10);

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
}

static void drawStatusIndicator(bool connected) {
    // Clear status area
    tft.fillRect(0, STATUS_Y_POS - 10, SCREEN_WIDTH, 20, COLOR_BACKGROUND);

    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);

    if (connected) {
        tft.setTextColor(COLOR_CONNECTED, COLOR_BACKGROUND);
        tft.fillCircle(SCREEN_WIDTH / 2 - 50, STATUS_Y_POS, 5, COLOR_CONNECTED);
        tft.drawString("CONNECTED", SCREEN_WIDTH / 2 + 10, STATUS_Y_POS);
    } else {
        // Blink the disconnected indicator
        uint16_t indicatorColor = blinkState ? COLOR_DISCONNECTED : COLOR_BACKGROUND;
        tft.setTextColor(COLOR_DISCONNECTED, COLOR_BACKGROUND);
        tft.fillCircle(SCREEN_WIDTH / 2 - 50, STATUS_Y_POS, 5, indicatorColor);
        tft.drawString("DISCONNECTED", SCREEN_WIDTH / 2 + 10, STATUS_Y_POS);
    }
}

static void drawTestButton(bool pressed) {
    uint16_t btnColor = pressed ? COLOR_BTN_PRESSED : COLOR_BTN_NORMAL;

    // Draw rounded rectangle button
    tft.fillRoundRect(BTN_X, BTN_Y, BTN_WIDTH, BTN_HEIGHT, BTN_RADIUS, btnColor);
    tft.drawRoundRect(BTN_X, BTN_Y, BTN_WIDTH, BTN_HEIGHT, BTN_RADIUS, COLOR_BTN_TEXT);

    // Draw button text
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(COLOR_BTN_TEXT, btnColor);
    tft.drawString("TEST", BTN_X + BTN_WIDTH / 2, BTN_Y + BTN_HEIGHT / 2);
}

static bool isTouchInTestButton(int16_t x, int16_t y) {
    return (x >= BTN_X && x <= BTN_X + BTN_WIDTH &&
            y >= BTN_Y && y <= BTN_Y + BTN_HEIGHT);
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

    File root = SD_MMC.open("/");
    if (!root || !root.isDirectory()) return;

    File file = root.openNextFile();
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
    // Clear file list area
    tft.fillRect(0, FILE_LIST_Y_START, SCREEN_WIDTH, FILE_LIST_Y_END - FILE_LIST_Y_START, COLOR_BACKGROUND);

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
                tft.drawString(fileList[i], 10, y);
            } else {
                tft.setTextColor(COLOR_RPM_TEXT, COLOR_BACKGROUND);
                tft.drawString(fileList[i], 10, y);
            }
        }
        y += FILE_LINE_HEIGHT;
    }

    // Draw scroll indicator if needed
    if (fileCount > MAX_VISIBLE_FILES) {
        int16_t scrollBarHeight = FILE_LIST_Y_END - FILE_LIST_Y_START;
        int16_t thumbHeight = (MAX_VISIBLE_FILES * scrollBarHeight) / fileCount;
        int16_t thumbY = FILE_LIST_Y_START + (scrollOffset * scrollBarHeight) / fileCount;

        tft.fillRect(SCREEN_WIDTH - 6, FILE_LIST_Y_START, 4, scrollBarHeight, COLOR_BTN_NORMAL);
        tft.fillRect(SCREEN_WIDTH - 6, thumbY, 4, thumbHeight, COLOR_BTN_TEXT);
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

    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
    int16_t y = 45;
    int16_t lineHeight = 18;

    // I2C Status
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.drawString("I2C Status:", 10, y);
    tft.setTextColor(i2cIsConnected() ? COLOR_CONNECTED : COLOR_DISCONNECTED, COLOR_BACKGROUND);
    tft.drawString(i2cIsConnected() ? "Connected" : "Disconnected", 120, y);
    y += lineHeight;

    // Valid packets
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.drawString("Valid Packets:", 10, y);
    tft.setTextColor(COLOR_RPM_TEXT, COLOR_BACKGROUND);
    char buf[32];
    snprintf(buf, sizeof(buf), "%lu", i2cGetValidPacketCount());
    tft.drawString(buf, 120, y);
    y += lineHeight;

    // Invalid packets
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.drawString("Invalid Packets:", 10, y);
    tft.setTextColor(COLOR_RPM_TEXT, COLOR_BACKGROUND);
    snprintf(buf, sizeof(buf), "%lu", i2cGetInvalidPacketCount());
    tft.drawString(buf, 120, y);
    y += lineHeight;

    // Last RPM
    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.drawString("Last RPM:", 10, y);
    tft.setTextColor(COLOR_RPM_TEXT, COLOR_BACKGROUND);
    snprintf(buf, sizeof(buf), "%u", i2cGetLastRpm());
    tft.drawString(buf, 120, y);
    y += lineHeight + 5;

    // SD Card section
    tft.drawLine(20, y, SCREEN_WIDTH - 20, y, COLOR_LABEL);
    y += 8;

    tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
    tft.drawString("SD Card:", 10, y);
    if (sdCardPresent()) {
        tft.setTextColor(COLOR_CONNECTED, COLOR_BACKGROUND);
        tft.drawString(sdCardType(), 120, y);
        y += lineHeight;

        tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
        tft.drawString("Total:", 10, y);
        tft.setTextColor(COLOR_RPM_TEXT, COLOR_BACKGROUND);
        snprintf(buf, sizeof(buf), "%llu MB", sdCardTotalBytes() / (1024 * 1024));
        tft.drawString(buf, 120, y);
        y += lineHeight;

        tft.setTextColor(COLOR_LABEL, COLOR_BACKGROUND);
        tft.drawString("Used:", 10, y);
        tft.setTextColor(COLOR_RPM_TEXT, COLOR_BACKGROUND);
        snprintf(buf, sizeof(buf), "%llu MB", sdCardUsedBytes() / (1024 * 1024));
        tft.drawString(buf, 120, y);
    } else {
        tft.setTextColor(COLOR_DISCONNECTED, COLOR_BACKGROUND);
        tft.drawString("Not Present", 120, y);
    }

    // Draw back button (center)
    drawBackButton(false);

    // Draw SD card button (bottom right) - only if SD card present
    if (sdCardPresent()) {
        drawSdButton(false);
    }
}

static void drawMainScreen() {
    drawBackground();
    drawRpmValue(displayedRpm, currentState == DISPLAY_CONNECTED);
    drawStatusIndicator(currentState == DISPLAY_CONNECTED);
    drawTestButton(testButtonPressed);
    drawGearButton(gearButtonPressed);
}

static void switchToScreen(ScreenType screen) {
    currentScreen = screen;
    if (screen == SCREEN_MAIN) {
        drawMainScreen();
    } else if (screen == SCREEN_DIAGNOSTICS) {
        drawDiagnosticsScreen();
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

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_FREQUENCY);  // Fast mode (400kHz)

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

    // Draw initial screen
    drawBackground();
    drawRpmValue(0, false);
    drawStatusIndicator(false);
    drawTestButton(false);
    drawGearButton(false);
    needsFullRedraw = false;

    // Switch to slave mode for RPM reception
    i2cEnableSlaveMode();
    Serial.println("I2C switched to slave mode for RPM reception");

    return true;
}

void displayUpdateRpm(uint16_t rpm) {
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

    if (newState != currentState) {
        currentState = newState;
        // Only update display if on main screen
        if (currentScreen == SCREEN_MAIN) {
            if (!connected) {
                drawRpmValue(0, false);
            }
            drawStatusIndicator(connected);
        }
    }
}

bool displayGetTouch(int16_t* x, int16_t* y) {
    if (!touchInitialized) return false;

    // Switch to master mode to read touch
    i2cEnableMasterMode();

    // Read touch data starting from register 0x02 (status)
    Wire.beginTransmission((uint8_t)FT6336G_ADDR);
    Wire.write(FT6336G_REG_STATUS);
    if (Wire.endTransmission() != 0) {
        i2cEnableSlaveMode();
        return false;
    }

    Wire.requestFrom((uint8_t)FT6336G_ADDR, (uint8_t)5);  // Status + XH, XL, YH, YL
    if (Wire.available() < 5) {
        i2cEnableSlaveMode();
        return false;
    }

    uint8_t status = Wire.read();
    uint8_t touches = status & 0x0F;

    if (touches == 0 || touches > 2) {
        i2cEnableSlaveMode();
        return false;
    }

    uint8_t xh = Wire.read();
    uint8_t xl = Wire.read();
    uint8_t yh = Wire.read();
    uint8_t yl = Wire.read();

    // Switch back to slave mode
    i2cEnableSlaveMode();

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
            // Check test button
            if (isTouchInTestButton(touchX, touchY) && !testButtonPressed) {
                testButtonPressed = true;
                drawTestButton(true);
                Serial.println("TEST button pressed");
            }
            // Check gear button
            if (isTouchInGearButton(touchX, touchY) && !gearButtonPressed) {
                gearButtonPressed = true;
                drawGearButton(true);
                Serial.println("GEAR button pressed");
            }
        } else {
            if (testButtonPressed) {
                testButtonPressed = false;
                drawTestButton(false);
                Serial.println("TEST button released");
            }
            if (gearButtonPressed) {
                gearButtonPressed = false;
                // Switch to diagnostics screen on release
                switchToScreen(SCREEN_DIAGNOSTICS);
                Serial.println("Switching to diagnostics screen");
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
        } else {
            if (backButtonPressed) {
                backButtonPressed = false;
                // Switch back to main screen on release
                switchToScreen(SCREEN_MAIN);
                Serial.println("Switching to main screen");
            }
        }
    }

    lastTouchState = currentTouch;
}

ScreenType displayGetScreen() {
    return currentScreen;
}
