/**
 * @file barometer.cpp
 * @brief BME280/BMP280 barometer module with I2C scanning and EMA smoothing
 * @author slopez.tech
 * @date 2025-11-30
 */

#include "barometer.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <math.h>

// ============================================================================
// Configuration Constants
// ============================================================================

// I2C Addresses
static const uint8_t BME280_ADDRESS_PRIMARY = 0x76;
static const uint8_t BME280_ADDRESS_SECONDARY = 0x77;
static const uint8_t BME280_CHIP_ID = 0x58;

// Sensor Configuration
static const int BAD_READS_MAX = 3;
static const uint8_t I2C_SCAN_TIMEOUT = 5;

// Pressure Smoothing (EMA)
static const float PRESSURE_EMA_ALPHA = 0.25f; // 0.0–1.0 (higher = less smoothing)

// Sensor Validation Ranges
static const float TEMPERATURE_MIN = -40.0F;
static const float TEMPERATURE_MAX = 85.0F;
static const float PRESSURE_MIN = 300.0F;
static const float PRESSURE_MAX = 1100.0F;

// ============================================================================
// Static Runtime Variables
// ============================================================================

static Adafruit_BME280 bme;
static Adafruit_BMP280 bmp;
static bool bme_ok = false;
static bool bmp_used = false;
static int bad_read_count = 0;
static float lastTemp = NAN;
static float lastPressure = NAN;
static float pressure_ema = NAN;
static int deviceCount = 0;

// ============================================================================
// Forward Declarations
// ============================================================================

static void configureBME280HighPrecision();
static void configureBMP280HighPrecision();
static void configureBME280FastMode();
static void configureBMP280FastMode();
static bool validateSensorReadings(float temperature, float pressure);
static void initializePressureEMA(float initial_pressure);
static void updatePressureEMA(float raw_pressure);
static bool attemptBME280Init(uint8_t address);
static bool attemptBMP280Init(uint8_t address);
static bool readChipID(uint8_t address, uint8_t &chipid);
static void scanAndTryInit();

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Configures a BME280 sensor using maximum oversampling and filtering.
 *
 * Enables the highest possible precision at the cost of update latency.
 */
static void configureBME280HighPrecision() {
  bme.setSampling(
    Adafruit_BME280::MODE_NORMAL,
    Adafruit_BME280::SAMPLING_X16,
    Adafruit_BME280::SAMPLING_X16,
    Adafruit_BME280::SAMPLING_X16,
    Adafruit_BME280::FILTER_X16,
    Adafruit_BME280::STANDBY_MS_125
  );
}

/**
 * @brief Configures a BMP280 sensor using maximum oversampling and filtering.
 */
static void configureBMP280HighPrecision() {
  bmp.setSampling(
    Adafruit_BMP280::MODE_NORMAL,
    Adafruit_BMP280::SAMPLING_X8,
    Adafruit_BMP280::SAMPLING_X8,
    Adafruit_BMP280::FILTER_X16,
    Adafruit_BMP280::STANDBY_MS_125
  );
}

/**
 * @brief Sets the BME280 sensor to low-oversampling fast mode.
 *
 * Useful for high-frequency polling during fast logging.
 */
static void configureBME280FastMode() {
  bme.setSampling(
    Adafruit_BME280::MODE_NORMAL,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::FILTER_OFF,
    Adafruit_BME280::STANDBY_MS_125
  );
}

/**
 * @brief Sets the BMP280 sensor to low-oversampling fast mode.
 */
static void configureBMP280FastMode() {
  bmp.setSampling(
    Adafruit_BMP280::MODE_NORMAL,
    Adafruit_BMP280::SAMPLING_X1,
    Adafruit_BMP280::SAMPLING_X1,
    Adafruit_BMP280::FILTER_OFF,
    Adafruit_BMP280::STANDBY_MS_1
  );
}

