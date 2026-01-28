# ESP32 Car Control

A dual-ESP32-S3 system for power steering RPM monitoring and display. The system consists of a master device that reads RPM data from the vehicle CAN bus and a slave device that displays the information on a TFT touchscreen.

## System Architecture

```
┌─────────────────────┐      SPI (1MHz)      ┌─────────────────────┐
│    MASTER ESP32     │◄────────────────────►│    SLAVE ESP32      │
│                     │                      │                     │
│  - CAN Bus Reader   │                      │  - TFT Display      │
│  - Rotary Encoder   │                      │  - Touch Input      │
│  - Mode Control     │                      │  - SD Card          │
│  - NVS Storage      │                      │  - WiFi             │
└─────────────────────┘                      └─────────────────────┘
         │
         │ SPI
         ▼
┌─────────────────────┐
│      MCP2515        │
│   CAN Controller    │
│         │           │
│    Vehicle CAN      │
└─────────────────────┘
```

## Features

### Master Device

| Feature | Description |
|---------|-------------|
| CAN Bus Interface | Reads RPM data from vehicle CAN bus via MCP2515 |
| Operating Modes | SNIFF (debug), RPM (normal), SIMULATE (testing) |
| Display Modes | AUTO (CAN-sourced RPM) or MANUAL (user-set RPM) |
| Rotary Encoder | KY-040 encoder for mode toggling and manual RPM adjustment |
| Settings Persistence | Stores mode and manual RPM in NVS |
| Full-duplex SPI | Bidirectional communication with slave device |
| Hardware Watchdog | 5-second timeout for safety |

### Slave Device

| Feature | Description |
|---------|-------------|
| TFT Display | ILI9341 320x240 LCD with multiple screens |
| Touch Input | FT6336G capacitive touch controller |
| Main Screen | RPM display with mode toggle and settings access |
| Diagnostics Screen | Connection status, SPI packet counts, SD card info |
| File Browser | Browse SD card contents |
| WiFi Settings | Configure WiFi connection via touch UI |
| QWERTY Keyboard | Full-screen keyboard for text input |
| SD Card Support | SDMMC interface for file storage |
| Status Indicators | CONNECTED, SYNCED, SYNCING, NO SIGNAL states |

## Pin Assignments

### Master ESP32-S3

#### CAN Bus (MCP2515)

| Function | GPIO | Description |
|----------|------|-------------|
| SCK | 12 | SPI Clock |
| MISO | 13 | SPI Data In |
| MOSI | 11 | SPI Data Out |
| CS | 10 | Chip Select |
| INT | 9 | Interrupt |

#### Rotary Encoder (KY-040)

| Function | GPIO | Description |
|----------|------|-------------|
| CLK | 4 | Encoder Clock/A |
| DT | 5 | Encoder Data/B |
| SW | 6 | Encoder Button |

#### SPI Communication to Slave (HSPI/SPI2)

| Function | GPIO | Description |
|----------|------|-------------|
| MOSI | 2 | Master Out, Slave In |
| MISO | 3 | Master In, Slave Out |
| SCK | 14 | SPI Clock |
| CS | 21 | Chip Select |

### Slave ESP32-S3

#### TFT Display (ILI9341)

| Function | GPIO | Description |
|----------|------|-------------|
| SCK | 12 | SPI Clock |
| MISO | 13 | SPI Data In |
| MOSI | 11 | SPI Data Out |
| CS | 10 | Chip Select |
| DC | 46 | Data/Command |
| BL | 45 | Backlight |
| RST | -1 | Reset (tied to VCC) |

#### Touch Controller (FT6336G)

| Function | GPIO | Description |
|----------|------|-------------|
| SDA | 16 | I2C Data |
| SCL | 15 | I2C Clock |
| INT | 17 | Interrupt |
| RST | 18 | Reset |

#### SD Card (SDMMC)

| Function | GPIO | Description |
|----------|------|-------------|
| CLK | 38 | SD Clock |
| CMD | 40 | SD Command |
| D0 | 39 | Data Line 0 |
| D1 | 41 | Data Line 1 |
| D2 | 48 | Data Line 2 |
| D3 | 47 | Data Line 3 |

#### SPI Communication from Master (SPI3)

| Function | GPIO | Description |
|----------|------|-------------|
| MOSI | 2 | Master Out, Slave In |
| MISO | 3 | Master In, Slave Out |
| SCK | 14 | SPI Clock |
| CS | 21 | Chip Select |

## Communication Protocol

### SPI Master-Slave Protocol

| Parameter | Value |
|-----------|-------|
| Frequency | 1 MHz |
| Mode | SPI Mode 0 (CPOL=0, CPHA=0) |
| Packet Size | 5 bytes |
| Update Rate | 10 Hz (100ms interval) |
| Timeout | 1 second |

**Packet Structure:**
```
Byte 0: Header (0xAA)
Byte 1: RPM High Byte
Byte 2: RPM Low Byte
Byte 3: Mode (0=AUTO, 1=MANUAL)
Byte 4: Checksum (XOR of bytes 1-3)
```

### CAN Bus Configuration

| Parameter | Value |
|-----------|-------|
| Speed | 500 kbps |
| Crystal | 8 MHz |
| Controller | MCP2515 |

## Build Commands

```bash
# Build the project
pio run

# Build for specific environment
pio run -e master    # Build master only
pio run -e slave     # Build slave only

# Upload to connected ESP32
pio run -t upload -e master
pio run -t upload -e slave

# Monitor serial output (115200 baud)
pio device monitor

# Build, upload, and monitor
pio run -t upload -t monitor -e master

# Clean build artifacts
pio run -t clean
```

## Project Structure

```
esp32-car-control/
├── include/
│   ├── master/          # Master-specific headers
│   ├── slave/           # Slave-specific headers
│   └── shared/
│       ├── config.h     # Shared configuration
│       └── protocol.h   # SPI protocol definitions
├── src/
│   ├── master/
│   │   ├── main.cpp         # Master entry point
│   │   ├── can_handler.cpp  # CAN bus handling
│   │   └── spi_master.cpp   # SPI master communication
│   └── slave/
│       ├── main.cpp         # Slave entry point
│       ├── display.cpp      # TFT display and UI
│       ├── spi_slave.cpp    # SPI slave communication
│       └── sd_card.cpp      # SD card handling
├── platformio.ini       # Build configuration
├── CLAUDE.md           # AI assistant instructions
└── README.md           # This file
```

## Hardware Components

| Component | Model | Interface |
|-----------|-------|-----------|
| Microcontroller | ESP32-S3 DevKit-C-1 (x2) | - |
| Display | ILI9341 320x240 TFT | SPI |
| Touch Controller | FT6336G | I2C |
| CAN Controller | MCP2515 | SPI |
| Rotary Encoder | KY-040 | GPIO |
| SD Card | microSD | SDMMC |

## Configuration

### Master Settings (NVS Persisted)
- Display Mode: AUTO or MANUAL
- Manual RPM: 0-4000 (100 RPM increments)

### Slave Settings (NVS Persisted)
- WiFi Mode: Disabled or Client
- WiFi SSID
- WiFi Password

### Timing Parameters
| Parameter | Value |
|-----------|-------|
| SPI Update Interval | 100ms |
| SPI Timeout | 1 second |
| Simulation Interval | 1 second |
| Button Debounce | 200ms |
| Watchdog Timeout | 5 seconds |

## License

This project is intended for AI/ML experimentation.
