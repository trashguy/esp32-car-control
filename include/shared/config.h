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

// Master - PWM Controller Interface (Analog Output)
// ESP32-S3 has no DAC, so PWM + RC filter is used to generate 0-3.3V
// Signal is then amplified to 0-5V via op-amp circuit
// See docs/schematics/pwm-controller-interface.md for circuit details
#define PWM_OUTPUT_PIN      7     // PWM output for motor speed control
#define PWM_OUTPUT_FREQ     5000  // 5kHz PWM frequency
#define PWM_OUTPUT_CHANNEL  0     // LEDC channel
#define PWM_OUTPUT_RESOLUTION 8   // 8-bit resolution (0-255)

// Encoder RPM adjustment settings
#define ENCODER_RPM_MIN  0
#define ENCODER_RPM_MAX  4000
#define ENCODER_RPM_STEP 100

// Master - SD Card (SPI Mode)
// Uses dedicated SPI bus for data logging and configuration storage
// See docs/schematics/master-custom-pcb.md for circuit details
#define SD_SPI_CS_PIN    15    // Chip select (active LOW)
#define SD_SPI_MOSI_PIN  16    // Master Out, Slave In
#define SD_SPI_SCK_PIN   17    // SPI Clock
#define SD_SPI_MISO_PIN  18    // Master In, Slave Out
#define SD_SPI_CD_PIN    8     // Card detect (optional, LOW = card present)
#define SD_SPI_FREQUENCY 25000000  // 25MHz (max for most SD cards in SPI mode)

// Master - JTAG Debug Interface (directly exposed to J10 header)
// Standard ARM Cortex 10-pin debug connector
// These pins are directly connected - no firmware configuration needed for JTAG
#define JTAG_TCK_PIN     39    // Test Clock
#define JTAG_TDO_PIN     40    // Test Data Out
#define JTAG_TDI_PIN     41    // Test Data In
#define JTAG_TMS_PIN     42    // Test Mode Select

// Master - UART Debug Interface (directly exposed to J11 header)
// Secondary serial port for diagnostics when USB unavailable
#define DEBUG_UART_TX_PIN  43   // UART TX (GPIO 43 = U0TXD default)
#define DEBUG_UART_RX_PIN  44   // UART RX (GPIO 44 = U0RXD default)
#define DEBUG_UART_BAUD    115200

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

// =============================================================================
// Virtual Memory - SD card backed PSRAM cache (Master only)
// =============================================================================
// Provides 16MB+ memory buffers using SD card as backing store with PSRAM cache
// Disabled by default to allow testing without SD card
#define VIRTUAL_MEMORY              0       // Set to 1 to enable
#define VIRTUAL_MEMORY_SIZE_MB      32      // Virtual address space in MB
#define VIRTUAL_MEMORY_PAGE_SIZE    8192    // 8KB pages (balance overhead vs efficiency)
#define VIRTUAL_MEMORY_CACHE_MB     6       // PSRAM cache size (~6MB, leave room for other uses)

// CAN Configuration
// CAN speed and clock are defined in can_handler.cpp using library types

#endif // SHARED_CONFIG_H
