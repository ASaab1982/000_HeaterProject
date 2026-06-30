/*
 * DiagFunctional.cpp — Functional diagnostics and safe state management
 *
 * TaskDiagFunctional runs every scheduler cycle and evaluates all diagnostic
 * conditions. If any condition fails, g_safeState is set to true, which causes:
 *   - HeaterActuator  : heater relay forced OFF
 *   - WaterActuator   : pump OFF, valve stopped
 *   - DataToCloud     : all incoming UI commands ignored
 *
 * Diagnostic conditions are added here one by one.
 * g_safeState is cleared only when ALL conditions pass.
 */

#include "ProjectHeater.h"
#include "DiagFunctional.h"
#include "XiaomiBLE.h"

static uint32_t s_heaterOnSince   = 0;
static float    s_boiler1AtOn     = NAN;
static float    s_boiler2AtOn     = NAN;
static bool     s_heaterWasOn     = false;

void TaskDiagFunctional(void* pv) {
    (void)pv;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        bool safe = true;

        // --- Diag 1: heater ON > 60s with no boiler temperature rise of 1°C ---
        if (heaterState) {
            if (!s_heaterWasOn) {
                // heater just turned ON — record baseline
                s_heaterOnSince = millis();
                s_boiler1AtOn   = g_boiler1Temp;
                s_boiler2AtOn   = g_boiler2Temp;
                s_heaterWasOn   = true;
            } else if (millis() - s_heaterOnSince > 60000) {
                bool boiler1Rose = !isnan(g_boiler1Temp) && !isnan(s_boiler1AtOn) && (g_boiler1Temp - s_boiler1AtOn >= 1.0f);
                bool boiler2Rose = !isnan(g_boiler2Temp) && !isnan(s_boiler2AtOn) && (g_boiler2Temp - s_boiler2AtOn >= 1.0f);
                if (!boiler1Rose && !boiler2Rose) {
                    safe = false;
                    Serial.println(F("[DIAG] Heater ON > 60s but no boiler temp rise — SAFE STATE"));
                }
            }
        } else {
            s_heaterWasOn = false;
        }

        // --- Diag 2: no Xiaomi reading for more than 30 minutes ---
        if (g_xiaomiValid && (millis() - g_xiaomiLastReadMs > 1800000UL)) {
            safe = false;
            Serial.println(F("[DIAG] No Xiaomi reading for > 30 min — SAFE STATE"));
        }

        if (!safe) {
            g_safeState = true;  // latch — never cleared automatically
            Serial.println(F("[DIAG] SAFE STATE ACTIVE — all actuators disabled"));
        }
    }
}
