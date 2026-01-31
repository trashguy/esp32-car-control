#include "mdns_discovery.h"

#include <avahi-client/client.h>
#include <avahi-client/lookup.h>
#include <avahi-common/simple-watch.h>
#include <avahi-common/malloc.h>
#include <avahi-common/error.h>

#include <iostream>
#include <cstring>
#include <map>
#include <mutex>

// =============================================================================
// Static State
// =============================================================================

static AvahiSimplePoll* simplePoll = nullptr;
static AvahiClient* client = nullptr;
static bool initialized = false;

static std::vector<DiscoveredDevice> discoveredDevices;
static std::mutex devicesMutex;
static std::function<void(const DiscoveredDevice&)> discoveryCallback;

// =============================================================================
// Avahi Callbacks
// =============================================================================

static void resolveCallback(
    AvahiServiceResolver* r,
    [[maybe_unused]] AvahiIfIndex interface,
    [[maybe_unused]] AvahiProtocol protocol,
    AvahiResolverEvent event,
    const char* name,
    [[maybe_unused]] const char* type,
    [[maybe_unused]] const char* domain,
    [[maybe_unused]] const char* host_name,
    const AvahiAddress* address,
    uint16_t port,
    AvahiStringList* txt,
    [[maybe_unused]] AvahiLookupResultFlags flags,
    [[maybe_unused]] void* userdata
) {
    if (event == AVAHI_RESOLVER_FOUND && address->proto == AVAHI_PROTO_INET) {
        char addrStr[AVAHI_ADDRESS_STR_MAX];
        avahi_address_snprint(addrStr, sizeof(addrStr), address);

        DiscoveredDevice device;
        device.hostname = name ? name : "";
        device.address = addrStr;
        device.port = port;

        // Extract version from TXT record if present
        while (txt) {
            char* key = nullptr;
            char* value = nullptr;
            size_t valueLen = 0;
            if (avahi_string_list_get_pair(txt, &key, &value, &valueLen) == 0) {
                if (key && strcmp(key, "version") == 0 && value) {
                    device.txtVersion = value;
                }
                avahi_free(key);
                avahi_free(value);
            }
            txt = avahi_string_list_get_next(txt);
        }

        {
            std::lock_guard<std::mutex> lock(devicesMutex);
            discoveredDevices.push_back(device);
        }

        if (discoveryCallback) {
            discoveryCallback(device);
        }
    }

    avahi_service_resolver_free(r);
}

static void browseCallback(
    AvahiServiceBrowser* b,
    AvahiIfIndex interface,
    AvahiProtocol protocol,
    AvahiBrowserEvent event,
    const char* name,
    const char* type,
    const char* domain,
    [[maybe_unused]] AvahiLookupResultFlags flags,
    [[maybe_unused]] void* userdata
) {
    if (event == AVAHI_BROWSER_NEW) {
        avahi_service_resolver_new(
            client,
            interface,
            protocol,
            name,
            type,
            domain,
            AVAHI_PROTO_INET,
            static_cast<AvahiLookupFlags>(0),
            resolveCallback,
            nullptr
        );
    } else if (event == AVAHI_BROWSER_ALL_FOR_NOW || event == AVAHI_BROWSER_FAILURE) {
        // Could stop polling here for single-shot discovery
    }
    (void)b;
}

static void clientCallback(
    AvahiClient* c,
    AvahiClientState state,
    [[maybe_unused]] void* userdata
) {
    if (state == AVAHI_CLIENT_FAILURE) {
        std::cerr << "Avahi client failure: " << avahi_strerror(avahi_client_errno(c)) << std::endl;
        avahi_simple_poll_quit(simplePoll);
    }
}

// =============================================================================
// Public Interface
// =============================================================================

bool mdnsInit() {
    if (initialized) return true;

    simplePoll = avahi_simple_poll_new();
    if (!simplePoll) {
        std::cerr << "Failed to create Avahi simple poll" << std::endl;
        return false;
    }

    int error = 0;
    client = avahi_client_new(
        avahi_simple_poll_get(simplePoll),
        static_cast<AvahiClientFlags>(0),
        clientCallback,
        nullptr,
        &error
    );

    if (!client) {
        std::cerr << "Failed to create Avahi client: " << avahi_strerror(error) << std::endl;
        avahi_simple_poll_free(simplePoll);
        simplePoll = nullptr;
        return false;
    }

    initialized = true;
    return true;
}

void mdnsCleanup() {
    if (client) {
        avahi_client_free(client);
        client = nullptr;
    }
    if (simplePoll) {
        avahi_simple_poll_free(simplePoll);
        simplePoll = nullptr;
    }
    initialized = false;
}

int mdnsDiscover(
    const std::string& serviceType,
    std::chrono::milliseconds timeout,
    std::function<void(const DiscoveredDevice&)> callback
) {
    if (!initialized && !mdnsInit()) {
        return 0;
    }

    {
        std::lock_guard<std::mutex> lock(devicesMutex);
        discoveredDevices.clear();
    }
    discoveryCallback = callback;

    // Create browser for the service type (e.g., "_esp32ota._tcp")
    std::string fullType = serviceType + "._tcp";
    AvahiServiceBrowser* browser = avahi_service_browser_new(
        client,
        AVAHI_IF_UNSPEC,
        AVAHI_PROTO_INET,
        fullType.c_str(),
        nullptr,
        static_cast<AvahiLookupFlags>(0),
        browseCallback,
        nullptr
    );

    if (!browser) {
        std::cerr << "Failed to create service browser: " 
                  << avahi_strerror(avahi_client_errno(client)) << std::endl;
        return 0;
    }

    // Run the event loop for the specified timeout
    auto startTime = std::chrono::steady_clock::now();
    while (true) {
        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed >= timeout) break;

        // Poll with short timeout to allow checking elapsed time
        int ret = avahi_simple_poll_iterate(simplePoll, 100);
        if (ret != 0) break;
    }

    avahi_service_browser_free(browser);
    discoveryCallback = nullptr;

    std::lock_guard<std::mutex> lock(devicesMutex);
    return static_cast<int>(discoveredDevices.size());
}

std::vector<DiscoveredDevice> mdnsDiscoverAll(
    const std::string& serviceType,
    std::chrono::milliseconds timeout
) {
    mdnsDiscover(serviceType, timeout, nullptr);
    
    std::lock_guard<std::mutex> lock(devicesMutex);
    return discoveredDevices;
}

bool mdnsFindDevice(
    const std::string& hostname,
    const std::string& serviceType,
    std::chrono::milliseconds timeout,
    DiscoveredDevice& outDevice
) {
    bool found = false;
    
    mdnsDiscover(serviceType, timeout, [&](const DiscoveredDevice& device) {
        if (device.hostname == hostname) {
            outDevice = device;
            found = true;
        }
    });
    
    return found;
}
