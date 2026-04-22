#include "ServoControl.h"


 void TaskServo(void* pv) {
  (void)pv;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    doServoSequence();
    D_PRINT(millis()); // Show exactly when this happened
    D_PRINTLN(F(" : Servo moved"));
  }
}


void doServoSequence() {
  /*
  // --- RANDOM WATCHDOG HANG SIMULATION ---
  // Schedule a hang to occur 15s (+/- 3s) after the code starts
  static unsigned long nextHangTime = millis() + random(12000, 18000);

  if (millis() > nextHangTime) {
    D_PRINTLN(F("!!! SIMULATING RANDOM SERVO TASK HANG !!!"));
    while(true) { vTaskDelay(pdMS_TO_TICKS(100)); }
  }
  // ---------------------------------------
  */

  // Generate a random position
  int posservo = random(0, 181);
  
  // Constrain it just to be safe (Servo hardware limit)
  int poslimit = constrain(posservo, 0, 180);
  
  // Move the servo
  myservo.write(poslimit);
  
  // Log the action to the Serial Monitor
  g_servoPositionDeg = poslimit;
  systemHealth |= (1 << 5); // Health bit for mic read is OK

}