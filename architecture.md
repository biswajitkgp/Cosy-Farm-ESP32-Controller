# ESP32 Cosy Farm Controller - Architecture (Updated Commit 2f34059)

## Overview
ESP32-S3 WiFi controller for Cosy Farm. Features: RGB LED status, NTP geo/tz sync, OTA from GitHub, Serial commands, RTC cache/drift correction, Voltage safety mode. PlatformIO Arduino. Latest: Version 1.0.0 init, 11s boot delay.

## Hardware
- Board: ESP32-S3-DevKitC-1 (8MB Flash)
- RGB LED: GPIO2(R ch0),1(G ch1),3(B ch2) PWM 1kHz 8bit
- Voltage sense: PIN_VOLTAGE_SENSE (define.h, ADC 12bit 11dB)

## Software Stack
PlatformIO Arduino-ESP32 3.x + FreeRTOS + ArduinoJson 7 + HTTPClient/Preferences/Update/WiFi
```
main (globals, setup tasks, idle loop)
├── WiFi_Manager (connect/monitor task)
├── NTP_Manager (geo/tz/ntp)
├── OTA_Manager (check/update task)
├── LED_Manager (PWM blink patterns)
├── Command_Manager (serial cmds)
├── RTC_Manager (cache/sync/drift)
├── Safety_Manager (voltage safe mode)
└── define.h (states/pins/defines)
```

## Components (15 files)
1. **main.cpp**: prefs/currentState/ntpRetryCount/wifiConnected def. setup(): Serial(115200), delay(11s), ledInit/wifiInit, wifiMonitorTask. loop(): 1s idle.
2. **define.h**: States (NTP_SYNC0 AP1 CONNECTING2 CONNECTED3 ERROR4 OTA_CHECK5 UPDATE6), pins PWM, OTA URLs/interval24h, FIRMWARE_VERSION"1.0.0", VOLTAGE defines. Globals extern.
3. **WiFi_Manager**: Load/save SSID/pass prefs. wifiInit/connect/reconnect. wifiMonitorTask(5s): check/ledBlink/ntpAttempt/otaCheckAfterNtp.
4. **NTP_Manager**: ntpUpdateOnConnect(): ip-api geo/tz/offset → configTime/prefs geo-cache. Globals g_lat/lon/timezone/epoch/local/utc.
5. **LED_Manager**: ledInit(PWM attach), ledSetColor(r g b), ledBlink(state,millis) patterns (magenta NTP, yellow OTA etc.). Respects safe mode (off).
6. **OTA_Manager**: localOtaVersion/lastCheck. otaCheckAfterNtp(): prefs load (init 1.0.0 if0), daily remote version.txt compare → otaUpdateTask(HTTP firmware.bin chunk→Update.end→restart).
7. **Command_Manager**: commandUpdate(): Serial cmds | W/w: prefs wifi clear restart | C/c: new SSID/pass input save restart | S/s: default define.h save restart | F/f: nvs_flash_erase init restart.
8. **RTC_Manager**: rtcInit(ntpInit), rtcSyncWithNTP(set lastSyncDay/timestamp), rtcUpdate(daychange→ntp resync drift), rtcGetLocalTimeStr("%Y-%m-%d %H:%M:%S (Last Sync)").
9. **Safety_Manager**: safetyInit(ADC pin/11db/12bit), getSystemVoltage(10sample avg * divider), isVoltageSafe(>=MIN_SAFE_MV), safetyUpdate(): Critical<MV→safe(on WiFi/LEDoff log), Recovery>→restart WiFi.

## Flow
Boot(11s delay) → WiFi connect(blue blink) → NTP sync(magenta) → OTA check(cyan) → Connected(green). Daily OTA yellow download restart. Serial cmds anytime. Voltage safe(red off). RTC drift daily fix.

| Serial Cmd | Action |
|------------|--------|
| W/w | Wipe WiFi prefs, restart defaults |
| C/c | Input new SSID/pass, save restart |
| S/s | Sync NVS to define.h defaults |
| F/f | Factory NVS erase restart |

## OTA
URLs define.h GitHub raw OTA Files/version.txt/firmware.bin. pio run → copy .pio/build/esp32-s3-devkitc-1/firmware.bin → git push → auto pull daily.

## Build/Flash
```
pio run [--target upload --upload-port COMX]
monitor: 115200 11dB safe
```
Default partition OTA OK (27% flash used).

Repo: https://github.com/Cosy-Farms/Cosy-Farm-ESP32-Controller main:2f34059
