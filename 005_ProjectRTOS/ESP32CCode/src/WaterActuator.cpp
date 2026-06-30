/*
 * WaterActuator.cpp — Water pump and valve relay driver
 *
 * Runs every 1.5 s (notified by TaskTimeScheduler).
 * Drives the pump relay and moves the valve one step per call toward
 * g_valveTarget, which is computed by TaskHomeTempControl (every 5 min).
 *
 * Startup: valve is driven fully open (CW relay ON for 150 s) before any
 * PI control activates, to establish a known reference position.
 *
 * Valve position is tracked by dead reckoning (no physical sensor):
 *   VALVE_STEP = 1.5 s / 150 s = 1% of full stroke per call.
 */

#include "WaterActuator.h"
#include "DebugMacros.h"

static constexpr float VALVE_STROKE_S = 150.0f;
static constexpr float TASK_PERIOD_S  = 2.0f;
static constexpr float VALVE_STEP     = TASK_PERIOD_S / VALVE_STROKE_S;

static bool     s_valveInitDone    = false;
static uint32_t s_valveInitStartMs = 0;
static float    s_valvePos         = 0.0f;   // current estimated position [0,1]

void TaskWaterActuator(void* pv) {
    (void)pv;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (g_safeState) {
            digitalWrite(PIN_RELAY_PUMP,      LOW);
            digitalWrite(PIN_RELAY_VALVE_CW,  LOW);
            digitalWrite(PIN_RELAY_VALVE_CCW, LOW);
            g_WaterPumpSpeed = 0;
            systemHealth |= (1 << 0);
            systemHealth |= (1 << 5);
            continue;
        }
        doWaterActuatorSequence();
    }
}

void driveWaterPump(bool on) {
    digitalWrite(PIN_RELAY_PUMP, on ? HIGH : LOW);
    g_WaterPumpSpeed = on ? 100 : 0;
    systemHealth |= (1 << 0);
}

void openValve() {
    digitalWrite(PIN_RELAY_VALVE_CW,  HIGH);
    digitalWrite(PIN_RELAY_VALVE_CCW, LOW);
}

void closeValve() {
    digitalWrite(PIN_RELAY_VALVE_CW,  LOW);
    digitalWrite(PIN_RELAY_VALVE_CCW, HIGH);
}

void stopValve() {
    digitalWrite(PIN_RELAY_VALVE_CW,  LOW);
    digitalWrite(PIN_RELAY_VALVE_CCW, LOW);
}

// Moves valve one step per call toward target. Stops when reached.
static void driveValveToward(float target) {
    float delta = target - s_valvePos;
    if (delta > VALVE_STEP) {
        openValve();
        s_valvePos += VALVE_STEP;
    } else if (delta < -VALVE_STEP) {
        closeValve();
        s_valvePos -= VALVE_STEP;
    } else {
        stopValve();
        s_valvePos = target;
    }
    g_waterValvePosition = (int)(s_valvePos * 100);
}

void doWaterActuatorSequence() {
    // Heating disabled — pump OFF, valve stopped
    if (!g_heatingEnabled) {
        driveWaterPump(false);
        stopValve();
        D_PRINTLN(F("[WaterActuator] Heating: DISABLED"));
        systemHealth |= (1 << 5);
        return;
    }

    // Startup: drive valve fully open for 150 s before PI activates
    if (!s_valveInitDone) {
        driveWaterPump(false);
        if (s_valveInitStartMs == 0) {
            openValve();
            s_valveInitStartMs = millis();
        }
        uint32_t elapsed = (millis() - s_valveInitStartMs) / 1000;
        g_waterValvePosition = (int)((float)elapsed / VALVE_STROKE_S * 100.0f);
        D_PRINT(F("[WaterActuator] Valve init: "));
        D_PRINT(elapsed);
        D_PRINTLN(F(" s / 150 s"));
        if (millis() - s_valveInitStartMs >= (uint32_t)(VALVE_STROKE_S * 1000.0f)) {
            stopValve();
            s_valvePos      = 1.0f;
            s_valveInitDone = true;
        }
        systemHealth |= (1 << 5);
        return;
    }

    // Drive pump and valve toward PI target
    bool controlOn = (g_valveTarget > 0.0f);
    driveWaterPump(controlOn);
    driveValveToward(g_valveTarget);

    D_PRINT(F("[WaterActuator] Pump: "));
    D_PRINT(controlOn ? F("ON") : F("OFF"));
    D_PRINT(F("  Valve: "));
    D_PRINT(g_waterValvePosition);
    D_PRINTLN(F("%"));
    D_PRINT(F("[WaterActuator] P: "));
    D_PRINT(g_piProportional);
    D_PRINT(F("  I: "));
    D_PRINTLN(g_piIntegral);

    systemHealth |= (1 << 5);
}
