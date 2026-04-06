#include <Servo.h>
#include <WiFiS3.h>
#include <Adafruit_Sensor.h> 
#include <DHT.h> 
#include <DHT_U.h>
#include "StepperControl.h"
#include "DCMotorControl.h"
#include "ServoControl.h"
#include "SensorReadings.h"

#define Voltage_Offset -0.34 // Measuring the offset voltage of the 5V power pin can 
#define DHTPIN 2     // Digital pin connected to the DHT sensor  
#define DHTTYPE    DHT11     // DHT 11 
DHT_Unified dht(DHTPIN, DHTTYPE); 
 
uint32_t delayMS; 
int outPorts[] = {13, 12, 11, 10}; 
int in1Pin = 8;      // Define L293D channel 1 pin 
int in2Pin = 7;       // Define L293D channel 2 pin 
int enable1Pin = 6;  // Define L293D enable 1 pin 
int posservo;                // variable to store the servo position 
int servoPin = 5;           // define the pin of servo signal line 
int touchPin = 3;           // touch pin
const int tempPin = A0;
const int micPin = A1;// Define the pin connected to the microphone
volatile bool touched = false;

// Replace with your network credentials
 char* ssid     = "Sunrise_2.4GHz_3C4FAC"; //Enter the router name 
 char* password = "Mp723phn3frd"; //Enter the router password  


boolean rotationDir = 0;  // Define a variable to save the motor's rotation direction, true and false are represented by positive rotation and reverse rotation. 
int rotationSpeed = 256;    // Define a variable to save the motor rotation speed 
Servo myservo;              // create servo object to control a servo //
WiFiServer server(80);
 
void setup() { 
    Serial.begin(115200);
    dht.begin(); 
  //  Initialize the step mottor pin
  for (int i = 0; i < 4; i++) { 
    pinMode(outPorts[i], OUTPUT); 
  } 
  // Initialize the DC motor pin into an output mode: 
  pinMode(in1Pin, OUTPUT); 
  pinMode(in2Pin, OUTPUT); 
  pinMode(enable1Pin, OUTPUT); 
   // Explicitly set A0 as an input (optional but recommended)
  pinMode(tempPin, INPUT); 
  // Microphone Analog input A1 
  pinMode(micPin, INPUT); 
   // set push touch pin into input mode 
  pinMode(touchPin, INPUT); 
  // Attach the interrupt to the touch sensor
  // digitalPinToInterrupt(3) maps the physical pin to the interrupt number
  // RISING means the trigger happens when the signal goes from LOW to HIGH
  attachInterrupt(digitalPinToInterrupt(touchPin), handleTouchInterrupt, RISING); 


  // attaches the servo on servoPin to the servo object 
  myservo.attach(servoPin); 

  //Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  //1. Wait for the physical connection to the radio
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  // 2. Wait specifically for a valid IP address (not 0.0.0.0)
  Serial.println("\nWiFi Connected! Waiting for DHCP address...");
  while (WiFi.localIP() == IPAddress(0, 0, 0, 0)) {
    delay(10);
    Serial.print(".");
  }
  // Print local IP address and start web server
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
  
} 
 
void loop() { 
  // Rotate a full turn 
  moveSteps(outPorts, false, 2 * 64, 20); 
  delay(1000); 
  // Rotate a full turn towards another direction 
  moveSteps(outPorts, true, 2 * 64, 20); 
  delay(1000); 
  driveMotor(in1Pin, in2Pin, enable1Pin, rotationDir,map(rotationSpeed, 0, 512, 0, 255)); 
  rotationDir = !rotationDir;
  delay(1000);
  driveMotor(in1Pin, in2Pin, enable1Pin, rotationDir,map(0, 0, 512, 0, 255)); 
  delay(500);
  driveMotor(in1Pin, in2Pin, enable1Pin, rotationDir,map(rotationSpeed, 0, 512, 0, 255)); 
  rotationDir = !rotationDir;
  delay(1000);
  driveMotor(in1Pin, in2Pin, enable1Pin, rotationDir,map(0, 0, 512, 0, 255)); 
  delay(1000);
  posservo= random(0, 181);
  moveServo(myservo, posservo);
  readAndDisplayTemperature(tempPin);
  delay(50);
  readDHTSensor(dht);
  delay(50);
  readMicrophone(micPin);
  if (touched) {
    displayDetectionTouch();
    
    // Reset the flag so we don't print repeatedly for one touch
    touched = false; 
  }
} 

// The Interrupt Service Routine (ISR) - must be fast!
void handleTouchInterrupt() {
  touched = true; 
}