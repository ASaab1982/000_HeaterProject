/*
 * WaterActuator.cpp — Water pump and servo valve control
 *
 * This file drives the H-bridge water pump (in1Pin / in2Pin / enablePin) and the servo
 * valve (servoPin) to regulate home air temperature towards targetHomeTemp.
 *
 * Control strategy — 3-zone logic with ±0.5 °C deadband and 0.2 °C hysteresis:
 *
 *   Zone 1 (too cold) : home temp < target − 0.6 °C
 *       → pump ON at full speed, valve fully open (180°)
 *
 *   Zone 2 (equilibrium) : target − 0.4 °C ≤ home temp ≤ target + 0.4 °C
 *       → pump ON, valve at the position where heat gain from boiler equals heat loss
 *          to outdoors (derived from HomeTempModel constants HEAT_GAIN_RATE / HEAT_LOSS_RATE)
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

// Drives the H-bridge motor controller to run or stop the water pump.
// spd > 0 → pump ON at full speed (analogWrite 255), IN1 HIGH / IN2 LOW.
// spd == 0 → pump OFF, both IN pins LOW, enable 0.
// Updates g_WaterPumpSpeed (100 = running, 0 = stopped) and sets health bit 0.
void driveWaterPump(bool dir, int spd) {
    if (spd > 0) {
        digitalWrite(in1Pin, HIGH);
        digitalWrite(in2Pin, LOW);
        analogWrite(enablePin, 255);
        g_WaterPumpSpeed = 100;
    } else {
        digitalWrite(in1Pin, LOW);
        digitalWrite(in2Pin, LOW);
        analogWrite(enablePin, 0);
        g_WaterPumpSpeed = 0;
    }
    systemHealth |= (1 << 0);
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
        // Too cold — full heat
        driveWaterPump(true, 255);
        myservo.write(180);
        g_waterValvePosition = 180;

    } else if (currentZone == 3) {
        // Too warm — stop heating
        driveWaterPump(false, 0);
        myservo.write(0);
        g_waterValvePosition = 0;

    } else {
        // Equilibrium — valve position where gain = loss
        // Solve: HEAT_GAIN_RATE * gainFactor * (boiler - home) = HEAT_LOSS_RATE * (home - outdoor)
        float heaterDelta = g_heaterWaterTemp - g_homeTemp;
        int pos = 0;
        if (heaterDelta > 0.0f) {
            float gainFactor = (HEAT_LOSS_RATE * (g_homeTemp - g_outdoorTemp)) / (HEAT_GAIN_RATE * heaterDelta);
            float valveF = (gainFactor - 0.10f) / 0.90f * 180.0f;
            pos = (int)constrain(valveF, 0.0f, 180.0f);
        }
        driveWaterPump(true, 255);
        myservo.write(pos);
        g_waterValvePosition = pos;
    }
}

// Called each cycle by TaskWaterActuator: runs controlHomeTemp() then sets health bit 5.
void doWaterActuatorSequence() {
    controlHomeTemp();
    systemHealth |= (1 << 5);
}
