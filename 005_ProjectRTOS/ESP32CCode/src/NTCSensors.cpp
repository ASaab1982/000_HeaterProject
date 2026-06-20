#include "NTCSensors.h"
#include "ProjectHeater.h"
#include "HeaterModel.h"
#include <math.h>

static float readNTC(uint8_t pin) {
    int raw = analogRead(pin);
    D_PRINT(F("[NTC raw] GPIO")); D_PRINT(pin); D_PRINT(F(" = ")); D_PRINTLN(raw);
    if (raw <= 0 || raw >= 4095) return NAN;
    float voltage    = raw * (NTC_VCC / NTC_ADC_MAX);
    float resistance = NTC_PULLUP * voltage / (NTC_VCC - voltage);
    float tempK      = 1.0f / (1.0f / NTC_T0_K + (1.0f / NTC_BETA) * logf(resistance / NTC_R0));
    return tempK - 273.15f + NTC_OFFSET;
}

void TaskNTC(void* pv) {
    (void)pv;
    analogReadResolution(12);
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        g_boiler1Temp     = readNTC(NTC_PIN_BOILER1);
        g_boiler2Temp     = readNTC(NTC_PIN_BOILER2);
        g_waterOutletTemp = readNTC(NTC_PIN_OUTLET);
        g_waterInletTemp  = readNTC(NTC_PIN_INLET);
        updateHeaterModel();
        systemHealth |= (1 << 1);
        D_PRINT(F("[NTC] Boiler1: "));   D_PRINT(isnan(g_boiler1Temp)     ? "---" : String(g_boiler1Temp,     1).c_str()); D_PRINT(F(" C  "));
        D_PRINT(F("Boiler2: "));         D_PRINT(isnan(g_boiler2Temp)     ? "---" : String(g_boiler2Temp,     1).c_str()); D_PRINT(F(" C  "));
        D_PRINT(F("Outlet: "));          D_PRINT(isnan(g_waterOutletTemp) ? "---" : String(g_waterOutletTemp, 1).c_str()); D_PRINT(F(" C  "));
        D_PRINT(F("Inlet: "));           D_PRINT(isnan(g_waterInletTemp)  ? "---" : String(g_waterInletTemp,  1).c_str()); D_PRINTLN(F(" C"));
    }
}
