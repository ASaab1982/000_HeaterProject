#include "ProjectHeater.h"
#include "HeaterModel.h"

// [HEATER MODEL] When heater is ON, boiler water rises at 1°C per 20s
// regardless of outdoor temperature
static const float HEAT_RATE_C_PER_S = 1.0f / 20.0f;

// [HEATER MODEL] Physical maximum for boiler water temperature
static const float HEATER_MAX_TEMP = 90.0f;

// [HEATER MODEL] Cooling rate interpolation endpoints:
// At -5°C outdoor → 1°C per 5 min (fastest cooling)
// At +30°C outdoor → 1°C per 10 min (slowest cooling)
// Outside this range the rate is clamped — no extrapolation.
static const float OUTDOOR_COLD   = -5.0f;
static const float OUTDOOR_WARM   =  30.0f;
static const float RATE_AT_COLD   = 1.0f / 600.0f;  // °C/s at -5°C  (1°C per 10 min)
static const float RATE_AT_WARM   = 1.0f / 1200.0f; // °C/s at 30°C  (1°C per 20 min)

void updateHeaterModel() {
    static unsigned long lastCallMs = 0;

    unsigned long now = millis();
    // [HEATER MODEL] First call only initialises the timer — no temperature change yet
    if (lastCallMs == 0) {
        lastCallMs = now;
        return;
    }

    float deltaS = (now - lastCallMs) / 1000.0f;
    lastCallMs = now;

    if (heaterState) {
        // [HEATER MODEL] Heater ON: rise at 1°C per 20s
        g_heaterWaterTemp += HEAT_RATE_C_PER_S * deltaS;
        if (g_heaterWaterTemp > HEATER_MAX_TEMP) g_heaterWaterTemp = HEATER_MAX_TEMP;

    } else {
        // [HEATER MODEL] Heater OFF: cooling rate is linearly interpolated between
        // RATE_AT_COLD and RATE_AT_WARM based on current outdoor temperature.
        // Outdoor temp is clamped to [OUTDOOR_COLD, OUTDOOR_WARM] — no extrapolation.
        float tOutdoor = g_outdoorTemp;
        if (tOutdoor < OUTDOOR_COLD) tOutdoor = OUTDOOR_COLD;
        if (tOutdoor > OUTDOOR_WARM) tOutdoor = OUTDOOR_WARM;

        // t = 0.0 at -5°C (fastest cooling), t = 1.0 at 30°C (slowest cooling)
        float t = (tOutdoor - OUTDOOR_COLD) / (OUTDOOR_WARM - OUTDOOR_COLD);
        float coolingRate = RATE_AT_COLD + t * (RATE_AT_WARM - RATE_AT_COLD);

        g_heaterWaterTemp -= coolingRate * deltaS;

        // [HEATER MODEL] Boiler water cannot drop below outdoor temperature
        if (g_heaterWaterTemp < g_outdoorTemp) g_heaterWaterTemp = g_outdoorTemp;
    }
}
