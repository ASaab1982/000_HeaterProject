#include "ServoControl.h"
#include <Arduino_FreeRTOS.h>

void doServoSequence() {
  // Generate a random position
  int posservo = random(0, 181);
  
  // Constrain it just to be safe (Servo hardware limit)
  int poslimit = constrain(posservo, 0, 180);
  
  // Move the servo
  myservo.write(poslimit);
  
  // Log the action to the Serial Monitor
  Serial.print(F("Servo moved to: "));
  Serial.println(poslimit);
}