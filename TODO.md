# TODO

## OTA
- [ ] OTA test mode is disabled (`OTA_ENABLE_TEST_MODE=0` in ota_protocol.h) until SPI DMA pipeline sync issues are resolved. The test fails at chunk 0 due to response timing mismatch between master and slave.

## Features
- [ ] Add audible alerting through the speaker when sync is lost for an extended time

## Cleanup
- [ ] Remove references to now non-existing J5 connector for single rotary encoder
- [ ] Ensure all connectors have appropriate pullup resistors built in to board

