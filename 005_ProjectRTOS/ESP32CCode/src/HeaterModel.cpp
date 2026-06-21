/*
 * HeaterModel.cpp — Boiler water temperature source selection
 *
 * Two modes selected at runtime by g_usePhysicalModel:
 *
 *   false (Option 1 — Sensor mode):
 *       g_heaterWaterTemp = mean of g_boiler1Temp / g_boiler2Temp each cycle.
 *       If both NTC sensors fail → 100 °C fault value so HeaterActuator shuts off.
 *
 *   true (Option 2 — Physics model):
 *       On the first valid NTC reading the model is seeded from the sensor mean.
 *       Subsequent calls advance the model by the real elapsed time (millis delta):
 *         Heater ON  → +HEAT_RATE °C/s, capped at BOILER_MAX
 *         Heater OFF → −coolingRate(g_outdoorTemp) °C/s, floored at g_outdoorTemp
 *       Cooling rate is linearly interpolated between RATE_AT_COLD (at −5 °C outdoor)
 *       and RATE_AT_WARM (at 30 °C outdoor) — same formula as test_heatermodel.js.
 *       Switching back to Option 1 resets modelInitialized so the model re-seeds
 *       correctly if Option 2 is selected again later.
 */

#include "ProjectHeater.h"
#include "HeaterModel.h"
#include <math.h>

// Physics constants — must match test_heatermodel.js
static constexpr float BOILER_MAX   = 90.0f;
static constexpr float HEAT_RATE    = 1.0f / 20.0f;    // °C/s when heater ON
static constexpr float OUTDOOR_COLD = -5.0f;            // lower interpolation bound
static constexpr float OUTDOOR_WARM = 30.0f;            // upper interpolation bound
static constexpr float RATE_AT_COLD = 1.0f / 600.0f;   // °C/s at -5°C outdoor
static constexpr float RATE_AT_WARM = 1.0f / 1200.0f;  // °C/s at 30°C outdoor

static bool     modelInitialized = false;
static float    modelBoilerTemp  = 0.0f;
static uint32_t lastCallMs       = 0;

static float getCoolingRate(float outdoorTemp) {
    float clamped = constrain(outdoorTemp, OUTDOOR_COLD, OUTDOOR_WARM);
    float t = (clamped - OUTDOOR_COLD) / (OUTDOOR_WARM - OUTDOOR_COLD);
    return RATE_AT_COLD + t * (RATE_AT_WARM - RATE_AT_COLD);
}

static float sensorMean() {
    bool b1ok = !isnan(g_boiler1Temp);
    bool b2ok = !isnan(g_boiler2Temp);
    if (b1ok && b2ok) return (g_boiler1Temp + g_boiler2Temp) / 2.0f;
    if (b1ok)         return g_boiler1Temp;
    if (b2ok)         return g_boiler2Temp;
    return NAN;
}

void updateHeaterModel() {
    float mean = sensorMean();

    if (!g_usePhysicalModel) {
        // Option 1: direct sensor value
        if (!isnan(mean)) g_heaterWaterTemp = mean;
        else              g_heaterWaterTemp = 100.0f; // both sensors failed → fault
        modelInitialized = false; // re-seed if switched to model later
        return;
    }

    // Option 2: physics model — seed from sensors on first valid read
    if (!modelInitialized) {
        if (isnan(mean)) return; // wait for first valid sensor reading
        modelBoilerTemp  = mean;
        modelInitialized = true;
        lastCallMs       = millis();
        return;
    }

    uint32_t now = millis();
    float dt = (now - lastCallMs) / 1000.0f;
    lastCallMs = now;

    if (heaterState) {
        modelBoilerTemp += HEAT_RATE * dt;
        if (modelBoilerTemp > BOILER_MAX) modelBoilerTemp = BOILER_MAX;
    } else {
        float coolingRate = getCoolingRate(g_outdoorTemp);
        modelBoilerTemp  -= coolingRate * dt;
        if (modelBoilerTemp < g_outdoorTemp) modelBoilerTemp = g_outdoorTemp;
    }

    g_heaterWaterTemp = modelBoilerTemp;
    g_boiler1Temp     = modelBoilerTemp;
    g_boiler2Temp     = modelBoilerTemp;
}
