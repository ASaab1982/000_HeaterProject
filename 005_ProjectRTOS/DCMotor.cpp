#include "DCMotor.h"


void driveDCMotor(bool dir, int spd) {
  if (dir) {
    digitalWrite(in1Pin, HIGH);
    digitalWrite(in2Pin, LOW);
  } else {
    digitalWrite(in1Pin, LOW);
    digitalWrite(in2Pin, HIGH);
  }

  analogWrite(enablePin, map(spd, 0,512, 0, 255)); // the PWM value is a fraction of the 255 (maximum register value)
  g_dcMotorSpeed = map(spd, 0,512, 0, 100); // store the PWM value 

}
