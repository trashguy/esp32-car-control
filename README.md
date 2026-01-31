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
| Hardware Watchdog | 2-second timeout for safety |
| Virtual Memory | Optional SD-backed PSRAM cache for 16MB+ buffers |

### Master FreeRTOS Architecture

The master device uses FreeRTOS tasks with safety-critical pump control isolated at highest priority.

```
┌─────────────────┐                    ┌─────────────────┐
│   Pump Task     │                    │  SPI Comm Task  │
│  (Priority 10)  │                    │  (Priority 5)   │
│                 │                    │                 │
│  100Hz PWM out  │◄───── State ──────►│  10Hz exchange  │
│  Watchdog feed  │       Mutex        │  Slave sync     │
└────────┬────────┘                    └────────┬────────┘
         │                                      │
         │ PWM                             SPI (HSPI)
         ▼                                      ▼
   Pump Motors                            Slave ESP32
```

| Task | Priority | Core | Rate | Purpose |
|------|----------|------|------|---------|
| Pump | 10 (Max) | 1 | 100Hz | Safety-critical PWM output, watchdog |
| SPI_Comm | 5 | 0 | 10Hz | Bidirectional slave communication |
| UI | 3 | 1 | 50Hz | Encoder input, serial commands |
| NVS | 1 | 0 | 1Hz | Debounced settings persistence |

**Safety Features:**
- Watchdog fed from highest-priority Pump task (2 sec timeout)
- Failsafe mode on SPI timeout (pumps run at 78% duty)
- RTC memory tracks crash count across resets
- Crash log written to SD card

**Serial Debug Commands:**
- `c` - Statistics (SPI, CAN, RPM, PWM)
- `h` - System health status
- `T` - Task stack/state info
- `?` - Help

### Virtual Memory System (Master)

The master device includes an optional virtual memory system that provides 16MB+ buffer capacity using SD card as backing store with PSRAM as a hot cache. This is useful for large data processing tasks that exceed the ESP32-S3's 8MB PSRAM.

```
┌─────────────────┐     Page Fault     ┌─────────────────┐
│   Application   │ ──────────────────>│  PSRAM Cache    │
│                 │<────────────────── │   (6MB, ~750    │
│  vmem.read()    │     Cache Hit      │    pages)       │
│  vmem.write()   │                    └────────┬────────┘
└─────────────────┘                             │
                                          LRU Eviction
                                                │
                                                ▼
                                    ┌─────────────────┐
                                    │   SD Card       │
                                    │  Swap File      │
                                    │  (32MB default) │
                                    └─────────────────┘
```

**Configuration** (in `include/shared/config.h`):
```cpp
#define VIRTUAL_MEMORY              0       // Set to 1 to enable
#define VIRTUAL_MEMORY_SIZE_MB      32      // Virtual address space
#define VIRTUAL_MEMORY_PAGE_SIZE    8192    // 8KB pages
#define VIRTUAL_MEMORY_CACHE_MB     6       // PSRAM cache size
```

**API Usage:**
```cpp
#include "master/virtual_memory.h"

// Read/write to virtual addresses (0 to VIRTUAL_MEMORY_SIZE-1)
vmem.write(0x100000, data, 1024);    // Write 1KB at 1MB offset
vmem.read(0x100000, buffer, 1024);   // Read it back

// Cache management
vmem.flush();                         // Write dirty pages to SD
vmem.prefetch(addr, len);            // Hint for sequential access

// Performance monitoring
vmem.printStats();                   // Print hit rate, evictions, etc.
float rate = vmem.hitRate();         // 0.0 - 1.0 (target: >80%)
```

**Performance Characteristics:**
| Access Type | Latency | Throughput |
|-------------|---------|------------|
| Cache Hit (PSRAM) | ~100ns | ~400 MB/s |
| Cache Miss (SD read) | ~1-5ms | ~3 MB/s |
| Write-back (SD write) | ~2-10ms | ~2 MB/s |

**Best Practices:**
- Works best with sequential or localized access patterns
- Monitor hit rate - below 70% indicates poor locality
- Call `vmem.flush()` before power-off to avoid data loss
- Use `vmem.prefetch()` for known sequential reads

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
| USB Mass Storage | Expose SD card as USB drive (production build only) |
| Status Indicators | CONNECTED, SYNCED, SYNCING, NO SIGNAL states |

### Slave FreeRTOS Architecture

The slave device uses FreeRTOS tasks for concurrent operation, ensuring responsive SPI communication while maintaining smooth UI rendering.

```
┌─────────────────┐     Queue      ┌─────────────────┐
│   SPI Task      │ ──────────────>│  Display Task   │
│  (High Pri)     │  RPM/Mode      │   (Med Pri)     │
│                 │<───────────────│                 │
│  100Hz polling  │  UI Commands   │   ~60Hz render  │
└─────────────────┘                └─────────────────┘
        │                                   │
        │ SPI3                    TFT (SPI2) + I2C
        ▼                                   ▼
   Master ESP32                    Display + Touch
```

