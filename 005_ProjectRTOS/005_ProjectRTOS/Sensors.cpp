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
    // No more vTaskDelay here!
    
    // Read the values immediately
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
        Serial.println(F("Error: DHT sensor failed!"));
    } else {
        dht_h = h; // Update global variables
        dht_t = t;
        Serial.print(F("Hum: ")); Serial.print(dht_h);
        Serial.print(F("%, Temp: ")); Serial.print(dht_t);
        Serial.println(F("C"));
    }
    // The task will now finish in milliseconds and wait 10s for the next signal
}