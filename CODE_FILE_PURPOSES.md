# Code File Purpose Guide

This guide explains what each **code-oriented file** in the repository is responsible for.

## Build/Project Configuration

### `platformio.ini`
Defines the PlatformIO environment for an ESP32-S3 board using the Arduino framework, serial monitor speed, third-party dependencies (ArduinoJson + DHT sensor library), and compile-time flags used for debugging and watchdog behavior.

## Application Source (`src/`)

### `src/define.h`
Central configuration and shared global declarations:
- Declares cross-module globals (`extern`) such as time/location fields, state flags, OTA metadata, and telemetry values.
- Defines state constants (`STATE_NTP_SYNC`, `STATE_CONNECTED`, etc.) used by LED, WiFi, and OTA modules.
- Stores firmware/network defaults (SSID/password, OTA URLs, firmware version).
- Defines RGB LED pin and PWM channel mappings and DHT22 pin.

### `src/main.cpp`
Runtime entry point and orchestrator:
- Defines shared global instances/variables declared in `define.h`.
- Generates and persists a unique device ID from eFuse MAC.
- Initializes RTC/NTP cache, LittleFS, LED, WiFi, and thermal sensor modules.
- Starts FreeRTOS tasks (`wifiMonitorTask`, `systemInfoTask`, `thermalTask`).
- Implements buffered log persistence (`logStatusToFile`) using RTC memory + LittleFS.
- Periodically emits system status reports and handles serial command polling in `loop()`.

### `src/LED_Manager.h` / `src/LED_Manager.cpp`
Status LED abstraction:
- Initializes PWM channels and GPIO attachment for RGB output.
- Exposes direct color setter (`ledSetColor`).
- Implements state-driven LED patterns (`ledBlink`) with smooth fade logic for connecting/error/NTP/OTA states and solid colors for stable states.

### `src/WiFi_Manager.h` / `src/WiFi_Manager.cpp`
Network connectivity and connection lifecycle:
- Loads/initializes WiFi credentials from NVS (`Preferences`).
- Registers WiFi event handlers (connected, got IP, disconnected).
- Triggers NTP sync and OTA check after successful IP acquisition.
- Maintains global connectivity state and reconnection logic.
- Implements OTA rollback-verification timeout handling in `wifiMonitorTask`.
- Continuously updates LED behavior based on system state.

### `src/NTP_Manager.h` / `src/NTP_Manager.cpp`
Time + geolocation synchronization:
- Fetches timezone/offset/geo data from `ip-api.com`.
- Configures system time via `configTime` and maintains UTC/local formatted strings.
- Persists timezone/geo cache to NVS and restores it on boot for faster startup context.
- Exposes `ntpUpdateOnConnect()` used by WiFi and RTC logic.
- Signals RTC manager when NTP sync succeeds.

### `src/RTC_Manager.h` / `src/RTC_Manager.cpp`
RTC coherence and day-change maintenance:
- Boots RTC workflow by initializing NTP cache support.
- Tracks last successful sync day/timestamp.
- Detects calendar day change and forces a fresh NTP resync to limit drift.
- Provides formatted “current local time + last sync” string for status reporting.

### `src/OTA_Manager.h` / `src/OTA_Manager.cpp`
Firmware update system:
- Checks remote OTA version from GitHub raw endpoint after network/time readiness.
- Stores OTA metadata (local version, last check time) in NVS.
- Spawns dedicated OTA update task for firmware binary download + flashing.
- Performs integrity checks (HTTP size + optional MD5 validation).
- Marks pending verification and supports rollback integration with WiFi manager.

### `src/Thermal_Manager.h` / `src/Thermal_Manager.cpp`
DHT22 sensor sampling and health management:
- Initializes DHT22 interface.
- Samples temp/humidity at 0.5 Hz in a FreeRTOS task.
- Calculates rolling averages.
- Tracks consecutive read failures; disables thermal monitoring when failures exceed threshold.
- Supports manual sensor state reset via command interface.

### `src/Command_Manager.h` / `src/Command_Manager.cpp`
Serial command control plane:
- Polls serial input for single-character maintenance commands.
- Supports WiFi credential wipe, manual credential update, syncing defaults from `define.h`, full NVS erase (factory reset), and thermal sensor reset.
- Coordinates persistent updates through `Preferences` and restart behavior.

## Non-source/Binary Support Files

### `OTA Files/firmware.bin`
Prebuilt binary image used by OTA delivery flow (artifact, not source code).

### `OTA Files/version.txt`
Remote version descriptor consumed by OTA version-check logic (often `version` or `version:md5` format).
