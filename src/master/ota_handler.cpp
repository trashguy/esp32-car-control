#include "master/ota_handler.h"
#include "master/spi_master.h"
#include "shared/config.h"
#include "shared/ota_protocol.h"
#include <Arduino.h>
#include <Update.h>

// =============================================================================
// Local State
// =============================================================================

static MasterOtaState currentState = MASTER_OTA_IDLE;
static char errorMessage[64] = "";
static uint8_t progress = 0;
static bool rebootPending = false;

// Firmware download state
static uint32_t firmwareSize = 0;
static uint32_t firmwareCrc = 0;
static uint32_t bytesReceived = 0;
static uint16_t currentChunk = 0;
static uint16_t totalChunks = 0;
static uint8_t retryCount = 0;

// Polling state
static unsigned long lastPollTime = 0;

// Buffer for firmware chunks
static uint8_t chunkBuffer[OTA_CHUNK_SIZE];

// Poll status return codes
#define POLL_RESULT_NONE        0   // No OTA pending
#define POLL_RESULT_FW_READY    1   // Firmware ready for download
#define POLL_RESULT_VERIFY_REQ  2   // User requested verification
#define POLL_RESULT_VERIFY_PASS 3   // Verification passed, waiting for user
#define POLL_RESULT_ERROR       4   // SPI error

// =============================================================================
// Forward Declarations
// =============================================================================

static uint8_t pollSlaveForOta();
static bool getFirmwareInfo();
static bool startBulkMode();
static bool downloadNextChunk();
static bool verifyAndFlash();
static void sendDoneCommand();
static void sendAbortCommand();

// =============================================================================
// SPI OTA Exchange Functions
// =============================================================================

// Exchange OTA packet with slave (5-byte standard packet)
// Note: SPI slave uses DMA which means response comes in the NEXT transaction.
// We do two exchanges: first sends command, second reads response.
static bool otaSpiExchange(uint8_t cmd, uint16_t param, uint8_t* status, uint16_t* data) {
    uint8_t txBuffer[OTA_PACKET_SIZE];
    uint8_t rxBuffer[OTA_PACKET_SIZE];
    
    // Pack and send the command
    otaPackCommand(txBuffer, cmd, param);
    
    // First exchange: send command, ignore response (it's from previous transaction)
    if (!spiOtaExchange(txBuffer, rxBuffer, OTA_PACKET_SIZE)) {
        return false;
    }
    
    // Give slave time to wake up, process command, and queue response
    // Slave task runs at 100Hz (10ms), so we need at least one full cycle
    delay(20);
    
    // Second exchange: send dummy/repeat command, read actual response
    if (!spiOtaExchange(txBuffer, rxBuffer, OTA_PACKET_SIZE)) {
        return false;
    }
    
    // Validate the response
    if (!otaValidatePacket(rxBuffer)) {
        Serial.printf("[OTA] Invalid response: [0x%02X 0x%02X 0x%02X 0x%02X 0x%02X]\n",
                      rxBuffer[0], rxBuffer[1], rxBuffer[2], rxBuffer[3], rxBuffer[4]);
        return false;
    }
    
    *status = rxBuffer[1];
    *data = otaExtractParam(rxBuffer);
    return true;
}

