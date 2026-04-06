#ifndef SENSOR_READINGS_H
#define SENSOR_READINGS_H

#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

void readAndDisplayTemperature(uint8_t tempPin);
void readDHTSensor(DHT_Unified &dht);
void readMicrophone(uint8_t micPin);
void displayDetectionTouch();

#endif
