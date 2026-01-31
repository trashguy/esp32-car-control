#include "mdns_discovery.h"
#include "ota_protocol.h"
#include "package.h"

#include <iostream>
#include <string>
#include <vector>
#include <getopt.h>
#include <cstdlib>

// =============================================================================
// Constants
// =============================================================================

constexpr const char* SERVICE_TYPE = "_esp32ota";
constexpr const char* DEFAULT_HOSTNAME = "VONDERWAGENCC1";
constexpr int DEFAULT_DISCOVER_TIMEOUT_MS = 3000;

// =============================================================================
// Usage
// =============================================================================

static void printUsage(const char* progName) {
    std::cout << "OTA Pusher - ESP32 OTA Update Tool for VONDERWAGENCC1\n\n";
    std::cout << "Usage:\n";
    std::cout << "  " << progName << " discover [--timeout <ms>]\n";
    std::cout << "      Discover devices on the network via mDNS\n\n";
    std::cout << "  " << progName << " package <output> <display.bin> <controller.bin> [--version <ver>]\n";
    std::cout << "      Create an OTA update package\n\n";
    std::cout << "  " << progName << " upload <package> [--host <hostname|ip>] [--port <port>]\n";
    std::cout << "      Upload a package to a device\n\n";
    std::cout << "  " << progName << " validate <package>\n";
    std::cout << "      Validate a package file\n\n";
    std::cout << "Options:\n";
    std::cout << "  --timeout <ms>     Discovery timeout in milliseconds (default: 3000)\n";
    std::cout << "  --version <ver>    Version string for package (default: git describe)\n";
    std::cout << "  --host <host>      Target hostname or IP (default: " << DEFAULT_HOSTNAME << ")\n";
    std::cout << "  --port <port>      Target port (default: " << OTA_PORT_PACKAGE << ")\n";
    std::cout << "  --help             Show this help\n";
}

// =============================================================================
// Get Version from Git
// =============================================================================

static std::string getGitVersion() {
    // Try to get version from git describe
    FILE* pipe = popen("git describe --tags --always --dirty 2>/dev/null", "r");
    if (!pipe) {
        return "unknown";
    }
    
    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    
    // Remove trailing newline
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
        result.pop_back();
    }
    
    return result.empty() ? "unknown" : result;
}

// =============================================================================
// Commands
// =============================================================================

static int cmdDiscover(int timeoutMs) {
    std::cout << "Discovering devices (timeout: " << timeoutMs << "ms)...\n";
    
    auto devices = mdnsDiscoverAll(SERVICE_TYPE, std::chrono::milliseconds(timeoutMs));
    
    if (devices.empty()) {
        std::cout << "No devices found.\n";
        return 1;
    }
    
    std::cout << "\nFound " << devices.size() << " device(s):\n";
    std::cout << std::string(60, '-') << "\n";
    
    for (const auto& device : devices) {
        std::cout << "  Hostname: " << device.hostname << "\n";
        std::cout << "  Address:  " << device.address << ":" << device.port << "\n";
        if (!device.txtVersion.empty()) {
            std::cout << "  Version:  " << device.txtVersion << "\n";
        }
        std::cout << std::string(60, '-') << "\n";
    }
    
    return 0;
}

static int cmdPackage(const std::string& output, 
                      const std::string& displayFw, 
                      const std::string& controllerFw,
                      const std::string& version) {
    std::string ver = version.empty() ? getGitVersion() : version;
    
    std::cout << "Creating OTA package...\n";
    std::cout << "  Output: " << output << "\n";
    std::cout << "  Display FW: " << displayFw << "\n";
    std::cout << "  Controller FW: " << controllerFw << "\n";
    std::cout << "  Version: " << ver << "\n\n";
    
    if (packageCreateFile(output, ver, displayFw, controllerFw)) {
        std::cout << "\nPackage created successfully: " << output << "\n";
        return 0;
    } else {
        std::cerr << "Failed to create package\n";
        return 1;
    }
}

