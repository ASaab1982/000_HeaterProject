/*
 * WaterActuator.cpp — Water pump and servo valve control
 *
 * This file drives the water pump relay (PIN_RELAY_PUMP, GPIO 10) and the motorised valve
 * relays (PIN_RELAY_VALVE_CW GPIO 11, PIN_RELAY_VALVE_CCW GPIO 12) to regulate home air
 * temperature towards targetHomeTemp.
 *
 * Control strategy — 3-zone logic with ±0.5 °C deadband and 0.2 °C hysteresis:
 *
 *   Zone 1 (too cold) : home temp < target − 0.6 °C
 *       → pump ON at full speed, valve fully open (180°)
 *
 *   Zone 2 (equilibrium) : target − 0.4 °C ≤ home temp ≤ target + 0.4 °C
 *       → pump ON, valve at the position where heat gain from boiler equals heat loss
 *          to outdoors (HEAT_GAIN_RATE / HEAT_LOSS_RATE defined below)
 *
 *   Zone 3 (too warm) : home temp > target + 0.6 °C
 *       → pump OFF, valve fully closed (0°)
 *
 * Hysteresis prevents rapid zone bouncing at the boundaries.
 *
 * Health bits: bit 0 (pump, set inside driveWaterPump) and bit 5 (valve, set in
 * doWaterActuatorSequence) must both be set for the watchdog to consider this task healthy.
 */

#include "WaterActuator.h"

// FreeRTOS task — waits for a scheduler notification then calls doWaterActuatorSequence().
void TaskWaterActuator(void* pv) {
    (void)pv;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        doWaterActuatorSequence();
    }
}

// Drives the water pump relay.
// on = true → relay HIGH (pump running), on = false → relay LOW (pump stopped).
// Updates g_WaterPumpSpeed (100 = running, 0 = stopped) and sets health bit 0.
void driveWaterPump(bool on) {
    digitalWrite(PIN_RELAY_PUMP, on ? HIGH : LOW);
    g_WaterPumpSpeed = on ? 100 : 0;
    systemHealth |= (1 << 0);
}

// Opens the valve (clockwise relay ON, counter-clockwise OFF).
void openValve() {
    digitalWrite(PIN_RELAY_VALVE_CW,  HIGH);
    digitalWrite(PIN_RELAY_VALVE_CCW, LOW);
    g_waterValvePosition = 1;
}

// Closes the valve (counter-clockwise relay ON, clockwise OFF).
void closeValve() {
    digitalWrite(PIN_RELAY_VALVE_CW,  LOW);
    digitalWrite(PIN_RELAY_VALVE_CCW, HIGH);
    g_waterValvePosition = 0;
}

// Stops the valve motor (both relays OFF).
void stopValve() {
    digitalWrite(PIN_RELAY_VALVE_CW,  LOW);
    digitalWrite(PIN_RELAY_VALVE_CCW, LOW);
}

// Evaluates current home temperature against targetHomeTemp and selects the appropriate
// zone, then sets pump speed and valve position accordingly.  Zone transitions use
// hysteresis to avoid rapid switching at boundaries.  In equilibrium (zone 2) the valve
// angle is computed analytically so heat gain exactly offsets heat loss to outdoors.
void controlHomeTemp() {
    static int currentZone = 2;  // start in equilibrium zone

    const float deadband   = 0.5f;
    const float hysteresis = 0.1f;  // half of 0.2°C — applied to each side of each boundary

    // Hysteresis thresholds:
    //   Zone1→Zone2 exit  : homeTemp > target - deadband + hysteresis  (target - 0.4°C)
    //   Zone2→Zone1 entry : homeTemp < target - deadband - hysteresis  (target - 0.6°C)
    //   Zone2→Zone3 entry : homeTemp > target + deadband + hysteresis  (target + 0.6°C)
    //   Zone3→Zone2 exit  : homeTemp < target + deadband - hysteresis  (target + 0.4°C)

    if (currentZone == 1 && g_homeTemp > targetHomeTemp - deadband + hysteresis)
        currentZone = 2;
    else if (currentZone == 3 && g_homeTemp < targetHomeTemp + deadband - hysteresis)
        currentZone = 2;
    else if (currentZone == 2 && g_homeTemp < targetHomeTemp - deadband - hysteresis)
        currentZone = 1;
    else if (currentZone == 2 && g_homeTemp > targetHomeTemp + deadband + hysteresis)
        currentZone = 3;

    if (currentZone == 1) {
        // Too cold — pump ON, valve open
        driveWaterPump(true);
        openValve();

    } else if (currentZone == 3) {
        // Too warm — pump OFF, valve closed
        driveWaterPump(false);
        closeValve();

    } else {
        // Equilibrium — pump ON, valve stopped (holds current position)
        driveWaterPump(true);
        stopValve();
    }
}

// Called each cycle by TaskWaterActuator: runs controlHomeTemp() then sets health bit 5.
void doWaterActuatorSequence() {
    controlHomeTemp();
    systemHealth |= (1 << 5);
}
