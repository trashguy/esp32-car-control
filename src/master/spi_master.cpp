#include "master/spi_master.h"
#include "shared/config.h"
#include "shared/protocol.h"
#include <Arduino.h>
#include <SPI.h>

// Use HSPI (SPI2) for communication - separate from VSPI used by other peripherals
static SPIClass* commSpi = nullptr;
static SPISettings spiSettings(COMM_SPI_FREQUENCY, MSBFIRST, SPI_MODE0);

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
