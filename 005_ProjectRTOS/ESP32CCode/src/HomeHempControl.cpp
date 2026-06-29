/*
 * HomeHempControl.cpp — PI controller for home temperature
 *
 * Triggered by TaskXiaomiBLE every 5 minutes, after each Xiaomi sensor read
 * and WaterModel update. Computes g_valveTarget [0,1] which WaterActuator
 * uses to drive the valve relay toward the correct position.
 *
 * Hysteresis:
 *   Control ON  when homeTemp < targetHomeTemp + 0.2°C
 *   Control OFF when homeTemp > targetHomeTemp + 1.0°C
 *
 * PI:
 *   error  = targetHomeTemp − homeTemp
 *   output = KP × error + KI × integral  →  clamped [0, 1]
 *   Integrator pre-loaded to 1.0/KI at first run so output starts at 100%.
 *   Integrator reset to 0 when control deactivates.
 */

#include "HomeHempControl.h"
#include "ProjectHeater.h"
#include "DebugMacros.h"

// PI gains — tune to match house thermal response
static constexpr float KP            = 1.0f;
static constexpr float KI            = 0.001f;
static constexpr float TASK_PERIOD_S = 300.0f;  // 5 minutes between calls

static bool  s_controlActive  = false;
static float s_integral       = 0.0f;

void TaskHomeTempControl(void* pv) {
    (void)pv;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (!g_heatingEnabled) {
            g_valveTarget  = 0.0f;
            s_controlActive = false;
            s_integral      = 0.0f;
            D_PRINTLN(F("[HomeTempControl] Heating disabled"));
            continue;
        }

        // Hysteresis
        if (s_controlActive && g_homeTemp > targetHomeTemp + 1.0f) {
            s_controlActive = false;
            s_integral      = 0.0f;
        } else if (!s_controlActive && g_homeTemp < targetHomeTemp + 0.2f) {
            s_controlActive = true;
        }

        if (!s_controlActive) {
            g_valveTarget = 0.0f;
            D_PRINT(F("[HomeTempControl] OFF  HomeTemp: "));
            D_PRINT(g_homeTemp);
            D_PRINT(F(" / Target: "));
            D_PRINTLN(targetHomeTemp);
            continue;
        }

        // PI calculation
        float error = targetHomeTemp - g_homeTemp;
        s_integral += error * TASK_PERIOD_S;

        float output = KP * error + KI * s_integral;
        g_piProportional = KP * error;
        g_piIntegral     = KI * s_integral;

        // Clamp with anti-windup
        if (output > 1.0f) {
            output = 1.0f;
            if (error > 0.0f) s_integral -= error * TASK_PERIOD_S;
        } else if (output < 0.0f) {
            output = 0.0f;
            if (error < 0.0f) s_integral -= error * TASK_PERIOD_S;
        }

        g_valveTarget = output;

        D_PRINT(F("[HomeTempControl] ON  Error: "));
        D_PRINT(error);
        D_PRINT(F("  ValveTarget: "));
        D_PRINT((int)(g_valveTarget * 100));
        D_PRINTLN(F("%"));
    }
}
