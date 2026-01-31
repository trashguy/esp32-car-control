#include "slave/spi_ota.h"
#include "slave/ota_handler.h"
#include "shared/ota_protocol.h"
#include "sd_card.h"
#include <Arduino.h>
#include <SD_MMC.h>

// =============================================================================
// Local State
// =============================================================================

static fs::File firmwareFile;
static uint32_t cachedFirmwareSize = 0;
static uint32_t cachedFirmwareCrc = 0;
static bool firmwareCrcCalculated = false;

// Bulk response buffer (for chunk data)
static uint8_t bulkResponseBuffer[OTA_BULK_PACKET_SIZE];
static size_t bulkResponseLen = 0;
static bool bulkResponsePending = false;

// Test mode state
static bool otaTestMode = false;

// Verification state: 0=none, 1=requested, 2=passed, 3=failed
static uint8_t verifyState = 0;

// OTA mode active - when true, slave ignores normal SPI and only responds to OTA
static bool otaModeActive = false;

// =============================================================================
// OTA Mode Control
// =============================================================================

bool spiOtaIsActive() {
    // OTA mode is active only when explicitly set (after TEST_START received)
    // verifyState == 1 is just "pending" - we still respond to normal SPI
    return otaModeActive;
}

void spiOtaEnterMode() {
    otaModeActive = true;
    Serial.println("[SPI OTA] Entered OTA mode - ignoring normal SPI");
}

void spiOtaExitMode() {
    otaModeActive = false;
    otaTestMode = false;
    verifyState = 0;
    Serial.println("[SPI OTA] Exited OTA mode - resuming normal SPI");
}

// =============================================================================
// Verification State Management
// =============================================================================

void spiOtaRequestVerify() {
    verifyState = 1;  // requested - master will see this on next poll
    // Don't enter OTA mode yet - wait for master to acknowledge with TEST_START
    Serial.println("[SPI OTA] Verification requested - waiting for master acknowledgment");
}

bool spiOtaIsVerifyRequested() {
    return verifyState == 1;
}

void spiOtaSetVerifyResult(bool passed) {
    verifyState = passed ? 2 : 3;
    Serial.printf("[SPI OTA] Verification result: %s\n", passed ? "PASSED" : "FAILED");
}

uint8_t spiOtaGetVerifyState() {
    return verifyState;
}

void spiOtaClearVerifyState() {
    verifyState = 0;
    Serial.println("[SPI OTA] Verification state cleared");
}

// =============================================================================
// Firmware Access
// =============================================================================

bool spiOtaHasFirmware() {
    // Check if controller firmware exists on SD card
    if (!sdCardPresent()) {
        return false;
    }
    
    return SD_MMC.exists(OTA_CONTROLLER_FW_PATH);
}

uint32_t spiOtaGetFirmwareSize() {
    if (cachedFirmwareSize > 0) {
        return cachedFirmwareSize;
    }
    
    if (!spiOtaHasFirmware()) {
        return 0;
    }
    
    fs::File f = SD_MMC.open(OTA_CONTROLLER_FW_PATH, FILE_READ);
    if (!f) {
        return 0;
    }
    
    cachedFirmwareSize = f.size();
    f.close();
    
    return cachedFirmwareSize;
}

uint32_t spiOtaGetFirmwareCrc() {
    if (firmwareCrcCalculated) {
        return cachedFirmwareCrc;
    }
    
    if (!spiOtaHasFirmware()) {
        return 0;
    }
    
    Serial.println("[SPI OTA] Calculating firmware CRC...");
    
    fs::File f = SD_MMC.open(OTA_CONTROLLER_FW_PATH, FILE_READ);
    if (!f) {
        return 0;
    }
    
    uint8_t buffer[512];
    uint32_t crc = 0xFFFFFFFF;
    
    while (f.available()) {
        size_t read = f.read(buffer, sizeof(buffer));
        crc = otaCrc32(buffer, read, crc);
    }
    
    f.close();
    
    // otaCrc32 returns inverted CRC, so we already have the final value
    // Actually, we passed running CRC so need to finalize
    cachedFirmwareCrc = ~crc;  // Final XOR
    firmwareCrcCalculated = true;
    
    Serial.printf("[SPI OTA] Firmware CRC: 0x%08X\n", cachedFirmwareCrc);
    
    return cachedFirmwareCrc;
}

