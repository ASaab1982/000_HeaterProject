#include "MicRead.h"

void doMicRead() {
  int sensorValue = analogRead(micPin);
  Serial.print(F("Microphone: "));
  Serial.println(sensorValue);
}