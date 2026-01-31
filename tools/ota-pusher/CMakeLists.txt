cmake_minimum_required(VERSION 3.16)
project(ota-pusher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVAHI REQUIRED avahi-client)
pkg_check_modules(LIBZIP REQUIRED libzip)

# OpenSSL for MD5 hashing
find_package(OpenSSL REQUIRED)

# Executable
add_executable(ota-pusher
    src/main.cpp
    src/mdns_discovery.cpp
    src/ota_protocol.cpp
    src/package.cpp
)

target_include_directories(ota-pusher PRIVATE
    src
    ${AVAHI_INCLUDE_DIRS}
    ${LIBZIP_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(ota-pusher PRIVATE
    ${AVAHI_LIBRARIES}
    ${LIBZIP_LIBRARIES}
    OpenSSL::Crypto
)

target_compile_options(ota-pusher PRIVATE
    -Wall -Wextra -Wpedantic
)

# Install target
install(TARGETS ota-pusher DESTINATION bin)
