# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32 Arduino project intended for AI/ML experimentation. Uses PlatformIO as the build system with the Arduino framework.

## Build Commands

```bash
# Build the project
pio run

# Build for specific environment (if multiple boards configured)
pio run -e esp32dev

# Upload to connected ESP32
pio run -t upload

# Monitor serial output (115200 baud)
pio device monitor

# Build and upload and monitor in one command
pio run -t upload -t monitor

# Clean build artifacts
pio run -t clean
```

## Project Structure

```
src/          - Main source files (.cpp)
include/      - Header files (.h)
lib/          - Project-specific libraries
platformio.ini - Build configuration and dependencies
```

## Adding Libraries

Add dependencies to `platformio.ini` under the environment:
```ini
lib_deps =
    library_name
    username/library_name@^1.0.0
```

## ESP32 Considerations

- Default board: `esp32dev` (generic ESP32)
- ESP32-S3 environment available (uncomment in platformio.ini) - recommended for AI workloads due to vector instructions and PSRAM support
- Serial monitor runs at 115200 baud
- Use `ESP.getFreeHeap()` to monitor memory during development