size_t spiOtaReadChunk(uint16_t chunkIndex, uint8_t* buffer, size_t maxLen) {
    if (!spiOtaHasFirmware()) {
        return 0;
    }
    
    fs::File f = SD_MMC.open(OTA_CONTROLLER_FW_PATH, FILE_READ);
    if (!f) {
        return 0;
    }
    
    uint32_t offset = (uint32_t)chunkIndex * OTA_CHUNK_SIZE;
    if (offset >= f.size()) {
        f.close();
        return 0;
    }
    
    f.seek(offset);
    
    size_t toRead = min(maxLen, (size_t)OTA_CHUNK_SIZE);
    size_t remaining = f.size() - offset;
    if (toRead > remaining) {
        toRead = remaining;
    }
    
    size_t read = f.read(buffer, toRead);
    f.close();
    
    return read;
}

void spiOtaClearFirmware() {
    Serial.println("[SPI OTA] Clearing controller firmware");
    
    if (SD_MMC.exists(OTA_CONTROLLER_FW_PATH)) {
        SD_MMC.remove(OTA_CONTROLLER_FW_PATH);
    }
    
    cachedFirmwareSize = 0;
    cachedFirmwareCrc = 0;
    firmwareCrcCalculated = false;
}

// =============================================================================
// Packet Processing
// =============================================================================

