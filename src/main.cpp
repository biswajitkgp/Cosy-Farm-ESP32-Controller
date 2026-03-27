#include <Arduino.h>
#include "define.h"
#include "LED_Manager.h"
#include "WiFi_Manager.h"
#include "NTP_Manager.h"
#include "OTA_Manager.h"
#include "RTC_Manager.h"
#include "Safety_Manager.h"
#include "Command_Manager.h"
#include <Preferences.h>
#include <LittleFS.h>
#include <esp_system.h>

// Global definitions (define.h extern impl)
Preferences prefs;
int currentState = 0;
int ntpRetryCount = 0;
bool wifiConnected = false;
bool isSafeMode = false;

#define LOG_FILE_PATH "/system_log.txt"
#define MAX_LOG_SIZE (128 * 1024) // 128KB limit for the log file

// RTC Memory Buffer to reduce Flash wear and preserve logs across resets/sleep
#define RAM_BUF_MAX_SIZE 2048
RTC_DATA_ATTR char rtcLogBuffer[RAM_BUF_MAX_SIZE];
RTC_DATA_ATTR size_t rtcLogIndex = 0;

/**
 * Appends a string to the system log file on LittleFS.
 * Implementation: Uses an RTC-backed buffer to preserve logs across resets.
 */
void logStatusToFile(const char* data, bool forceFlush = false)
{
  uint32_t currentV = getSystemVoltage();
  size_t dataLen = strlen(data);
  
  // Check if adding this data would overflow the buffer
  bool willOverflow = (rtcLogIndex + dataLen >= RAM_BUF_MAX_SIZE - 1);

  // Flush to Flash if forced or if an overflow is about to occur
  if ((forceFlush || willOverflow) && rtcLogIndex > 0)
  {
    // Brownout Prevention: Abort Flash write if voltage is unstable
    if (!isVoltageSafe()) {
      Serial.printf("LOG ERROR: Voltage too low (%u mV). Postponing Flash write.\n", currentV);
      return; 
    }

    File file = LittleFS.open(LOG_FILE_PATH, FILE_APPEND);
    if (file) {
      if (file.size() > MAX_LOG_SIZE) {
        file.close();
        file = LittleFS.open(LOG_FILE_PATH, FILE_WRITE);
      }
      
      if (file) {
        file.write((const uint8_t*)rtcLogBuffer, rtcLogIndex);
        file.close();
        rtcLogIndex = 0;
        rtcLogBuffer[0] = '\0';
      }
    }
  }

  // Store data in the RTC buffer if it fits
  if (dataLen < RAM_BUF_MAX_SIZE - 1) {
    memcpy(rtcLogBuffer + rtcLogIndex, data, dataLen);
    rtcLogIndex += dataLen;
    rtcLogBuffer[rtcLogIndex] = '\0'; // Ensure null-termination
  }
}

