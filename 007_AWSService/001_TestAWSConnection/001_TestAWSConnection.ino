#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include <SoftwareATSE.h>
#include "Secrets.h"

// ========== STEP 1: ADD YOUR DER CONVERTED CERTIFICATE ==========
// Convert your AWS certificate and private key to DER format using:
// openssl x509 -in your-thing.cert.pem -out certificate.der -outform DER
// openssl ec -in your-thing.private.key -out private.der -outform DER
//
// Then convert those DER files to byte arrays using a tool like:
// https://tomeko.net/online_tools/hex_to_file.php?lang=en
// (Upload the .der file and copy the "C array" output)


// ========== CONFIGURATION ==========
const char* ssid     = WIFI_SSID;
const char* pass     = WIFI_PASS;
const char* broker   = SECRET_BROKER;

WiFiSSLClient sslClient;
MqttClient    mqttClient(sslClient);

// DO NOT declare SATSE here - the SoftwareATSE library already creates a global instance
// The library provides 'extern SoftwareATSEClass SATSE;' so we just use it directly

// AWS Thing Name - MUST MATCH EXACTLY what's in AWS Console
const char* clientId = "Heater_Arduino_Main";

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("=== AWS IoT Uno R4 Connection ===");
  Serial.println("Attempting ECC Key Load via SoftwareATSE...");

  // 1. Initialize the SoftwareATSE engine (using library's global instance)
  if (!SATSE.begin()) {
    Serial.println("ERROR: Failed to initialize SoftwareATSE!");
    while (1);
  }
  Serial.println("SoftwareATSE initialized.");

  // 2. Write the private key to a slot (Slot 1)
  if (!SATSE.writeSlot(1, private_key_der, sizeof(private_key_der))) {
    Serial.println("ERROR: Failed to write private key to slot 1!");
    Serial.println("Make sure private_key_der is a valid EC PRIVATE KEY in DER format.");
    while (1);
  }
  Serial.println("Private key written to slot 1.");

  // 3. Write the certificate to a slot (Slot 2)
  if (!SATSE.writeSlot(2, certificate_der, sizeof(certificate_der))) {
    Serial.println("ERROR: Failed to write certificate to slot 2!");
    while (1);
  }
  Serial.println("Certificate written to slot 2.");

  // 4. Start WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // 5. Sync Time (Required for TLS certificate validation)
  Serial.print("Syncing time with NTP...");
  unsigned long epochTime = WiFi.getTime();
  int retries = 0;
  while (epochTime == 0 && retries < 30) {
    delay(1000);
    Serial.print(".");
    epochTime = WiFi.getTime();
    retries++;
  }
  
  if (epochTime == 0) {
    Serial.println("\nERROR: Failed to sync time! TLS will fail.");
    Serial.println("Check that your WiFi allows NTP traffic (port 123).");
    while (1);
  }
  Serial.println("\nTime synced successfully!");
  Serial.print("Current epoch time: ");
  Serial.println(epochTime);

  // 6. Configure SSL Client
  sslClient.setCACert(SECRET_CA);
  Serial.println("Root CA configured.");
  
  // Configure ECC slot for mTLS (void function - no return value to check)
  sslClient.setEccSlot(2, certificate_der, sizeof(certificate_der));
  Serial.println("ECC slot configured for SSL client.");

  Serial.println("\nAttempting secure connection to AWS...");
}

void loop() {
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.poll();
  delay(100);  // Small delay to prevent tight loop
}

void connectMQTT() {
  Serial.print("\nConnecting to AWS broker: ");
  Serial.println(broker);

  mqttClient.setId(clientId);
  Serial.print("Client ID: ");
  Serial.println(clientId);

  // Attempt connection on port 8883 (AWS IoT Core MQTT)
  if (!mqttClient.connect(broker, 8883)) {
    int err = mqttClient.connectError();
    Serial.print("MQTT connection failed! Error code: ");
    Serial.println(err);
    
    // Common error codes
    if (err == -2) {
      Serial.println("\n=== DEBUG: Connection Refused ===");
      Serial.println("Possible causes:");
      Serial.println("1. setEccSlot() may not work - R4 mTLS support is experimental");
      Serial.println("2. Certificate not ACTIVE in AWS Console");
      Serial.println("3. Policy not attached to certificate");
      Serial.println("4. Policy doesn't allow 'iot:Connect' for this Client ID");
      Serial.println("5. Client ID doesn't match Thing name in AWS");
      Serial.println("6. Time sync failed - certificate appears expired/invalid");
      Serial.println("7. Private key and certificate don't match");
    } else if (err == -3) {
      Serial.println("\n=== DEBUG: TLS handshake failed ===");
      Serial.println("Certificate rejected by AWS. Check that:");
      Serial.println("1. Your certificate_der array is correct");
      Serial.println("2. Your private_key_der array is correct");
      Serial.println("3. The certificate is ACTIVE in AWS Console");
      Serial.println("4. The certificate matches the private key");
    } else if (err == -4) {
      Serial.println("\n=== DEBUG: Connection timeout ===");
      Serial.println("Check network connectivity and firewall settings.");
    }
    
    delay(5000);
  } else {
    Serial.println("✅ SUCCESS! Connected to AWS IoT Core.");
    
    // Subscribe to a topic
    if (mqttClient.subscribe("arduino/incoming")) {
      Serial.println("Subscribed to arduino/incoming");
    } else {
      Serial.println("Subscribe failed");
    }
    
    // Publish a test message
    Serial.println("Publishing test message to arduino/outgoing...");
    mqttClient.beginMessage("arduino/outgoing");
    mqttClient.print("Hello from Arduino R4 WiFi!");
    mqttClient.endMessage();
    Serial.println("Test message published.");
  }
}