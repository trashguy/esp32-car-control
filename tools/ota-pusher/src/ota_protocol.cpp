#include "ota_protocol.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <fstream>

// =============================================================================
// Helper Functions
// =============================================================================

std::string otaResultToString(OtaResult result) {
    switch (result) {
        case OtaResult::Success: return "Success";
        case OtaResult::ConnectionFailed: return "Connection failed";
        case OtaResult::ConnectionTimeout: return "Connection timeout";
        case OtaResult::TransferFailed: return "Transfer failed";
        case OtaResult::Rejected: return "Update rejected by device";
        case OtaResult::InvalidResponse: return "Invalid response from device";
        default: return "Unknown error";
    }
}

static bool setNonBlocking(int fd, bool nonBlocking) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return false;
    
    if (nonBlocking) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    
    return fcntl(fd, F_SETFL, flags) >= 0;
}

static bool waitForSocket(int fd, bool forWrite, int timeoutMs) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = forWrite ? POLLOUT : POLLIN;
    pfd.revents = 0;
    
    int ret = poll(&pfd, 1, timeoutMs);
    return ret > 0 && (pfd.revents & (forWrite ? POLLOUT : POLLIN));
}

// =============================================================================
// OTA Protocol Implementation
// =============================================================================

OtaResult otaSendPackage(
    const std::string& host,
    uint16_t port,
    const std::vector<uint8_t>& packageData,
    OtaProgressCallback progressCallback,
    int timeoutSeconds
) {
    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
        return OtaResult::ConnectionFailed;
    }

    // Set non-blocking for connect with timeout
    setNonBlocking(sock, true);

    // Connect to device
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "Invalid address: " << host << std::endl;
        close(sock);
        return OtaResult::ConnectionFailed;
    }

    int ret = connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
    if (ret < 0 && errno != EINPROGRESS) {
        std::cerr << "Connect failed: " << strerror(errno) << std::endl;
        close(sock);
        return OtaResult::ConnectionFailed;
    }

    // Wait for connection with timeout
    if (!waitForSocket(sock, true, 5000)) {
        std::cerr << "Connection timeout" << std::endl;
        close(sock);
        return OtaResult::ConnectionTimeout;
    }

    // Check if connection succeeded
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
        std::cerr << "Connection failed: " << strerror(error) << std::endl;
        close(sock);
        return OtaResult::ConnectionFailed;
    }

    // Set back to blocking for data transfer
    setNonBlocking(sock, false);

    // Set socket timeout
    struct timeval tv;
    tv.tv_sec = timeoutSeconds;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Send header
    OtaPacketHeader header;
    header.magic = OTA_MAGIC;
    header.version = OTA_PROTOCOL_VERSION;
    header.packageSize = static_cast<uint32_t>(packageData.size());
    header.reserved = 0;

    ssize_t sent = send(sock, &header, sizeof(header), 0);
    if (sent != sizeof(header)) {
        std::cerr << "Failed to send header: " << strerror(errno) << std::endl;
        close(sock);
        return OtaResult::TransferFailed;
    }

    // Send package data in chunks
    const size_t chunkSize = 4096;
    size_t totalSent = 0;

    while (totalSent < packageData.size()) {
        size_t remaining = packageData.size() - totalSent;
        size_t toSend = std::min(remaining, chunkSize);
        
        sent = send(sock, packageData.data() + totalSent, toSend, 0);
        if (sent <= 0) {
            std::cerr << "Failed to send data: " << strerror(errno) << std::endl;
            close(sock);
            return OtaResult::TransferFailed;
        }
        
        totalSent += static_cast<size_t>(sent);
        
        if (progressCallback) {
            progressCallback(totalSent, packageData.size());
        }
    }

    // Wait for acknowledgment (simple 1-byte response)
    uint8_t response = 0;
    ssize_t received = recv(sock, &response, 1, 0);
    
    close(sock);

    if (received != 1) {
        std::cerr << "No response from device" << std::endl;
        return OtaResult::InvalidResponse;
    }

    if (response == 0x00) {
        return OtaResult::Success;
    } else if (response == 0xFF) {
        return OtaResult::Rejected;
    } else {
        return OtaResult::InvalidResponse;
    }
}

OtaResult otaSendPackageFile(
    const std::string& host,
    uint16_t port,
    const std::string& packagePath,
    OtaProgressCallback progressCallback,
    int timeoutSeconds
) {
    // Read file into memory
    std::ifstream file(packagePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Failed to open package file: " << packagePath << std::endl;
        return OtaResult::TransferFailed;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        std::cerr << "Failed to read package file" << std::endl;
        return OtaResult::TransferFailed;
    }

    return otaSendPackage(host, port, data, progressCallback, timeoutSeconds);
}
