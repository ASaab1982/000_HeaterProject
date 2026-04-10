#ifndef SENSORS_H
#define SENSORS_H
#include <Arduino.h>
#include <DHT.h>

extern const uint8_t tempPin;

void doThermistorRead();
void doDHTRead();

#endif