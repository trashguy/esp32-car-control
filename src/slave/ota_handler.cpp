#include "slave/ota_handler.h"
#include "slave/spi_ota.h"
#include "display/display_common.h"
#include "sd_card.h"
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <SD_MMC.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <MD5Builder.h>

// OTA popup - conditionally include based on UI system
#if USE_LVGL_UI
#include "display/lvgl/ui_ota_popup.h"
// Map legacy function names to LVGL equivalents
#define otaPopupSetProgress(p) ui_ota_popup_set_progress(p)
#else
#include "display/legacy/screen_ota_popup.h"
#endif

// =============================================================================
// Local State
// =============================================================================

static OtaState currentState = OTA_STATE_IDLE;
static OtaPackageInfo packageInfo;
static char errorMessage[64] = "";
static uint8_t currentProgress = 0;
static bool otaInitialized = false;
static bool mdnsStarted = false;
static bool controllerUpdateActive = false;  // True when user pressed Update for controller

// TCP server for receiving update packages
static WiFiServer* packageServer = nullptr;
static WiFiClient packageClient;
static fs::File packageFile;
static uint32_t bytesReceived = 0;
static uint32_t expectedBytes = 0;
static unsigned long receiveStartTime = 0;

// =============================================================================
// Forward Declarations
// =============================================================================

static void initArduinoOTA();
static void initPackageServer();
static void handlePackageServer();
static bool extractPackage();
static bool parseManifest();
static bool verifyFirmware(const char* path, uint32_t expectedSize, const char* expectedMd5);
static void saveState();
static void loadState();

// =============================================================================
// Initialization
// =============================================================================

void otaHandlerInit() {
    if (otaInitialized) return;
    
    // Only initialize if WiFi is connected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[OTA] WiFi not connected, skipping init");
        return;
    }
    
    Serial.println("[OTA] Initializing...");
    
    // Start mDNS
    if (!mdnsStarted) {
        if (MDNS.begin(OTA_HOSTNAME)) {
            // Add service for discovery
            MDNS.addService(OTA_SERVICE_TYPE, "tcp", OTA_PORT_ARDUINO);
            MDNS.addServiceTxt(OTA_SERVICE_TYPE, "tcp", "board", "esp32s3");
            MDNS.addServiceTxt(OTA_SERVICE_TYPE, "tcp", "type", "display");
            mdnsStarted = true;
            Serial.printf("[OTA] mDNS started: %s.local\n", OTA_HOSTNAME);
        } else {
            Serial.println("[OTA] mDNS failed to start");
        }
    }
    
    // Initialize ArduinoOTA for self-updates
    initArduinoOTA();
    
    // Initialize package server for receiving update packages
    initPackageServer();
    
    // Check if there's a pending update from before reboot
    loadState();
    
    otaInitialized = true;
    Serial.printf("[OTA] Ready. IP: %s\n", WiFi.localIP().toString().c_str());
}

static void initArduinoOTA() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPort(OTA_PORT_ARDUINO);
    
#if PRODUCTION_BUILD
    ArduinoOTA.setPassword(OTA_PASSWORD);
    Serial.println("[OTA] Password protection enabled");
#endif
    
    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        Serial.printf("[OTA] Start updating %s\n", type.c_str());
        currentState = OTA_STATE_INSTALLING_DISPLAY;
        otaDrawSelfUpdateStart();
    });
    
    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Update complete!");
        otaDrawSelfUpdateEnd(true);
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        currentProgress = (progress * 100) / total;
        otaDrawSelfUpdateProgress(progress, total);
        Serial.printf("[OTA] Progress: %u%%\r", currentProgress);
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        currentState = OTA_STATE_ERROR;
        const char* errMsg = "Unknown error";
        switch (error) {
            case OTA_AUTH_ERROR: errMsg = "Auth failed"; break;
            case OTA_BEGIN_ERROR: errMsg = "Begin failed"; break;
            case OTA_CONNECT_ERROR: errMsg = "Connect failed"; break;
            case OTA_RECEIVE_ERROR: errMsg = "Receive failed"; break;
            case OTA_END_ERROR: errMsg = "End failed"; break;
        }
        snprintf(errorMessage, sizeof(errorMessage), "OTA Error: %s", errMsg);
        Serial.printf("\n[OTA] Error: %s\n", errMsg);
        otaDrawSelfUpdateEnd(false);
    });
    
    ArduinoOTA.begin();
    Serial.printf("[OTA] ArduinoOTA listening on port %d\n", OTA_PORT_ARDUINO);
}

