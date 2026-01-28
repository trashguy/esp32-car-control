#include "slave/spi_slave.h"
#include "shared/config.h"
#include "shared/protocol.h"
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
        .max_transfer_sz = SPI_PACKET_SIZE,
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
    // Check for transaction timeout (master may have rebooted)
    if (transactionPending && (millis() - transactionQueuedTime > SPI_TIMEOUT_MS)) {
        // Transaction timed out - reset SPI state
        spi_slave_transaction_t* staleTrans;
        // Try to get any stale transaction result (non-blocking)
        spi_slave_get_trans_result(SPI3_HOST, &staleTrans, 0);
        transactionPending = false;
        Serial.println("SPI transaction timeout - resetting");
    }

    if (!transactionPending) {
        // Previous transaction completed - check results
        spi_slave_transaction_t* completedTrans;
        if (spi_slave_get_trans_result(SPI3_HOST, &completedTrans, 0) == ESP_OK) {
            // Validate received packet from master
            if (validateSpiPacket(rxBuffer)) {
                lastRpm = extractSpiRpm(rxBuffer);
                lastMasterMode = extractSpiMode(rxBuffer);
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
            } else {
                invalidPacketCount++;
            }
        }

        // Update TX buffer with slave's request to master
        packSlavePacket(txBuffer, requestedMode, requestedRpm);

        // Queue next transaction
        memset(&transaction, 0, sizeof(transaction));
        transaction.length = SPI_PACKET_SIZE * 8;
        transaction.tx_buffer = txBuffer;
        transaction.rx_buffer = rxBuffer;

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
