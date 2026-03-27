#ifndef DEFINE_H
#define DEFINE_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
// No need <String.h>, Arduino.h covers

// Global variables, declared as extern to be defined in a single .cpp file (e.g., NTP_Manager.cpp or main.cpp).
// Geographic latitude.
extern String g_lat;
// Geographic longitude.
extern String g_lon;
// Current epoch time (seconds since Jan 1, 1970).
extern time_t g_epochTime;
// Current UTC time string.
extern String g_utcTime;
// Current local time string.
extern String g_localTime;
// Timezone name (e.g., "Europe/London").
extern String g_timezone;

// Current operational state of the device.
extern int currentState; 

// Define various states for the device's operation.
#define STATE_NTP_SYNC 0
#define STATE_AP 1
#define STATE_CONNECTING 2
#define STATE_CONNECTED 3
#define STATE_ERROR 4
#define STATE_OTA_CHECK 5
#define STATE_OTA_UPDATE 6
// NVS Preferences object.
extern Preferences prefs;
// Flag indicating if WiFi is currently connected.
extern bool wifiConnected;
// Global flag for Safe Mode status
extern bool isSafeMode;
// Counter for NTP synchronization retries.
extern int ntpRetryCount;
// Local firmware version string.
extern String localOtaVersion;
// Timestamp of the last OTA check.
extern time_t lastOtaCheck;

// Default WiFi credentials, used if no saved credentials are found.
#define DEFAULT_SSID "COSYFARM"
#define DEFAULT_PASS "12345678"

// The current version of the firmware. This is used to compare against the remote version
// to decide if an update is required.
#define FIRMWARE_VERSION "1.0.0"

#define OTA_VERSION_URL "https://raw.githubusercontent.com/PrathameshMestry/CosyFarm-ESP32/main/version.txt"
#define OTA_FIRMWARE_URL "https://raw.githubusercontent.com/PrathameshMestry/CosyFarm-ESP32/main/firmware.bin"
#define OTA_CHECK_INTERVAL 86400UL // How often to check for updates (in seconds, e.g., 24 hours)

// Pin definitions for the RGB LED.
#define PIN_R 2
#define PIN_G 1
#define PIN_B 3

// PWM (Pulse Width Modulation) settings for the RGB LED.
#define PWM_FREQ 1000
#define PWM_RES 8
#define PWM_CH_R 0 // PWM channel for Red LED
#define PWM_CH_G 1 // PWM channel for Green LED
#define PWM_CH_B 2 // PWM channel for Blue LED

// NTP (Network Time Protocol) settings.
#define NTP_TIMEOUT_MS 3000

// Voltage Monitoring
#define PIN_VOLTAGE_SENSE 4      // Use a GPIO with ADC capability (e.g., GPIO 4)
#define VOLTAGE_DIVIDER_RATIO 2.0 // Adjust based on your resistor divider (e.g., 10k/10k = 2)
#define VOLTAGE_MIN_SAFE_MV 3100  // Minimum millivolts required to safely write to Flash
#define VOLTAGE_CRITICAL_MV 3000  // Threshold to enter Safe Mode
#define VOLTAGE_RECOVERY_MV 3250  // Threshold to exit Safe Mode (Hysteresis)

#endif
