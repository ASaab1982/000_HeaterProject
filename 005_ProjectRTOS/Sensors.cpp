#include "Sensors.h"
#include <Arduino_FreeRTOS.h>
#include "ProjectHeater.h"

void doThermistorRead() {
    int16_t adcVal = analogRead(tempPin);
    int16_t tempC10 = map(adcVal, 350, 650, 450, 150); // wer are not using the log funcion to reduce RAM size
    Serial.print(F("Temp: "));
    Serial.print(tempC10 / 10); Serial.print(F("."));
    Serial.print(tempC10 % 10); Serial.println(F(" C"));
    g_thermistorTempC = tempC10 / 10.0f;
}

void doDHTRead() {
    // No more vTaskDelay here!
    
    // Read the values immediately
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
        Serial.println(F("Error: DHT sensor failed!"));
        g_dhtHumidity = 0; // Update global variables
        g_dhtTempC = 0;
        Serial.print(F("Hum: ")); Serial.print(g_dhtHumidity);
        Serial.print(F("%, Temp: ")); Serial.print(g_dhtTempC);
        Serial.println(F("C"));

    } else {
        g_dhtHumidity = h; // Update global variables
        g_dhtTempC = t;
        Serial.print(F("Hum: ")); Serial.print(g_dhtHumidity);
        Serial.print(F("%, Temp: ")); Serial.print(g_dhtTempC);
        Serial.println(F("C"));
    }
    // The task will now finish in milliseconds and wait 10s for the next signal
}