static void initPackageServer() {
    if (packageServer != nullptr) {
        delete packageServer;
    }
    packageServer = new WiFiServer(OTA_PORT_PACKAGE);
    packageServer->begin();
    Serial.printf("[OTA] Package server listening on port %d\n", OTA_PORT_PACKAGE);
}

// =============================================================================
// Main Loop
// =============================================================================

void otaHandlerLoop() {
    if (!otaInitialized) {
        // Try to initialize if WiFi just connected
        if (WiFi.status() == WL_CONNECTED) {
            otaHandlerInit();
        }
        return;
    }
    
    // Handle ArduinoOTA
    ArduinoOTA.handle();
    
    // Handle package uploads
    handlePackageServer();
    
    // Check for receive timeout
    if (currentState == OTA_STATE_RECEIVING) {
        if (millis() - receiveStartTime > OTA_RECEIVE_TIMEOUT_MS) {
            currentState = OTA_STATE_ERROR;
            snprintf(errorMessage, sizeof(errorMessage), "Receive timeout");
            Serial.println("[OTA] Receive timeout");
            if (packageFile) {
                packageFile.close();
            }
            if (packageClient) {
                packageClient.stop();
            }
        }
    }
}

// =============================================================================
// Package Server Handler
// =============================================================================

static void handlePackageServer() {
    if (packageServer == nullptr) return;
    
    // Check for new client connection
    if (!packageClient || !packageClient.connected()) {
        WiFiClient newClient = packageServer->available();
        if (newClient) {
            packageClient = newClient;
            Serial.println("[OTA] Package client connected");
            
            // Read binary header (16 bytes)
            OtaPacketHeader header;
            size_t headerBytesRead = 0;
            unsigned long headerTimeout = millis() + 5000;  // 5 second timeout
            
            while (headerBytesRead < sizeof(header) && millis() < headerTimeout) {
                if (packageClient.available()) {
                    size_t toRead = sizeof(header) - headerBytesRead;
                    size_t read = packageClient.read(
                        reinterpret_cast<uint8_t*>(&header) + headerBytesRead, 
                        toRead
                    );
                    headerBytesRead += read;
                }
                if (headerBytesRead < sizeof(header)) {
                    delay(1);  // Small delay to avoid busy-waiting
                }
            }
            
            if (headerBytesRead != sizeof(header)) {
                Serial.printf("[OTA] Header timeout (got %u bytes)\n", headerBytesRead);
                packageClient.write((uint8_t)0xFF);  // Reject
                packageClient.stop();
                return;
            }
            
            // Validate header
            if (header.magic != OTA_MAGIC) {
                Serial.printf("[OTA] Invalid magic: 0x%08X (expected 0x%08X)\n", 
                              header.magic, OTA_MAGIC);
                packageClient.write((uint8_t)0xFF);  // Reject
                packageClient.stop();
                return;
            }
            
            if (header.version != OTA_PROTOCOL_VERSION) {
                Serial.printf("[OTA] Unsupported protocol version: %u\n", header.version);
                packageClient.write((uint8_t)0xFF);  // Reject
                packageClient.stop();
                return;
            }
            
            expectedBytes = header.packageSize;
            Serial.printf("[OTA] Expecting %u bytes (protocol v%u)\n", 
                          expectedBytes, header.version);
            
            // Create OTA directory if needed
            if (!SD_MMC.exists(OTA_DIR)) {
                SD_MMC.mkdir(OTA_DIR);
            }
            
            // Open file for writing
            packageFile = SD_MMC.open(OTA_PACKAGE_PATH, FILE_WRITE);
            if (!packageFile) {
                Serial.println("[OTA] Failed to open file for writing");
                packageClient.write((uint8_t)0xFF);  // Reject
                packageClient.stop();
                currentState = OTA_STATE_ERROR;
                snprintf(errorMessage, sizeof(errorMessage), "Failed to open file");
                return;
            }
            
            bytesReceived = 0;
            receiveStartTime = millis();
            currentState = OTA_STATE_RECEIVING;
            currentProgress = 0;
        }
    }
    
    // Receive data
    if (currentState == OTA_STATE_RECEIVING && packageClient && packageClient.connected()) {
        while (packageClient.available()) {
            uint8_t buffer[1024];
            size_t len = packageClient.read(buffer, sizeof(buffer));
            if (len > 0) {
                packageFile.write(buffer, len);
                bytesReceived += len;
                currentProgress = (bytesReceived * 100) / expectedBytes;
                
                // Update progress occasionally
                static unsigned long lastProgressPrint = 0;
                if (millis() - lastProgressPrint > 500) {
                    Serial.printf("[OTA] Received %u / %u bytes (%u%%)\r", 
                                  bytesReceived, expectedBytes, currentProgress);
                    lastProgressPrint = millis();
                }
            }
        }
        
        // Check if complete
        if (bytesReceived >= expectedBytes) {
            packageFile.close();
            Serial.printf("\n[OTA] Package received: %u bytes\n", bytesReceived);
            
            // Extract and parse
            if (extractPackage() && parseManifest()) {
                currentState = OTA_STATE_PACKAGE_READY;
                packageClient.write((uint8_t)0x00);  // Success
                Serial.printf("[OTA] Package ready: v%s\n", packageInfo.version);
            } else {
                currentState = OTA_STATE_ERROR;
                packageClient.write((uint8_t)0xFF);  // Error
            }
            packageClient.stop();
        }
    }
}

