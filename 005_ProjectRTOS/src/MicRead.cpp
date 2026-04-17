#include "MicRead.h"

void doMicRead() {
  int sensorValue = analogRead(micPin);
  g_micAdc = sensorValue;
  D_PRINT(F("Microphone: "));
  D_PRINTLN(sensorValue);
  systemHealth |= (1 << 1); // Health bit for mic read is OK

}