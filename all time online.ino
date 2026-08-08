#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>
#include <DHT.h>

// ============================================
// PIN DEFINITIONS (UNCHANGED)
// ============================================
#define DHTPIN D4
#define DHTTYPE DHT11
#define MOISTURE_PIN A0
#define IR_LEFT_PIN D1
#define IR_RIGHT_PIN D2
#define RELAY_PIN D5
#define wifinot D7
#define wifiyes D6

// ============================================
// INITIALIZE
// ============================================
DHT dht(DHTPIN, DHTTYPE);
ESP8266WebServer server(80);

// ============================================
// EEPROM CONFIG
// ============================================
#define EEPROM_SIZE 512
#define SSID_ADDR 0
#define PASS_ADDR 32
#define EMAIL_ADDR 96
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64
#define MAX_EMAIL_LEN 64

char ssid[MAX_SSID_LEN] = "";
char password[MAX_PASS_LEN] = "";
char userEmail[MAX_EMAIL_LEN] = "";
bool relayState = false;
bool credentialsSaved = false;

// ============================================
// DEBUG
// ============================================
void debugPrint(const String& msg) {
  Serial.println("[DEBUG] " + msg);
}

// ============================================
// EEPROM FUNCTIONS
// ============================================
void readCredentialsFromEEPROM() {
  debugPrint("Reading credentials from EEPROM...");
  EEPROM.begin(EEPROM_SIZE);
  
  for (int i = 0; i < MAX_SSID_LEN; i++) {
    ssid[i] = EEPROM.read(SSID_ADDR + i);
    if (ssid[i] == 0) break;
  }
  ssid[MAX_SSID_LEN - 1] = '\0';
  
  for (int i = 0; i < MAX_PASS_LEN; i++) {
    password[i] = EEPROM.read(PASS_ADDR + i);
    if (password[i] == 0) break;
  }
  password[MAX_PASS_LEN - 1] = '\0';
  
  for (int i = 0; i < MAX_EMAIL_LEN; i++) {
    userEmail[i] = EEPROM.read(EMAIL_ADDR + i);
    if (userEmail[i] == 0) break;
  }
  userEmail[MAX_EMAIL_LEN - 1] = '\0';
  
  debugPrint("SSID: " + String(ssid));
  EEPROM.end();
}

void saveCredentialsToEEPROM() {
  debugPrint("Saving credentials to EEPROM...");
  EEPROM.begin(EEPROM_SIZE);
  
  for (int i = 0; i < MAX_SSID_LEN; i++) {
    EEPROM.write(SSID_ADDR + i, i < strlen(ssid) ? ssid[i] : 0);
  }
  for (int i = 0; i < MAX_PASS_LEN; i++) {
    EEPROM.write(PASS_ADDR + i, i < strlen(password) ? password[i] : 0);
  }
  for (int i = 0; i < MAX_EMAIL_LEN; i++) {
    EEPROM.write(EMAIL_ADDR + i, i < strlen(userEmail) ? userEmail[i] : 0);
  }
  
  EEPROM.commit();
  EEPROM.end();
  credentialsSaved = true;
  debugPrint("✅ Credentials saved!");
}

bool areCredentialsValid() {
  return (strlen(ssid) > 0 && strlen(password) > 0);
}

// ============================================
// WIFI FUNCTIONS
// ============================================
bool connectToWiFi() {
  debugPrint("Connecting to WiFi: " + String(ssid));
  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    debugPrint("✅ Connected! IP: " + WiFi.localIP().toString());
    digitalWrite(wifiyes, LOW);
    digitalWrite(wifinot, HIGH);
    return true;
  } else {
    debugPrint("❌ Failed to connect");
    digitalWrite(wifiyes, HIGH);
    digitalWrite(wifinot, LOW);
    return false;
  }
}

void setupAPMode() {
  debugPrint("Setting up AP mode...");
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("ESP8266_config", "config123");
  debugPrint("✅ AP Started: ESP8266_config | IP: 192.168.4.1");
}

void setupMDNS() {
  if (MDNS.begin("esp8266")) {
    debugPrint("✅ mDNS: esp8266.local");
    MDNS.addService("http", "tcp", 80);
  } else {
    debugPrint("❌ mDNS failed");
  }
}

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    debugPrint("⚠️ WiFi lost, reconnecting...");
    connectToWiFi();
  }
}