// =============================================================================
// Package Extraction
// =============================================================================

static bool extractPackage() {
    Serial.println("[OTA] Extracting package...");
    
    // For now, we expect the package to be a simple concatenated format:
    // - 4 bytes: manifest size
    // - manifest.json
    // - 4 bytes: display.bin size  
    // - display.bin
    // - 4 bytes: controller.bin size
    // - controller.bin
    //
    // TODO: Implement proper ZIP extraction using miniz or similar
    // For MVP, the ota-pusher tool will create this simple format
    
    fs::File pkg = SD_MMC.open(OTA_PACKAGE_PATH, FILE_READ);
    if (!pkg) {
        snprintf(errorMessage, sizeof(errorMessage), "Cannot open package");
        return false;
    }
    
    // Read manifest
    uint32_t manifestSize = 0;
    if (pkg.read((uint8_t*)&manifestSize, 4) != 4 || manifestSize > 4096) {
        pkg.close();
        snprintf(errorMessage, sizeof(errorMessage), "Invalid manifest size");
        return false;
    }
    
    char* manifestData = (char*)malloc(manifestSize + 1);
    if (!manifestData) {
        pkg.close();
        snprintf(errorMessage, sizeof(errorMessage), "Out of memory");
        return false;
    }
    
    if (pkg.read((uint8_t*)manifestData, manifestSize) != manifestSize) {
        free(manifestData);
        pkg.close();
        snprintf(errorMessage, sizeof(errorMessage), "Failed to read manifest");
        return false;
    }
    manifestData[manifestSize] = '\0';
    
    // Write manifest to file
    fs::File manifestFile = SD_MMC.open(OTA_MANIFEST_PATH, FILE_WRITE);
    if (manifestFile) {
        manifestFile.write((uint8_t*)manifestData, manifestSize);
        manifestFile.close();
    }
    free(manifestData);
    
    // Read display firmware
    uint32_t displaySize = 0;
    if (pkg.read((uint8_t*)&displaySize, 4) != 4) {
        pkg.close();
        snprintf(errorMessage, sizeof(errorMessage), "Invalid display size");
        return false;
    }
    
    fs::File displayFile = SD_MMC.open(OTA_DISPLAY_FW_PATH, FILE_WRITE);
    if (!displayFile) {
        pkg.close();
        snprintf(errorMessage, sizeof(errorMessage), "Cannot create display.bin");
        return false;
    }
    
    uint8_t buffer[1024];
    uint32_t remaining = displaySize;
    while (remaining > 0) {
        size_t toRead = min((size_t)remaining, sizeof(buffer));
        size_t read = pkg.read(buffer, toRead);
        if (read == 0) break;
        displayFile.write(buffer, read);
        remaining -= read;
    }
    displayFile.close();
    
    if (remaining > 0) {
        pkg.close();
        snprintf(errorMessage, sizeof(errorMessage), "Display firmware incomplete");
        return false;
    }
    
    // Read controller firmware
    uint32_t controllerSize = 0;
    if (pkg.read((uint8_t*)&controllerSize, 4) != 4) {
        pkg.close();
        snprintf(errorMessage, sizeof(errorMessage), "Invalid controller size");
        return false;
    }
    
    fs::File controllerFile = SD_MMC.open(OTA_CONTROLLER_FW_PATH, FILE_WRITE);
    if (!controllerFile) {
        pkg.close();
        snprintf(errorMessage, sizeof(errorMessage), "Cannot create controller.bin");
        return false;
    }
    
    remaining = controllerSize;
    while (remaining > 0) {
        size_t toRead = min((size_t)remaining, sizeof(buffer));
        size_t read = pkg.read(buffer, toRead);
        if (read == 0) break;
        controllerFile.write(buffer, read);
        remaining -= read;
    }
    controllerFile.close();
    pkg.close();
    
    if (remaining > 0) {
        snprintf(errorMessage, sizeof(errorMessage), "Controller firmware incomplete");
        return false;
    }
    
    // Delete original package to save space
    SD_MMC.remove(OTA_PACKAGE_PATH);
    
    Serial.println("[OTA] Package extracted successfully");
    return true;
}

