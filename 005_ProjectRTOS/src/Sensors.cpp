
#include "ProjectHeater.h"
#include "Sensors.h"


 void TaskHomeTemp(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    D_PRINT(millis());
    doHomeTempRead();  }
}


 void TaskBoilerTemp(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    D_PRINT(millis());
    doBoilerTempRead();  }
}

 void TaskDHT(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    D_PRINT(millis()); // Show exactly when this happened
    doDHTRead();
  }
}

void doBoilerTempRead() {
    updateHeaterModel();
    D_PRINT(F(" : Boiler Temp: ")); D_PRINTLN(g_boilerWaterTemp);
    systemHealth |= (1 << 2);
}

void doHomeTempRead() {
    updateHomeTempModel();
    D_PRINT(F(" : Home Temp: ")); D_PRINTLN(g_homeTemp);
    systemHealth |= (1 << 1);
}

void doDHTRead() {
    // [WEATHER] If Node.js has already pushed real outdoor data from Open-Meteo,
    // skip the physical DHT read so it doesn't overwrite g_dhtTempC / g_dhtHumidity.
    // We still mark the health bit so the watchdog doesn't flag this task as dead.
    if (g_useCloudWeather) {
        systemHealth |= (1 << 3);
        return;
    }

    // No more vTaskDelay here!

    // Read the values immediately
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
        D_PRINT(F("   : Error: DHT sensor failed!   "));
        g_dhtHumidity = 100; // Update global variables
        g_dhtTempC = 100;
        D_PRINT(F(" DHT Hum Sensor: ")); D_PRINT(g_dhtHumidity);
        D_PRINT(F("%, DHT Temp Sensor: ")); D_PRINT(g_dhtTempC);
        D_PRINTLN(F("C"));

    } else {
        g_dhtHumidity = h; // Update global variables
        g_dhtTempC = t;
        D_PRINT(F(" :  DHT Hum Sensor: ")); D_PRINT(g_dhtHumidity);
        D_PRINT(F("%, DHT Temp Sensor: ")); D_PRINT(g_dhtTempC);
        D_PRINTLN(F("C"));
    }
    // The task will now finish in milliseconds and wait 10s for the next signal
    systemHealth |= (1 << 3); // Health bit for mic read is OK

}