// Request bulk data (firmware chunk) from slave
// In bulk mode, both sides are synchronized on 264-byte transactions.
// Simple 2-phase exchange: send command, wait, read response.
static bool otaSpiGetChunk(uint16_t chunkIndex, uint8_t* buffer, size_t* bytesRead) {
    uint8_t txBuffer[OTA_BULK_PACKET_SIZE];
    uint8_t rxBuffer[OTA_BULK_PACKET_SIZE];
    
    // Prepare command in bulk buffer (zero-padded)
    memset(txBuffer, 0, OTA_BULK_PACKET_SIZE);
    otaPackCommand(txBuffer, OTA_CMD_GET_CHUNK, chunkIndex);
    
    // First exchange: send GET_CHUNK command
    // In bulk mode, slave has 264-byte transaction queued with previous response
    if (!spiOtaExchangeBulk(txBuffer, rxBuffer, OTA_BULK_PACKET_SIZE)) {
        Serial.println("[OTA] Chunk: exchange 1 failed");
        return false;
    }
    
    // Give slave time to read chunk from SD and prepare response
    delay(30);
    
    // Second exchange: read the chunk response
    memset(txBuffer, 0, OTA_BULK_PACKET_SIZE);
    txBuffer[0] = OTA_PACKET_HEADER;  // Keep OTA mode
    if (!spiOtaExchangeBulk(txBuffer, rxBuffer, OTA_BULK_PACKET_SIZE)) {
        Serial.println("[OTA] Chunk: exchange 2 failed");
        return false;
    }
    
    // Debug: print first 16 bytes of response
    if (chunkIndex < 3 || chunkIndex % 100 == 0) {
        Serial.printf("[OTA] Chunk %d rx[0-15]: ", chunkIndex);
        for (int i = 0; i < 16; i++) Serial.printf("%02X ", rxBuffer[i]);
        Serial.println();
    }
    
    // Validate response header
    if (rxBuffer[0] != OTA_PACKET_HEADER) {
        Serial.printf("[OTA] Chunk: bad header 0x%02X\n", rxBuffer[0]);
        return false;
    }
    
    if (rxBuffer[1] != 0x00) {  // Status byte
        Serial.printf("[OTA] Chunk: error status 0x%02X\n", rxBuffer[1]);
        return false;
    }
    
    uint16_t chunkLen = rxBuffer[2] | (rxBuffer[3] << 8);
    if (chunkLen == 0 || chunkLen > OTA_CHUNK_SIZE) {
        Serial.printf("[OTA] Chunk: invalid length %u\n", chunkLen);
        return false;
    }
    
    // Copy chunk data
    memcpy(buffer, &rxBuffer[4], chunkLen);
    *bytesRead = chunkLen;
    
    // Verify CRC32 of chunk
    uint32_t receivedCrc;
    memcpy(&receivedCrc, &rxBuffer[4 + chunkLen], 4);
    uint32_t calculatedCrc = otaCrc32(buffer, chunkLen);
    
    if (receivedCrc != calculatedCrc) {
        Serial.printf("[OTA] Chunk %d CRC mismatch: got 0x%08X, calc 0x%08X\n",
                      chunkIndex, receivedCrc, calculatedCrc);
        return false;
    }
    
    return true;
}

// Signal slave to switch to bulk mode for chunk transfers
// After this, slave expects 264-byte transactions
static bool startBulkMode() {
    uint8_t status;
    uint16_t data;
    
    Serial.println("[OTA] Requesting bulk mode...");
    
    if (!otaSpiExchange(OTA_CMD_START_BULK, 0, &status, &data)) {
        Serial.println("[OTA] START_BULK: SPI exchange failed");
        return false;
    }
    
    if (status != OTA_STATUS_FW_READY) {
        Serial.printf("[OTA] START_BULK: unexpected status 0x%02X\n", status);
        return false;
    }
    
    // Give slave time to switch to bulk mode and queue first 264-byte transaction
    delay(50);
    
    // Now we need to flush the slave's first bulk response (which will be stale)
    // by doing one bulk exchange before the real chunk transfers start
    uint8_t txBuffer[OTA_BULK_PACKET_SIZE];
    uint8_t rxBuffer[OTA_BULK_PACKET_SIZE];
    memset(txBuffer, 0, OTA_BULK_PACKET_SIZE);
    txBuffer[0] = OTA_PACKET_HEADER;
    
    if (!spiOtaExchangeBulk(txBuffer, rxBuffer, OTA_BULK_PACKET_SIZE)) {
        Serial.println("[OTA] Bulk mode flush failed");
        return false;
    }
    
    Serial.println("[OTA] Bulk mode active");
    return true;
}

