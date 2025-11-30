/**
 * @file main.cpp
 * @brief Main firmware for ESP32 AP with barometer and FDR API
 * @author slopez.tech
 * @date 2025-11-30
 */

#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <WebServer.h>
#include <SPIFFS.h>
#include <FS.h>

#include "barometer.h"
#include "led.h"
#include "fdr.h"


/**
 * @brief HTTP server instance running on port 80
 */
WebServer server(80);

// ============================================================================
// WiFi AP Configuration
// ============================================================================
/**
 * @brief Access Point SSID
 */
const char* ssid = "MiESP_AP";

/**
 * @brief Access Point password
 */
const char* password = "12345678";

// ============================================================================
// Setup and Initialization
// ============================================================================

/**
 * @brief Arduino setup function.
 * 
 * Initializes serial communication, LED, WiFi AP, I2C bus,
 * barometer and FDR modules, and sets up HTTP endpoints.
 */
void setup() {
  Serial.begin(115200);

  // Initialize LED system and blink startup sequence
  led_init();
  led_startupBlink(10, 250);

  // Start WiFi Access Point
  WiFi.softAP(ssid, password);
  Serial.println("Creating AP...");
  delay(1000);
  led_setBlue();
  Serial.println("AP ready, blue LED on.");

  // Initialize I2C
  Wire.begin(8, 9); // SDA, SCL pins
  Wire.setClock(100000); // Set I2C clock to 100 kHz

  // Initialize sensors and modules
  barometer_init();
  fdr_init();

  // ========================================================================
  // HTTP API Endpoints
  // ========================================================================

  /**
   * @brief Returns barometer readings in JSON format.
   * Endpoint: /api/barometer
   */
  server.on("/api/barometer", HTTP_GET, []() {
    if (!barometer_isReady()) {
      server.send(503, "application/json", "{\"error\":\"barometer not ready\"}");
      return;
    }

    float t = barometer_getTemperature();
    float p = barometer_getPressure();
    char buf[128];
    // Produce simple JSON with 2 decimals
    snprintf(buf, sizeof(buf), "{\"temperature\":%.2f,\"pressure\":%.2f}", t, p);
    server.send(200, "application/json", buf);
  });

  /**
   * @brief Start FDR sampling with optional duration and frequency.
   * Endpoint: /api/fdr/start?duration=<seconds>&frequency=<Hz>
   */
  server.on("/api/fdr/start", HTTP_GET, []() {
    String dur = server.arg("duration");
    String freq = server.arg("frequency");
    uint32_t d = 180; // default duration (seconds)
    uint32_t f = 1;   // default frequency (Hz)

    if (dur.length() > 0) d = (uint32_t)dur.toInt();
    if (freq.length() > 0) f = (uint32_t)freq.toInt();

    fdr_start(d, f);

    char response[256];
    snprintf(response, sizeof(response), 
             "{\"status\":\"started\",\"duration\":%u,\"frequency\":%u,\"interval_ms\":%u}",
             (unsigned)d, (unsigned)f, (unsigned)(f > 0 ? 1000/f : 1000));
    server.send(200, "application/json", response);
  });

  /**
   * @brief Stop FDR sampling.
   * Endpoint: /api/fdr/stop
   */
  server.on("/api/fdr/stop", HTTP_GET, []() {
    fdr_stop();
    server.send(200, "application/json", "{\"status\":\"stopped\"}");
  });

  /**
   * @brief Reset FDR data and counters.
   * Endpoint: /api/fdr/reset
   */
  server.on("/api/fdr/reset", HTTP_GET, []() {
    fdr_reset();
    server.send(200, "application/json", "{\"status\":\"reset\"}");
  });

  /**
   * @brief Download FDR data file.
   * Endpoint: /api/fdr/download
   */
  server.on("/api/fdr/download", HTTP_GET, []() {
    fdr_streamFile(server);
  });

  // Start HTTP server
  server.begin();
  Serial.println("HTTP server started (AP IP: 192.168.4.1)");
}

// ============================================================================
// Main Loop
// ============================================================================

/**
 * @brief Arduino loop function.
 * 
 * Processes sensor readings, handles incoming HTTP requests,
 * and delegates FDR sampling and control to its module.
 */
void loop() {
  // Process barometer readings
  barometer_process();

  // Handle incoming HTTP requests
  server.handleClient();

  // Delegate FDR sampling and control
  fdr_process();
}
