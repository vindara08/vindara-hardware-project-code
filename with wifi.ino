#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <EEPROM.h>

// Pin definitions
#define DHTPIN D4 // DHT11 sensor pin
#define DHTTYPE DHT11 // DHT sensor type
#define MOISTURE_PIN A0 // Soil moisture sensor pin
#define IR_LEFT_PIN D1 // Left IR sensor pin
#define IR_RIGHT_PIN D2 // Right IR sensor pin
#define RELAY_PIN D5// Relay pin for irrigation control
#define wifinot D7
#define wifiyes D6


// Initialize DHT sensor
DHT dht(DHTPIN, DHTTYPE);

// Initialize web server on port 80
ESP8266WebServer server(80);

// Relay state
bool relayState = false;

// EEPROM addresses for storing Wi-Fi credentials
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 32
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64

// Buffer for Wi-Fi credentials
char ssid[MAX_SSID_LEN];
char password[MAX_PASS_LEN];

void setup() {
  // Start serial communication
  Serial.begin(115200);
  Serial.setTimeout(10000); // Set timeout for serial input (10 seconds)

  // Initialize pins
  pinMode(IR_LEFT_PIN, INPUT);
  pinMode(wifiyes, OUTPUT);
  pinMode(wifinot, OUTPUT);
  pinMode(IR_RIGHT_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH); // Relay off initially (active LOW)
  digitalWrite(wifiyes, HIGH);
  digitalWrite(wifinot, HIGH);

  
  // Initialize DHT sensor
  dht.begin();

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);

  // Read stored Wi-Fi credentials
  readCredentials();

  // Attempt to connect with stored credentials
  if (!connectWiFi()) {
    // If connection fails, start configuration AP
    startConfigAP();
  }

  // Define server routes for normal operation
  server.on("/data", handleData);
  server.on("/on", handleRelayOn);
  server.on("/off", handleRelayOff);

  // Start server
  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  reconnectWiFi();
  server.handleClient(); // Handle incoming client requests
}

// Read Wi-Fi credentials from EEPROM
void readCredentials() {
  for (int i = 0; i < MAX_SSID_LEN; i++) {
    ssid[i] = EEPROM.read(SSID_ADDR + i);
    if (ssid[i] == 0) break; // Null terminator
  }
  ssid[MAX_SSID_LEN - 1] = '\0'; // Ensure null termination

  for (int i = 0; i < MAX_PASS_LEN; i++) {
    password[i] = EEPROM.read(PASS_ADDR + i);
    if (password[i] == 0) break; // Null terminator
  }
  password[MAX_PASS_LEN - 1] = '\0'; // Ensure null termination

  Serial.print("Read SSID from EEPROM: ");
  Serial.println(ssid);
  Serial.print("Read Password from EEPROM: ");
  Serial.println(password);
}

// Save Wi-Fi credentials to EEPROM
void saveCredentials() {
  for (int i = 0; i < MAX_SSID_LEN; i++) {
    EEPROM.write(SSID_ADDR + i, i < strlen(ssid) ? ssid[i] : 0);
  }
  for (int i = 0; i < MAX_PASS_LEN; i++) {
    EEPROM.write(PASS_ADDR + i, i < strlen(password) ? password[i] : 0);
  }
  EEPROM.commit();
  Serial.println("Credentials saved to EEPROM");
}

// Start configuration Access Point and web server for entering credentials
void startConfigAP() {
  Serial.println("Starting configuration mode...");
  digitalWrite(wifinot, LOW); // Indicate no WiFi connection

  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP8266_Config", "config123"); // AP name and password

  // Define config server routes
  server.on("/", HTTP_GET, handleConfigPage);
  server.on("/save", HTTP_POST, handleSaveConfig);

  server.begin();
  Serial.println("Config server started. Connect to AP 'ESP8266_Config' with password 'config123' and visit 192.168.4.1");

  // Loop until credentials are saved and device restarts
  while (true) {
    server.handleClient();
    delay(10); // Small delay to avoid watchdog reset
  }
}

// Handle the configuration page (HTML form)
void handleConfigPage() {
  String html = "<html><body><h1>WiFi Configuration</h1>";
  html += "<form action='/save' method='POST'>";
  html += "SSID: <input type='text' name='ssid'><br>";
  html += "Password: <input type='password' name='pass'><br>";
  html += "<input type='submit' value='Save'>";
  html += "</form></body></html>";
  server.send(200, "text/html", html);
}

// Handle saving the configuration
void handleSaveConfig() {
  if (server.hasArg("ssid") && server.hasArg("pass")) {
    String newSSID = server.arg("ssid");
    String newPass = server.arg("pass");

    newSSID.toCharArray(ssid, MAX_SSID_LEN);
    newPass.toCharArray(password, MAX_PASS_LEN);

    saveCredentials();

    server.send(200, "text/html", "<html><body><h1>Credentials saved. Restarting...</h1></body></html>");
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Invalid request");
  }
}

// Connect to Wi-Fi
bool connectWiFi() {
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  int attempts = 0;
  const int maxAttempts = 20;

  while (WiFi.status() != WL_CONNECTED && attempts < maxAttempts) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(wifiyes, LOW);
    digitalWrite(wifinot, HIGH); // Turn off no-WiFi LED
    return true;
  } else {
    Serial.println("\nFailed to connect to Wi-Fi.");
    digitalWrite(wifinot, LOW);
    digitalWrite(wifiyes, HIGH); // Turn off WiFi-yes LED
    return false;
  }
}

// Reconnect to Wi-Fi if connection is lost
void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, attempting to reconnect...");
    if (!connectWiFi()) {
      // If reconnect fails, enter config mode
      startConfigAP();
    }
  }
}

void handleData() {
  // Read sensor data
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int moisture = analogRead(MOISTURE_PIN);
  int irLeft = digitalRead(IR_LEFT_PIN); // LOW (0) = object detected, HIGH (1) = no object
  int irRight = digitalRead(IR_RIGHT_PIN); // LOW (0) = object detected, HIGH (1) = no object

  // Check for valid readings
  if (isnan(temperature) || isnan(humidity)) {
    temperature = 0.0;
    humidity = 0.0;
  }

  // Create JSON response
  String json = "{";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"humidity\":" + String(humidity, 1) + ",";
  json += "\"moisture\":" + String(moisture) + ",";
  json += "\"irLeft\":" + String(irLeft) + ",";
  json += "\"irRight\":" + String(irRight) + ",";
  json += "\"relay\":" + String(relayState ? "true" : "false");
  json += "}";

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void handleRelayOn() {
  digitalWrite(RELAY_PIN, LOW); // Turn relay ON (active LOW)
  relayState = true;
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Relay turned ON");
}

void handleRelayOff() {
  digitalWrite(RELAY_PIN, HIGH); // Turn relay OFF
  relayState = false;
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Relay turned OFF");
}
