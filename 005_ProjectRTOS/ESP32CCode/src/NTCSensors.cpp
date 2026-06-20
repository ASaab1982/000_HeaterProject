#include "NTCSensors.h"
#include "ProjectHeater.h"
#include <math.h>

static float readNTC(uint8_t pin) {
    int raw = analogRead(pin);
    if (raw <= 0 || raw >= 4095) return NAN;
    float voltage    = raw * (NTC_VCC / NTC_ADC_MAX);
    float resistance = NTC_PULLUP * voltage / (NTC_VCC - voltage);
    float tempK      = 1.0f / (1.0f / NTC_T0_K + (1.0f / NTC_BETA) * logf(resistance / NTC_R0));
    return tempK - 273.15f;
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
        systemHealth |= (1 << 1);
    }
}