/**
 * @brief Ensures temperature and pressure readings fall within realistic ranges.
 *
 * @param temperature Celsius temperature reading.
 * @param pressure Pressure in hPa.
 * @return true Valid range, false otherwise.
 */
static bool validateSensorReadings(float temperature, float pressure) {
  return (temperature >= TEMPERATURE_MIN && temperature <= TEMPERATURE_MAX) &&
         (pressure >= PRESSURE_MIN && pressure <= PRESSURE_MAX);
}

/**
 * @brief Initializes the pressure EMA filter with a first valid reading.
 *
 * @param initial_pressure First pressure value in hPa.
 */
static void initializePressureEMA(float initial_pressure) {
  if (isnan(pressure_ema)) {
    pressure_ema = initial_pressure;
  }
}

/**
 * @brief Updates the exponential moving average filter for pressure.
 *
 * @param raw_pressure Latest pressure reading in hPa.
 */
static void updatePressureEMA(float raw_pressure) {
  pressure_ema = PRESSURE_EMA_ALPHA * raw_pressure +
                 (1.0f - PRESSURE_EMA_ALPHA) * pressure_ema;
}

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initializes the barometer module.
 *
 * This performs a simple probe on the primary BME280 address.  
 * Full I2C scanning and fallback initialization occurs later inside barometer_process().
 */
void barometer_init() {
  bme_ok = bme.begin(BME280_ADDRESS_PRIMARY);
  bmp_used = false;
  bad_read_count = 0;
  lastTemp = NAN;
  lastPressure = NAN;
  deviceCount = 0;

  if (bme_ok) {
    configureBME280HighPrecision();
    Serial.println("Barometer: BME/BMP280 initialized with high precision oversampling.");
  } else {
    Serial.println("Barometer: Initial probe failed — will scan on process().");
  }
}

/**
 * @brief Scans the entire I2C bus searching for a BME280 or BMP280.
 *
 * Performs structured fallback:
 *  - Try BME280 at primary address
 *  - Try BME280 at secondary address
 *  - Try BMP280 if chip ID matches
 */
static void scanAndTryInit() {
  Serial.println("Barometer: Scanning I2C bus...");
  byte count = 0;
  uint8_t chipid = 0;

  for (byte i = 1; i < 120; i++) {
    Wire.beginTransmission(i);

    if (Wire.endTransmission() == 0) {
      Serial.print("  I2C device at 0x");
      if (i < 16) Serial.print("0");
      Serial.println(i, HEX);

      count++;

      if (i == BME280_ADDRESS_PRIMARY && !bme_ok) {

        if (attemptBME280Init(BME280_ADDRESS_PRIMARY)) break;

        if (readChipID(BME280_ADDRESS_PRIMARY, chipid)) {
          Serial.print("  Chip ID: 0x");
          Serial.println(chipid, HEX);
        }

        if (attemptBME280Init(BME280_ADDRESS_PRIMARY)) break;

        if (attemptBME280Init(BME280_ADDRESS_SECONDARY)) break;

        if (chipid == BME280_CHIP_ID && attemptBMP280Init(BME280_ADDRESS_PRIMARY)) break;

        Serial.println("  Barometer init failed — continuing scan.");
      }

      delay(I2C_SCAN_TIMEOUT);
    }
  }

  deviceCount = count;

  if (count == 0) {
    Serial.println("Barometer: No I2C devices found.");
  } else {
    Serial.print("Barometer: Total devices found: ");
    Serial.println(count);
  }
}

/**
 * @brief Attempts to initialize a BME280 sensor at a given address.
 *
 * @param address I2C address to probe.
 * @return true if successful, false otherwise.
 */
static bool attemptBME280Init(uint8_t address) {
  if (!bme.begin(address)) return false;

  bme_ok = true;
  bmp_used = false;
  configureBME280HighPrecision();

  Serial.print("Barometer: BME280 initialized at 0x");
  if (address < 16) Serial.print("0");
  Serial.println(address, HEX);
  return true;
}

