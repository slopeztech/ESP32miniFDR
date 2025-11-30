/**
 * @file fdr.cpp
 * @brief Flight Data Recorder (FDR) module for ESP32
 * @author slopez.tech
 * @date 2025-11-30
 */

#include "fdr.h"
#include "barometer.h"
#include "led.h"
#include <SPIFFS.h>
#include <FS.h>

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Path where the FDR CSV file is stored in SPIFFS.
 */
static constexpr const char* FDR_PATH = "/fdr.csv";

/**
 * @brief Default sample interval (1 Hz).
 */
static constexpr uint32_t DEFAULT_SAMPLE_INTERVAL_MS = 1000;

/**
 * @brief Maximum supported sampling frequency (50 Hz).
 */
static constexpr uint32_t MAX_SAMPLES_PER_SEC = 50;

/**
 * @brief Size threshold (bytes) at which the RAM buffer is flushed to disk.
 */
static constexpr uint32_t BUFFER_FLUSH_THRESHOLD = 1024;

/**
 * @brief Time interval (ms) after which a flush is forced even if buffer is small.
 */
static constexpr uint32_t BUFFER_FLUSH_INTERVAL_MS = 250;

// ============================================================================
// Module State
// ============================================================================

/**
 * @brief True while FDR is actively recording.
 */
static bool fdr_active = false;

/**
 * @brief Time (millis) when FDR recording ends.
 */
static uint32_t fdr_end_ms = 0;

/**
 * @brief Time (millis) when FDR recording started.
 */
static uint32_t fdr_start_ms = 0;

/**
 * @brief Timestamp (millis) of last recorded sample.
 */
static uint32_t last_sample_ms = 0;

/**
 * @brief Interval between samples in milliseconds (can be changed at fdr_start()).
 */
static uint32_t fdr_sample_interval_ms = DEFAULT_SAMPLE_INTERVAL_MS;

/**
 * @brief True if SPIFFS has already been mounted.
 */
static bool spiffs_mounted = false;

/**
 * @brief Open file handle used during active recording.
 */
static File fdr_file;

/**
 * @brief In-RAM buffer for CSV lines waiting to be flushed to disk.
 */
static String fdr_write_buffer;

/**
 * @brief Timestamp (millis) of last buffer flush.
 */
static uint32_t last_flush_ms = 0;

// ============================================================================
// Helpers (small, focused functions to improve readability)
// ============================================================================

/**
 * @brief Attempts to mount SPIFFS lazily. Formats if mount fails.
 *
 * @return true if SPIFFS is mounted successfully, false otherwise.
 */
static bool ensureSpiffs() {
  if (spiffs_mounted) return true;
  if (SPIFFS.begin(false)) {
    spiffs_mounted = true;
    Serial.println("FDR: SPIFFS mounted successfully");
    return true;
  }
  // last-resort: format and mount
  Serial.println("FDR: SPIFFS mount failed, attempting format...");
  spiffs_mounted = SPIFFS.begin(true);
  if (spiffs_mounted) {
    Serial.println("FDR: SPIFFS formatted and mounted");
  } else {
    Serial.println("FDR: SPIFFS format/mount failed -- check partition table");
  }
  return spiffs_mounted;
}

/**
 * @brief Opens the FDR file for writing and writes the CSV header.
 *
 * @return true on success, false on failure.
 */
static bool openFdrFileForWrite() {
  if (fdr_file) fdr_file.close();
  if (SPIFFS.exists(FDR_PATH)) SPIFFS.remove(FDR_PATH);
  fdr_file = SPIFFS.open(FDR_PATH, FILE_WRITE);
  if (!fdr_file) return false;
  fdr_file.println("timestamp_s,pressure_hpa");
  fdr_file.flush();
  return true;
}

/**
 * @brief Opens the FDR file for appending. No header written.
 *
 * @return true on success.
 */
static bool openFdrFileForAppend() {
  if (!fdr_file) {
    fdr_file = SPIFFS.open(FDR_PATH, FILE_APPEND);
  }
  return (bool)fdr_file;
}

/**
 * @brief Flushes the RAM buffer to the FDR file.
 *
 * If the file is not open, attempts to reopen it.
 * If only part of the buffer is written, the unwritten tail remains.
 */
static void flushBufferToFile() {
  if (fdr_write_buffer.length() == 0) return;
  if (!fdr_file) {
    if (!ensureSpiffs()) {
      Serial.println("FDR: SPIFFS lost, cannot flush buffer");
      return;
    }
    if (!openFdrFileForAppend()) {
      Serial.println("FDR: cannot open file to flush buffer");
      return;
    }
  }

  size_t wrote = fdr_file.write((const uint8_t*)fdr_write_buffer.c_str(), fdr_write_buffer.length());
  fdr_file.flush();

  if (wrote == (size_t)fdr_write_buffer.length()) {
    fdr_write_buffer = "";
  } else if (wrote > 0 && wrote < (size_t)fdr_write_buffer.length()) {
    // remove the written prefix
    fdr_write_buffer = fdr_write_buffer.substring(wrote);
  }
  last_flush_ms = millis();
}