static bool parseManifest() {
    Serial.println("[OTA] Parsing manifest...");
    
    fs::File manifestFile = SD_MMC.open(OTA_MANIFEST_PATH, FILE_READ);
    if (!manifestFile) {
        snprintf(errorMessage, sizeof(errorMessage), "Cannot open manifest");
        return false;
    }
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, manifestFile);
    manifestFile.close();
    
    if (error) {
        snprintf(errorMessage, sizeof(errorMessage), "JSON parse error: %s", error.c_str());
        return false;
    }
    
    // Extract info
    const char* version = doc["version"] | "unknown";
    strncpy(packageInfo.version, version, sizeof(packageInfo.version) - 1);
    
    packageInfo.displaySize = doc["display"]["size"] | 0;
    packageInfo.controllerSize = doc["controller"]["size"] | 0;
    
    const char* displayMd5 = doc["display"]["md5"] | "";
    strncpy(packageInfo.displayMd5, displayMd5, sizeof(packageInfo.displayMd5) - 1);
    
    const char* controllerMd5 = doc["controller"]["md5"] | "";
    strncpy(packageInfo.controllerMd5, controllerMd5, sizeof(packageInfo.controllerMd5) - 1);
    
    // Verify files exist and match expected sizes
    fs::File df = SD_MMC.open(OTA_DISPLAY_FW_PATH, FILE_READ);
    if (!df || df.size() != packageInfo.displaySize) {
        if (df) df.close();
        snprintf(errorMessage, sizeof(errorMessage), "Display firmware size mismatch");
        return false;
    }
    df.close();
    
    fs::File cf = SD_MMC.open(OTA_CONTROLLER_FW_PATH, FILE_READ);
    if (!cf || cf.size() != packageInfo.controllerSize) {
        if (cf) cf.close();
        snprintf(errorMessage, sizeof(errorMessage), "Controller firmware size mismatch");
        return false;
    }
    cf.close();
    
    packageInfo.valid = true;
    
    Serial.printf("[OTA] Manifest: v%s, display=%u bytes, controller=%u bytes\n",
                  packageInfo.version, packageInfo.displaySize, packageInfo.controllerSize);
    
    return true;
}

