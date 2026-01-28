#ifndef SHARED_CONFIG_H
#define SHARED_CONFIG_H

// SPI Configuration for Master-Slave Communication
// Full-duplex SPI allows simultaneous bidirectional data transfer
// Master sends: RPM data
// Slave sends: Mode + Manual RPM setting
#define COMM_SPI_MOSI_PIN  2   // Master Out, Slave In
#define COMM_SPI_MISO_PIN  3   // Master In, Slave Out
#define COMM_SPI_SCK_PIN   14  // Clock (master drives)
#define COMM_SPI_CS_PIN    21  // Chip Select (directly wired, no GPIO needed on slave)
#define COMM_SPI_FREQUENCY 1000000  // 1MHz SPI clock

// I2C Configuration for Touch Controller (Slave device only)
// Wire (I2C0): Touch controller on GPIO 16/15
#define I2C_FREQUENCY 100000  // Standard mode (100kHz)

// Wire - Touch controller (Slave device uses this as master to read touch)
#define I2C_TOUCH_SDA_PIN 16
#define I2C_TOUCH_SCL_PIN 15

// Master - MCP2515 SPI Pins
#define MCP2515_SCK_PIN  12
#define MCP2515_MISO_PIN 13
#define MCP2515_MOSI_PIN 11
#define MCP2515_CS_PIN   10
#define MCP2515_INT_PIN  9

// Master - KY-040 Rotary Encoder
// Button press toggles AUTO/MANUAL mode
// Rotation adjusts manual RPM (0-4000 in 100 increments)
#define ENCODER_CLK_PIN  4   // Clock (A) pin
#define ENCODER_DT_PIN   5   // Data (B) pin
#define ENCODER_SW_PIN   6   // Switch (button) pin

// Encoder RPM adjustment settings
#define ENCODER_RPM_MIN  0
#define ENCODER_RPM_MAX  4000
#define ENCODER_RPM_STEP 100

// Slave - ILI9341 SPI Pins (configured via TFT_eSPI build flags in platformio.ini)
// TFT_SCLK=12, TFT_MISO=13, TFT_MOSI=11, TFT_CS=10, TFT_DC=46, TFT_RST=-1 (shared with ESP32-S3)

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
#define SPI_SEND_INTERVAL_MS 100  // 10Hz update rate (SPI is fast and reliable)
#define SPI_TIMEOUT_MS 1000       // Show "NO SIGNAL" after 1 second
#define SIM_CHANGE_INTERVAL_MS 1000  // Simulation mode: RPM changes every 1 second

// CAN Configuration
// CAN speed and clock are defined in can_handler.cpp using library types

#endif // SHARED_CONFIG_H
