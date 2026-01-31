#ifndef MDNS_DISCOVERY_H
#define MDNS_DISCOVERY_H

#include <string>
#include <vector>
#include <functional>
#include <chrono>

// =============================================================================
// Device Information
// =============================================================================

struct DiscoveredDevice {
    std::string hostname;       // e.g., "VONDERWAGENCC1"
    std::string address;        // IPv4 address
    uint16_t port;              // OTA port (3233)
    std::string txtVersion;     // Version from TXT record (if available)
};

// =============================================================================
// mDNS Discovery Functions
// =============================================================================

// Initialize Avahi client
bool mdnsInit();

// Cleanup Avahi resources
void mdnsCleanup();

// Discover devices with given service type (e.g., "_esp32ota._tcp")
// Blocks for specified timeout, calls callback for each device found
// Returns number of devices found
int mdnsDiscover(
    const std::string& serviceType,
    std::chrono::milliseconds timeout,
    std::function<void(const DiscoveredDevice&)> callback
);

// Discover and return all devices as a vector
std::vector<DiscoveredDevice> mdnsDiscoverAll(
    const std::string& serviceType,
    std::chrono::milliseconds timeout
);

// Find a specific device by hostname
// Returns empty optional if not found within timeout
bool mdnsFindDevice(
    const std::string& hostname,
    const std::string& serviceType,
    std::chrono::milliseconds timeout,
    DiscoveredDevice& outDevice
);

#endif // MDNS_DISCOVERY_H
