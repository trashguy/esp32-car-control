# ESP32 Car Control

Dual-ESP32-S3 automotive monitoring system with TFT touchscreen display and OTA firmware updates.

## Overview

```
┌─────────────────────┐      SPI       ┌─────────────────────┐
│   Controller MCU    │◄──────────────►│    Display MCU      │
│                     │                │                     │
│  • CAN Bus          │                │  • 320x240 TFT      │
│  • Water Temp       │                │  • Touch UI (LVGL)  │
│  • RPM / VSS        │                │  • WiFi / OTA       │
│  • PWM Output       │                │  • SD Card          │
└─────────────────────┘                └─────────────────────┘
```

## Features

**Controller MCU (master)**
- CAN bus interface (MCP2515) for vehicle data
- Water temperature sensor (GM LS1 NTC thermistor)
- Engine RPM and vehicle speed inputs
- PWM output for pump/motor control
- Hardware watchdog with failsafe mode

**Display MCU (slave)**
- LVGL-based dark theme UI with dashboard widgets
- Water temperature gauge with overheat warning (235°F)
- Power steering and water pump status displays
- WiFi configuration and OTA firmware updates
- SD card file browser and USB mass storage

## Quick Start

```bash
# Build both firmwares
make firmware

# Flash via USB (first time)
make flash-controller
make flash-display

# OTA update (after initial flash)
make upload
```

See [BUILD.md](BUILD.md) for detailed build instructions.

## Project Structure

```
src/
├── master/          # Controller MCU firmware
└── slave/           # Display MCU firmware
include/
├── shared/          # Common protocol and config
├── master/          # Controller headers
└── slave/           # Display headers
hardware/            # PCB design files (EasyEDA/KiCad)
docs/                # Technical documentation
tools/ota-pusher/    # Desktop OTA upload tool
```

## Documentation

| Document | Description |
|----------|-------------|
| [BUILD.md](BUILD.md) | Build system, flashing, OTA updates |
| [CHANGELOG.md](CHANGELOG.md) | Version history |
| [docs/schematics/](docs/schematics/) | Hardware circuit designs |
| [docs/esp32-s3-info.md](docs/esp32-s3-info.md) | ESP32-S3 hardware reference |

## Hardware

| Component | Description |
|-----------|-------------|
| MCU | ESP32-S3-N16R8 (x2) - 16MB Flash, 8MB PSRAM |
| Display | ILI9341 320x240 TFT with FT6336G touch |
| CAN | MCP2515 controller |
| Sensors | GM LS1 water temp, RPM tach, VSS |

## Serial Commands

**Controller:** `c` stats, `h` health, `w`/`W` water temp on/off, `?` help

**Display:** `c` stats, `t` tasks, `w` wifi, `o` OTA status, `?` help

## License

This project is for experimental and educational purposes.
