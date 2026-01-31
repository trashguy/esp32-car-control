#include "slave/spi_slave.h"
#include "slave/spi_ota.h"
#include "slave/ota_handler.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include "shared/ota_protocol.h"
#include <Arduino.h>
#include <driver/spi_slave.h>

static MasterDataCallback masterCallback = nullptr;
static volatile uint16_t lastRpm = 0;
static volatile uint8_t lastMasterMode = MODE_AUTO;
static volatile unsigned long lastPacketTime = 0;
static volatile uint32_t validPacketCount = 0;
static volatile uint32_t invalidPacketCount = 0;

// Requested state (what slave UI wants to send to master)
static volatile uint8_t requestedMode = MODE_AUTO;
static volatile uint16_t requestedRpm = 3000;

// Track previous connection state for reconnection sync
static volatile bool wasConnected = false;
static volatile bool justReconnected = false;

// DMA-capable buffers (must be in DMA-capable memory and word-aligned)
WORD_ALIGNED_ATTR uint8_t rxBuffer[SPI_PACKET_SIZE + 4];  // Extra padding for DMA
WORD_ALIGNED_ATTR uint8_t txBuffer[SPI_PACKET_SIZE + 4];

// OTA buffers (larger for bulk transfers)
WORD_ALIGNED_ATTR uint8_t otaRxBuffer[OTA_BULK_PACKET_SIZE + 4];
WORD_ALIGNED_ATTR uint8_t otaTxBuffer[OTA_BULK_PACKET_SIZE + 4];
static bool otaBulkMode = false;      // True when in 264-byte bulk transfer mode
static bool otaResponsePending = false;  // True when we have OTA response to send
static size_t otaResponseLen = OTA_PACKET_SIZE;

// Transaction descriptor
static spi_slave_transaction_t transaction;
static volatile bool transactionPending = false;
static volatile unsigned long transactionQueuedTime = 0;

// Callback after transaction complete
static void IRAM_ATTR spiPostTransCallback(spi_slave_transaction_t* trans) {
    transactionPending = false;
}

bool spiSlaveInit(MasterDataCallback callback) {
    masterCallback = callback;

    // SPI slave bus configuration
    spi_bus_config_t busConfig = {
        .mosi_io_num = COMM_SPI_MOSI_PIN,
        .miso_io_num = COMM_SPI_MISO_PIN,
        .sclk_io_num = COMM_SPI_SCK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = OTA_BULK_PACKET_SIZE,  // Support larger OTA transfers
    };

    // SPI slave interface configuration
    spi_slave_interface_config_t slaveConfig = {
        .spics_io_num = COMM_SPI_CS_PIN,
        .flags = 0,
        .queue_size = 1,
        .mode = 0,  // SPI Mode 0 (CPOL=0, CPHA=0)
        .post_setup_cb = nullptr,
        .post_trans_cb = spiPostTransCallback,
    };

    // Initialize SPI slave on SPI3 (HSPI) - SPI2 is used by TFT display
    esp_err_t ret = spi_slave_initialize(SPI3_HOST, &busConfig, &slaveConfig, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        Serial.printf("SPI Slave init failed: %d\n", ret);
        return false;
    }

    // Prepare initial response (slave's request to master)
    packSlavePacket(txBuffer, requestedMode, requestedRpm);

    // Queue first transaction
    memset(&transaction, 0, sizeof(transaction));
    transaction.length = SPI_PACKET_SIZE * 8;  // Length in bits
    transaction.tx_buffer = txBuffer;
    transaction.rx_buffer = rxBuffer;

    ret = spi_slave_queue_trans(SPI3_HOST, &transaction, 0);
    if (ret == ESP_OK) {
        transactionPending = true;
        transactionQueuedTime = millis();
    }

    Serial.printf("SPI Slave initialized (MOSI=%d, MISO=%d, SCK=%d, CS=%d)\n",
                  COMM_SPI_MOSI_PIN, COMM_SPI_MISO_PIN, COMM_SPI_SCK_PIN, COMM_SPI_CS_PIN);

    return true;
}