// FreeRTOS task to periodically print system information
void systemInfoTask(void *parameter)
{
  static int lastLoggedState = -1;
  static unsigned long lastFlashWrite = 0;
  const unsigned long FLASH_WRITE_INTERVAL = 600000; // 10 minutes

  // Wait for the system to reach a stable connected state (WiFi, NTP, and Initial OTA check finished)
  while (currentState != STATE_CONNECTED)
  {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  Serial.println("System Info Task: Stable state reached. Periodic reporting active.");

  for (;;)
  {
    rtcUpdate(); // Check for day change drift correction

    // Use a stack-allocated buffer for formatting. 
    // This prevents heap allocations and fragmentation entirely.
    char buffer[512];
    int offset = 0;

    // Calculate uptime in days, hours, minutes
    unsigned long totalSeconds = millis() / 1000;
    int days = totalSeconds / 86400;
    int hours = (totalSeconds % 86400) / 3600;
    int mins = (totalSeconds % 3600) / 60;

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\n--- System Status Report ---\n");
    if (isSafeMode) offset += snprintf(buffer + offset, sizeof(buffer) - offset, "MODE:          SAFE MODE (LOW POWER)\n");
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Uptime:        %dd %dh %dm\n", days, hours, mins);
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Internal RTC:  %s\n", rtcGetLocalTimeStr().c_str());
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "WiFi Status:   %s\n", wifiConnected ? "Connected" : "Disconnected");
    
    uint32_t vcc = getSystemVoltage();
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Voltage:       %u mV (%s)\n", vcc, vcc < VOLTAGE_MIN_SAFE_MV ? "LOW" : "OK");

    if (wifiConnected)
    {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "IP Address:    %s\n", WiFi.localIP().toString().c_str());
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "Signal (RSSI): %d dBm\n", WiFi.RSSI());
    }

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, 
        "Free Heap:     %u KB\nFree PSRAM:    %u KB\nState:         %d\nOTA:           %s\n",
        ESP.getFreeHeap() / 1024, 
        ESP.getFreePsram() / 1024, 
        currentState, 
        isOtaInProgress() ? "Yes" : "No"
    );

    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "----------------------------\n");

    // Output to Serial
    Serial.print(buffer);

    // Decide if we should force a write to Flash
    bool criticalChange = (currentState != lastLoggedState);
    bool intervalReached = (millis() - lastFlashWrite >= FLASH_WRITE_INTERVAL);

    logStatusToFile(buffer, criticalChange || intervalReached);

    if (criticalChange || intervalReached) lastFlashWrite = millis();
    lastLoggedState = currentState;

    // Wait for 1 minute before printing the next report
    vTaskDelay(pdMS_TO_TICKS(60000));
  }
}

void setup()
{
  Serial.begin(115200);
  delay(10000); // 10sec boot delay for serial monitor
  Serial.println("ESP32-S3 WiFi + RGB LED + NTP + OTA Starting...");

  // Handle RTC Memory persistence logic
  esp_reset_reason_t reason = esp_reset_reason();
  if (reason == ESP_RST_POWERON || reason == ESP_RST_BROWNOUT) {
    // Fresh boot: zero out the index to prevent reading garbage
    rtcLogIndex = 0;
    rtcLogBuffer[0] = '\0';
    Serial.println("RTC Memory: Initialized fresh log buffer.");
  } else {
    // Software reset or Deep Sleep wake: preserve index and log status
    Serial.printf("RTC Memory: Preserved %u bytes of log data. (Reset Reason: %d)\n", rtcLogIndex, reason);
  }

  // Initialize LittleFS
  if (!LittleFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting LittleFS");
  }
  else
  {
    Serial.println("LittleFS mounted successfully");
  }

  // Print ESP32 Chip and Memory Information
  Serial.println("\n--- Hardware Information ---");
  Serial.printf("Chip Model:    %s\n", ESP.getChipModel());
  Serial.printf("Chip Revision: %d\n", ESP.getChipRevision());
  Serial.printf("Cores:         %d\n", ESP.getChipCores());
  Serial.printf("CPU Frequency: %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("Flash Size:    %d MB\n", ESP.getFlashChipSize() / (1024 * 1024));
  Serial.printf("Heap Size:     %d KB\n", ESP.getHeapSize() / 1024);
  Serial.printf("Free Heap:     %d KB\n", ESP.getFreeHeap() / 1024);
  Serial.printf("PSRAM Size:    %d KB\n", ESP.getPsramSize() / 1024);
  Serial.printf("Free PSRAM:    %d KB\n", ESP.getFreePsram() / 1024);
  Serial.println("---------------------------\n");

  ledInit();
  wifiInit();

  xTaskCreate(
      wifiMonitorTask,
      "WiFiMonitor",
      4096,
      NULL,
      1,
      NULL);

  // Create a FreeRTOS task for printing system information every minute
  xTaskCreate(
      systemInfoTask, // Function to be executed by the task
      "SystemInfo",   // Name of the task
      4096,           // Stack size in words
      NULL,           // Parameter to pass to the task
      1,              // Priority of the task
      NULL);          // Task handle

  Serial.println("Setup complete - monitor LED/WiFi/OTA");
}

void loop()
{
  // Process Serial commands
  commandUpdate();
  
  vTaskDelay(pdMS_TO_TICKS(100));
}