/**
 * @brief Attempts to initialize a BMP280 sensor at a given address.
 *
 * @param address I2C address to probe.
 * @return true if successful, false otherwise.
 */
static bool attemptBMP280Init(uint8_t address) {
  if (!bmp.begin(address)) return false;

  bme_ok = true;
  bmp_used = true;
  configureBMP280HighPrecision();

  Serial.print("Barometer: BMP280 initialized at 0x");
  if (address < 16) Serial.print("0");
  Serial.println(address, HEX);
  return true;
}

/**
 * @brief Reads the chip ID register from a sensor address.
 *
 * @param address I2C device address.
 * @param chipid Output reference for the ID.
 * @return true if the ID was successfully read.
 */
static bool readChipID(uint8_t address, uint8_t &chipid) {
  Wire.beginTransmission(address);
  Wire.write(0xD0);
  Wire.endTransmission();
  Wire.requestFrom(address, (uint8_t)1);

  if (!Wire.available()) {
    Serial.println("  Failed to read chip ID.");
    return false;
  }

  chipid = Wire.read();
  return true;
}

/**
 * @brief Performs a complete sensor read cycle (temperature and pressure).
 *
 * Handles detection fallback, bad reading tolerance, and pressure smoothing.
 */
void barometer_process() {
  if (!bme_ok) {
    scanAndTryInit();
    return;
  }

  float temperature;
  float raw_pressure;

  if (bmp_used) {
    temperature = bmp.readTemperature();
    raw_pressure = bmp.readPressure() / 100.0F;
  } else {
    temperature = bme.readTemperature();
    raw_pressure = bme.readPressure() / 100.0F;
  }

  initializePressureEMA(raw_pressure);
  updatePressureEMA(raw_pressure);

  lastTemp = temperature;
  lastPressure = pressure_ema;

  if (validateSensorReadings(temperature, raw_pressure)) {
    bad_read_count = 0;
  } else {
    bad_read_count++;
    Serial.print("Barometer: Bad reading #");
    Serial.println(bad_read_count);

    if (bad_read_count >= BAD_READS_MAX) {
      Serial.println("Barometer: Consecutive bad readings — forcing re-scan.");
      bme_ok = false;
      bmp_used = false;
      bad_read_count = 0;
    }
  }
}

/**
 * @brief Checks whether a barometer sensor is currently initialized and operational.
 *
 * @return true If a BME280 or BMP280 has been successfully initialized.
 */
bool barometer_isReady() {
  return bme_ok;
}

/**
 * @brief Indicates whether the detected sensor is a BMP280.
 *
 * @return true If BMP280 is in use; false if BME280 or no sensor found.
 */
bool barometer_isBMP() {
  return bmp_used;
}

/**
 * @brief Retrieves the most recently read temperature value.
 *
 * @return Celsius temperature, or NAN if unavailable.
 */
float barometer_getTemperature() {
  return lastTemp;
}

/**
 * @brief Retrieves the most recently processed pressure value.
 *
 * This value is smoothed via EMA for stability.
 *
 * @return Pressure in hPa, or NAN if unavailable.
 */
float barometer_getPressure() {
  return lastPressure;
}

/**
 * @brief Switches between fast (low-latency) and high-precision sampling modes.
 *
 * @param fast true → Fast mode (low oversampling).  
 *             false → High precision mode (maximum oversampling).
 */
void barometer_setFastMode(bool fast) {
  if (!bme_ok) return;

  if (bmp_used) {
    if (fast) {
      configureBMP280FastMode();
      Serial.println("Barometer: BMP280 fast mode enabled (low latency)");
    } else {
      configureBMP280HighPrecision();
      Serial.println("Barometer: BMP280 high precision mode restored");
    }
  } else {
    if (fast) {
      configureBME280FastMode();
      Serial.println("Barometer: BME280 fast mode enabled (low latency)");
    } else {
      configureBME280HighPrecision();
      Serial.println("Barometer: BME280 high precision mode restored");
    }
  }
}
