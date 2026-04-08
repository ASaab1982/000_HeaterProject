#ifndef SENSORS_H
#define SENSORS_H
#include <Arduino.h>
#include <DHT.h>

extern DHT dht;
extern const uint8_t tempPin;
extern float dht_h, dht_t;

void doThermistorRead();
void doDHTRead();

#endif