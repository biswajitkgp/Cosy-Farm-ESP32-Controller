#ifndef SAFETY_MANAGER_H
#define SAFETY_MANAGER_H

#include <Arduino.h>

/**
 * Initializes safety-related hardware (ADC pins).
 */
void safetyInit();

/**
 * Monitors system voltage and manages Safe Mode transitions.
 */
void safetyUpdate();

/**
 * Returns current system voltage in millivolts.
 */
uint32_t getSystemVoltage();

/**
 * Returns true if voltage is above the minimum safe threshold for Flash operations.
 */
bool isVoltageSafe();

#endif