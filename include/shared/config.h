#ifndef SHARED_CONFIG_H
#define SHARED_CONFIG_H

// I2C Configuration (shared bus for touch and RPM slave on GPIO 16/15)
#define I2C_SLAVE_ADDRESS 0x42
#define I2C_SDA_PIN 16
#define I2C_SCL_PIN 15
#define I2C_FREQUENCY 100000  // Standard mode (100kHz)

// Master - MCP2515 SPI Pins
#define MCP2515_SCK_PIN  12
#define MCP2515_MISO_PIN 13
#define MCP2515_MOSI_PIN 11
#define MCP2515_CS_PIN   10
#define MCP2515_INT_PIN  14

// Slave - ILI9341 SPI Pins
#define TFT_SCK_PIN  12
#define TFT_MISO_PIN 13
#define TFT_MOSI_PIN 11
#define TFT_CS_PIN   10
#define TFT_DC_PIN   14
#define TFT_RST_PIN  21

// Slave - FT6336G Capacitive Touch (I2C)
#define TOUCH_I2C_ADDR 0x38   // FT6336G default I2C address
#define TOUCH_INT_PIN 17
#define TOUCH_RST_PIN 18

// Slave - LCD Backlight
#define TFT_BL_PIN 45

// Slave - SD Card (SDIO/SDMMC interface)
#define SD_MMC_CLK  38
#define SD_MMC_CMD  40
#define SD_MMC_D0   39
#define SD_MMC_D1   41
#define SD_MMC_D2   48
#define SD_MMC_D3   47
// Set to true for 1-bit mode, false for 4-bit mode
#define SD_MMC_1BIT_MODE true

// Timing
#define I2C_SEND_INTERVAL_MS 500  // 2Hz update rate (reduced for stability without pullups)
#define I2C_TIMEOUT_MS 1000       // Show "NO SIGNAL" after 1 second

// CAN Configuration
// CAN speed and clock are defined in can_handler.cpp using library types

#endif // SHARED_CONFIG_H
