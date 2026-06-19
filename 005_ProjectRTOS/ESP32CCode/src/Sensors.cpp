/*
 * Sensors.cpp — FreeRTOS sensor tasks
 *
 * This file contains two FreeRTOS tasks, each woken by a notification from
 * TaskTimeScheduler every 250 ms × their slot offset:
 *
 *   TaskHeaterTemp — advances the boiler water temperature simulation (HeaterModel)
 *   TaskDHT        — reads the physical DHT11 sensor for ambient humidity and temperature;
 *                    skipped when g_useCloudWeather is true (real Open-Meteo data in use)
 *
 * Health bit assignments:
 *   bit 2 — HeaterTemp (1 << 2)
 *   bit 3 — DHT        (1 << 3)
 *
 * Note: g_homeTemp is written directly by TaskXiaomiBLE (Core 0) — no sensor task needed.
 */

#include "ProjectHeater.h"
#include "Sensors.h"

// FreeRTOS task — waits for a scheduler notification then calls doHeaterTempRead().
void TaskHeaterTemp(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doHeaterTempRead();
  }
}

// FreeRTOS task — waits for a scheduler notification then calls doDHTRead().
void TaskDHT(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doDHTRead();
  }
}

// Advances the boiler water temperature simulation by one time step and sets health bit 2.
void doHeaterTempRead() {
    updateHeaterModel();
    systemHealth |= (1 << 2);
}

// Reads humidity and temperature from the physical DHT11 sensor and stores results in
// g_dhtHumidity and g_dhtTempC.  If the read returns NaN (sensor error) both values are
// set to 100 as an out-of-range sentinel.  Skipped entirely when g_useCloudWeather is true
// so real Open-Meteo data pushed via MQTT is not overwritten.  Sets health bit 3.
void doDHTRead() {
    // [WEATHER] If Node.js has already pushed real outdoor data from Open-Meteo,
    // skip the physical DHT read so it doesn't overwrite g_dhtTempC / g_dhtHumidity.
    if (g_useCloudWeather) {
        systemHealth |= (1 << 3);
        return;
    }

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
        g_dhtHumidity = 100;
        g_dhtTempC = 100;
    } else {
        g_dhtHumidity = h;
        g_dhtTempC = t;
    }
    systemHealth |= (1 << 3);
}