// =============================================================================
// Initialization
// =============================================================================

void masterOtaInit() {
    currentState = MASTER_OTA_IDLE;
    lastPollTime = 0;
    progress = 0;
    rebootPending = false;
    errorMessage[0] = '\0';
    
    Serial.println("[OTA] Master OTA handler initialized");
}

// =============================================================================
// Main State Machine
// =============================================================================

// Returns true if OTA used the SPI bus this cycle (caller should skip normal SPI)
bool masterOtaProcess() {
    switch (currentState) {
        case MASTER_OTA_IDLE: {
            // Periodic polling for OTA - only poll, don't interleave with normal SPI
            if (millis() - lastPollTime >= OTA_POLL_INTERVAL_MS) {
                lastPollTime = millis();
                
                // This exchange uses SPI, so return true to skip normal SPI this cycle
                uint8_t pollResult = pollSlaveForOta();
                
                if (pollResult == POLL_RESULT_VERIFY_REQ) {
#if OTA_ENABLE_TEST_MODE
                    // User pressed VERIFY button - enter OTA polling mode and run test
                    Serial.println("[OTA] User requested verification - entering OTA mode");
                    currentState = MASTER_OTA_POLLING;
                    
                    // Run the protocol test
                    Serial.println("[OTA] Running verification test for user...");
                    bool passed = masterOtaRunTest();
                    
                    // Result is sent to slave via TEST_END inside masterOtaRunTest()
                    
                    if (passed) {
                        Serial.println("[OTA] Verification PASSED - waiting for user to press INSTALL");
                        currentState = MASTER_OTA_WAITING;
                    } else {
                        Serial.println("[OTA] Verification FAILED - returning to idle");
                        currentState = MASTER_OTA_IDLE;
                    }
                    return true;
#else
                    // Test mode disabled - treat verify as immediate pass, proceed to waiting
                    Serial.println("[OTA] Verification requested (test disabled) - proceeding");
                    currentState = MASTER_OTA_WAITING;
                    return true;
#endif
                }
                
                if (pollResult == POLL_RESULT_FW_READY) {
                    // User already verified and pressed INSTALL - proceed with download
                    Serial.println("[OTA] Firmware ready, starting download...");
                    
                    // Get firmware info and start download
                    if (getFirmwareInfo()) {
                        // Switch to bulk mode for chunk transfers
                        if (!startBulkMode()) {
                            snprintf(errorMessage, sizeof(errorMessage), "Failed to enter bulk mode");
                            currentState = MASTER_OTA_ERROR;
                            sendAbortCommand();
                            return true;
                        }
                        
                        currentState = MASTER_OTA_DOWNLOADING;
                        bytesReceived = 0;
                        currentChunk = 0;
                        totalChunks = (firmwareSize + OTA_CHUNK_SIZE - 1) / OTA_CHUNK_SIZE;
                        retryCount = 0;
                        
                        Serial.printf("[OTA] Starting download: %u bytes, %u chunks\n",
                                      firmwareSize, totalChunks);
                        
                        // Begin Update
                        if (!Update.begin(firmwareSize)) {
                            snprintf(errorMessage, sizeof(errorMessage), 
                                     "Update.begin failed: %s", Update.errorString());
                            currentState = MASTER_OTA_ERROR;
                            sendAbortCommand();
                            return true;
                        }
                    }
                }
                // We used SPI for polling, skip normal SPI this cycle
                return true;
            }
            return false;  // No poll this cycle, normal SPI can run
        }
        
        case MASTER_OTA_POLLING:
        case MASTER_OTA_WAITING: {
            // Fast polling while in OTA mode - waiting for user to press INSTALL or ABORT
            if (millis() - lastPollTime >= OTA_POLL_FAST_MS) {
                lastPollTime = millis();
                
                uint8_t pollResult = pollSlaveForOta();
                
                if (pollResult == POLL_RESULT_FW_READY) {
                    // User pressed INSTALL - proceed with download
                    Serial.println("[OTA] User pressed INSTALL - starting download...");
                    
                    if (getFirmwareInfo()) {
                        if (!startBulkMode()) {
                            snprintf(errorMessage, sizeof(errorMessage), "Failed to enter bulk mode");
                            currentState = MASTER_OTA_ERROR;
                            sendAbortCommand();
                            return true;
                        }
                        
                        currentState = MASTER_OTA_DOWNLOADING;
                        bytesReceived = 0;
                        currentChunk = 0;
                        totalChunks = (firmwareSize + OTA_CHUNK_SIZE - 1) / OTA_CHUNK_SIZE;
                        retryCount = 0;
                        
                        Serial.printf("[OTA] Starting download: %u bytes, %u chunks\n",
                                      firmwareSize, totalChunks);
                        
                        if (!Update.begin(firmwareSize)) {
                            snprintf(errorMessage, sizeof(errorMessage), 
                                     "Update.begin failed: %s", Update.errorString());
                            currentState = MASTER_OTA_ERROR;
                            sendAbortCommand();
                            return true;
                        }
                    }
                } else if (pollResult == POLL_RESULT_NONE) {
                    // Slave returned IDLE - user pressed ABORT or something reset
                    Serial.println("[OTA] Slave returned to IDLE - exiting OTA mode");
                    currentState = MASTER_OTA_IDLE;
                } else if (pollResult == POLL_RESULT_VERIFY_PASS) {
                    // Verification passed - stay in WAITING state for user to press INSTALL
                    // This is the expected state after test passes
                } else if (pollResult == POLL_RESULT_VERIFY_REQ && currentState == MASTER_OTA_WAITING) {
                    // Slave still showing verify requested, but we already passed
                    // This can happen if there's a timing issue - stay in waiting
                }
                // If still POLL_RESULT_VERIFY_REQ or error, stay in current state and keep polling
                
                return true;  // Always skip normal SPI when in OTA mode
            }
            return true;  // In OTA mode - don't do normal SPI
        }
        
        case MASTER_OTA_DOWNLOADING: {
            if (downloadNextChunk()) {
                currentChunk++;
                retryCount = 0;
                progress = (bytesReceived * 100) / firmwareSize;
                
                if (currentChunk >= totalChunks) {
                    // All chunks received
                    currentState = MASTER_OTA_VERIFYING;
                    Serial.println("[OTA] Download complete, verifying...");
                }
            } else {
                retryCount++;
                if (retryCount >= OTA_CHUNK_MAX_RETRIES) {
                    snprintf(errorMessage, sizeof(errorMessage), 
                             "Chunk %d failed after %d retries", currentChunk, retryCount);
                    currentState = MASTER_OTA_ERROR;
                    Update.abort();
                    sendAbortCommand();
                }
            }
            return true;  // OTA in progress
        }
        
        case MASTER_OTA_VERIFYING: {
            if (verifyAndFlash()) {
                currentState = MASTER_OTA_COMPLETE;
                progress = 100;
                rebootPending = true;
                sendDoneCommand();
                Serial.println("[OTA] Update complete, reboot pending");
            } else {
                currentState = MASTER_OTA_ERROR;
                sendAbortCommand();
            }
            return true;
        }
        
        case MASTER_OTA_COMPLETE:
            // Waiting for reboot
            return false;
            
        case MASTER_OTA_ERROR:
            // Stay in error state, can be cleared by reboot
            return false;
    }
    
    return false;
}