bool spiOtaProcessPacket(const uint8_t* rxData, size_t rxLen, 
                          uint8_t* txResponse, size_t* txLen,
                          bool* enterBulkMode, bool* exitBulkMode) {
    // Default: don't change bulk mode
    *enterBulkMode = false;
    *exitBulkMode = false;
    
    // Check if this is an OTA packet
    if (rxLen < OTA_PACKET_SIZE || rxData[0] != OTA_PACKET_HEADER) {
        return false;  // Not an OTA packet
    }
    
    // Validate checksum (only for 5-byte command packets)
    if (!otaValidatePacket(rxData)) {
        Serial.println("[SPI OTA] Invalid packet checksum");
        otaPackResponse(txResponse, OTA_STATUS_ERROR, 0);
        *txLen = OTA_PACKET_SIZE;
        return true;
    }
    
    uint8_t cmd = rxData[1];
    uint16_t param = otaExtractParam(rxData);
    
    switch (cmd) {
        case OTA_CMD_STATUS: {
            // Master is polling for OTA status - stay in normal mode
            // Priority: verification state > firmware ready
            if (verifyState == 1) {
                // Verification requested - tell master to run test
                otaPackResponse(txResponse, OTA_STATUS_VERIFY_REQUESTED, 0);
                Serial.println("[SPI OTA] Status: verify requested");
            } else if (verifyState == 2) {
                // Verification passed - report this once, then clear
                otaPackResponse(txResponse, OTA_STATUS_VERIFY_PASSED, 0);
                Serial.println("[SPI OTA] Status: verify passed");
            } else if (verifyState == 3) {
                // Verification failed - report this once, then clear
                otaPackResponse(txResponse, OTA_STATUS_VERIFY_FAILED, 0);
                Serial.println("[SPI OTA] Status: verify failed");
            } else if (spiOtaHasFirmware()) {
                otaPackResponse(txResponse, OTA_STATUS_FW_READY, 0);
                Serial.println("[SPI OTA] Status: firmware ready");
            } else {
                otaPackResponse(txResponse, OTA_STATUS_IDLE, 0);
            }
            *txLen = OTA_PACKET_SIZE;
            return true;
        }
        
        case OTA_CMD_GET_INFO: {
            // Master wants firmware info - stay in normal mode
            // Response fits in 12 bytes, still use small transaction
            uint32_t size = spiOtaGetFirmwareSize();
            uint32_t crc = spiOtaGetFirmwareCrc();
            
            // Response: header(1) + status(1) + reserved(2) + size(4) + crc(4) = 12 bytes
            txResponse[0] = OTA_PACKET_HEADER;
            txResponse[1] = OTA_STATUS_FW_READY;
            txResponse[2] = 0;
            txResponse[3] = 0;
            memcpy(&txResponse[4], &size, 4);
            memcpy(&txResponse[8], &crc, 4);
            *txLen = 12;
            
            Serial.printf("[SPI OTA] Info: size=%u, crc=0x%08X\n", size, crc);
            return true;
        }
        
        case OTA_CMD_START_BULK: {
            // Master signals it's ready for bulk mode transfers
            Serial.println("[SPI OTA] Master requested bulk mode - switching");
            *enterBulkMode = true;
            
            // Acknowledge with simple response
            otaPackResponse(txResponse, OTA_STATUS_FW_READY, 0);
            *txLen = OTA_PACKET_SIZE;
            return true;
        }
        
        case OTA_CMD_GET_CHUNK: {
            // Master wants a chunk of firmware (should only happen in bulk mode)
            uint16_t chunkIndex = param;
            
            // Read chunk data
            uint8_t chunkData[OTA_CHUNK_SIZE];
            size_t bytesRead = spiOtaReadChunk(chunkIndex, chunkData, OTA_CHUNK_SIZE);
            
            if (bytesRead == 0) {
                otaPackResponse(txResponse, OTA_STATUS_ERROR, 0);
                *txLen = OTA_PACKET_SIZE;
                Serial.printf("[SPI OTA] Chunk %d read failed\n", chunkIndex);
                return true;
            }
            
            // Build bulk response
            // Format: header(1) + status(1) + len(2) + data(256) + crc(4)
            txResponse[0] = OTA_PACKET_HEADER;
            txResponse[1] = 0x00;  // Success
            txResponse[2] = bytesRead & 0xFF;
            txResponse[3] = (bytesRead >> 8) & 0xFF;
            memcpy(&txResponse[4], chunkData, bytesRead);
            
            // Calculate and append CRC of chunk data
            uint32_t chunkCrc = otaCrc32(chunkData, bytesRead);
            memcpy(&txResponse[4 + bytesRead], &chunkCrc, 4);
            
            *txLen = 4 + bytesRead + 4;
            
            if (chunkIndex % 50 == 0) {
                Serial.printf("[SPI OTA] Sent chunk %d (%d bytes)\n", chunkIndex, bytesRead);
            }
            return true;
        }
        
        case OTA_CMD_DONE: {
            // Master completed download successfully
            Serial.println("[SPI OTA] Master completed download, clearing firmware");
            spiOtaClearFirmware();
            
            // Also clear OTA state
            otaClearState();
            
            // Exit OTA mode completely
            spiOtaExitMode();
            
            // Signal to exit bulk mode
            *exitBulkMode = true;
            
            otaPackResponse(txResponse, OTA_STATUS_IDLE, 0);
            *txLen = OTA_PACKET_SIZE;
            return true;
        }
        
        case OTA_CMD_ABORT: {
            // Master aborted OTA
            Serial.println("[SPI OTA] Master aborted OTA");
            // Don't clear firmware - allow retry
            
            // Exit OTA mode completely
            spiOtaExitMode();
            
            // Signal to exit bulk mode
            *exitBulkMode = true;
            
            otaPackResponse(txResponse, OTA_STATUS_IDLE, 0);
            *txLen = OTA_PACKET_SIZE;
            return true;
        }
        
        // =====================================================================
        // Test Mode Commands (compile-time optional)
        // =====================================================================
        
#if OTA_ENABLE_TEST_MODE
        case OTA_CMD_TEST_START: {
            // Master acknowledged verify request - NOW enter OTA mode
            Serial.println("[SPI OTA] === TEST MODE START ===");
            otaModeActive = true;  // Now we fully enter OTA mode
            otaTestMode = true;
            *enterBulkMode = true;  // Switch to bulk mode for test chunks
            
            // Respond with test ready status and test "firmware" size
            txResponse[0] = OTA_PACKET_HEADER;
            txResponse[1] = OTA_STATUS_TEST_READY;
            txResponse[2] = 0;
            txResponse[3] = 0;
            // Include test firmware size info
            uint32_t testSize = OTA_TEST_FIRMWARE_SIZE;
            memcpy(&txResponse[4], &testSize, 4);
            // Calculate CRC of test pattern (predictable pattern)
            uint32_t testCrc = 0x12345678;  // Placeholder, we'll verify pattern instead
            memcpy(&txResponse[8], &testCrc, 4);
            *txLen = 12;
            
            Serial.printf("[SPI OTA] Test mode: size=%u, chunks=%u\n", 
                          OTA_TEST_FIRMWARE_SIZE, OTA_TEST_NUM_CHUNKS);
            return true;
        }
        
        case OTA_CMD_TEST_CHUNK: {
            // Master wants a test data chunk
            if (!otaTestMode) {
                Serial.println("[SPI OTA] Test chunk requested but not in test mode");
                otaPackResponse(txResponse, OTA_STATUS_ERROR, 0);
                *txLen = OTA_PACKET_SIZE;
                return true;
            }
            
            uint16_t chunkIndex = param;
            
            // Generate predictable test pattern for this chunk
            // Pattern: each byte = (chunkIndex + byteIndex) & 0xFF
            uint8_t chunkData[OTA_CHUNK_SIZE];
            size_t bytesInChunk = OTA_CHUNK_SIZE;
            
            // Last chunk may be smaller
            uint32_t offset = (uint32_t)chunkIndex * OTA_CHUNK_SIZE;
            if (offset + bytesInChunk > OTA_TEST_FIRMWARE_SIZE) {
                bytesInChunk = OTA_TEST_FIRMWARE_SIZE - offset;
            }
            
            // Fill with predictable pattern
            for (size_t i = 0; i < bytesInChunk; i++) {
                chunkData[i] = (uint8_t)((chunkIndex + i) & 0xFF);
            }
            
            // Build bulk response
            txResponse[0] = OTA_PACKET_HEADER;
            txResponse[1] = 0x00;  // Success
            txResponse[2] = bytesInChunk & 0xFF;
            txResponse[3] = (bytesInChunk >> 8) & 0xFF;
            memcpy(&txResponse[4], chunkData, bytesInChunk);
            
            // Calculate and append CRC of chunk data
            uint32_t chunkCrc = otaCrc32(chunkData, bytesInChunk);
            memcpy(&txResponse[4 + bytesInChunk], &chunkCrc, 4);
            
            *txLen = 4 + bytesInChunk + 4;
            
            if (chunkIndex % 10 == 0 || chunkIndex == OTA_TEST_NUM_CHUNKS - 1) {
                Serial.printf("[SPI OTA] Test chunk %d/%d (%d bytes)\n", 
                              chunkIndex, OTA_TEST_NUM_CHUNKS, bytesInChunk);
            }
            return true;
        }
        
        case OTA_CMD_TEST_END: {
            // Master completed test - param indicates result (1=passed, 0=failed)
            bool passed = (param != 0);
            Serial.printf("[SPI OTA] === TEST MODE END (result: %s) ===\n", 
                          passed ? "PASSED" : "FAILED");
            otaTestMode = false;
            *exitBulkMode = true;
            
            // Update verification state for UI
            spiOtaSetVerifyResult(passed);
            
            // If test failed, exit OTA mode so master can resume normal operation
            if (!passed) {
                spiOtaExitMode();
            }
            // If passed, stay in OTA mode waiting for INSTALL command
            
            otaPackResponse(txResponse, OTA_STATUS_IDLE, 0);
            *txLen = OTA_PACKET_SIZE;
            return true;
        }
#endif // OTA_ENABLE_TEST_MODE
        
        default:
            Serial.printf("[SPI OTA] Unknown command: 0x%02X\n", cmd);
            otaPackResponse(txResponse, OTA_STATUS_ERROR, 0);
            *txLen = OTA_PACKET_SIZE;
            return true;
    }
}
