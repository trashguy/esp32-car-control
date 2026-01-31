#ifndef SHARED_VERSION_H
#define SHARED_VERSION_H

// Firmware version - set by build system via -DFIRMWARE_VERSION="..."
// Falls back to "unknown" if not defined
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "unknown"
#endif

// Build timestamp - set by build system via -DBUILD_TIMESTAMP="..."
#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP "unknown"
#endif

#endif // SHARED_VERSION_H
