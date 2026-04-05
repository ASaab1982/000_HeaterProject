#include <Servo.h>
#include <WiFiS3.h>
#include <Adafruit_Sensor.h> 
#include <DHT.h> 
#include <DHT_U.h>

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
const int tempPin = A0;
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
  pinMode(tempPin, INPUT);   // Explicitly set A0 as an input (optional but recommended)

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
  moveSteps(false, 2 * 64, 20); 
  delay(1000); 
  // Rotate a full turn towards another direction 
  moveSteps(true, 2 * 64, 20); 
  delay(1000); 
  driveMotor(rotationDir,map(rotationSpeed, 0, 512, 0, 255)); 
  rotationDir = !rotationDir;
  delay(1000);
  driveMotor(rotationDir,map(0, 0, 512, 0, 255)); 
  delay(500);
  driveMotor(rotationDir,map(rotationSpeed, 0, 512, 0, 255)); 
  rotationDir = !rotationDir;
  delay(1000);
  driveMotor(rotationDir,map(0, 0, 512, 0, 255)); 
  delay(1000);
  posservo= random(0, 181);
  moveServo(posservo);
  readAndDisplayTemperature();
  readDHTSensor();
} 


// from here down we will put all the functions 

void moveSteps(bool dir, int steps, unsigned int ms) { 
  for (int i = 0; i < steps; i++) { 
    moveOneStep(dir); // Rotate a step 
    delay(ms);        // Control the speed 
  } 
      Serial.println(" Stepper moved");
} 


void moveServo(int pos) {
    // On limite la valeur reçue "pos" et on stocke le résultat dans "posservo"
    int poslimit = constrain(pos, 0, 180); 
    
    // On utilise la valeur limitée pour piloter le servo
    myservo.write(poslimit); 
      Serial.println(" Servo moved");

}

void moveOneStep(bool dir) { 
  // Define a variable, use four low bit to indicate the state of port 
  static byte out = 0x01; 
  // Decide the shift direction according to the rotation direction 
  if (dir) {
      out = (out << 1);
      if (out > 0x08) out = 0x01;
  } else {
      out = (out >> 1);
      if (out == 0x00) out = 0x08;
  } 
  // Output singal to each port 
  for (int i = 0; i < 4; i++) {
      digitalWrite(outPorts[i], (out & (0x01 << i)) ? HIGH : LOW); 
  } 
} 

void driveMotor(boolean dir, int spd) { 
  // Control motor rotation direction 
  if (dir) { 
    digitalWrite(in1Pin, HIGH); 
    digitalWrite(in2Pin, LOW); 
  } 
 else { 
    digitalWrite(in1Pin, LOW); 
    digitalWrite(in2Pin, HIGH); 
  } 
  // Control motor rotation speed  
  analogWrite(enable1Pin, constrain(spd, 0, 255)); 
  if (spd >0){
  Serial.println(" DC moved");
  }
} 

void readAndDisplayTemperature() {
  // 1. Read the raw value from Analog Pin A0 (returns 0 to 1023)
  int adcVal = analogRead(A0);

 // Calculate voltage 
  float v = adcVal * 5.0 / 1024; 
  // Calculate resistance value of thermistor 
  float Rt = 10 * v / (5 - v); 
  // Calculate temperature (Kelvin) 
  float tempK = 1 / (log(Rt / 10) / 3950 + 1 / (273.15 + 25)); 
  // Calculate temperature (Celsius) 
  float tempC = tempK - 273.15; 

  Serial.print("Current temperature is: "); 
  Serial.print(tempK); 
  Serial.print(" K, "); 
  Serial.print(tempC); 
  Serial.println(" C");
  Serial.print("Raw ADC: "); Serial.println(adcVal);
}

void readDHTSensor() {
    delay(1000);

  sensors_event_t event;

  dht.humidity().getEvent(&event); 
  if (isnan(event.relative_humidity)) { 
    Serial.println("Error reading humidity!"); 
  } 
  else { 
    Serial.print("Humidity: "); 
    Serial.print(event.relative_humidity); 
    Serial.print("%, "); 
  } 
 
  dht.temperature().getEvent(&event); 
    if (isnan(event.temperature)) { 
    Serial.println("Error reading temperature!"); 
  } 
  else { 
    Serial.print("Temperature: "); 
    Serial.print(event.temperature); 
    Serial.println("℃"); 
  } 
}