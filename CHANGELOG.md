# Changelog

All notable changes to this project are documented in this file.

## [1.3.0] - 2026-01-31

Water temperature monitoring with display widget and hardware support.

### Added
- **Water Temperature Sensor Support** - GM LS1-style NTC thermistor coolant temperature sensor:
  - New `water_temp.cpp/h` module with Steinhart-Hart temperature calculation
  - ADC-based reading with configurable averaging (default 16 samples)
  - Sensor health detection: open circuit, short circuit, and disabled states
  - Serial commands: `w` to enable/show, `W` to disable
- **Water Temperature Display Widget** - Pill-shaped widget on main screen:
  - Shows temperature in °F with status indicators (DISC, SHORT, OFF)
  - Overheat warning at >= 235°F with red pill background
  - Screen background flashes red during overheat (500ms blink interval)
  - Positioned below Water Pump widget, hidden when disconnected
- **SPI Protocol Extension** - Expanded packet from 5 to 8 bytes:
  - Added water temperature (int16_t, F*10 format for 0.1°F precision)
  - Added water temperature status byte (OK, DISCONNECTED, SHORTED, DISABLED)
  - Backward-compatible checksum calculation
- **Simulated Water Temperature** - For testing without hardware:
  - Ramps from 122°F to 302°F and back in 5°F steps every 500ms
  - Active in SIMULATE mode or when sensor disabled
- **Hardware Circuit Documentation** - `docs/schematics/water-temp-input-circuit.md`:
  - Full schematic with ESD protection (10K + 3.3V Zener)
  - RC low-pass filter (1.6kHz cutoff) for ignition noise rejection
  - Resistance/temperature lookup table for GM LS1 CTS
  - Bill of materials and wiring notes

### Changed
- **SPI Packet Structure** - Extended to 8 bytes for water temp data:
  - Master->Slave: Header + RPM (2B) + Mode + WaterTemp (2B) + Status + Checksum
  - Slave->Master: Header + RPM (2B) + Mode + Reserved (3B) + Checksum
- **`spiExchange()` Signature** - Added `waterTempF10` and `waterStatus` parameters
- **`SpiToDisplayMsg` Struct** - Added `waterTempF10` and `waterTempStatus` fields
- **Master PCB Schematic** - Updated with water temp input circuit (J18 connector, GPIO 4)
- **Config Header** - Added `WATER_TEMP_INPUT_PIN` (GPIO 4) and related constants

### Technical
- Protocol constants: `WATER_TEMP_STATUS_OK/DISCONNECTED/SHORTED/DISABLED`
- Special value: `WATER_TEMP_INVALID` (0x7FFF) for error conditions
- Warning threshold: 235°F triggers visual overheat alarm

## [1.2.0] - 2026-01-31

Android-style dark theme redesign with new dashboard widgets.

### Added
- **Power Steering Widget** - Oval pill-shaped container showing mode (AUTO/MANUAL) and RPM value
- **Water Pump Widget** - Oval container showing mode and percentage value
- **WiFi Status Icon** - Green WiFi indicator in top-right when connected
- **Menu Bar Auto-Hide** - Bottom menu bar hides after 3 seconds, tap screen to reveal
- **Background Generator** - Python tool (`tools/generate_background.py`) for creating gradient backgrounds
- **VONDERWAGEN Branding** - Brand identity at top of main screen

### Changed
- **Dark Theme** - Grey background (#303030) with black 48px bottom menu bar
- **Navigation UX** - Moved all back/action buttons to bottom menu bar
- **Button Styling** - Transparent nav buttons with white icons and subtle press highlight
- **Mode Display** - Changed from "A"/"M" letters to full "AUTO"/"MANUAL" text
- **WiFi Settings** - Redesigned with scrollable container and pill-style toggle buttons
- **LVGL Memory** - Increased heap allocation from 48KB to 64KB for stability

### Fixed
- **File Browser** - Fixed memory issues with list item radius
- **WiFi Screen** - Fixed memory issues with list item radius
- **Keyboard Dismiss** - Tapping outside text fields now hides keyboard

## [1.1.0] - 2026-01-31

LVGL UI system with legacy TFT fallback and display code reorganization.

### Added
- **LVGL 9.x UI System** - Modern graphics library for touchscreen interface:
  - New LVGL-based screens: main, settings, filebrowser, wifi, OTA popup
  - LVGL driver (`lvgl_driver.cpp/h`) for TFT_eSPI integration
  - Theme system (`ui_theme.cpp/h`) with consistent styling
  - Reusable keyboard wrapper (`ui_keyboard.cpp/h`)
- **Build Configuration Split** - Separate environments for UI systems:
  - `slave` / `slave_prod` - LVGL UI (40.6% flash, 54.4% RAM)
  - `slave_legacy` / `slave_legacy_prod` - Legacy TFT UI (28.1% flash, 15.8% RAM)
  - `USE_LVGL_UI` build flag for conditional compilation
- **Makefile Targets** - Legacy build support:
  - `make display-legacy` - Build legacy TFT firmware
  - `make flash-display-legacy` - Flash legacy firmware via USB

### Changed
- **Display Code Reorganization** - Restructured `src/slave/display/`:
  - `legacy/` - TFT_eSPI direct-drawing screens and keyboard
  - `lvgl/` - LVGL-based screens, theme, and keyboard wrapper
  - Shared modules remain in parent directory
- **OTA Popup** - Moved to respective UI directories with conditional compilation

### Technical
- Added `exclude_arm_asm.py` - Build script to disable ARM assembly in LVGL
- Updated `lv_conf.h` - LVGL configuration for ESP32-S3

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
