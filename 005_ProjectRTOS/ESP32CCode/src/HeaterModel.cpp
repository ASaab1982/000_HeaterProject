#include "ProjectHeater.h"
#include "HeaterModel.h"

// Sets g_heaterWaterTemp to the mean of the two boiler NTC sensors.
// If one sensor is NAN (open/short circuit), uses the other alone.
// If both are NAN, g_heaterWaterTemp is left unchanged.
void updateHeaterModel() {
    bool b1ok = !isnan(g_boiler1Temp);
    bool b2ok = !isnan(g_boiler2Temp);

    if (b1ok && b2ok)       g_heaterWaterTemp = (g_boiler1Temp + g_boiler2Temp) / 2.0f;
    else if (b1ok)          g_heaterWaterTemp = g_boiler1Temp;
    else if (b2ok)          g_heaterWaterTemp = g_boiler2Temp;
    else                    g_heaterWaterTemp = 100.0f;  // both failed → fault value, heater will shut off
}
