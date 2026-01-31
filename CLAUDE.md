# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32-S3 dual-MCU car control system with OTA firmware updates. Uses PlatformIO with the Arduino framework.

- **Display MCU (slave)**: TFT touch display, WiFi, SD card, OTA reception
- **Controller MCU (master)**: CAN bus, sensors, motor control

## Build Commands

```bash
# Build both MCU firmwares
make firmware

# Build individually
make display      # Display MCU (slave)
make controller   # Controller MCU (master)

# Create OTA update package
make package

# Discover devices and upload OTA package
make discover
make upload

# USB flash (initial programming)
make flash-display
make flash-controller

# Serial monitor
make monitor

# Clean build artifacts
make clean
```

See `BUILD.md` for comprehensive build documentation.

## Project Structure

```
src/
├── master/       - Controller MCU source
└── slave/        - Display MCU source
include/
├── master/       - Controller headers
├── slave/        - Display headers
└── shared/       - Common protocol/config
hardware/         - PCB schematics (EasyEDA/KiCad)
tools/
└── ota-pusher/   - Desktop OTA upload tool
docs/             - Technical documentation
```

## Key Files

- `platformio.ini` - PlatformIO build configuration
- `Makefile` - Build orchestration
- `VERSION` - Firmware version (semantic versioning)
- `include/shared/config.h` - Pin assignments and constants
- `include/shared/protocol.h` - SPI protocol definitions

## Adding Libraries

Add dependencies to `platformio.ini`:
```ini
lib_deps =
    library_name
    username/library_name@^1.0.0
```