// =============================================================================
// State Persistence
// =============================================================================

static void saveState() {
    StaticJsonDocument<256> doc;
    doc["state"] = (int)currentState;
    doc["version"] = packageInfo.version;
    
    fs::File stateFile = SD_MMC.open(OTA_STATE_PATH, FILE_WRITE);
    if (stateFile) {
        serializeJson(doc, stateFile);
        stateFile.close();
        Serial.printf("[OTA] State saved: %d\n", (int)currentState);
    }
}

static void loadState() {
    if (!SD_MMC.exists(OTA_STATE_PATH)) {
        return;
    }
    
    fs::File stateFile = SD_MMC.open(OTA_STATE_PATH, FILE_READ);
    if (!stateFile) return;
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, stateFile);
    stateFile.close();
    
    if (error) return;
    
    int savedState = doc["state"] | 0;
    
    // If we rebooted during display install, controller should be pending
    if (savedState == OTA_STATE_INSTALLING_DISPLAY) {
        Serial.println("[OTA] Detected reboot after display update");
        
        // Check if controller firmware exists
        if (SD_MMC.exists(OTA_CONTROLLER_FW_PATH)) {
            currentState = OTA_STATE_PENDING_CONTROLLER;
            parseManifest();  // Reload package info
            Serial.println("[OTA] Controller update pending");
        } else {
            // No controller firmware, update is complete
            currentState = OTA_STATE_COMPLETE;
            otaClearState();
        }
    }
    // If we were waiting for user, restore that state
    else if (savedState == OTA_STATE_PACKAGE_READY) {
        if (SD_MMC.exists(OTA_DISPLAY_FW_PATH) && SD_MMC.exists(OTA_CONTROLLER_FW_PATH)) {
            currentState = OTA_STATE_PACKAGE_READY;
            parseManifest();
            Serial.println("[OTA] Pending update restored");
        } else {
            otaClearState();
        }
    }
}

// =============================================================================
// Public API
// =============================================================================

bool otaInProgress() {
    return currentState == OTA_STATE_RECEIVING ||
           currentState == OTA_STATE_INSTALLING_DISPLAY ||
           currentState == OTA_STATE_INSTALLING_CONTROLLER;
}

OtaState otaGetState() {
    return currentState;
}

const OtaPackageInfo* otaGetPackageInfo() {
    return packageInfo.valid ? &packageInfo : nullptr;
}

const char* otaGetErrorMessage() {
    return errorMessage;
}

uint8_t otaGetProgress() {
    return currentProgress;
}