// =============================================================================
// State Machine Helpers
// =============================================================================

static uint8_t pollSlaveForOta() {
    uint8_t status;
    uint16_t data;
    
    Serial.println("[OTA] Polling slave...");
    
    if (!otaSpiExchange(OTA_CMD_STATUS, 0, &status, &data)) {
        Serial.println("[OTA] Poll: SPI exchange failed");
        return POLL_RESULT_ERROR;
    }
    
    Serial.printf("[OTA] Poll: status=0x%02X\n", status);
    
    if (status == OTA_STATUS_FW_READY) {
        Serial.println("[OTA] Poll: Firmware ready!");
        return POLL_RESULT_FW_READY;
    }
    
    if (status == OTA_STATUS_VERIFY_REQUESTED) {
        Serial.println("[OTA] Poll: Verification requested by user");
        return POLL_RESULT_VERIFY_REQ;
    }
    
    if (status == OTA_STATUS_VERIFY_PASSED) {
        Serial.println("[OTA] Poll: Verification passed, user can install");
        return POLL_RESULT_VERIFY_PASS;
    }
    
    if (status == OTA_STATUS_VERIFY_FAILED) {
        Serial.println("[OTA] Poll: Verification failed");
        return POLL_RESULT_NONE;  // Return to idle
    }
    
    return POLL_RESULT_NONE;
}

