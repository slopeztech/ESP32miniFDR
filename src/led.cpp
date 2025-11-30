/**
 * @file led.cpp
 * @brief NeoPixel LED control module
 * @author slopez.tech
 * @date 2025-11-30
 */

#include "led.h"
#include <Adafruit_NeoPixel.h>

// ============================================================================
// Configuration Constants
// ============================================================================

/**
 * @brief Pin used to control the NeoPixel LED.
 */
static const uint8_t LED_PIN = 10;

/**
 * @brief Number of NeoPixel LEDs in the strip.
 */
static const uint8_t NUM_LEDS = 1;

/**
 * @brief NeoPixel LED controller instance.
 */
static Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize the NeoPixel LED system.
 *
 * Must be called before using any LED functions.
 */
void led_init() {
  leds.begin();
  leds.show();
}

/**
 * @brief Blink the LED in red for a given number of times at startup.
 *
 * @param times Number of blinks.
 * @param delayMs Delay between on/off states in milliseconds.
 */
void led_startupBlink(int times, int delayMs) {
  for (int i = 0; i < times; i++) {
    leds.setPixelColor(0, leds.Color(255, 0, 0));
    leds.show();
    delay(delayMs);

    leds.setPixelColor(0, leds.Color(0, 0, 0));
    leds.show();
    delay(delayMs);
  }
}

/**
 * @brief Set the LED to a custom RGB color.
 *
 * @param r Red intensity   (0–255)
 * @param g Green intensity (0–255)
 * @param b Blue intensity  (0–255)
 */
void led_setColor(uint8_t r, uint8_t g, uint8_t b) {
  leds.setPixelColor(0, leds.Color(r, g, b));
  leds.show();
}

/**
 * @brief Set the LED to blue.
 */
void led_setBlue() {
  led_setColor(0, 0, 255);
}

/**
 * @brief Set the LED to red.
 */
void led_setRed() {
  led_setColor(255, 0, 0);
}

/**
 * @brief Turn the LED off.
 */
void led_off() {
  led_setColor(0, 0, 0);
}