static int cmdUpload(const std::string& packagePath, 
                     const std::string& host, 
                     uint16_t port) {
    // Validate package first
    PackageInfo info;
    if (!packageValidateFile(packagePath, info)) {
        std::cerr << "Invalid package file: " << packagePath << "\n";
        return 1;
    }
    
    std::cout << "Package info:\n";
    std::cout << "  Version: " << info.version << "\n";
    std::cout << "  Display FW: " << info.displaySize << " bytes\n";
    std::cout << "  Controller FW: " << info.controllerSize << " bytes\n\n";
    
    std::string targetHost = host;
    uint16_t targetPort = port;
    
    // If host looks like a hostname (not IP), try to resolve via mDNS
    if (!host.empty() && host.find('.') == std::string::npos) {
        std::cout << "Resolving hostname '" << host << "' via mDNS...\n";
        
        DiscoveredDevice device;
        if (mdnsFindDevice(host, SERVICE_TYPE, std::chrono::milliseconds(3000), device)) {
            targetHost = device.address;
            if (port == OTA_PORT_PACKAGE) {
                targetPort = device.port;
            }
            std::cout << "  Resolved to: " << targetHost << ":" << targetPort << "\n\n";
        } else {
            std::cerr << "Failed to resolve hostname: " << host << "\n";
            return 1;
        }
    }
    
    std::cout << "Uploading to " << targetHost << ":" << targetPort << "...\n";
    
    OtaResult result = otaSendPackageFile(
        targetHost, 
        targetPort, 
        packagePath,
        [](size_t sent, size_t total) {
            int percent = static_cast<int>((sent * 100) / total);
            std::cout << "\r  Progress: " << percent << "% (" 
                      << sent << "/" << total << " bytes)" << std::flush;
        }
    );
    
    std::cout << "\n\n";
    
    if (result == OtaResult::Success) {
        std::cout << "Upload successful!\n";
        std::cout << "The device will install the update and reboot.\n";
        return 0;
    } else {
        std::cerr << "Upload failed: " << otaResultToString(result) << "\n";
        return 1;
    }
}

static int cmdValidate(const std::string& packagePath) {
    std::cout << "Validating package: " << packagePath << "\n\n";
    
    PackageInfo info;
    if (!packageValidateFile(packagePath, info)) {
        std::cerr << "Invalid package!\n";
        return 1;
    }
    
    std::cout << "Package is valid.\n\n";
    std::cout << "Package info:\n";
    std::cout << "  Version:        " << info.version << "\n";
    std::cout << "  Created:        " << info.created << "\n";
    std::cout << "  Display FW:     " << info.displaySize << " bytes\n";
    std::cout << "  Display MD5:    " << info.displayMd5 << "\n";
    std::cout << "  Controller FW:  " << info.controllerSize << " bytes\n";
    std::cout << "  Controller MD5: " << info.controllerMd5 << "\n";
    
    return 0;
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "--help" || command == "-h") {
        printUsage(argv[0]);
        return 0;
    }
    
    // Parse options
    int timeoutMs = DEFAULT_DISCOVER_TIMEOUT_MS;
    std::string version;
    std::string host = DEFAULT_HOSTNAME;
    uint16_t port = OTA_PORT_PACKAGE;
    
    static struct option longOptions[] = {
        {"timeout", required_argument, nullptr, 't'},
        {"version", required_argument, nullptr, 'v'},
        {"host", required_argument, nullptr, 'H'},
        {"port", required_argument, nullptr, 'p'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };
    
    // Skip command name for getopt
    optind = 2;
    
    int opt;
    while ((opt = getopt_long(argc, argv, "t:v:H:p:h", longOptions, nullptr)) != -1) {
        switch (opt) {
            case 't':
                timeoutMs = std::atoi(optarg);
                break;
            case 'v':
                version = optarg;
                break;
            case 'H':
                host = optarg;
                break;
            case 'p':
                port = static_cast<uint16_t>(std::atoi(optarg));
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                return 1;
        }
    }
    
    // Initialize mDNS
    if (command == "discover" || command == "upload") {
        if (!mdnsInit()) {
            std::cerr << "Failed to initialize mDNS\n";
            return 1;
        }
    }
    
    int result = 0;
    
    if (command == "discover") {
        result = cmdDiscover(timeoutMs);
    } 
    else if (command == "package") {
        // Collect remaining arguments
        std::vector<std::string> args;
        for (int i = optind; i < argc; i++) {
            args.push_back(argv[i]);
        }
        
        if (args.size() < 3) {
            std::cerr << "Error: package command requires <output> <display.bin> <controller.bin>\n";
            result = 1;
        } else {
            result = cmdPackage(args[0], args[1], args[2], version);
        }
    }
    else if (command == "upload") {
        std::vector<std::string> args;
        for (int i = optind; i < argc; i++) {
            args.push_back(argv[i]);
        }
        
        if (args.empty()) {
            std::cerr << "Error: upload command requires <package>\n";
            result = 1;
        } else {
            result = cmdUpload(args[0], host, port);
        }
    }
    else if (command == "validate") {
        std::vector<std::string> args;
        for (int i = optind; i < argc; i++) {
            args.push_back(argv[i]);
        }
        
        if (args.empty()) {
            std::cerr << "Error: validate command requires <package>\n";
            result = 1;
        } else {
            result = cmdValidate(args[0]);
        }
    }
    else {
        std::cerr << "Unknown command: " << command << "\n\n";
        printUsage(argv[0]);
        result = 1;
    }
    
    // Cleanup
    mdnsCleanup();
    
    return result;
}
