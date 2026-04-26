#include "DCMotor.h"


 void TaskDC(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY); // portMAX_DELAY tells the task to enter a "Blocked" state indefinitely. 
    // It will not move to the next line of code until it receives a specific notification signal from another task or an interrupt.
    driveDCMotor(dir, spd);
    D_PRINT(millis()); // Show exactly when this happened
    D_PRINTLN(F(" : DC moved"));
  }
}

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
  systemHealth |= (1 << 0); // Health bit for Thermistor is OK

}
