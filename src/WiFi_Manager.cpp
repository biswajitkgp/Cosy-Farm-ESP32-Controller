#include "WiFi_Manager.h"
#include <WiFi.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <Update.h>
#include "NTP_Manager.h"
#include "OTA_Manager.h"

// Global WiFi credentials (defined here, declared extern in define.h)
String g_ssid;
String g_password;

// Rollback protection variables
bool isPendingVerify = false;
unsigned long rollbackTimer = 0;
// Allow 5 minutes for the new firmware to successfully connect and sync.
#define ROLLBACK_TIMEOUT_MS 300000 

// Forward declaration or move the function definition up to ensure 
// it is in scope for the event handlers.
bool ntpAttempt()
{
  // Set current state to NTP synchronization.
  currentState = STATE_NTP_SYNC;
  // Call the NTP update function which also fetches geo-location.
  ntpUpdateOnConnect();
  if (g_epochTime > 0 && g_lat.length() > 0)
  {
    currentState = STATE_CONNECTED;
    ntpRetryCount = 0;
    return true;
  }
  return false;
}

// Event handler for WiFi station connected to AP.
// This event signifies that the ESP32 has successfully connected to a WiFi access point,
// but it might not have received an IP address yet.
void WiFiEventConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.printf("WiFi connected to AP: %s\n", WiFi.SSID().c_str());
  // The currentState is not immediately set to STATE_CONNECTED here because
  // we need to wait for an IP address (handled by WiFiEventGotIP) and
  // complete NTP/OTA checks.
}

// Event handler for WiFi station obtaining an IP address.
// This event signifies that the ESP32 has a valid IP address and is fully connected to the network.
void WiFiEventGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  wifiConnected = true;
  Serial.printf("WiFi Connected! IP=%s, Signal=%d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  ntpRetryCount = 0; // Reset NTP retry counter on successful connection.
  
  // If this boot was a trial for new firmware, mark it as successful.
  if (isPendingVerify) {
    isPendingVerify = false;
    prefs.begin("ota", false);
    prefs.putBool("pending-verify", false);
    prefs.end();
    Serial.println("Rollback Protection: WiFi verified! Success flag cleared.");
  }

  // Attempt NTP synchronization.
  if (!ntpAttempt()) {
    ntpRetryCount++;
    // If NTP fails multiple times, skip it and proceed to connected state.
    if (ntpRetryCount >= 3) {
      Serial.println("NTP failed after 3 tries, skip");
      currentState = STATE_CONNECTED; // Force state to connected even if NTP failed.
    }
  }
  otaCheckAfterNtp(); // After NTP, check for OTA updates.
  // The currentState will be set by ntpAttempt() or otaCheckAfterNtp()
  // to STATE_CONNECTED or STATE_ERROR depending on their outcomes.
}

// Event handler for WiFi station disconnected from AP.
// This event is triggered when the ESP32 loses its connection to the WiFi access point.
void WiFiEventDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  wifiConnected = false;
  currentState = STATE_ERROR; // Set state to error on disconnection.
  // In ESP32 Core 3.0+, 'disconnected' was renamed to 'wifi_sta_disconnected'
  Serial.printf("WiFi disconnected from AP, reason: %d. Retrying...\n", info.wifi_sta_disconnected.reason);
  
  // Attempt to reconnect using the stored global credentials.
  WiFi.begin(g_ssid.c_str(), g_password.c_str());
  currentState = STATE_CONNECTING; // Set state to connecting while attempting to reconnect.
}

// Initializes WiFi connection.
// It first attempts to load saved WiFi credentials from NVS.
// If no credentials are found, it uses default ones and saves them.
// Then, it tries to connect to the WiFi network.
void wifiInit()
{
  // Begin NVS preferences in "wifi-creds" namespace.
  prefs.begin("wifi-creds", false);
  g_ssid = prefs.getString("ssid", "");
  g_password = prefs.getString("pass", "");
  if (g_ssid == "")
  {
    Serial.printf("No saved WiFi creds. Using default: %s\n", DEFAULT_SSID);
    g_ssid = DEFAULT_SSID;
    g_password = DEFAULT_PASS;
    prefs.putString("ssid", g_ssid);
    prefs.putString("pass", g_password);
  }
  else {
    Serial.printf("Using saved WiFi creds from NVS: %s\n", g_ssid.c_str());
    // Diagnostic: Warn if saved SSID differs from the current hardcoded default
    if (g_ssid != DEFAULT_SSID) {
      Serial.printf("INFO: Saved SSID differs from DEFAULT_SSID ('%s').\n", DEFAULT_SSID);
      Serial.println("      To use the new default, send 'W' over Serial to wipe NVS.");
    }
  }
  prefs.end();

  // Check if we are in a "trial" boot following an OTA update.
  prefs.begin("ota", true);
  isPendingVerify = prefs.getBool("pending-verify", false);
  prefs.end();

  if (isPendingVerify) rollbackTimer = millis();
  
  // Register WiFi event handlers to enable event-driven connection management.
  WiFi.onEvent(WiFiEventConnected, ARDUINO_EVENT_WIFI_STA_CONNECTED);
  WiFi.onEvent(WiFiEventGotIP, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiEventDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  Serial.printf("Attempting to connect to '%s'...\n", g_ssid.c_str());
  WiFi.begin(g_ssid.c_str(), g_password.c_str());
  currentState = STATE_CONNECTING; // Set initial state to connecting.
}

// FreeRTOS task to continuously monitor WiFi connection status.
// It handles reconnections, triggers NTP updates, and OTA checks when WiFi is connected.
// Also updates the LED status based on the current state.
void wifiMonitorTask(void *parameter)
{
  Serial.println("WiFi Monitor Task started");

  for (;;)
  {
    unsigned long now = millis(); // Get current time.

    // If in Safe Mode, skip connectivity logic and keep LEDs off
    if (isSafeMode) {
      ledSetColor(0, 0, 0);
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    
    // Update LED based on the current state.
    ledBlink(currentState, now);
    
    // Rollback Logic: If WiFi fails to connect within the timeout after an update.
    if (isPendingVerify && (now - rollbackTimer > ROLLBACK_TIMEOUT_MS)) {
      Serial.println("Rollback Protection: Connection timeout! Reverting to previous firmware...");
      
      prefs.begin("ota", false);
      prefs.putBool("pending-verify", false);
      // Revert the NVS version string so it reflects the version we are rolling back to.
      prefs.putString("ota-version", FIRMWARE_VERSION); 
      prefs.end();

      if (Update.rollBack()) {
        ESP.restart();
      } else {
        Serial.println("Rollback Protection: ERROR - Rollback failed (no previous image?)");
        isPendingVerify = false; // Stop trying to rollback if it's impossible.
      }
    }

    // Delay the task for 100 milliseconds to allow other FreeRTOS tasks to run.
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