bool otaStartInstall() {
    if (currentState != OTA_STATE_PACKAGE_READY) {
        return false;
    }
    
    Serial.println("[OTA] Starting display firmware update...");
    currentState = OTA_STATE_INSTALLING_DISPLAY;
    saveState();
    
    // Use ESP OTA APIs to write display firmware
    fs::File fw = SD_MMC.open(OTA_DISPLAY_FW_PATH, FILE_READ);
    if (!fw) {
        currentState = OTA_STATE_ERROR;
        snprintf(errorMessage, sizeof(errorMessage), "Cannot open display.bin");
        return false;
    }
    
    if (!Update.begin(fw.size())) {
        fw.close();
        currentState = OTA_STATE_ERROR;
        snprintf(errorMessage, sizeof(errorMessage), "Update.begin failed");
        return false;
    }
    
    uint8_t buffer[1024];
    size_t written = 0;
    size_t totalSize = fw.size();
    uint8_t lastProgress = 0;
    while (fw.available()) {
        size_t len = fw.read(buffer, sizeof(buffer));
        if (Update.write(buffer, len) != len) {
            fw.close();
            Update.abort();
            currentState = OTA_STATE_ERROR;
            snprintf(errorMessage, sizeof(errorMessage), "Update.write failed");
            return false;
        }
        written += len;
        currentProgress = (written * 100) / totalSize;
        
        // Update progress bar on display
        if (currentProgress != lastProgress) {
            lastProgress = currentProgress;
            otaPopupSetProgress(currentProgress);
        }
    }
    fw.close();
    
    // Delete display firmware file (controller still needed after reboot)
    SD_MMC.remove(OTA_DISPLAY_FW_PATH);
    
    if (!Update.end(true)) {
        currentState = OTA_STATE_ERROR;
        snprintf(errorMessage, sizeof(errorMessage), "Update.end failed");
        return false;
    }
    
    Serial.println("[OTA] Display firmware written, rebooting...");
    delay(500);
    ESP.restart();
    
    return true;  // Won't reach here
}

void otaDismissUpdate() {
    if (currentState == OTA_STATE_PACKAGE_READY || currentState == OTA_STATE_COMPLETE) {
        Serial.println("[OTA] Update dismissed");
        otaClearState();
        currentState = OTA_STATE_IDLE;
    }
}

bool otaControllerPending() {
    return currentState == OTA_STATE_PENDING_CONTROLLER;
}

bool otaStartControllerUpdate() {
    // User pressed Update button - activate controller OTA mode
    // This tells the SPI slave to pause normal traffic and handle OTA packets
    if (!spiOtaHasFirmware()) {
        Serial.println("[OTA] No controller firmware available");
        return false;
    }
    
    controllerUpdateActive = true;
    currentState = OTA_STATE_INSTALLING_CONTROLLER;
    Serial.println("[OTA] Controller update started - SPI OTA mode active");
    return true;
}

bool otaControllerUpdateInProgress() {
    return controllerUpdateActive;
}

void otaAbortControllerUpdate() {
    // Master returned to normal SPI mode without sending DONE
    // This happens if master reboots or user cancels on master side
    if (controllerUpdateActive) {
        Serial.println("[OTA] Controller update aborted - master returned to normal mode");
        controllerUpdateActive = false;
        currentState = OTA_STATE_PACKAGE_READY;  // Keep firmware for retry
    }
}

void otaClearState() {
    // Remove all OTA files
    if (SD_MMC.exists(OTA_PACKAGE_PATH)) SD_MMC.remove(OTA_PACKAGE_PATH);
    if (SD_MMC.exists(OTA_MANIFEST_PATH)) SD_MMC.remove(OTA_MANIFEST_PATH);
    if (SD_MMC.exists(OTA_DISPLAY_FW_PATH)) SD_MMC.remove(OTA_DISPLAY_FW_PATH);
    if (SD_MMC.exists(OTA_CONTROLLER_FW_PATH)) SD_MMC.remove(OTA_CONTROLLER_FW_PATH);
    if (SD_MMC.exists(OTA_STATE_PATH)) SD_MMC.remove(OTA_STATE_PATH);
    
    memset(&packageInfo, 0, sizeof(packageInfo));
    errorMessage[0] = '\0';
    currentProgress = 0;
    controllerUpdateActive = false;  // Reset OTA mode flag
    currentState = OTA_STATE_IDLE;
    
    Serial.println("[OTA] State cleared");
}

// =============================================================================
// Progress Display (called by ArduinoOTA callbacks)
// =============================================================================