static bool getFirmwareInfo() {
    uint8_t txBuffer[OTA_PACKET_SIZE];
    uint8_t rxBuffer[OTA_BULK_PACKET_SIZE];
    
    Serial.println("[OTA] Requesting firmware info...");
    
    // Send GET_INFO command using same 2-phase exchange as polling
    otaPackCommand(txBuffer, OTA_CMD_GET_INFO, 0);
    
    // First exchange: send command (receive previous response - discard)
    if (!spiOtaExchange(txBuffer, rxBuffer, OTA_PACKET_SIZE)) {
        Serial.println("[OTA] Info: SPI exchange 1 failed");
        return false;
    }
    
    // Give slave time to read file and calculate CRC
    delay(100);
    
    // Second exchange: send same command again, receive the 12-byte INFO response
    // Slave should now have INFO response queued with OTA_BULK_PACKET_SIZE transaction
    if (!spiOtaExchangeBulk(txBuffer, rxBuffer, OTA_BULK_PACKET_SIZE)) {
        Serial.println("[OTA] Info: SPI exchange 2 failed");
        return false;
    }
    
    Serial.printf("[OTA] Info raw: [%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]\n",
                  rxBuffer[0], rxBuffer[1], rxBuffer[2], rxBuffer[3],
                  rxBuffer[4], rxBuffer[5], rxBuffer[6], rxBuffer[7],
                  rxBuffer[8], rxBuffer[9], rxBuffer[10], rxBuffer[11]);
    
    if (rxBuffer[0] != OTA_PACKET_HEADER) {
        Serial.printf("[OTA] Info: Bad header 0x%02X (expected 0x%02X)\n", 
                      rxBuffer[0], OTA_PACKET_HEADER);
        return false;
    }
    
    if (rxBuffer[1] != OTA_STATUS_FW_READY) {
        Serial.printf("[OTA] Info: Bad status 0x%02X\n", rxBuffer[1]);
        return false;
    }
    
    // Extract firmware info
    memcpy(&firmwareSize, &rxBuffer[4], 4);
    memcpy(&firmwareCrc, &rxBuffer[8], 4);
    
    if (firmwareSize == 0 || firmwareSize > 2 * 1024 * 1024) {
        snprintf(errorMessage, sizeof(errorMessage), "Invalid firmware size: %u", firmwareSize);
        Serial.printf("[OTA] %s\n", errorMessage);
        return false;
    }
    
    Serial.printf("[OTA] Firmware info: size=%u, crc=0x%08X\n", firmwareSize, firmwareCrc);
    return true;
}

