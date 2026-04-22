
#include "ProjectHeater.h"
#include "Sensors.h"


 void TaskThermistor(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    D_PRINT(millis()); // Show exactly when this happened
    doThermistorRead();  }
}

 void TaskDHT(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    D_PRINT(millis()); // Show exactly when this happened
    doDHTRead();
  }
}


void doThermistorRead() {
    int16_t adcVal = analogRead(tempPin);
    int16_t tempC10 = map(adcVal, 350, 650, 450, 150); // wer are not using the log funcion to reduce RAM size
    D_PRINT(F(" :  Temp Thermistor : "));
    D_PRINT(tempC10 / 10); D_PRINT(F("."));
    D_PRINT(tempC10 % 10); D_PRINTLN(F(" C"));
    g_thermistorTempC = tempC10 / 10.0f;
      systemHealth |= (1 << 2); // Health bit for mic read is OK

}

void doDHTRead() {
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