#include "MicRead.h"

void doMicRead() {
  int sensorValue = analogRead(micPin);
  g_micAdc = sensorValue;
  D_PRINT(F("Microphone: "));
  D_PRINTLN(sensorValue);
}