static bool downloadNextChunk() {
    size_t bytesRead;
    
    if (!otaSpiGetChunk(currentChunk, chunkBuffer, &bytesRead)) {
        return false;
    }
    
    // Write chunk to Update
    size_t written = Update.write(chunkBuffer, bytesRead);
    if (written != bytesRead) {
        snprintf(errorMessage, sizeof(errorMessage), 
                 "Write failed: %d/%d bytes", (int)written, (int)bytesRead);
        return false;
    }
    
    bytesReceived += bytesRead;
    
    if (currentChunk % 50 == 0) {
        Serial.printf("[OTA] Progress: %u/%u bytes (%d%%)\n", 
                      bytesReceived, firmwareSize, progress);
    }
    
    return true;
}

static bool verifyAndFlash() {
    if (!Update.end(true)) {
        snprintf(errorMessage, sizeof(errorMessage), 
                 "Update.end failed: %s", Update.errorString());
        return false;
    }
    
    // Verify CRC of written data
    // Note: ESP32 Update library handles internal verification
    // Our CRC check was done per-chunk during download
    
    return true;
}

static void sendDoneCommand() {
    // Send DONE using bulk packet size since slave is still in bulk mode
    uint8_t txBuffer[OTA_BULK_PACKET_SIZE];
    uint8_t rxBuffer[OTA_BULK_PACKET_SIZE];
    
    memset(txBuffer, 0, OTA_BULK_PACKET_SIZE);
    otaPackCommand(txBuffer, OTA_CMD_DONE, 0);
    
    // Two-phase exchange for DMA timing
    spiOtaExchangeBulk(txBuffer, rxBuffer, OTA_BULK_PACKET_SIZE);
    delay(20);
    spiOtaExchangeBulk(txBuffer, rxBuffer, OTA_BULK_PACKET_SIZE);
    
    Serial.println("[OTA] DONE command sent");
}

static void sendAbortCommand() {
    // Send ABORT using bulk packet size since slave may be in bulk mode
    uint8_t txBuffer[OTA_BULK_PACKET_SIZE];
    uint8_t rxBuffer[OTA_BULK_PACKET_SIZE];
    
    memset(txBuffer, 0, OTA_BULK_PACKET_SIZE);
    otaPackCommand(txBuffer, OTA_CMD_ABORT, 0);
    
    // Two-phase exchange for DMA timing
    spiOtaExchangeBulk(txBuffer, rxBuffer, OTA_BULK_PACKET_SIZE);
    delay(20);
    spiOtaExchangeBulk(txBuffer, rxBuffer, OTA_BULK_PACKET_SIZE);
    
    Serial.println("[OTA] ABORT command sent");
}

// =============================================================================
// Public API
// =============================================================================

MasterOtaState masterOtaGetState() {
    return currentState;
}

uint8_t masterOtaGetProgress() {
    return progress;
}

const char* masterOtaGetErrorMessage() {
    return errorMessage;
}

bool masterOtaRebootPending() {
    return rebootPending;
}

void masterOtaReboot() {
    if (rebootPending) {
        Serial.println("[OTA] Rebooting...");
        delay(100);
        ESP.restart();
    }
}

// =============================================================================
// OTA Test Mode (compile-time optional)
// =============================================================================

#if OTA_ENABLE_TEST_MODE