void spiSlaveSetRequest(uint8_t mode, uint16_t rpm) {
    requestedMode = mode;
    requestedRpm = rpm;
    // Update TX buffer - will be used in next transaction
    packSlavePacket(txBuffer, requestedMode, requestedRpm);
}

void spiSlaveProcess() {
    // Check if user has initiated controller OTA update
    // Only enter OTA mode when user explicitly pressed Update button
    bool otaFirmwareAvailable = otaControllerUpdateInProgress();
    
    // Check for transaction timeout (master may have rebooted)
    if (transactionPending && (millis() - transactionQueuedTime > SPI_TIMEOUT_MS)) {
        // Transaction timed out - reset SPI state
        spi_slave_transaction_t* staleTrans;
        // Try to get any stale transaction result (non-blocking)
        spi_slave_get_trans_result(SPI3_HOST, &staleTrans, 0);
        transactionPending = false;
        otaBulkMode = false;  // Reset OTA mode on timeout
        otaResponsePending = false;
        Serial.println("SPI transaction timeout - resetting");
    }

    if (!transactionPending) {
        // Previous transaction completed - check results
        spi_slave_transaction_t* completedTrans;
        if (spi_slave_get_trans_result(SPI3_HOST, &completedTrans, 0) == ESP_OK) {
            
            // Check if this was an OTA packet (header 0xBB)
            uint8_t* currentRxBuffer = otaBulkMode ? otaRxBuffer : rxBuffer;
            
            if (currentRxBuffer[0] == OTA_PACKET_HEADER) {
                // This is an OTA packet - process it
                size_t responseLen = OTA_PACKET_SIZE;
                bool enterBulkMode = false;
                bool exitBulkMode = false;
                if (spiOtaProcessPacket(currentRxBuffer, OTA_PACKET_SIZE, 
                                        otaTxBuffer, &responseLen, &enterBulkMode, &exitBulkMode)) {
                    // Mark that we have an OTA response to send
                    otaResponsePending = true;
                    otaResponseLen = responseLen;
                    
                    // Switch to bulk mode when master requests it
                    if (enterBulkMode) {
                        otaBulkMode = true;
                        Serial.println("[SPI] Entering OTA bulk mode");
                    }
                    
                    // Exit bulk mode when OTA is complete (DONE/ABORT received)
                    if (exitBulkMode) {
                        otaBulkMode = false;
                        Serial.println("[SPI] Exiting OTA bulk mode - OTA complete");
                    }
                    
                    lastPacketTime = millis();  // Keep connection alive
                    validPacketCount++;
                }
            } 
            // Normal SPI packet (header 0xAA)
            else if (validateSpiPacket(currentRxBuffer)) {
                // If we were in OTA bulk mode but received normal packet,
                // master has returned to normal mode (completed or aborted OTA)
                if (otaBulkMode) {
                    Serial.println("[SPI] Master returned to normal mode - exiting OTA bulk mode");
                    otaBulkMode = false;
                    otaResponsePending = false;
                    otaAbortControllerUpdate();  // Clear the OTA active flag
                    spiOtaExitMode();  // Exit OTA mode completely
                }
                
                // If OTA mode is active (user pressed VERIFY or we have firmware ready),
                // ignore normal packets and respond with OTA status
                if (spiOtaIsActive()) {
                    // Tell master to enter OTA mode by responding with current OTA status
                    uint8_t verifyState = spiOtaGetVerifyState();
                    if (verifyState == 1) {
                        otaPackResponse(otaTxBuffer, OTA_STATUS_VERIFY_REQUESTED, 0);
                    } else if (verifyState == 2) {
                        otaPackResponse(otaTxBuffer, OTA_STATUS_FW_READY, 0);  // Verified, ready to install
                    } else if (spiOtaHasFirmware()) {
                        otaPackResponse(otaTxBuffer, OTA_STATUS_FW_READY, 0);
                    } else {
                        otaPackResponse(otaTxBuffer, OTA_STATUS_IDLE, 0);
                    }
                    otaResponsePending = true;
                    otaResponseLen = OTA_PACKET_SIZE;
                    lastPacketTime = millis();
                    validPacketCount++;
                } else if (otaFirmwareAvailable) {
                    // Queue OTA status response to tell master we have firmware
                    otaPackResponse(otaTxBuffer, OTA_STATUS_FW_READY, 0);
                    otaResponsePending = true;
                    otaResponseLen = OTA_PACKET_SIZE;
                    lastPacketTime = millis();
                    validPacketCount++;
                } else {
                    // Normal operation - process the packet
                    otaBulkMode = false;
                    otaResponsePending = false;
                    
                    lastRpm = extractSpiRpm(currentRxBuffer);
                    lastMasterMode = extractSpiMode(currentRxBuffer);
                    lastPacketTime = millis();
                    validPacketCount++;

                    // Check if we just reconnected (master is source of truth)
                    // On reconnection, adopt master's mode and RPM as our requested state
                    if (!wasConnected) {
                        wasConnected = true;
                        justReconnected = true;
                        requestedMode = lastMasterMode;
                        if (lastMasterMode == MODE_MANUAL) {
                            requestedRpm = lastRpm;
                        }
                        Serial.printf("Reconnected - syncing to master: mode=%s, rpm=%u\n",
                                      lastMasterMode == MODE_AUTO ? "AUTO" : "MANUAL", lastRpm);
                    }

                    if (masterCallback != nullptr) {
                        masterCallback(lastRpm, lastMasterMode);
                    }
                }
            } else {
                invalidPacketCount++;
            }
        }

        // Queue next transaction based on mode
        memset(&transaction, 0, sizeof(transaction));
        
        if (otaBulkMode) {
            // Bulk OTA mode - use larger buffers with max size for chunk transfers
            transaction.length = OTA_BULK_PACKET_SIZE * 8;  // Length in bits
            transaction.tx_buffer = otaTxBuffer;
            transaction.rx_buffer = otaRxBuffer;
        } else if (otaResponsePending) {
            // Have OTA response to send but not in bulk mode yet
            // Use normal size transaction with OTA response in otaTxBuffer
            // Copy response to beginning of normal-sized buffer
            transaction.length = SPI_PACKET_SIZE * 8;
            transaction.tx_buffer = otaTxBuffer;  // Use OTA buffer which has the response
            transaction.rx_buffer = rxBuffer;
            otaResponsePending = false;  // Response will be sent, clear flag
        } else {
            // Normal mode - update TX buffer with slave's request to master
            packSlavePacket(txBuffer, requestedMode, requestedRpm);
            transaction.length = SPI_PACKET_SIZE * 8;
            transaction.tx_buffer = txBuffer;
            transaction.rx_buffer = rxBuffer;
        }

        if (spi_slave_queue_trans(SPI3_HOST, &transaction, 0) == ESP_OK) {
            transactionPending = true;
            transactionQueuedTime = millis();
        }
    }
}

