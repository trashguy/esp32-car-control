# VONDERWAGENCC1 Build System

This document describes how to build, package, and deploy firmware for the VONDERWAGENCC1 system.

## Overview

The VONDERWAGENCC1 consists of two ESP32-S3 microcontrollers:

| MCU | Internal Name | Role |
|-----|---------------|------|
| **Display** | slave | TFT display, touch UI, WiFi, SD card |
| **Controller** | master | CAN bus, sensors, motor control |

Both firmwares are built and packaged together for OTA updates.

## Prerequisites

### Quick Check

```bash
make check
```

### Install Dependencies

```bash
# Automatic installation (Ubuntu/Debian)
make install-deps

# Manual installation
sudo apt install build-essential cmake libavahi-client-dev libzip-dev python3-pip
pip3 install --user platformio
```

### Required Tools

| Tool | Purpose | Install |
|------|---------|---------|
| PlatformIO | ESP32 build system | `pip3 install platformio` |
| CMake | Build ota-pusher tool | `apt install cmake` |
| Avahi | mDNS device discovery | `apt install libavahi-client-dev` |
| libzip | ZIP package creation | `apt install libzip-dev` |

## Quick Start

```bash
# Build everything and upload to device
make upload

# Or with specific version
make upload VERSION=1.0.0
```

## Build Commands

### Show Help

```bash
make
# or
make help
```

### Build Firmware

```bash
# Build both MCUs
make firmware

# Build individually
make display      # Display MCU (slave)
make controller   # Controller MCU (master)
```

### Build Desktop Tool

```bash
make tools
# or
make ota-pusher
```

### Build Everything

```bash
make all
```

## OTA Updates

### Create Update Package

```bash
make package
# Creates: dist/update-<version>.zip
```

The package contains:
- `manifest.json` - Version and checksums
- `display.bin` - Display MCU firmware
- `controller.bin` - Controller MCU firmware

### Discover Devices

```bash
make discover
```

Output:
```
Scanning for ESP32 OTA devices...

NAME              IP              TYPE
VONDERWAGENCC1    192.168.1.50    display

Found 1 device(s)
```

### Upload to Device

```bash
# Auto-discover and upload
make upload

# With specific version
make upload VERSION=1.2.0

# With password (production)
make upload PASSWORD=secret

# To specific device/IP
make upload DEVICE=192.168.1.50
```

## USB Flashing

For initial programming or recovery:

```bash
# Flash display MCU
make flash-display

# Flash controller MCU
make flash-controller

# Flash both
make flash-all
```

## Release Workflow

Create a tagged release:

```bash
make release VERSION=1.0.0
```

This will:
1. Check for uncommitted changes
2. Create git tag `v1.0.0`
3. Build both firmwares
4. Create `dist/update-1.0.0.zip`

Then push the tag:
```bash
git push origin v1.0.0
```

## Configuration

### Makefile Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `VERSION` | git describe | Package version |
| `DEVICE` | VONDERWAGENCC1.local | Target device hostname/IP |
| `PASSWORD` | (none) | OTA password for production |

### Examples

```bash
# Custom version
make package VERSION=2.0.0-beta

# Direct IP upload
make upload DEVICE=192.168.1.100

# Production upload with password
make upload VERSION=1.0.0 PASSWORD=mysecret
```

## Development

### Serial Monitor

```bash
# Monitor display MCU
make monitor

# Monitor controller MCU (if USB connected)
make monitor-controller
```

### Build Info

```bash
make info
```

Output:
```
VONDERWAGENCC1 Build Info
=========================
  Version:     v1.2.0-3-g1a2b3c4
  Device:      VONDERWAGENCC1.local
  Display FW:  .pio/build/slave/firmware.bin
  Controller:  .pio/build/master/firmware.bin
  Package Dir: dist
```

## Cleaning

```bash
# Clean everything
make clean

# Clean only firmware
make clean-firmware

# Clean only desktop tools
make clean-tools

# Clean only packages
make clean-packages
```

## Directory Structure

```
esp32-car-control/
├── Makefile                 # Build orchestration
├── BUILD.md                 # This file
├── platformio.ini           # PlatformIO configuration
├── src/
│   ├── master/              # Controller MCU source
│   └── slave/               # Display MCU source
├── include/
│   ├── master/
│   ├── slave/
│   └── shared/
├── tools/
│   └── ota-pusher/          # Desktop OTA tool
│       ├── CMakeLists.txt
│       └── src/
├── dist/                    # Built packages (gitignored)
│   └── update-x.y.z.zip
└── .pio/                    # PlatformIO build (gitignored)
    └── build/
        ├── slave/firmware.bin
        └── master/firmware.bin
```

## OTA Update Process

When you run `make upload`, the following happens:

```
1. Desktop sends update.zip to VONDERWAGENCC1 via WiFi
2. Display MCU saves package to SD card, extracts contents
3. Popup appears on screen: "Update v1.x.0 Ready"
4. User taps INSTALL
5. Display MCU updates itself, reboots
6. Display MCU sends controller firmware via SPI
7. Controller MCU updates itself, reboots
8. Complete - both MCUs running new firmware
```

## Troubleshooting

### Device Not Found

```bash
# Check if device is on network
ping VONDERWAGENCC1.local

# Check mDNS
avahi-browse -art | grep esp32ota

# Use direct IP
make upload DEVICE=192.168.1.x
```

### Upload Fails

- Check WiFi connection on device
- Verify password if production build
- Ensure device isn't mid-update
- Check SD card is inserted

### Build Errors

```bash
# Clean and rebuild
make clean
make all

# Update PlatformIO
pio upgrade
pio pkg update
```

## Security

### Development Mode

- No password required for OTA
- Built with: `pio run -e slave`

### Production Mode

- Password required for OTA
- Built with: `pio run -e slave_prod`
- Password defined in `include/slave/ota_handler.h`

**Important:** Change `OTA_PASSWORD` before deploying to production!

```cpp
// include/slave/ota_handler.h
#define OTA_PASSWORD "change-this-password"
```