void otaDrawSelfUpdateStart() {
    TFT_eSPI& tft = getTft();
    
    // Dim background
    for (int y = 0; y < SCREEN_HEIGHT; y += 2) {
        for (int x = 0; x < SCREEN_WIDTH; x += 2) {
            tft.drawPixel(x, y, COLOR_BACKGROUND);
        }
    }
    
    // Draw modal
    int16_t modalW = 280;
    int16_t modalH = 100;
    int16_t modalX = (SCREEN_WIDTH - modalW) / 2;
    int16_t modalY = (SCREEN_HEIGHT - modalH) / 2;
    
    tft.fillRoundRect(modalX, modalY, modalW, modalH, 8, COLOR_BTN_NORMAL);
    tft.drawRoundRect(modalX, modalY, modalW, modalH, 8, COLOR_BTN_TEXT);
    
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(COLOR_RPM_TEXT, COLOR_BTN_NORMAL);
    tft.setTextSize(2);
    tft.drawString("UPDATING FIRMWARE", SCREEN_WIDTH / 2, modalY + 15);
    
    tft.setTextSize(1);
    tft.setTextColor(COLOR_WARNING, COLOR_BTN_NORMAL);
    tft.drawString("Do not power off!", SCREEN_WIDTH / 2, modalY + modalH - 20);
}

void otaDrawSelfUpdateProgress(unsigned int progress, unsigned int total) {
    TFT_eSPI& tft = getTft();
    
    int16_t modalW = 280;
    int16_t modalH = 100;
    int16_t modalX = (SCREEN_WIDTH - modalW) / 2;
    int16_t modalY = (SCREEN_HEIGHT - modalH) / 2;
    
    // Progress bar
    int16_t barX = modalX + 20;
    int16_t barY = modalY + 50;
    int16_t barW = modalW - 40;
    int16_t barH = 20;
    
    int pct = (progress * 100) / total;
    int16_t fillW = (barW * pct) / 100;
    
    tft.drawRect(barX, barY, barW, barH, COLOR_BTN_TEXT);
    tft.fillRect(barX + 1, barY + 1, fillW - 2, barH - 2, COLOR_CONNECTED);
    
    // Percentage text
    char pctStr[8];
    snprintf(pctStr, sizeof(pctStr), "%d%%", pct);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COLOR_RPM_TEXT, COLOR_CONNECTED);
    tft.setTextSize(1);
    tft.drawString(pctStr, SCREEN_WIDTH / 2, barY + barH / 2);
}

void otaDrawSelfUpdateEnd(bool success) {
    TFT_eSPI& tft = getTft();
    
    int16_t modalW = 280;
    int16_t modalH = 100;
    int16_t modalX = (SCREEN_WIDTH - modalW) / 2;
    int16_t modalY = (SCREEN_HEIGHT - modalH) / 2;
    
    tft.fillRoundRect(modalX, modalY, modalW, modalH, 8, COLOR_BTN_NORMAL);
    tft.drawRoundRect(modalX, modalY, modalW, modalH, 8, COLOR_BTN_TEXT);
    
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(2);
    
    if (success) {
        tft.setTextColor(COLOR_CONNECTED, COLOR_BTN_NORMAL);
        tft.drawString("UPDATE COMPLETE", SCREEN_WIDTH / 2, modalY + 35);
        tft.setTextSize(1);
        tft.drawString("Rebooting...", SCREEN_WIDTH / 2, modalY + 65);
    } else {
        tft.setTextColor(COLOR_DISCONNECTED, COLOR_BTN_NORMAL);
        tft.drawString("UPDATE FAILED", SCREEN_WIDTH / 2, modalY + 35);
        tft.setTextSize(1);
        tft.setTextColor(COLOR_BTN_TEXT, COLOR_BTN_NORMAL);
        tft.drawString(errorMessage, SCREEN_WIDTH / 2, modalY + 65);
    }
}
