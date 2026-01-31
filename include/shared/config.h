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

// Master - Available GPIOs (formerly direct encoder, now freed up)
// GPIO 4, 5, 6 are available for other uses
// All rotary encoders now go through MCP23017 I2C expander

// =============================================================================
// Master - I2C Bus for MCP23017 GPIO Expander
// =============================================================================
// Provides 16 additional GPIOs for multiple rotary encoders
// See docs/schematics/master-custom-pcb.md Sheet 13 for circuit details
#define I2C_MASTER_SDA_PIN    47    // I2C Data
#define I2C_MASTER_SCL_PIN    48    // I2C Clock
#define I2C_MASTER_FREQ       400000 // 400kHz Fast mode

// MCP23017 Configuration
#define MCP23017_ADDR         0x20  // I2C address (A0=A1=A2=GND)
#define MCP23017_INT_PIN      45    // Interrupt pin (optional, directly connected to INTA)

// =============================================================================
// Master - Multiplexed Rotary Encoders (via MCP23017)
// =============================================================================
// Up to 5 encoders supported on single MCP23017 (16 pins / 3 pins per encoder)
// Encoder pins are on MCP23017 GPIO expander, not ESP32 directly
//
// Connector assignments:
//   J14 (ENC1): Power Steering Speed Control
//   J15 (ENC2): Available for future use
//   J16 (ENC3): Available for future use
//   J17 (ENC4): Available for future use
//   J18 (ENC5): Available for future use

// MCP23017 pin assignments for each encoder
// ENC1 (J14) - Power Steering Speed
#define ENC1_MCP_CLK    0     // GPA0
#define ENC1_MCP_DT     1     // GPA1
#define ENC1_MCP_SW     2     // GPA2

// ENC2 (J15) - Future use
#define ENC2_MCP_CLK    3     // GPA3
#define ENC2_MCP_DT     4     // GPA4
#define ENC2_MCP_SW     5     // GPA5

// ENC3 (J16) - Future use
#define ENC3_MCP_CLK    6     // GPA6
#define ENC3_MCP_DT     7     // GPA7
#define ENC3_MCP_SW     8     // GPB0

// ENC4 (J17) - Future use
#define ENC4_MCP_CLK    9     // GPB1
#define ENC4_MCP_DT     10    // GPB2
#define ENC4_MCP_SW     11    // GPB3

// ENC5 (J18) - Future use
#define ENC5_MCP_CLK    12    // GPB4
#define ENC5_MCP_DT     13    // GPB5
#define ENC5_MCP_SW     14    // GPB6
// Note: GPB7 (pin 15) is unused/available

// Number of encoders enabled (set to number actually connected)
#define MCP_ENCODER_COUNT     5

// Encoder function assignments
#define ENC_POWER_STEERING    0     // ENC1 - Power steering assist speed
#define ENC_UNUSED_1          1     // ENC2 - Available
#define ENC_UNUSED_2          2     // ENC3 - Available
#define ENC_UNUSED_3          3     // ENC4 - Available
#define ENC_UNUSED_4          4     // ENC5 - Available

// Master - PWM Controller Interface (Analog Output)
// ESP32-S3 has no DAC, so PWM + RC filter is used to generate 0-3.3V
// Signal is then amplified to 0-5V via op-amp circuit
// See docs/schematics/pwm-controller-interface.md for circuit details
#define PWM_OUTPUT_PIN      7     // PWM output for motor speed control
#define PWM_OUTPUT_FREQ     5000  // 5kHz PWM frequency
#define PWM_OUTPUT_CHANNEL  0     // LEDC channel
#define PWM_OUTPUT_RESOLUTION 8   // 8-bit resolution (0-255)

// Power Steering Speed Settings (ENC1 on J14)
// Controls assist motor speed via PWM output
#define POWER_STEERING_MIN      0       // Minimum assist (0%)
#define POWER_STEERING_MAX      100     // Maximum assist (100%)
#define POWER_STEERING_STEP     5       // Step size per encoder detent
#define POWER_STEERING_DEFAULT  50      // Default assist level (50%)

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

// Master - RPM Input (12V square wave via level shifter)
// Expects 1 pulse per revolution, level-shifted from 12V to 3.3V
// Recommended circuit: Optocoupler (PC817) or transistor level shifter
// See docs/schematics/rpm-input-circuit.md for circuit details
#define RPM_INPUT_PIN      1    // GPIO 1 - available input pin

// Master - VSS Input (Vehicle Speed Sensor - GM 700R4 VR Sensor)
// 2-wire Variable Reluctance sensor, 8000 pulses per mile
// Uses LM1815 VR sensor interface IC for adaptive threshold detection
// See docs/schematics/vss-input-circuit.md for circuit details
#define VSS_INPUT_PIN      38   // GPIO 38 - expansion GPIO
#define VSS_PULSES_PER_MILE 8000  // GM 700R4 transmission

// Master - Water Temperature Sensor (GM LS1 Coolant Temperature Sensor)
// NTC thermistor with 1kΩ pull-up to 3.3V creates voltage divider
// ADC reads 0-3.3V proportional to sensor resistance (temp)
// See docs/schematics/water-temp-input-circuit.md for circuit details
#define WATER_TEMP_INPUT_PIN    4     // GPIO 4 - ADC1_CH3
#define WATER_TEMP_PULLUP_OHMS  1000  // 1kΩ pull-up resistor
#define WATER_TEMP_ADC_ATTEN    ADC_ATTEN_DB_12  // Full 0-3.3V range

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
