#include "MicRead.h"

void doMicRead() {
  int sensorValue = analogRead(micPin);
  g_micAdc = sensorValue;
  Serial.print(F("Microphone: "));
  Serial.println(sensorValue);
}