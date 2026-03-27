#include <Arduino.h>
#include "define.h"
#include "LED_Manager.h"
#include "WiFi_Manager.h"
#include "NTP_Manager.h"
#include "OTA_Manager.h"
#include <Preferences.h>

// Global definitions (define.h extern impl)
Preferences prefs;
int currentState = 0;
int ntpRetryCount = 0;
bool wifiConnected = false;



void setup()
{
  Serial.begin(115200);
  delay(10000); // 10sec boot delay for serial monitor
  Serial.println("ESP32-S3 WiFi + RGB LED + NTP + OTA Starting...");

  ledInit();
  wifiInit();

  xTaskCreate(
      wifiMonitorTask,
      "WiFiMonitor",
      4096,
      NULL,
      1,
      NULL);

  Serial.println("Setup complete - monitor LED/WiFi/OTA");
}

void loop()
{
  vTaskDelay(pdMS_TO_TICKS(1000));
  // The main loop is mostly empty as critical operations are handled by FreeRTOS tasks.
  // A small delay is added to prevent the loop from running too fast and consuming CPU unnecessarily.
}
