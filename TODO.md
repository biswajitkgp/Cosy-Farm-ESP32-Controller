# ESP32 Controller Project - Task Progress

## Approved Plan: Fix Local Version Mismatch (0.0.0 -> 1.0.0)
- [x] Create TODO.md with steps
- [ ] Step 1: Edit src/define.h to add #define FIRMWARE_VERSION "1.0.0"
- [ ] Step 2: Edit src/OTA_Manager.cpp to initialize localOtaVersion from FIRMWARE_VERSION if "0.0.0" or empty
- [ ] Step 3: Commit and push changes
- [ ] Step 4: Build new firmware.bin (pio run)
- [ ] Step 5: Flash to device and verify serial print shows 1.0.0
- [ ] Step 6: Mark complete

