#include "Sensors.h"
#include <Arduino_FreeRTOS.h>

void doThermistorRead() {
    int16_t adcVal = analogRead(tempPin);
    int16_t tempC10 = map(adcVal, 350, 650, 450, 150);
    Serial.print(F("Temp: "));
    Serial.print(tempC10 / 10); Serial.print(F("."));
    Serial.print(tempC10 % 10); Serial.println(F(" C"));
}

void doDHTRead() {
    vTaskDelay(pdMS_TO_TICKS(2000));
    dht_h = dht.readTemperature(); // Example update
    // ... rest of your DHT logic
}