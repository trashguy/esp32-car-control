# Changelog

All notable changes to this project are documented in this file.

## [1.0.0] - 2026-01-31

First production-ready release with OTA firmware update capability.

### Added
- **OTA Firmware Update System** - Wireless firmware updates for both MCUs:
  - WiFi-based package reception on display MCU
  - SPI-based firmware transfer to controller MCU
  - SD card storage for update packages
  - Interactive popup UI with install/abort options and progress tracking
  - CRC32 verification for data integrity
- **OTA Pusher Desktop Tool** - C++ utility for firmware deployment (`tools/ota-pusher/`):
  - mDNS device discovery (`make discover`)
  - Package creation with dual-firmware bundling
  - WiFi upload with progress feedback
  - Custom binary package format with JSON manifest
- **USB Mass Storage** - SD card access via USB (`usb_msc.cpp/h`):
  - Composite USB device (CDC serial + MSC storage)
  - Host-initiated eject detection
  - Production build support (disabled in dev builds)
- **Build System** - Makefile-based workflow for firmware management:
  - `make firmware` - Build both MCU firmwares
  - `make package` - Create OTA update package
  - `make upload` - Discover and upload to device
  - `make release` - Tagged release workflow
- **Version System** - Semantic versioning with build-time injection:
  - `VERSION` file for version management
  - `version.h` with compile-time version/timestamp macros
  - `get_version.py` for CI/build integration

### Documentation
- Added `BUILD.md` - Comprehensive build system guide
- Added `docs/esp32-s3-info.md` - ESP32-S3 hardware reference

### Infrastructure
- Added `Makefile` for unified build orchestration
- Updated `platformio.ini` with OTA and production build configurations

## [0.0.1] - 2026-01-30

Initial development release of the ESP32 car control system.

### Added
- **Initial Project Structure** - Base ESP32 Arduino project with PlatformIO
- **Master-Slave Architecture** - Dual ESP32 system with SPI communication:
  - Master: CAN bus interface, motor control, sensor reading
  - Slave: TFT display, SD card, user interface
- **SPI Communication** - High-speed SPI bus for master-slave data exchange (`spi_master.cpp/h`, `spi_slave.cpp/h`)
- **CAN Bus Handler** - CAN bus communication module for vehicle integration (`can_handler.cpp/h`)
- **TFT Display Interface** - Touch-enabled display with LVGL graphics (`display.cpp/h`)
- **SD Card Support** - SD card interface on slave for data logging (`sd_card.cpp/h`)
- **Shared Protocol** - Common data structures for inter-processor communication (`protocol.h`)
- **Configuration Header** - Centralized pin and parameter configuration (`config.h`)
- **Virtual Memory System** - Virtual memory module (`virtual_memory.cpp/h`) for persistent data storage
- **SD Card Handler** - SD card handler module (`sd_handler.cpp/h`) for file operations on master
- **Modular Touch Keyboard** - Reusable keyboard module (`keyboard.cpp/h`)
- **Modular Screen System** - Split display code into modular screen components:
  - `screen_main.cpp/h` - Main dashboard screen
  - `screen_wifi.cpp/h` - WiFi configuration screen with on-screen keyboard
  - `screen_settings.cpp/h` - Settings/diagnostics screen
  - `screen_filebrowser.cpp/h` - File browser interface
  - `display_common.cpp/h` - Shared display utilities and types
- **Task Modules** - Task management modules for both master and slave (`tasks.cpp/h`)
- **Vehicle Speed Sensor (VSS) Input** - VSS counter module (`vss_counter.cpp/h`) for reading vehicle speed signals
- **Engine RPM Input** - RPM counter module (`rpm_counter.cpp/h`) for reading engine tachometer signals
- **Encoder Multiplexer** - Encoder multiplexer module (`encoder_mux.cpp/h`) for managing multiple rotary encoders with reduced pin count
- **Hardware Schematics Documentation**:
  - 12V to 3.3V regulator circuit design
  - Master PCB complete schematic
  - Custom master PCB design with component selection
  - PWM controller interface documentation
  - RPM input circuit design
  - VSS input circuit design
- **EasyEDA Hardware Project** - EasyEDA PCB project files with BOM and netlist
- **KiCad Schematic Export** - KiCad project files and netlist generation scripts for the master PCB
- **Power Steering Documentation** - Technical docs for power steering torque equations and Volvo XC60 EPS CAN bus integration

### Infrastructure
- Added `.gitignore` for build artifacts and IDE files
- Added `CLAUDE.md` with AI assistant guidelines
- Added `TODO.md` for tracking planned features
- Configured `platformio.ini` for ESP32 and ESP32-S3 targets
