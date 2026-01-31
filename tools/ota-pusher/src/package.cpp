#include "package.h"

#include <openssl/md5.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <iostream>
#include <cstring>

// =============================================================================
// Utility Functions
// =============================================================================

std::string calculateMd5(const std::vector<uint8_t>& data) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(data.data(), data.size(), digest);
    
    std::ostringstream oss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(digest[i]);
    }
    return oss.str();
}

std::string calculateMd5File(const std::string& filePath) {
    std::vector<uint8_t> data = readFile(filePath);
    if (data.empty()) return "";
    return calculateMd5(data);
}

std::vector<uint8_t> readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return {};
    }
    
    auto size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "Failed to read file: " << path << std::endl;
        return {};
    }
    
    return data;
}

std::string getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::tm tm;
    gmtime_r(&time, &tm);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

// =============================================================================
// Package Creation
// =============================================================================

static void appendU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

static uint32_t readU32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

std::vector<uint8_t> packageCreate(
    const std::string& version,
    const std::string& displayFirmwarePath,
    const std::string& controllerFirmwarePath
) {
    // Read firmware files
    std::vector<uint8_t> displayFw = readFile(displayFirmwarePath);
    std::vector<uint8_t> controllerFw = readFile(controllerFirmwarePath);
    
    if (displayFw.empty() || controllerFw.empty()) {
        std::cerr << "Failed to read firmware files" << std::endl;
        return {};
    }
    
    // Calculate MD5 hashes
    std::string displayMd5 = calculateMd5(displayFw);
    std::string controllerMd5 = calculateMd5(controllerFw);
    
    // Create manifest JSON
    std::ostringstream manifest;
    manifest << "{\n";
    manifest << "  \"version\": \"" << version << "\",\n";
    manifest << "  \"created\": \"" << getTimestamp() << "\",\n";
    manifest << "  \"display\": {\n";
    manifest << "    \"size\": " << displayFw.size() << ",\n";
    manifest << "    \"md5\": \"" << displayMd5 << "\"\n";
    manifest << "  },\n";
    manifest << "  \"controller\": {\n";
    manifest << "    \"size\": " << controllerFw.size() << ",\n";
    manifest << "    \"md5\": \"" << controllerMd5 << "\"\n";
    manifest << "  }\n";
    manifest << "}\n";
    
    std::string manifestStr = manifest.str();
    
    // Build package: [manifest_size][manifest][display_size][display][controller_size][controller]
    std::vector<uint8_t> package;
    
    // Reserve space (approximate)
    package.reserve(12 + manifestStr.size() + displayFw.size() + controllerFw.size());
    
    // Manifest
    appendU32(package, static_cast<uint32_t>(manifestStr.size()));
    package.insert(package.end(), manifestStr.begin(), manifestStr.end());
    
    // Display firmware
    appendU32(package, static_cast<uint32_t>(displayFw.size()));
    package.insert(package.end(), displayFw.begin(), displayFw.end());
    
    // Controller firmware
    appendU32(package, static_cast<uint32_t>(controllerFw.size()));
    package.insert(package.end(), controllerFw.begin(), controllerFw.end());
    
    std::cout << "Created package: " << package.size() << " bytes" << std::endl;
    std::cout << "  Version: " << version << std::endl;
    std::cout << "  Display FW: " << displayFw.size() << " bytes (MD5: " << displayMd5 << ")" << std::endl;
    std::cout << "  Controller FW: " << controllerFw.size() << " bytes (MD5: " << controllerMd5 << ")" << std::endl;
    
    return package;
}

bool packageCreateFile(
    const std::string& outputPath,
    const std::string& version,
    const std::string& displayFirmwarePath,
    const std::string& controllerFirmwarePath
) {
    std::vector<uint8_t> package = packageCreate(version, displayFirmwarePath, controllerFirmwarePath);
    if (package.empty()) {
        return false;
    }
    
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to create output file: " << outputPath << std::endl;
        return false;
    }
    
    file.write(reinterpret_cast<const char*>(package.data()), 
               static_cast<std::streamsize>(package.size()));
    
    return file.good();
}

// =============================================================================
// Package Validation
// =============================================================================

bool packageValidate(const std::vector<uint8_t>& packageData, PackageInfo& outInfo) {
    outInfo.valid = false;
    
    if (packageData.size() < 12) {
        std::cerr << "Package too small" << std::endl;
        return false;
    }
    
    size_t offset = 0;
    
    // Read manifest
    uint32_t manifestSize = readU32(packageData.data() + offset);
    offset += 4;
    
    if (offset + manifestSize > packageData.size()) {
        std::cerr << "Invalid manifest size" << std::endl;
        return false;
    }
    
    std::string manifest(reinterpret_cast<const char*>(packageData.data() + offset), manifestSize);
    offset += manifestSize;
    
    // Read display firmware size
    if (offset + 4 > packageData.size()) {
        std::cerr << "Missing display firmware size" << std::endl;
        return false;
    }
    
    uint32_t displaySize = readU32(packageData.data() + offset);
    offset += 4;
    
    if (offset + displaySize > packageData.size()) {
        std::cerr << "Invalid display firmware size" << std::endl;
        return false;
    }
    
    // Calculate display MD5
    std::vector<uint8_t> displayFw(packageData.begin() + offset, 
                                    packageData.begin() + offset + displaySize);
    std::string displayMd5 = calculateMd5(displayFw);
    offset += displaySize;
    
    // Read controller firmware size
    if (offset + 4 > packageData.size()) {
        std::cerr << "Missing controller firmware size" << std::endl;
        return false;
    }
    
    uint32_t controllerSize = readU32(packageData.data() + offset);
    offset += 4;
    
    if (offset + controllerSize > packageData.size()) {
        std::cerr << "Invalid controller firmware size" << std::endl;
        return false;
    }
    
    // Calculate controller MD5
    std::vector<uint8_t> controllerFw(packageData.begin() + offset, 
                                       packageData.begin() + offset + controllerSize);
    std::string controllerMd5 = calculateMd5(controllerFw);
    
    // Parse manifest (simple parsing without full JSON library)
    // Look for version field
    auto findValue = [&manifest](const std::string& key) -> std::string {
        std::string searchKey = "\"" + key + "\": \"";
        size_t pos = manifest.find(searchKey);
        if (pos == std::string::npos) {
            searchKey = "\"" + key + "\":\"";
            pos = manifest.find(searchKey);
        }
        if (pos == std::string::npos) return "";
        
        pos += searchKey.length();
        size_t end = manifest.find("\"", pos);
        if (end == std::string::npos) return "";
        
        return manifest.substr(pos, end - pos);
    };
    
    outInfo.version = findValue("version");
    outInfo.created = findValue("created");
    outInfo.displaySize = displaySize;
    outInfo.controllerSize = controllerSize;
    outInfo.displayMd5 = displayMd5;
    outInfo.controllerMd5 = controllerMd5;
    outInfo.valid = true;
    
    return true;
}

bool packageValidateFile(const std::string& packagePath, PackageInfo& outInfo) {
    std::vector<uint8_t> data = readFile(packagePath);
    if (data.empty()) {
        return false;
    }
    return packageValidate(data, outInfo);
}
