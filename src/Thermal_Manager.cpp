#include "Thermal_Manager.h"
#include <DHT.h>

DHT dht(PIN_DHT22, DHT22);

float avg_temp_c = 0.0f;
float avg_humid_pct = 0.0f;
bool dhtEnabled = true; // Tracks sensor health

static int consecutiveFails = 0;
const int MAX_DHT_FAILS = 5; // Disable after 10 seconds of failure (5 samples * 2s)

const int AVG_SAMPLES = 5; // 10s at 0.5Hz
float temp_samples[AVG_SAMPLES];
float humid_samples[AVG_SAMPLES];
int sample_index = 0;
bool samples_filled = false;

void thermalInit() {
  dht.begin();
  Serial.printf("Thermal Manager: DHT22 on GPIO%d initialized\n", PIN_DHT22);
  // Init samples to 0
  for (int i = 0; i < AVG_SAMPLES; i++) {
    temp_samples[i] = 0.0f;
    humid_samples[i] = 0.0f;
  }
}

void thermalReset() {
  consecutiveFails = 0;
  samples_filled = false;
  sample_index = 0;
  dhtEnabled = true;
  Serial.println("Thermal: Sensor flag reset. Attempting to resume monitoring...");
}

void thermalUpdate() {
  float temp = dht.readTemperature();
  float humid = dht.readHumidity();

  if (isnan(temp) || isnan(humid)) {
    consecutiveFails++;
    Serial.printf("Thermal: DHT read failed (%d/%d)\n", consecutiveFails, MAX_DHT_FAILS);
    
    if (consecutiveFails >= MAX_DHT_FAILS) {
      dhtEnabled = false;
      Serial.println("Thermal: CRITICAL - Sensor marked as failed. Disabling thermal monitoring.");
    }
    return;
  }

  // Success: Reset failure counter
  consecutiveFails = 0;
  dhtEnabled = true;

  temp_samples[sample_index] = temp;
  humid_samples[sample_index] = humid;
  sample_index = (sample_index + 1) % AVG_SAMPLES;
  if (sample_index == 0) samples_filled = true;

  float sum_temp = 0.0f;
  float sum_humid = 0.0f;
  int count = samples_filled ? AVG_SAMPLES : sample_index;

  for (int i = 0; i < count; i++) {
    sum_temp += temp_samples[i];
    sum_humid += humid_samples[i];
  }

  avg_temp_c = sum_temp / count;
  avg_humid_pct = sum_humid / count;
}

void thermalTask(void *parameter) {
  Serial.println("Thermal Task: 0.5Hz (2s) sampling started");
  for (;;) {
    if (dhtEnabled) {
      thermalUpdate();
    } else {
      vTaskDelay(pdMS_TO_TICKS(5000)); 
    }
    yield();
    vTaskDelay(pdMS_TO_TICKS(2000)); // 0.5Hz - DHT22 requirement
  }
}

