#include "usb_msc.h"
#include "sd_card.h"
#include <Arduino.h>

// =============================================================================
// USB Mass Storage Implementation
// =============================================================================
//
// PRODUCTION_BUILD=1: Full MSC implementation with TinyUSB (ARDUINO_USB_MODE=0)
// PRODUCTION_BUILD=0: Stub functions only (ARDUINO_USB_MODE=1, easy flashing)
//
// The development build uses hardware USB-JTAG which doesn't support composite
// CDC+MSC devices, so we compile out all MSC functionality.
// =============================================================================

#if PRODUCTION_BUILD

#include <SD_MMC.h>

// ESP-IDF includes for raw SD card access via FatFS
#include "ff.h"
#include "diskio.h"

// ESP32-S3 USB includes (TinyUSB-based, ARDUINO_USB_MODE=0)
#include "USB.h"
#include "USBMSC.h"

// =============================================================================
// USB Mass Storage Instance - GLOBAL
// =============================================================================
// 
// With ARDUINO_USB_MODE=0 (TinyUSB) and ARDUINO_USB_CDC_ON_BOOT=1:
// - USB stack starts in app_main() BEFORE setup()
// - MSC object registers itself with TinyUSB when constructed
// - We configure callbacks and begin() in usbMscInit()
// - mediaPresent(false) keeps drive hidden until user enables it
// =============================================================================

static USBMSC msc;
static bool mscInitialized = false;
static bool mscEnabled = false;
static volatile bool mscMounted = false;
static volatile bool mscBusy = false;
static volatile bool mscHostEjected = false;

static uint32_t cachedSectorCount = 0;

#define SD_SECTOR_SIZE 512
#define SDMMC_PDRV 0

// Forward declarations
static int32_t onMscRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize);
static int32_t onMscWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize);
static bool onMscStartStop(uint8_t power_condition, bool start, bool load_eject);

void usbInit() {
    // No-op - USB is already started by app_main()
}

bool usbMscInit() {
    if (mscInitialized) {
        return true;
    }

    if (!sdCardPresent()) {
        Serial.println("USB MSC: SD card not present");
        return false;
    }

    Serial.println("Initializing USB Mass Storage...");

    uint64_t cardSize = SD_MMC.cardSize();
    cachedSectorCount = cardSize / SD_SECTOR_SIZE;

    Serial.printf("  Card size: %llu bytes (%u sectors)\n", cardSize, cachedSectorCount);

    // Configure MSC
    msc.vendorID("ESP32-S3");
    msc.productID("SD Card");
    msc.productRevision("1.0");
    msc.onRead(onMscRead);
    msc.onWrite(onMscWrite);
    msc.onStartStop(onMscStartStop);
    msc.mediaPresent(false);  // Hidden until enabled
    
    if (!msc.begin(cachedSectorCount, SD_SECTOR_SIZE)) {
        Serial.println("USB MSC: Failed to begin");
        return false;
    }

    mscInitialized = true;
    mscEnabled = false;
    
    Serial.println("USB MSC initialized (disabled)");
    return true;
}

bool usbMscEnable() {
    if (!mscInitialized) {
        Serial.println("USB MSC: Not initialized");
        return false;
    }

    if (mscEnabled) {
        return true;
    }

    if (!sdCardPresent()) {
        Serial.println("USB MSC: SD card not present");
        return false;
    }

    msc.mediaPresent(true);
    mscEnabled = true;
    Serial.println("USB MSC: Enabled");
    return true;
}

void usbMscDisable() {
    if (!mscInitialized || !mscEnabled) {
        return;
    }

    msc.mediaPresent(false);
    mscEnabled = false;
    mscMounted = false;
    Serial.println("USB MSC: Disabled");
}

bool usbMscIsEnabled() {
    return mscEnabled;
}

bool usbMscMounted() {
    return mscEnabled && mscMounted;
}

bool usbMscBusy() {
    return mscEnabled && mscBusy;
}

void usbMscEject() {
    if (mscInitialized && mscEnabled) {
        msc.mediaPresent(false);
        mscMounted = false;
        mscEnabled = false;
        Serial.println("USB MSC: Ejected");
    }
}

bool usbMscCheckEjected() {
    if (mscHostEjected) {
        mscHostEjected = false;
        if (mscEnabled) {
            mscEnabled = false;
            mscMounted = false;
            Serial.println("USB MSC: Host ejected");
        }
        return true;
    }
    return false;
}

// =============================================================================
// USB MSC Callbacks - run in USB task context
// =============================================================================

static int32_t onMscRead(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    mscBusy = true;
    uint32_t sectors = bufsize / SD_SECTOR_SIZE;
    DRESULT res = disk_read(SDMMC_PDRV, (uint8_t*)buffer, lba, sectors);
    mscBusy = false;
    return (res == RES_OK) ? bufsize : -1;
}

static int32_t onMscWrite(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    mscBusy = true;
    uint32_t sectors = bufsize / SD_SECTOR_SIZE;
    DRESULT res = disk_write(SDMMC_PDRV, buffer, lba, sectors);
    mscBusy = false;
    return (res == RES_OK) ? bufsize : -1;
}

static bool onMscStartStop(uint8_t power_condition, bool start, bool load_eject) {
    // Runs in USB interrupt context - keep minimal, no Serial
    if (load_eject && !start) {
        mscMounted = false;
        mscHostEjected = true;
    } else if (load_eject && start) {
        mscMounted = true;
    }
    return true;
}

#else // !PRODUCTION_BUILD

// =============================================================================
// Development Build Stubs - No USB MSC functionality
// =============================================================================
// Hardware USB-JTAG mode (ARDUINO_USB_MODE=1) doesn't support composite devices.
// These stubs allow the code to compile and link without MSC support.
// =============================================================================

void usbInit() {}
bool usbMscInit() { return false; }
bool usbMscEnable() { return false; }
void usbMscDisable() {}
bool usbMscIsEnabled() { return false; }
bool usbMscMounted() { return false; }
bool usbMscBusy() { return false; }
void usbMscEject() {}
bool usbMscCheckEjected() { return false; }

#endif // PRODUCTION_BUILD