uint16_t spiSlaveGetLastRpm() {
    return lastRpm;
}

unsigned long spiSlaveGetTimeSinceLastPacket() {
    if (lastPacketTime == 0) {
        return ULONG_MAX;
    }
    return millis() - lastPacketTime;
}

bool spiSlaveIsConnected() {
    bool connected = spiSlaveGetTimeSinceLastPacket() < SPI_TIMEOUT_MS;
    // Track disconnection for reconnection sync
    if (!connected) {
        wasConnected = false;
    }
    return connected;
}

uint32_t spiSlaveGetValidPacketCount() {
    return validPacketCount;
}

uint32_t spiSlaveGetInvalidPacketCount() {
    return invalidPacketCount;
}

uint8_t spiSlaveGetMasterMode() {
    return lastMasterMode;
}

void spiSlaveSetRequestedMode(uint8_t mode) {
    requestedMode = mode;
    packSlavePacket(txBuffer, requestedMode, requestedRpm);
}

uint8_t spiSlaveGetRequestedMode() {
    return requestedMode;
}

void spiSlaveSetRequestedRpm(uint16_t rpm) {
    requestedRpm = rpm;
    packSlavePacket(txBuffer, requestedMode, requestedRpm);
}

uint16_t spiSlaveGetRequestedRpm() {
    return requestedRpm;
}

bool spiSlaveCheckReconnected() {
    if (justReconnected) {
        justReconnected = false;
        return true;
    }
    return false;
}