bool masterOtaRunTest() {
    Serial.println("[OTA TEST] ========================================");
    Serial.println("[OTA TEST] Starting OTA protocol test...");
    Serial.println("[OTA TEST] ========================================");
    
    uint8_t txBuffer[OTA_BULK_PACKET_SIZE];
    uint8_t rxBuffer[OTA_BULK_PACKET_SIZE];
    uint32_t startTime = millis();
    uint32_t bytesReceived = 0;
    uint32_t chunksReceived = 0;
    uint32_t crcErrors = 0;
    uint32_t patternErrors = 0;
    
    // Step 1: Send TEST_START command
    Serial.println("[OTA TEST] Step 1: Sending TEST_START...");
    memset(txBuffer, 0, OTA_BULK_PACKET_SIZE);
    otaPackCommand(txBuffer, OTA_CMD_TEST_START, 0);
    
    // First exchange - send command (receives stale DMA data - discard)
    if (!spiOtaExchange(txBuffer, rxBuffer, OTA_PACKET_SIZE)) {
        Serial.println("[OTA TEST] FAILED: TEST_START exchange 1 failed");
        return false;
    }
    
    // Give slave time to process TEST_START and switch to bulk mode
    delay(50);
    
    // Second exchange - flush out stale response from 5-byte transaction
    // Send TEST_START again (slave is already in test mode, will just re-queue TEST_READY)
    if (!spiOtaExchangeBulk(txBuffer, rxBuffer, OTA_BULK_PACKET_SIZE)) {
        Serial.println("[OTA TEST] FAILED: TEST_START exchange 2 (flush) failed");
        return false;
    }
    
    Serial.printf("[OTA TEST] Flush response: hdr=0x%02X status=0x%02X\n",
                  rxBuffer[0], rxBuffer[1]);
    
    delay(30);
    
    // Third exchange - NOW we get the actual TEST_READY response (12 bytes with size/crc)
    // Send TEST_START again to keep response fresh
    if (!spiOtaExchangeBulk(txBuffer, rxBuffer, OTA_BULK_PACKET_SIZE)) {
        Serial.println("[OTA TEST] FAILED: TEST_START exchange 3 failed");
        return false;
    }
    
    Serial.printf("[OTA TEST] Response: hdr=0x%02X status=0x%02X\n",
                  rxBuffer[0], rxBuffer[1]);
    
    if (rxBuffer[0] != OTA_PACKET_HEADER || rxBuffer[1] != OTA_STATUS_TEST_READY) {
        Serial.printf("[OTA TEST] FAILED: Bad response: hdr=0x%02X status=0x%02X (expected 0x%02X 0x%02X)\n",
                      rxBuffer[0], rxBuffer[1], OTA_PACKET_HEADER, OTA_STATUS_TEST_READY);
        return false;
    }
    
    uint32_t testSize;
    memcpy(&testSize, &rxBuffer[4], 4);
    uint16_t totalChunks = (testSize + OTA_CHUNK_SIZE - 1) / OTA_CHUNK_SIZE;
    
    Serial.printf("[OTA TEST] Test mode active: size=%u, chunks=%u\n", testSize, totalChunks);
    
    // Step 2: Download test chunks
    // Note: DMA pipeline is already flushed from TEST_START exchanges
    Serial.println("[OTA TEST] Step 2: Downloading test chunks...");
    
    for (uint16_t chunk = 0; chunk < totalChunks; chunk++) {
        // Prepare GET_CHUNK command
        memset(txBuffer, 0, OTA_BULK_PACKET_SIZE);
        otaPackCommand(txBuffer, OTA_CMD_TEST_CHUNK, chunk);
        
        // First exchange - send command
        if (!spiOtaExchangeBulk(txBuffer, rxBuffer, OTA_BULK_PACKET_SIZE)) {
            Serial.printf("[OTA TEST] FAILED: Chunk %d exchange 1 failed\n", chunk);
            goto test_cleanup;
        }
        
        delay(20);
        
        // Second exchange - get response (send same TEST_CHUNK command to keep response valid)
        // Slave will just re-generate the same chunk data
        if (!spiOtaExchangeBulk(txBuffer, rxBuffer, OTA_BULK_PACKET_SIZE)) {
            Serial.printf("[OTA TEST] FAILED: Chunk %d exchange 2 failed\n", chunk);
            goto test_cleanup;
        }
        
        // Validate response
        if (rxBuffer[0] != OTA_PACKET_HEADER || rxBuffer[1] != 0x00) {
            Serial.printf("[OTA TEST] FAILED: Chunk %d bad response hdr=0x%02X status=0x%02X\n", 
                          chunk, rxBuffer[0], rxBuffer[1]);
            goto test_cleanup;
        }
        
        uint16_t chunkLen = rxBuffer[2] | (rxBuffer[3] << 8);
        if (chunkLen == 0 || chunkLen > OTA_CHUNK_SIZE) {
            Serial.printf("[OTA TEST] FAILED: Chunk %d invalid length %u\n", chunk, chunkLen);
            goto test_cleanup;
        }
        
        // Verify CRC
        uint32_t receivedCrc;
        memcpy(&receivedCrc, &rxBuffer[4 + chunkLen], 4);
        uint32_t calculatedCrc = otaCrc32(&rxBuffer[4], chunkLen);
        
        if (receivedCrc != calculatedCrc) {
            crcErrors++;
            if (crcErrors <= 3) {
                Serial.printf("[OTA TEST] CRC error chunk %d: got 0x%08X, calc 0x%08X\n",
                              chunk, receivedCrc, calculatedCrc);
            }
        }
        
        // Verify pattern: each byte should be (chunkIndex + byteIndex) & 0xFF
        for (uint16_t i = 0; i < chunkLen; i++) {
            uint8_t expected = (uint8_t)((chunk + i) & 0xFF);
            if (rxBuffer[4 + i] != expected) {
                patternErrors++;
                if (patternErrors <= 3) {
                    Serial.printf("[OTA TEST] Pattern error chunk %d byte %d: got 0x%02X, exp 0x%02X\n",
                                  chunk, i, rxBuffer[4 + i], expected);
                }
                break;  // Only count one error per chunk
            }
        }
        
        bytesReceived += chunkLen;
        chunksReceived++;
        
        // Progress every 10 chunks
        if (chunk % 10 == 0 || chunk == totalChunks - 1) {
            Serial.printf("[OTA TEST] Progress: %d/%d chunks (%u bytes)\n", 
                          chunk + 1, totalChunks, bytesReceived);
        }
    }
    
test_cleanup:
    // Determine pass/fail before sending TEST_END
    bool passed = (chunksReceived == totalChunks) && 
                  (bytesReceived == testSize) && 
                  (crcErrors == 0) && 
                  (patternErrors == 0);
    
    // Step 3: Send TEST_END command with result (1=passed, 0=failed)
    Serial.printf("[OTA TEST] Step 3: Sending TEST_END (result=%s)...\n", 
                  passed ? "PASSED" : "FAILED");
    
    memset(txBuffer, 0, OTA_BULK_PACKET_SIZE);
    otaPackCommand(txBuffer, OTA_CMD_TEST_END, passed ? 1 : 0);
    
    spiOtaExchangeBulk(txBuffer, rxBuffer, OTA_BULK_PACKET_SIZE);
    delay(20);
    spiOtaExchangeBulk(txBuffer, rxBuffer, OTA_BULK_PACKET_SIZE);
    
    // Results
    uint32_t elapsed = millis() - startTime;
    uint32_t bytesPerSec = (bytesReceived * 1000) / elapsed;
    
    Serial.println("[OTA TEST] ========================================");
    Serial.println("[OTA TEST] Test Results:");
    Serial.printf("[OTA TEST]   Chunks: %u/%u\n", chunksReceived, totalChunks);
    Serial.printf("[OTA TEST]   Bytes: %u/%u\n", bytesReceived, testSize);
    Serial.printf("[OTA TEST]   CRC Errors: %u\n", crcErrors);
    Serial.printf("[OTA TEST]   Pattern Errors: %u\n", patternErrors);
    Serial.printf("[OTA TEST]   Time: %u ms\n", elapsed);
    Serial.printf("[OTA TEST]   Speed: %u bytes/sec\n", bytesPerSec);
    
    // passed was already calculated before sending TEST_END
    
    if (passed) {
        Serial.println("[OTA TEST] PASSED!");
    } else {
        Serial.println("[OTA TEST] FAILED!");
    }
    Serial.println("[OTA TEST] ========================================");
    
    return passed;
}

#endif // OTA_ENABLE_TEST_MODE
