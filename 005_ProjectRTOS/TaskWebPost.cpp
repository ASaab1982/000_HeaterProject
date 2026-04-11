#include "TaskWebPost.h"

void doTaskWebPost() {
    // Wait for the Master Timer to trigger this task
    if (client.connect(serverAddress, serverPort)) {
      // 1. Create the JSON string
      String json = "{";
        json += "\"dhtTempC\":"        + String(g_dhtTempC) + ",";
        json += "\"dhtHumidity\":"     + String(g_dhtHumidity) + ",";
        json += "\"dcMotorSpeed\":"    + String(g_dcMotorSpeed) + ",";
        json += "\"micAdc\":"          + String(g_micAdc) + ",";
        json += "\"thermistorTempC\":" + String(g_thermistorTempC) + ",";
        json += "\"stepperAngleDeg\":" + String(g_stepperAngleDeg) + ",";
        json += "\"servoPositionDeg\":" + String(g_servoPositionDeg) + ",";
        json += "\"touched\":"         + String(touched ? "true" : "false");
        json += "}";

      // 2. Send the HTTP POST Header
      client.println("POST /api/telemetry HTTP/1.1");
      client.println("Host: " + String(serverAddress));
      client.print("Authorization: Bearer "); 
      client.println(apiToken);
      client.println("Content-Type: application/json");
      client.print("Content-Length: ");
      client.println(json.length());
      client.println(); // Extra empty line to separate header from body
      client.print(json); // Send the actual data
      
      // --- ADD THIS TO SEE THE REAL TRUTH ---
      delay(100); // Give server a moment to respond
      if (client.available()) {
        String statusLine = client.readStringUntil('\n');
        Serial.print(F("Server Reality Check: "));
        Serial.println(statusLine); // This will show "HTTP/1.1 401 Unauthorized"
      }
      // --------------------------------------

      client.stop(); // Close connection
      Serial.println(F("Telemetry sent to Server!"));
    } else {
      Serial.println(F("Connection to Server failed"));
  }
}
