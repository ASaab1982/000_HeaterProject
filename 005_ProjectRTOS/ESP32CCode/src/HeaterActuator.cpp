/*
 * HeaterActuator.cpp — Heater relay control and automatic thermostat logic
 *
 * This file controls the physical heater relay (PIN_RELAY_HEATER, GPIO 9) and implements the automatic
 * thermostat that decides whether the heater should be ON or OFF based on boiler water
 * temperature vs. the user-defined setpoint.
 *
 * Two operating modes:
 *   Manual override ON  — heaterState is set directly from the UI via MQTT command;
 *                         the relay simply follows that value each cycle.
 *   Manual override OFF — automatic mode: heater turns ON when boiler water drops below
 *                         heaterTempSetPoint and turns OFF when it exceeds setpoint + 5 °C
 *                         (5 °C hysteresis to prevent rapid cycling).
 *
 * Health bit: bit 4 (1 << 4) is set each cycle so the watchdog knows this task ran.
 */

#include "HeaterActuator.h"

// FreeRTOS task — waits for a scheduler notification then calls doHeaterSequence().
void TaskHeater(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doHeaterSequence();
  }
}

// Drives the heater relay pin to match heaterState, then (if not in manual override)
// recalculates heaterState from boiler water temperature vs. setpoint with 5 °C hysteresis.
// Sets health bit 4 to signal a successful cycle to the watchdog.
void doHeaterSequence() {
  if (heaterState) {
    digitalWrite(PIN_RELAY_HEATER, HIGH);
    g_heaterPosition = 1; // 1 = ON
  } else {
    digitalWrite(PIN_RELAY_HEATER, LOW);
    g_heaterPosition = 0; // 0 = OFF
  }
  // [MANUAL OVERRIDE] When override is OFF, calculate heaterState automatically.
  // Hysteresis: ON below setpoint, OFF above setpoint+5°C, no change in between.
  if (!manualOverride) {
      if (g_heaterWaterTemp < heaterTempSetPoint)
          heaterState = true;
      else if (g_heaterWaterTemp > heaterTempSetPoint + 5.0f)
          heaterState = false;
  }

  systemHealth |= (1 << 4);
}
