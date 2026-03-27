#include "Safety_Manager.h"
#include "define.h"
#include "WiFi_Manager.h"
#include "LED_Manager.h"
#include <WiFi.h>

// Extern declaration for logging function defined in main.cpp
extern void logStatusToFile(const char* data, bool forceFlush = false);

void safetyInit() {
    pinMode(PIN_VOLTAGE_SENSE, INPUT);
    // Set attenuation to 11dB to allow reading up to ~3.3V on the pin.
    // This is required to measure voltages > 1.1V through the voltage divider.
    analogSetAttenuation(ADC_11db);
    // Explicitly set the ADC resolution to 12-bit (0-4095) for the ESP32-S3.
    // This ensures analogReadMilliVolts has the correct bit-depth for its internal math.
    analogReadResolution(12);
}

uint32_t getSystemVoltage() {
    // Implementation: Multi-sample averaging (10 samples) to filter out electrical noise
    // and prevent accidental Safe Mode triggers.
    uint32_t sum = 0;
    const int samples = 10;
    for(int i = 0; i < samples; i++) {
        sum += analogReadMilliVolts(PIN_VOLTAGE_SENSE);
        delay(2); // Short delay between samples for ADC stability
    }
    
    return (uint32_t)((sum / samples) * VOLTAGE_DIVIDER_RATIO);
}

bool isVoltageSafe() {
    return getSystemVoltage() >= VOLTAGE_MIN_SAFE_MV;
}

void safetyUpdate() {
    uint32_t vcc = getSystemVoltage();

    // --- Safe Mode Entry Logic ---
    if (!isSafeMode && vcc < VOLTAGE_CRITICAL_MV) {
        isSafeMode = true;
        Serial.printf("\n!!! CRITICAL POWER: Entering Safe Mode (%u mV) !!!\n", vcc);
        
        // Action: Shut down high-power peripherals
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        ledSetColor(0, 0, 0); 
        
        logStatusToFile("SYSTEM: Entered Safe Mode due to low voltage.", true);
    } 
    // --- Safe Mode Recovery Logic ---
    else if (isSafeMode && vcc > VOLTAGE_RECOVERY_MV) {
        isSafeMode = false;
        Serial.printf("\n--- Power Recovered: Exiting Safe Mode (%u mV) ---\n", vcc);
        
        // Action: Restart WiFi. The wifiMonitorTask will handle the rest.
        WiFi.mode(WIFI_STA);
        wifiInit();
        
        logStatusToFile("SYSTEM: Exited Safe Mode. Power stabilized.", true);
    }
}