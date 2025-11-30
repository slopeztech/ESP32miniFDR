#ifndef FDR_H
#define FDR_H

#include <Arduino.h>
#include <WebServer.h>

// Inicializa el módulo FDR. Llamar en setup.
void fdr_init();

// Ejecutar desde loop() para muestreo y control de tiempo.
void fdr_process();

// Control API
// duration_s: recording duration in seconds
// samples_per_sec: sampling frequency (e.g., 1 = 1 sample/sec, 10 = 10 samples/sec); default 1 if 0
bool fdr_start(uint32_t duration_s, uint32_t samples_per_sec);
void fdr_stop();
void fdr_reset();
bool fdr_isActive();

// Stream the stored CSV file via WebServer (returns true on success)
bool fdr_streamFile(WebServer &server);

#endif // FDR_H
