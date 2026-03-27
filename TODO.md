# ESP32 Controller Project - Task Progress

## Approved Plan: Fix Local Version Mismatch (0.0.0 -> 1.0.0)
- [x] Create TODO.md with steps
- [x] Step 1: Edit src/define.h to add #define FIRMWARE_VERSION "1.0.0"
- [x] Step 2: Edit src/OTA_Manager.cpp to initialize localOtaVersion from FIRMWARE_VERSION if "0.0.0" or empty
- [x] Step 3: Commit (b459348) and push to main
- [x] Step 4: Build new firmware.bin (pio run) ✓
- [ ] Step 5: Flash to device and verify serial print shows 1.0.0
- [x] Step 6: Mark complete (code fixes done)

## New Task: Add 10sec boot delay
- [ ] Step 1: Edit src/main.cpp add delay(10000);
- [ ] Step 2: Commit/push
- [ ] Step 3: Build (pio run)
- [ ] Step 4: Flash/verify

