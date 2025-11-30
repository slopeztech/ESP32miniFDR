#ifndef LED_H
#define LED_H

#include <Arduino.h>

// Inicializa el driver del LED (llamar en setup)
void led_init();

// Parpadeo de arranque (n veces, ms de delay entre toggles)
void led_startupBlink(int times = 10, int delayMs = 250);

// Colores rápidos
void led_setColor(uint8_t r, uint8_t g, uint8_t b);
void led_setBlue();
void led_setRed();
void led_off();

#endif // LED_H
