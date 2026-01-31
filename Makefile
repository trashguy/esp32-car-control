# =============================================================================
# VONDERWAGENCC1 Build System
# =============================================================================
#
# Usage:
#   make              - Show help
#   make all          - Build everything
#   make package      - Create OTA update package
#   make upload       - Upload package to device
#   make release      - Full release workflow (tag, build, package)
#
# See BUILD.md for full documentation.
# =============================================================================

# === Configuration ===
VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || echo "dev")
BUILD_DIR := .pio/build
OTA_DIR := tools/ota-pusher
OTA_BUILD := $(OTA_DIR)/build
PACKAGE_DIR := dist
DEVICE ?= VONDERWAGENCC1.local
PASSWORD ?=

# Firmware paths
DISPLAY_FW := $(BUILD_DIR)/slave/firmware.bin
CONTROLLER_FW := $(BUILD_DIR)/master/firmware.bin
OTA_PUSHER := $(OTA_BUILD)/ota-pusher

# === Colors ===
CYAN := \033[36m
GREEN := \033[32m
YELLOW := \033[33m
RED := \033[31m
RESET := \033[0m

# =============================================================================
# Primary Targets
# =============================================================================

.PHONY: all
all: firmware tools  ## Build everything (firmware + tools)
	@echo "$(GREEN)Build complete!$(RESET)"

.PHONY: firmware
firmware: display controller  ## Build both MCU firmwares

.PHONY: tools
tools: ota-pusher  ## Build desktop tools

.PHONY: package
package: firmware $(OTA_PUSHER)  ## Create OTA update package
	@mkdir -p $(PACKAGE_DIR)
	@echo "$(CYAN)Creating update package v$(VERSION)...$(RESET)"
	$(OTA_PUSHER) package \
	    $(PACKAGE_DIR)/update-$(VERSION).pkg \
	    $(DISPLAY_FW) \
	    $(CONTROLLER_FW) \
	    --version $(VERSION)
	@echo "$(GREEN)Package created: $(PACKAGE_DIR)/update-$(VERSION).pkg$(RESET)"

.PHONY: upload
upload: package  ## Upload OTA package to device
	@echo "$(CYAN)Uploading to $(DEVICE)...$(RESET)"
	$(OTA_PUSHER) upload $(PACKAGE_DIR)/update-$(VERSION).pkg --host $(DEVICE)

.PHONY: discover
discover: $(OTA_PUSHER)  ## Discover devices on network
	$(OTA_PUSHER) discover

# =============================================================================
# Firmware Targets
# =============================================================================

.PHONY: display
display:  ## Build display (slave) firmware (LVGL UI)
	@echo "$(CYAN)Building display firmware (LVGL)...$(RESET)"
	pio run -e slave

.PHONY: display-legacy
display-legacy:  ## Build display firmware with legacy TFT UI
	@echo "$(CYAN)Building display firmware (legacy TFT)...$(RESET)"
	pio run -e slave_legacy

.PHONY: controller
controller:  ## Build controller (master) firmware
	@echo "$(CYAN)Building controller firmware...$(RESET)"
	pio run -e master

# =============================================================================
# Tools Targets
# =============================================================================

.PHONY: ota-pusher
ota-pusher: $(OTA_PUSHER)

$(OTA_PUSHER): $(OTA_DIR)/CMakeLists.txt $(wildcard $(OTA_DIR)/src/*.cpp) $(wildcard $(OTA_DIR)/src/*.h)
	@echo "$(CYAN)Building ota-pusher...$(RESET)"
	@mkdir -p $(OTA_BUILD)
	cd $(OTA_BUILD) && cmake .. && make
	@echo "$(GREEN)ota-pusher built: $(OTA_PUSHER)$(RESET)"

# =============================================================================
# USB Flash Targets
# =============================================================================

.PHONY: flash-display
flash-display:  ## Flash display firmware via USB (LVGL UI)
	pio run -e slave -t upload

.PHONY: flash-display-legacy
flash-display-legacy:  ## Flash display firmware via USB (legacy TFT UI)
	pio run -e slave_legacy -t upload

.PHONY: flash-controller
flash-controller:  ## Flash controller firmware via USB
	pio run -e master -t upload

.PHONY: flash-all
flash-all: flash-display flash-controller  ## Flash both via USB

# =============================================================================
# Development Targets
# =============================================================================

.PHONY: monitor
monitor:  ## Monitor serial output (display)
	pio device monitor

.PHONY: monitor-controller
monitor-controller:  ## Monitor controller serial (if connected)
	pio device monitor -e master

# =============================================================================
# Release Workflow
# =============================================================================

.PHONY: release
release:  ## Full release: validate, tag, build, package
	@if [ "$(VERSION)" = "dev" ] || echo "$(VERSION)" | grep -q dirty; then \
	    echo "$(RED)Error: Cannot release with dirty/dev version$(RESET)"; \
	    echo "Usage: make release VERSION=x.y.z"; \
	    exit 1; \
	fi
	@echo "$(YELLOW)Creating release v$(VERSION)...$(RESET)"
	@echo ""
	@echo "$(CYAN)Step 1/4: Checking for uncommitted changes...$(RESET)"
	@if ! git diff-index --quiet HEAD --; then \
	    echo "$(RED)Error: Uncommitted changes detected. Commit first.$(RESET)"; \
	    exit 1; \
	fi
	@echo "$(CYAN)Step 2/4: Creating git tag...$(RESET)"
	git tag -a v$(VERSION) -m "Release v$(VERSION)"
	@echo "$(CYAN)Step 3/4: Building firmware...$(RESET)"
	$(MAKE) firmware
	@echo "$(CYAN)Step 4/4: Creating package...$(RESET)"
	$(MAKE) package VERSION=$(VERSION)
	@echo ""
	@echo "$(GREEN)========================================$(RESET)"
	@echo "$(GREEN)Release v$(VERSION) complete!$(RESET)"
	@echo "$(GREEN)========================================$(RESET)"
	@echo ""
	@echo "Package: $(PACKAGE_DIR)/update-$(VERSION).pkg"
	@echo ""
	@echo "Next steps:"
	@echo "  1. Test: make upload VERSION=$(VERSION)"
	@echo "  2. Push tag: git push origin v$(VERSION)"

# =============================================================================
# Setup & Dependencies
# =============================================================================

.PHONY: check
check:  ## Verify build tools are installed
	@echo "$(CYAN)Checking build dependencies...$(RESET)"
	@echo ""
	@printf "  PlatformIO: "
	@which pio > /dev/null 2>&1 && echo "$(GREEN)OK$(RESET)" || echo "$(RED)NOT FOUND$(RESET)"
	@printf "  CMake:      "
	@which cmake > /dev/null 2>&1 && echo "$(GREEN)OK$(RESET)" || echo "$(RED)NOT FOUND$(RESET)"
	@printf "  Make:       "
	@which make > /dev/null 2>&1 && echo "$(GREEN)OK$(RESET)" || echo "$(RED)NOT FOUND$(RESET)"
	@printf "  Git:        "
	@which git > /dev/null 2>&1 && echo "$(GREEN)OK$(RESET)" || echo "$(RED)NOT FOUND$(RESET)"
	@printf "  Avahi:      "
	@pkg-config --exists avahi-client 2>/dev/null && echo "$(GREEN)OK$(RESET)" || echo "$(RED)NOT FOUND$(RESET)"
	@printf "  libzip:     "
	@pkg-config --exists libzip 2>/dev/null && echo "$(GREEN)OK$(RESET)" || echo "$(RED)NOT FOUND$(RESET)"
	@echo ""

.PHONY: install-deps
install-deps:  ## Install system dependencies (requires sudo)
	@echo "$(CYAN)Installing system dependencies...$(RESET)"
	sudo apt update
	sudo apt install -y \
	    build-essential \
	    cmake \
	    libavahi-client-dev \
	    libzip-dev \
	    python3-pip
	@echo ""
	@echo "$(CYAN)Installing PlatformIO...$(RESET)"
	pip3 install --user platformio
	@echo ""
	@echo "$(GREEN)Dependencies installed!$(RESET)"
	@echo "You may need to restart your shell or run: source ~/.profile"

# =============================================================================
# Cleanup
# =============================================================================

.PHONY: clean
clean:  ## Clean all build artifacts
	@echo "$(CYAN)Cleaning all build artifacts...$(RESET)"
	pio run -t clean || true
	rm -rf $(OTA_BUILD)
	rm -rf $(PACKAGE_DIR)
	@echo "$(GREEN)Clean complete$(RESET)"

.PHONY: clean-firmware
clean-firmware:  ## Clean only firmware builds
	pio run -t clean

.PHONY: clean-tools
clean-tools:  ## Clean only tools build
	rm -rf $(OTA_BUILD)

.PHONY: clean-packages
clean-packages:  ## Clean only OTA packages
	rm -rf $(PACKAGE_DIR)

# =============================================================================
# Info
# =============================================================================

.PHONY: info
info:  ## Show build information
	@echo ""
	@echo "$(CYAN)VONDERWAGENCC1 Build Info$(RESET)"
	@echo "========================="
	@echo "  Version:     $(VERSION)"
	@echo "  Device:      $(DEVICE)"
	@echo "  Display FW:  $(DISPLAY_FW)"
	@echo "  Controller:  $(CONTROLLER_FW)"
	@echo "  Package Dir: $(PACKAGE_DIR)"
	@echo ""

# =============================================================================
# Help
# =============================================================================

.PHONY: help
help:  ## Show this help
	@echo ""
	@echo "$(CYAN)VONDERWAGENCC1 Build System$(RESET)"
	@echo ""
	@echo "$(YELLOW)Usage:$(RESET)"
	@echo "  make [target] [OPTIONS]"
	@echo ""
	@echo "$(YELLOW)Options:$(RESET)"
	@echo "  VERSION=x.y.z   Override version (default: git describe)"
	@echo "  DEVICE=name     Target device (default: VONDERWAGENCC1.local)"
	@echo "  PASSWORD=xxx    OTA password (production mode)"
	@echo ""
	@echo "$(YELLOW)Targets:$(RESET)"
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | \
	    awk 'BEGIN {FS = ":.*?## "}; {printf "  $(CYAN)%-18s$(RESET) %s\n", $$1, $$2}'
	@echo ""

.DEFAULT_GOAL := help
