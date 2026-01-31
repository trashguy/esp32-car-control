#ifndef USB_MSC_H
#define USB_MSC_H

#include <stdbool.h>

// =============================================================================
// USB Mass Storage API
// =============================================================================
//
// Build configurations:
//   PRODUCTION_BUILD=1 (slave_prod): Full MSC implementation
//   PRODUCTION_BUILD=0 (slave):      Stub functions (always return false/no-op)
//
// In development builds, all functions compile to no-ops to save code space
// and avoid issues with ARDUINO_USB_MODE=1 (hardware USB-JTAG).
// =============================================================================

// Initialize USB subsystem (CDC + MSC composite device)
// MUST be called BEFORE any Serial usage when ARDUINO_USB_CDC_ON_BOOT=0
// Call this early in setup(), before Serial.begin()
void usbInit();

// Initialize USB Mass Storage for SD card access
// Must be called AFTER sdCardInit() succeeds
// Note: This only prepares MSC, call usbMscEnable() to activate
// Returns true if USB MSC initialized successfully
bool usbMscInit();

// Enable USB Mass Storage (makes SD card visible to PC)
// Returns true if enabled successfully
bool usbMscEnable();

// Disable USB Mass Storage (hides SD card from PC)
// This ejects the drive first if mounted
void usbMscDisable();

// Check if USB MSC is enabled
bool usbMscIsEnabled();

// Check if USB MSC is currently active/mounted by host
bool usbMscMounted();

// Check if USB host is currently reading/writing
// Use this to avoid SD card conflicts during file operations
bool usbMscBusy();

// Safely eject - waits for pending operations to complete
void usbMscEject();

// Check if the host ejected the drive (clears flag after reading)
// Call this periodically to detect host-initiated eject and auto-disable MSC
bool usbMscCheckEjected();

#endif // USB_MSC_H
