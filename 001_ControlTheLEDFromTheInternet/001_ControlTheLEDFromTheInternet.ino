#include <WiFiS3.h>
/*
 char* ssid     = "OnePlus 10T 5G 8bdd"; //Enter the router name 
 char* password = "aavj4755"; //Enter the router password  
*/

  char* ssid     = "Sunrise_2.4GHz_3C4FAC"; //Enter the router name 
 char* password = "Mp723phn3frd"; //Enter the router password  

// Replace with your laptop IP from ipconfig, no http://, no port
const char* server = "192.168.1.156"; // <-- CHANGE THIS

const int serverPort = 3000; // same as server.js
const int LED_PIN = LED_BUILTIN;

WiFiClient client;

unsigned long lastPoll = 0;
const unsigned long pollInterval = 2000; // ms

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Connected to WiFi");
  Serial.print("Arduino IP: ");
  Serial.println(WiFi.localIP());
}

String readResponse(WiFiClient& c) {
  String response = "";
  unsigned long start = millis();
  while (millis() - start < 2000) { // 2s timeout
    while (c.available()) {
      char ch = c.read();
      response += ch;
    }
    if (!c.connected()) break;
  }
  return response;
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);
  while (!Serial) { }

  connectToWiFi();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectToWiFi();
  }

  unsigned long now = millis();
  if (now - lastPoll >= pollInterval) {
    lastPoll = now;

    if (client.connect(server, serverPort)) {
      // Send HTTP GET /api/led
      client.print("GET /api/led HTTP/1.1\r\n");
      client.print("Host: ");
      client.print(server);
      client.print("\r\n");
      client.print("Connection: close\r\n");
      client.print("\r\n");

      String resp = readResponse(client);
      client.stop();

      // Debug full response
      Serial.println("---- Response ----");
      Serial.println(resp);
      Serial.println("------------------");

      // Very simple "parsing": just search for "state":"on"
      if (resp.indexOf("\"state\":\"on\"") >= 0) {
        digitalWrite(LED_PIN, HIGH);
        Serial.println("LED -> ON");
      } else if (resp.indexOf("\"state\":\"off\"") >= 0) {
        digitalWrite(LED_PIN, LOW);
        Serial.println("LED -> OFF");
      } else {
        Serial.println("Could not find state in response");
      }
    } else {
      Serial.println("Connection to server failed");
    }
  }
}