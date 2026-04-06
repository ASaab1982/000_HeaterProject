#include "SensorReadings.h"

#include <math.h>

void readAndDisplayTemperature(uint8_t tempPin) {
  int adcVal = analogRead(tempPin);

  float v = adcVal * 5.0 / 1024;
  float Rt = 10 * v / (5 - v);
  float tempK = 1 / (log(Rt / 10) / 3950 + 1 / (273.15 + 25));
  float tempC = tempK - 273.15;

  Serial.print("Current temperature is: ");
  Serial.print(tempK);
  Serial.print(" K, ");
  Serial.print(tempC);
  Serial.println(" C");
  Serial.print("Raw ADC: ");
  Serial.println(adcVal);
}

void readDHTSensor(DHT_Unified &dht) {
  delay(1000);

  sensors_event_t event;

  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println("Error reading humidity!");
  } else {
    Serial.print("Humidity: ");
    Serial.print(event.relative_humidity);
    Serial.print("%, ");
  }

  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println("Error reading temperature!");
  } else {
    Serial.print("Temperature: ");
    Serial.print(event.temperature);
    Serial.println("℃");
  }
}

void readMicrophone(uint8_t micPin) {
  int sensorValue = analogRead(micPin);
  Serial.print("Microphone: ");
  Serial.println(sensorValue);
}

void displayDetectionTouch() {
  Serial.println("Touch Detected!");
}
