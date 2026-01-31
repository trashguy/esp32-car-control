# TODO

## OTA
- [ ] OTA test mode is disabled (`OTA_ENABLE_TEST_MODE=0` in ota_protocol.h) until SPI DMA pipeline sync issues are resolved. The test fails at chunk 0 due to response timing mismatch between master and slave.

## Features
- [ ] Add audible alerting through the speaker when sync is lost for an extended time

## Display/LVGL Improvements
- [ ] **PSRAM integration for LVGL** - ESP32-S3 has 8MB embedded PSRAM that could be used for LVGL's memory pool. This would allow:
  - Larger `LV_MEM_SIZE` (currently 64KB, could be 512KB+)
  - Restore rounded corners (radius 12) on file list widget without memory issues
  - Support for more complex UI elements, animations, and larger images
  - Implementation: Set `LV_MEM_CUSTOM=1` in lv_conf.h and provide custom allocator using `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`
  - Trade-off: PSRAM is 4-10x slower than internal SRAM; keep draw buffers in internal RAM for performance
- [ ] **Move all contextual buttons to menu bar** - Consolidate screen-specific action buttons (back, save, cancel, etc.) into the bottom Android-style navigation bar for consistent UX across all screens
- [ ] Add file browser directory navigation (currently only shows root)
- [ ] Add file type icons based on extension (.bin, .txt, etc.)

## Cleanup
- [ ] Remove references to now non-existing J5 connector for single rotary encoder
- [ ] Ensure all connectors have appropriate pullup resistors built in to board

