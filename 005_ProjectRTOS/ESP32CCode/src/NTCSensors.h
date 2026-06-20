#ifndef NTC_SENSORS_H
#define NTC_SENSORS_H

#include <Arduino.h>

// --- NTC thermistor parameters (10 kΩ NTC, β = 3950, 10 kΩ pull-up to 3.3 V) ---
#define NTC_BETA      3950.0f    // β coefficient
#define NTC_OFFSET    -1.5f      // calibration offset (°C) — sensors read ~1.5 °C high vs reference thermometer
#define NTC_R0        9500.0f    // nominal resistance at 25 °C (Ω) — measured 9.5 kΩ
#define NTC_T0_K      298.15f    // reference temperature in Kelvin (25 °C)
#define NTC_PULLUP    9900.0f    // pull-up resistor to 3.3 V (Ω) — measured 9.9 kΩ
#define NTC_VCC       3.3f       // supply voltage (V)
#define NTC_ADC_MAX   4095.0f    // 12-bit ADC full scale

// --- GPIO pin assignments ---
#define NTC_PIN_BOILER1   4   // Boiler 1 temperature
#define NTC_PIN_BOILER2   5   // Boiler 2 temperature
#define NTC_PIN_OUTLET    6   // Water outlet temperature
#define NTC_PIN_INLET     7   // Water inlet temperature

void TaskNTC(void* pv);

#endif
