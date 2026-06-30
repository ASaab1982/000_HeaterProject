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

void TaskDiagFunctional(void* pv) {
    (void)pv;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        bool safe = true;

        // --- Diagnostic conditions will be added here ---

        g_safeState = !safe;

        if (g_safeState) {
            Serial.println(F("[DIAG] SAFE STATE ACTIVE — all actuators disabled"));
        }
    }
}