/**
 * @brief Closes the FDR file if open.
 */
static void closeFdrFileIfOpen() {
  if (fdr_file) {
    fdr_file.close();
  }
}

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initializes the FDR module.
 *
 * Does NOT mount SPIFFS immediately to avoid blocking setup().
 */
void fdr_init() {
  Serial.println("FDR: initialized (SPIFFS mount deferred)");
}

/**
 * @brief Starts the FDR recording session.
 *
 * @param duration_s Duration of recording in seconds.
 * @param samples_per_sec Sampling rate (Hz). Clamped to max allowed.
 *
 * @return true if recording successfully started.
 */
bool fdr_start(uint32_t duration_s, uint32_t samples_per_sec) {
  if (!ensureSpiffs()) return false;

  if (samples_per_sec == 0) samples_per_sec = 1;
  if (samples_per_sec > MAX_SAMPLES_PER_SEC) {
    Serial.printf("FDR: requested %u samples/sec too high, capping to %u\n",
                  (unsigned)samples_per_sec, (unsigned)MAX_SAMPLES_PER_SEC);
    samples_per_sec = MAX_SAMPLES_PER_SEC;
  }

  fdr_sample_interval_ms = 1000 / samples_per_sec;

  if (!openFdrFileForWrite()) {
    Serial.println("FDR: failed to create file");
    return false;
  }

  fdr_write_buffer = "";
  last_flush_ms = millis();

  fdr_active = true;
  fdr_start_ms = millis();
  fdr_end_ms = fdr_start_ms + duration_s * 1000UL;
  last_sample_ms = 0;

  barometer_setFastMode(true);
  led_setColor(0, 255, 0); // green
  Serial.printf("FDR: started for %u seconds at %u samples/sec (interval %u ms)\n",
                (unsigned)duration_s, (unsigned)samples_per_sec,
                (unsigned)fdr_sample_interval_ms);
  return true;
}

/**
 * @brief Stops FDR recording, flushes buffers and closes files.
 */
void fdr_stop() {
  if (!fdr_active) return;

  flushBufferToFile();
  closeFdrFileIfOpen();

  fdr_active = false;
  barometer_setFastMode(false);
  led_setBlue();
  Serial.println("FDR: stopped (file flushed and closed)");
}

/**
 * @brief Deletes the recorded data file (if it exists).
 */
void fdr_reset() {
  if (!ensureSpiffs()) return;
  closeFdrFileIfOpen();
  if (SPIFFS.exists(FDR_PATH)) SPIFFS.remove(FDR_PATH);
  Serial.println("FDR: data reset (file removed)");
}

/**
 * @brief Returns whether FDR recording is active.
 *
 * @return true if recording, false otherwise.
 */
bool fdr_isActive() {
  return fdr_active;
}

/**
 * @brief Main FDR loop handler. Must be called frequently.
 *
 * Takes samples according to the configured rate, stores them in the
 * buffer and flushes periodically.
 */
void fdr_process() {
  if (!fdr_active) return;
  const uint32_t now = millis();
  if (now >= fdr_end_ms) {
    fdr_stop();
    return;
  }

  if (last_sample_ms == 0 || (now - last_sample_ms) >= fdr_sample_interval_ms) {
    last_sample_ms = now;

    if (!barometer_isReady()) {
      Serial.println("FDR: barometer not ready, skipping sample");
      return;
    }

    const float pressure = barometer_getPressure();
    const float t_sec = (float)(now - fdr_start_ms) / 1000.0f;

    char line[64];
    snprintf(line, sizeof(line), "%.3f,%.2f\n", t_sec, pressure);
    fdr_write_buffer += String(line);

    if ((uint32_t)fdr_write_buffer.length() >= BUFFER_FLUSH_THRESHOLD ||
        (now - last_flush_ms) >= BUFFER_FLUSH_INTERVAL_MS) {
      flushBufferToFile();
    }
  }
}

/**
 * @brief Streams the recorded FDR CSV file via HTTP.
 *
 * Flushes any pending buffer first if recording is active.
 *
 * @param server Reference to the WebServer handling the request.
 * @return true on successful transfer.
 */
bool fdr_streamFile(WebServer &server) {
  if (!ensureSpiffs()) {
    server.send(500, "application/json", R"({"error":"SPIFFS mount failed"})");
    return false;
  }

  if (!SPIFFS.exists(FDR_PATH)) {
    server.send(404, "application/json", R"({"error":"no data"})");
    return false;
  }

  // If FDR is still active, flush the buffer before sending
  if (fdr_active && fdr_write_buffer.length() > 0) {
    flushBufferToFile();
  }

  File f = SPIFFS.open(FDR_PATH, FILE_READ);
  if (!f) {
    server.send(500, "application/json", R"({"error":"cannot open file"})");
    return false;
  }

  server.sendHeader("Content-Type", "text/csv");
  server.sendHeader("Content-Disposition", "attachment; filename=fdrecord.csv");
  server.streamFile(f, "text/csv");
  f.close();
  return true;
}
