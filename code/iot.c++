#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <NewPing.h>

#define TRIGGER_PIN D5
#define ECHO_PIN D6
#define MOTOR_PIN D3
#define FLOW_SENSOR_PIN D7

const char* ssid = "Guna"; // Your WiFi SSID
const char* password = "1234567890"; // Your WiFi password

const char* thingspeakServer = "api.thingspeak.com"; // ThingSpeak server
const char* apiKey = "BO3QWO7FB2FS2QE6"; // Your ThingSpeak API key

WiFiClient client;
ESP8266WebServer httpServer(80);

NewPing sonar(TRIGGER_PIN, ECHO_PIN);

volatile int flowPulseCount = 0; // Flow sensor pulse count
const float calibrationFactor = 450.0; // Calibration factor for the YF-S201 flow sensor (pulses per liter)
unsigned long oldTime = 0; // To keep track of time for flow rate calculation
float flowRate = 0.0; // Flow rate in liters per minute
float totalVolume = 0.0; // Total volume of water that has passed through the sensor

// Interrupt service routine for flow sensor
void ICACHE_RAM_ATTR flowPulseISR() {
  flowPulseCount++;
}

void setup() {
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  
  Serial.begin(9600);
  delay(10);

  Serial.println();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Attach interrupt for flow sensor
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowPulseISR, FALLING);
  
  // Start the HTTP server
  httpServer.begin();
  Serial.println("HTTP server started");
}

void loop() {
  // Handle client requests
  httpServer.handleClient();

  delay(1000); // Update every second
  digitalWrite(MOTOR_PIN, HIGH);

  int distance = sonar.ping_cm(); // Get distance from ultrasonic sensor in cm
  Serial.print("Distance: ");
  Serial.println(distance);
  if (distance <=4 ) { // Adjust this value based on your tank's empty level
    digitalWrite(MOTOR_PIN, LOW); // Turn off motor when water level is low
  }

  unsigned long currentTime = millis();
  unsigned long elapsedTime = currentTime - oldTime;

  // Calculate flow rate and total volume
  if (elapsedTime > 1000) { // Update every second
    flowRate = ((flowPulseCount / calibrationFactor) / (elapsedTime / 1000.0)) * 60.0; // Flow rate in liters per minute
    totalVolume += (flowPulseCount / calibrationFactor); // Total volume in liters

    float totalVolumeInMilliLiters = totalVolume * 1000.0; // Convert total volume to milliliters

    Serial.print("Flow Rate: ");
    Serial.print(flowRate);
    Serial.println(" L/min");
    Serial.print("Total Volume: ");
    Serial.print(totalVolume);
    Serial.println(" L");
    Serial.print("Total Volume: ");
    Serial.print(totalVolumeInMilliLiters);
    Serial.println(" mL");

    flowPulseCount = 0; // Reset pulse count for next calculation
    oldTime = currentTime; // Update the old time

    // Data transmission to ThingSpeak server
    if (client.connect(thingspeakServer, 80)) {
      String postStr = "api_key=" + String(apiKey) + "&field1=" + String(distance) + "&field2=" + String(flowRate) + "&field3=" + String(totalVolume);
      client.println("POST /update HTTP/1.1");
      client.println("Host: api.thingspeak.com");
      client.println("Connection: close");
      client.println("Content-Type: application/x-www-form-urlencoded");
      client.print("Content-Length: ");
      client.println(postStr.length());
      client.println();
      client.print(postStr);
      delay(500); // Wait for server response
      client.stop();
    }
  }

  // Check if WiFi is still connected, if not, reconnect
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost connection. Reconnecting...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi reconnected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
}