// ============================================
// WEB HANDLERS - ULTRA LIGHTWEIGHT PURE HTML
// ============================================
void handleRoot() {
  debugPrint("Serving pure HTML page...");
  
  String html = "<!DOCTYPE html><html><body>";
  
  // Header
  html += "<h2>🌱 ESP8266 Config</h2>";
  html += "<hr>";
  
  // Status
  html += "<b>Status:</b> ";
  if (WiFi.status() == WL_CONNECTED) {
    html += "✅ Connected<br>";
    html += "<b>SSID:</b> " + String(WiFi.SSID()) + "<br>";
    html += "<b>IP:</b> " + WiFi.localIP().toString() + "<br>";
    html += "<b>mDNS:</b> http://esp8266.local<br>";
  } else {
    html += "⚠️ AP Mode<br>";
    html += "<b>AP:</b> ESP8266_config<br>";
    html += "<b>Password:</b> config123<br>";
    html += "<b>Connect to:</b> 192.168.4.1<br>";
  }
  
  html += "<hr>";
  
  // Form
  html += "<b>Enter WiFi Credentials:</b><br><br>";
  html += "<form action='/save' method='POST'>";
  html += "SSID:<br>";
  html += "<input type='text' name='ssid' required><br><br>";
  html += "Password:<br>";
  html += "<input type='password' name='pass' required><br><br>";
  html += "Email (optional):<br>";
  html += "<input type='text' name='email'><br><br>";
  html += "<input type='submit' value='Save & Reboot'>";
  html += "</form>";
  
  html += "<hr>";
  html += "<small>ESP8266 Smart Irrigation</small>";
  
  html += "</body></html>";
  
  server.send(200, "text/html", html);
  debugPrint("Page served");
}

void handleSave() {
  debugPrint("Processing save...");
  
  if (server.hasArg("ssid") && server.hasArg("pass")) {
    String newSSID = server.arg("ssid");
    String newPass = server.arg("pass");
    String newEmail = server.hasArg("email") ? server.arg("email") : "";
    
    newSSID.trim();
    newPass.trim();
    newEmail.trim();
    
    debugPrint("SSID: " + newSSID);
    debugPrint("Email: " + newEmail);
    
    if (newSSID.length() < 3 || newPass.length() < 3) {
      String html = "<!DOCTYPE html><html><body>";
      html += "<h3>❌ Error</h3>";
      html += "<p>SSID and Password must be at least 3 characters.</p>";
      html += "<a href='/'>← Back</a>";
      html += "</body></html>";
      server.send(400, "text/html", html);
      return;
    }
    
    newSSID.toCharArray(ssid, MAX_SSID_LEN);
    newPass.toCharArray(password, MAX_PASS_LEN);
    newEmail.toCharArray(userEmail, MAX_EMAIL_LEN);
    
    saveCredentialsToEEPROM();
    
    String html = "<!DOCTYPE html><html><body>";
    html += "<h3>✅ Success!</h3>";
    html += "<p>Saved: " + String(ssid) + "</p>";
    html += "<p>Rebooting in 5 seconds...</p>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
    debugPrint("✅ Saved, restarting...");
    
    delay(5000);
    ESP.restart();
  } else {
    String html = "<!DOCTYPE html><html><body>";
    html += "<h3>❌ Error</h3>";
    html += "<p>Missing fields.</p>";
    html += "<a href='/'>← Back</a>";
    html += "</body></html>";
    server.send(400, "text/html", html);
  }
}

void handleData() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  int moisture = analogRead(MOISTURE_PIN);
  int irLeft = digitalRead(IR_LEFT_PIN);
  int irRight = digitalRead(IR_RIGHT_PIN);
  
  if (isnan(temperature) || isnan(humidity)) {
    temperature = 0.0;
    humidity = 0.0;
  }
  
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
  digitalWrite(RELAY_PIN, LOW);
  relayState = true;
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Relay ON");
}

void handleRelayOff() {
  digitalWrite(RELAY_PIN, HIGH);
  relayState = false;
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Relay OFF");
}

void handleNotFound() {
  server.send(404, "text/plain", "404");
}

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  debugPrint("========================================");
  debugPrint("🚀 ESP8266 Smart Irrigation");
  debugPrint("========================================");
  
  // Initialize pins
  pinMode(IR_LEFT_PIN, INPUT);
  pinMode(IR_RIGHT_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(wifiyes, OUTPUT);
  pinMode(wifinot, OUTPUT);
  
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(wifiyes, HIGH);
  digitalWrite(wifinot, HIGH);
  
  dht.begin();
  
  // Read credentials
  readCredentialsFromEEPROM();
  
  // Setup AP
  setupAPMode();
  
  // Connect to WiFi if credentials exist
  if (areCredentialsValid()) {
    connectToWiFi();
  } else {
    debugPrint("No credentials found. AP mode only.");
    digitalWrite(wifinot, LOW);
  }
  
  // Setup mDNS
  setupMDNS();
  
  // Web server routes
  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/data", handleData);
  server.on("/on", handleRelayOn);
  server.on("/off", handleRelayOff);
  server.onNotFound(handleNotFound);
  
  server.begin();
  debugPrint("✅ Web server started");
  
  debugPrint("========================================");
  debugPrint("📡 Access at:");
  if (WiFi.status() == WL_CONNECTED) {
    debugPrint("  http://" + WiFi.localIP().toString());
  }
  debugPrint("  http://192.168.4.1 (AP Mode)");
  debugPrint("  http://esp8266.local (mDNS)");
  debugPrint("========================================");
}

// ============================================
// LOOP
// ============================================
void loop() {
  server.handleClient();
  MDNS.update();
  
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    checkWiFiConnection();
  }
  
  delay(10);
}
