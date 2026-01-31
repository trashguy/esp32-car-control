#include "master/spi_master.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include <Arduino.h>
#include <SPI.h>

// Use HSPI (SPI2) for communication - separate from VSPI used by other peripherals
static SPIClass* commSpi = nullptr;
static SPISettings spiSettings(COMM_SPI_FREQUENCY, MSBFIRST, SPI_MODE0);

// Use same SPI settings for OTA (full speed)
static SPISettings spiSettingsOta(COMM_SPI_FREQUENCY, MSBFIRST, SPI_MODE0);

static uint32_t successCount = 0;
static uint32_t errorCount = 0;

bool spiMasterInit() {
    // Initialize HSPI (SPI2) on custom pins
    commSpi = new SPIClass(HSPI);
    commSpi->begin(COMM_SPI_SCK_PIN, COMM_SPI_MISO_PIN, COMM_SPI_MOSI_PIN, COMM_SPI_CS_PIN);
    
    // Configure CS pin as output
    pinMode(COMM_SPI_CS_PIN, OUTPUT);
    digitalWrite(COMM_SPI_CS_PIN, HIGH);  // Deselect slave
    
    Serial.printf("SPI Master initialized (SCK=%d, MISO=%d, MOSI=%d, CS=%d)\n",
                  COMM_SPI_SCK_PIN, COMM_SPI_MISO_PIN, COMM_SPI_MOSI_PIN, COMM_SPI_CS_PIN);
    Serial.printf("SPI Frequency: %d Hz\n", COMM_SPI_FREQUENCY);
    
    return true;
}

bool spiExchange(uint16_t rpmToSend, uint8_t modeToSend, uint8_t* requestedMode, uint16_t* requestedRpm) {
    if (!commSpi) return false;

    // Prepare master->slave packet (master is authoritative)
    uint8_t txBuffer[SPI_PACKET_SIZE];
    uint8_t rxBuffer[SPI_PACKET_SIZE];

    packMasterPacket(txBuffer, rpmToSend, modeToSend);

    // Begin SPI transaction
    commSpi->beginTransaction(spiSettings);
    digitalWrite(COMM_SPI_CS_PIN, LOW);  // Select slave

    // Delay for slave DMA to prepare
    delayMicroseconds(100);

    // Transfer entire buffer at once (better for DMA slave)
    commSpi->transferBytes(txBuffer, rxBuffer, SPI_PACKET_SIZE);

    // Small delay before releasing CS
    delayMicroseconds(10);

    digitalWrite(COMM_SPI_CS_PIN, HIGH);  // Deselect slave
    commSpi->endTransaction();

    // Gap between transactions for slave to re-queue
    delayMicroseconds(50);

    // Validate received packet from slave (contains UI input requests)
    if (validateSpiPacket(rxBuffer)) {
        *requestedMode = extractSpiMode(rxBuffer);
        *requestedRpm = extractSpiRpm(rxBuffer);
        successCount++;
        return true;
    }

    errorCount++;
    return false;
}

uint32_t spiGetSuccessCount() {
    return successCount;
}

uint32_t spiGetErrorCount() {
    return errorCount;
}

// =============================================================================
// OTA SPI Functions
// =============================================================================

bool spiOtaExchange(const uint8_t* txBuffer, uint8_t* rxBuffer, size_t len) {
    if (!commSpi) return false;
    
    // Use slower SPI settings for OTA (more reliable over jumper wires)
    commSpi->beginTransaction(spiSettingsOta);
    digitalWrite(COMM_SPI_CS_PIN, LOW);
    
    delayMicroseconds(100);  // Slave DMA prep time
    
    // Use transferBytes for proper SPI exchange
    commSpi->transferBytes(txBuffer, rxBuffer, len);
    
    delayMicroseconds(10);
    
    digitalWrite(COMM_SPI_CS_PIN, HIGH);
    commSpi->endTransaction();
    
    delayMicroseconds(50);  // Gap for slave to re-queue
    
    return true;
}

bool spiOtaExchangeBulk(const uint8_t* txBuffer, uint8_t* rxBuffer, size_t len) {
    if (!commSpi) return false;
    
    // Use slower SPI settings for OTA bulk transfers (more reliable over jumper wires)
    commSpi->beginTransaction(spiSettingsOta);
    digitalWrite(COMM_SPI_CS_PIN, LOW);
    
    delayMicroseconds(200);  // Longer prep time for bulk transfer
    
    // Transfer in chunks to avoid issues with large transfers
    size_t offset = 0;
    while (offset < len) {
        size_t chunkSize = min(len - offset, (size_t)64);  // 64 bytes at a time
        commSpi->transferBytes(txBuffer + offset, rxBuffer + offset, chunkSize);
        offset += chunkSize;
    }
    
    delayMicroseconds(10);
    
    digitalWrite(COMM_SPI_CS_PIN, HIGH);
    commSpi->endTransaction();
    
    delayMicroseconds(100);  // Longer gap after bulk transfer
    
    return true;
}
