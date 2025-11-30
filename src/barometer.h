#ifndef BAROMETER_H
#define BAROMETER_H

#include <Arduino.h>

// Inicializa el módulo de barómetro. Llamar después de `Wire.begin(...)`.
void barometer_init();

// Ejecutar periódicamente desde `loop()` para permitir scans/reintentos.
void barometer_process();

// Estado
bool barometer_isReady();
bool barometer_isBMP();

// Últimas lecturas (temperatura en °C, presión en hPa)
float barometer_getTemperature();
float barometer_getPressure();

// Switch sensor between normal (high-precision) and fast (low-latency) sampling.
// Call `barometer_setFastMode(true)` before starting high-rate recordings,
// and `barometer_setFastMode(false)` to restore high-precision mode.
void barometer_setFastMode(bool fast);

#endif // BAROMETER_H