| Task | Priority | Core | Rate | Purpose |
|------|----------|------|------|---------|
| SPI_Comm | 5 (High) | 1 | 100Hz | Master communication, packet validation |
| Display | 3 (Med) | 1 | ~60Hz | UI rendering, touch input, animations |
| Serial | 1 (Low) | 0 | 20Hz | Debug commands and statistics |

**Inter-task Communication:**
- `queueSpiToDisplay` - RPM/mode updates from master (SPI → Display)
- `queueDisplayToSpi` - User commands to master (Display → SPI)
- `mutexTft` - Protects TFT SPI access
- `mutexI2C` - Protects touch controller I2C access

**Serial Debug Commands:**
- `c` - Show statistics (packet counts, task stack usage, heap)
- `t` - Show task state and priorities
- `e` - Eject USB mass storage (production build only)
- `?` - Help

## Pin Assignments

**Note:** ESP32-S3-N16R8 reserves GPIO 26-32 (flash) and GPIO 33-37 (PSRAM). Do not use these pins.

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

#### PWM Controller Interface (Analog Output)

| Function | GPIO | Description |
|----------|------|-------------|
| PWM OUT | 7 | PWM signal for motor speed (filtered to analog) |

Outputs 0-3.3V analog signal (via RC filter) to control external PWM motor controller.
Requires op-amp circuit to amplify to 0-5V. See `docs/schematics/pwm-controller-interface.md`.

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
pio run -e slave     # Build slave (development)
pio run -e slave_prod # Build slave (production with USB MSC)

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

## Slave Build Environments

The slave device has two build environments to handle the USB hardware limitations of ESP32-S3:

| Environment | USB Mode | Features | Flash Method |
|-------------|----------|----------|--------------|
| `slave` | Hardware USB-JTAG | Easy flashing, no USB MSC | Automatic |
| `slave_prod` | TinyUSB | USB Mass Storage + Serial | BOOT button required |

### Development Build (slave)

Uses `ARDUINO_USB_MODE=1` (hardware USB-JTAG) for easy flashing:

```bash
pio run -e slave -t upload
```

- Auto-reset works normally
- No USB Mass Storage capability
- USB button hidden in Settings screen
- Recommended for active development

### Production Build (slave_prod)

Uses `ARDUINO_USB_MODE=0` (TinyUSB) for USB Mass Storage:

```bash
# First, enter bootloader mode:
# 1. Unplug USB cable
# 2. Hold BOOT button on ESP32-S3
# 3. Plug in USB cable (keep holding BOOT)
# 4. Wait 2 seconds, release BOOT button

# Then flash:
pio run -e slave_prod -t upload
```

**Features:**
- USB Mass Storage to copy files (BMP images) to SD card via USB
- Serial console still available (CDC + MSC composite device)
- USB button visible in Settings screen (green when enabled)
- Toggle USB MSC via touchscreen or serial command 'e' to eject

**Bootloader Recovery:**

If the device becomes unresponsive or only shows as a mass storage device:

1. Unplug USB cable
2. Hold BOOT button
3. Plug in USB cable (keep holding BOOT)
4. Wait 2 seconds, release BOOT
5. Flash immediately with `pio run -e slave_prod -t upload`

**Note:** The BOOT button sequence is required because TinyUSB mode doesn't support the automatic reset-to-bootloader feature of hardware USB-JTAG.

## Project Structure

```
esp32-car-control/
├── include/
│   ├── master/
│   │   ├── sd_handler.h       # SD card API
│   │   └── virtual_memory.h   # Virtual memory API (optional)
│   ├── slave/                 # Slave-specific headers
│   └── shared/
│       ├── config.h           # Shared configuration
│       └── protocol.h         # SPI protocol definitions
├── src/
│   ├── master/
│   │   ├── main.cpp            # Master entry point
│   │   ├── tasks.cpp           # FreeRTOS task management
│   │   ├── can_handler.cpp     # CAN bus handling
│   │   ├── spi_master.cpp      # SPI master communication
│   │   ├── sd_handler.cpp      # SD card handling
│   │   └── virtual_memory.cpp  # SD-backed PSRAM cache (optional)
│   └── slave/
│       ├── main.cpp         # Slave entry point
│       ├── tasks.cpp        # FreeRTOS task management
│       ├── display.cpp      # TFT display and UI
│       ├── display_common.cpp # Shared display utilities
│       ├── screen_main.cpp  # Main RPM screen
│       ├── screen_settings.cpp # Diagnostics screen
│       ├── screen_filebrowser.cpp # SD card browser
│       ├── screen_wifi.cpp  # WiFi configuration
│       ├── spi_slave.cpp    # SPI slave communication
│       └── sd_card.cpp      # SD card handling
├── platformio.ini       # Build configuration
├── CLAUDE.md           # AI assistant instructions
└── README.md           # This file
```

## Hardware Components

| Component | Model | Interface |
|-----------|-------|-----------|
| Microcontroller | ESP32-S3-N16R8 DevKitC-1 (x2) | 16MB Flash, 8MB PSRAM |
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
| Watchdog Timeout | 2 seconds |

## License

This project is intended for AI/ML experimentation.
