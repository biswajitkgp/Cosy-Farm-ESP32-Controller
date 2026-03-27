#include "LED_Manager.h"
#include "define.h" // Include define.h for state definitions

// Variables for managing LED blinking.
unsigned long lastBlink = 0;
bool blinkState = false;

// Initializes the LED pins for PWM control.
void ledInit() {
  // Configure PWM channels for Red, Green, and Blue LEDs.
  // ledcSetup(channel, frequency, resolution_bits)
  ledcSetup(PWM_CH_R, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_G, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_B, PWM_FREQ, PWM_RES);
  
  // Attach PWM channels to their respective GPIO pins.
  // ledcAttachPin(pin, channel)
  ledcAttachPin(PIN_R, PWM_CH_R);
  ledcAttachPin(PIN_G, PWM_CH_G);
  ledcAttachPin(PIN_B, PWM_CH_B);
  
  // Set initial LED color to off (0,0,0).
  ledSetColor(0,0,0);
}

// Sets the RGB color of the LED.
// r, g, b: 8-bit values (0-255) for Red, Green, and Blue intensity.
void ledSetColor(uint8_t r, uint8_t g, uint8_t b) {
  // Write the analog value (PWM duty cycle) to each LED channel.
  // The resolution is 8 bits, so values range from 0 to 255.
  ledcWrite(PWM_CH_R, r);
  ledcWrite(PWM_CH_G, g);
  ledcWrite(PWM_CH_B, b);
}

// Controls LED blinking patterns based on the current device state.
// state: The current operational state of the device (from define.h).
// now: The current time in milliseconds (from millis()).
void ledBlink(int state, unsigned long now) {
  // Respect Safe Mode: Keep LEDs off
  if (isSafeMode) return;

  switch(state) {
    case STATE_NTP_SYNC:
      if (now - lastBlink > 150) {
        blinkState = !blinkState;
        lastBlink = now;
        if (blinkState) {
          ledSetColor(100, 0, 255); // Magenta bright
        } else {
          ledSetColor(50, 0, 100); // Magenta dim
        }
      }
      break;
    case STATE_AP:
      ledSetColor(255, 255, 0);  // Solid yellow
      break;
    case STATE_CONNECTING:
      if (now - lastBlink > 1000) {
        blinkState = !blinkState;
        lastBlink = now;
        if (blinkState) {
          ledSetColor(0, 0, 255); // Blue bright
        } else {
          ledSetColor(0, 0, 50); // Blue dim
        }
      }
      break;
    case STATE_CONNECTED:
      ledSetColor(0, 255, 0);  // Solid green
      break;
    case STATE_ERROR:
      if (now - lastBlink > 200) {
        blinkState = !blinkState;
        lastBlink = now;
        if (blinkState) {
          ledSetColor(255, 0, 0); // Red bright
        } else {
          ledSetColor(100, 0, 0); // Red dim
        }
      }
      break;
    case STATE_OTA_CHECK:
      if (now - lastBlink > 1000) {
        blinkState = !blinkState;
        lastBlink = now;
        if (blinkState) {
          ledSetColor(0, 255, 255); // Cyan bright
        } else {
          ledSetColor(0, 100, 100); // Cyan dim
        }
      }
      break;
    case STATE_OTA_UPDATE:
      if (now - lastBlink > 200) {
        blinkState = !blinkState;
        lastBlink = now;
        if (blinkState) {
          ledSetColor(255, 255, 0); // Yellow bright
        } else {
          ledSetColor(100, 100, 0); // Yellow dim
        }
      }
      break;
    default:
      ledSetColor(0, 0, 0); // Off
      break;
  }
}
