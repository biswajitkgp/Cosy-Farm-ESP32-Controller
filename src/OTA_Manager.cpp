#include "OTA_Manager.h"
#include <WiFi.h>
#include <Preferences.h>
#include "LED_Manager.h"  // for ledSetColor if direct

// Globals (definitions for externs in define.h)
String localOtaVersion = "";
time_t lastOtaCheck = 0;
bool otaInProgress = false;
TaskHandle_t otaTaskHandle = NULL;

bool isOtaInProgress() {
  return otaInProgress;
}

void otaCheckAfterNtp() {
  if (currentState != STATE_CONNECTED) return;
  
  Serial.println("OTA: Starting version check after NTP...");
  currentState = STATE_OTA_CHECK;
  
  // Load local version from prefs
  if (!prefs.begin("ota", false)) {
    Serial.println("OTA: Failed to open prefs");
    currentState = STATE_CONNECTED;
    return;
  }

  // Check if keys exist before reading to avoid NVS "NOT_FOUND" error logs.
  localOtaVersion = prefs.isKey("ota-version") ? prefs.getString("ota-version") : "0.0.0";
  lastOtaCheck = prefs.isKey("last-check") ? prefs.getLong("last-check") : 0;

  // Initialize local version if default (first boot)
  if (localOtaVersion == "0.0.0") {
    localOtaVersion = FIRMWARE_VERSION;
    prefs.putString("ota-version", localOtaVersion);
    Serial.printf("OTA: Initialized local version to %s\n", localOtaVersion.c_str());
  }
  prefs.end();
  
  Serial.printf("OTA: Local version: %s, Last check: %ld\n", localOtaVersion.c_str(), lastOtaCheck);
  
  // Check if need to check (daily)
  if (g_epochTime > 0 && (g_epochTime - lastOtaCheck > OTA_CHECK_INTERVAL)) {
    HTTPClient http;
    http.begin(OTA_VERSION_URL);
    http.addHeader("User-Agent", "ESP32-OTA");
    int code = http.GET();
    
    if (code == HTTP_CODE_OK) {
      WiFiClient * versionStream = http.getStreamPtr();
      String remoteVersion = "";
      while (versionStream->available()) {
        remoteVersion += (char)versionStream->read();
      }
      http.end();
      remoteVersion.trim();
      Serial.printf("OTA: Remote version: %s\n", remoteVersion.c_str());
      
      if (remoteVersion.length() > 0 && remoteVersion != localOtaVersion) {
        // Simple string compare; assume semantic versioning
        Serial.println("OTA: New version found! Starting update...");
        // Save new version
        prefs.begin("ota", false);
        prefs.putString("ota-version", remoteVersion);
        prefs.end();
        otaInProgress = true;
        currentState = STATE_OTA_UPDATE;
        xTaskCreate(otaUpdateTask, "OTAUpdate", 8192, NULL, 2, &otaTaskHandle);
        return;
      }
    } else {
      // Log an error if fetching the version failed.
      Serial.printf("OTA: Version fetch failed: %d\n", code);
      http.end();
    }
  } else {
    // Log if no OTA check is needed (either recently checked or NTP time is not yet available).
    Serial.println("OTA: No check needed (recent or no time)");
  }
  
  // Save the current epoch time as the last OTA check time.
  // This happens regardless of whether an update was found or not,
  // to ensure the interval is respected.
  prefs.begin("ota", false);
  prefs.putLong("last-check", g_epochTime);
  prefs.end();
  
  // Revert to connected state if no update was initiated.
  currentState = STATE_CONNECTED;
  Serial.println("OTA: Check complete - up to date");
}

// FreeRTOS task for performing the actual firmware update.
// This runs in a separate task to avoid blocking the main loop.
void otaUpdateTask(void *parameter) {
  Serial.println("OTA Task: Starting firmware download...");
  
  HTTPClient http;
  http.begin(OTA_FIRMWARE_URL);
  http.addHeader("User-Agent", "ESP32-OTA");
  
  // Fetch the firmware binary.
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("OTA: Firmware fetch failed: %d\n", code);
    http.end();
    // Reset OTA flag, set error state, and delete the task.
    otaInProgress = false;
    currentState = STATE_ERROR;
    vTaskDelete(NULL);
    return;
  }
  
  int contentLength = http.getSize();
  Serial.printf("OTA: Firmware size: %d bytes\n", contentLength); // Log firmware size.
  
  // Get the stream to read the firmware data.
  WiFiClient *stream = http.getStreamPtr();
  
  // Begin the OTA update process.
  // UPDATE_SIZE_UNKNOWN is used if the content length is not reliable or known beforehand.
  if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
    Serial.println("OTA: Update begin failed");
    http.end();
    // Reset OTA flag and delete the task.
    otaInProgress = false;
    vTaskDelete(NULL);
    return;
  }
  
  uint32_t written = 0; // Keep track of bytes written.
  // Buffer for reading firmware data in chunks.
  uint8_t buff[512];
  size_t len;
  
  // Read data from the stream and write it to the Update partition.
  while (http.connected() && (len = stream->available()) && (contentLength == -1 || written < (uint32_t)contentLength)) {
    size_t read_len = min(len, sizeof(buff));
    size_t read = stream->readBytes(buff, read_len);
    if (read > 0) {
      size_t w = Update.write(buff, read);
      written += w;
    }
    yield(); // Allow other tasks to run.
  }
  
  http.end(); // Close HTTP connection.
  
  // Finalize the update. 'true' indicates a successful update.
  if (Update.end(true)) {
    Serial.printf("OTA: Success! Written %u bytes, restarting...\n", written);
    // If successful, restart the ESP32 to boot into the new firmware.
  } else {
    // If update failed, log the error.
    Serial.printf("OTA: Update failed! Error %d: %s (written %u)\n", Update.getError(), Update.errorString(), written);
    // Reset OTA flag, set error state, and delete the task.
    otaInProgress = false;
    currentState = STATE_ERROR;
    vTaskDelete(NULL);
    return;
  }
  
  // If update was successful and device is about to restart, set otaInProgress to false.
  otaInProgress = false;
  ESP.restart();
}
