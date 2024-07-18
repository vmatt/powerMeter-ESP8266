#include <FS.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <EmonLib.h>
#include <CircularBuffer.hpp>
#include <DoubleResetDetector.h>

// Double Reset Detector settings
#define DRD_TIMEOUT 10
#define DRD_ADDRESS 0
DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

// MQTT settings
char mqtt_server[40];
char mqtt_port[6];
char mqtt_user[40];
char mqtt_password[40];

double average;
double Irms;
CircularBuffer<double, 60> meas;  // Circular buffer to store up to 60 measurements
static unsigned long check = 0;

WiFiManager wifiManager;

// Flag for saving data
bool shouldSaveConfig = false;

// EmonLib settings
EnergyMonitor emon1;
const int CURRENT_SENSOR_PIN = A0;

WiFiClient espClient;
PubSubClient client(espClient);

// Callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setup() {
  Serial.begin(115200);
  delay(10);
  Serial.println("\n Starting");
  
  // Initialize SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  // Load saved configuration
  loadConfig();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  emon1.current(CURRENT_SENSOR_PIN, 30);

  // WiFiManager custom parameters
  WiFiManagerParameter custom_mqtt_server("server", "MQTT server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT user", mqtt_user, 40);
  WiFiManagerParameter custom_mqtt_password("password", "MQTT password", mqtt_password, 40);

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);

  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    digitalWrite(LED_BUILTIN, LOW);
    wifiManager.startConfigPortal("PowerMeterAP");
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
    wifiManager.autoConnect("PowerMeterAP");
  }

  // Read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());
  
  // Save the custom parameters to FS
  if (shouldSaveConfig) {
    saveConfig();
  }

  client.setServer(mqtt_server, atoi(mqtt_port));
  
  if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
    Serial.println("Connected to MQTT");
    publishDiscoveryMessage();
  }
}

void loadConfig() {
  Serial.println("Loading config");
  if (SPIFFS.exists("/config.json")) {
    File configFile = SPIFFS.open("/config.json", "r");
    if (configFile) {
      size_t size = configFile.size();
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);

      DynamicJsonDocument json(1024);
      DeserializationError error = deserializeJson(json, buf.get());
      
      if (!error) {
        strcpy(mqtt_server, json["mqtt_server"] | "192.168.100.11");
        strcpy(mqtt_port, json["mqtt_port"] | "1883");
        strcpy(mqtt_user, json["mqtt_user"] | "valq");
        strcpy(mqtt_password, json["mqtt_password"] | "pw");
        
        Serial.println("Loaded MQTT Server: " + String(mqtt_server));
        Serial.println("Loaded MQTT Port: " + String(mqtt_port));
        Serial.println("Loaded MQTT User: " + String(mqtt_user));
      } else {
        Serial.println("Failed to load json config");
      }
      configFile.close();
    }
  } else {
    Serial.println("Config file does not exist");
    // Set default values if config doesn't exist
    strcpy(mqtt_server, "192.168.100.11");
    strcpy(mqtt_port, "1883");
    strcpy(mqtt_user, "valq");
    strcpy(mqtt_password, "pw");
  }
}

void saveConfig() {
  Serial.println("Saving config");
  DynamicJsonDocument json(1024);
  json["mqtt_server"] = mqtt_server;
  json["mqtt_port"] = mqtt_port;
  json["mqtt_user"] = mqtt_user;
  json["mqtt_password"] = mqtt_password;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
  } else {
    serializeJson(json, configFile);
    configFile.close();
    Serial.println("Config saved");
    Serial.println("Saved MQTT Server: " + String(mqtt_server));
    Serial.println("Saved MQTT Port: " + String(mqtt_port));
    Serial.println("Saved MQTT User: " + String(mqtt_user));
    
    // Verify saved config
    loadConfig();
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  float current = measureCurrent();
  char currentStr[10];
  dtostrf(current, 4, 2, currentStr);
  client.publish("homeassistant/sensor/pm1/current", currentStr);

  delay(5000);
  drd.loop();
}

void reconnect() {
  int retries = 0;
  while (!client.connected() && retries < 5) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("connected");
      publishDiscoveryMessage();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
      retries++;
    }
  }
}


void publishDiscoveryMessage() {
  // Increase the buffer size
  DynamicJsonDocument doc(1024);
  doc["name"] = "Power Meter Current";
  doc["state_topic"] = "homeassistant/sensor/pm1/current";
  doc["unit_of_measurement"] = "A";
  doc["value_template"] = "{{ value | float }}";
  doc["unique_id"] = "power_meter_current_sensor";
  
  JsonObject device = doc.createNestedObject("device");
  device["identifiers"].add("power_meter_01");
  device["name"] = "Power Meter";
  device["model"] = "ESP8266 Power Meter";
  device["manufacturer"] = "DIY";

  // Use a larger buffer for the output
  char output[512];
  
  // Serialize JSON to buffer
  size_t n = serializeJson(doc, output);

  // Check if serialization was successful
  if (n > 0 && n < sizeof(output)) {
    Serial.println("Attempting to publish discovery message:");
    Serial.println(output);
    Serial.print("Message length: ");
    Serial.println(n);
    client.setBufferSize(512);
    
    if (client.publish("homeassistant/sensor/pm1/config", output, n)) {
      Serial.println("Discovery message published successfully");
    } else {
      Serial.println("Failed to publish discovery message");
      Serial.print("MQTT client state: ");
      Serial.println(client.state());
      client.setBufferSize(128);
    }
  } else {
    Serial.println("Error serializing JSON. Buffer may be too small.");
  }
}

double measureCurrent() {
  Irms = emon1.calcIrms(1480);
  Irms = Irms - (0.1064309542 * pow(1.0986371705, Irms));
  if (Irms <= 0) Irms = 0;
  meas.push(Irms);
  
  unsigned long now = millis();
  if (now - check >= 57834) {
    check = now;
    
    double sum = 0;
    for (int i = 0; i < meas.size(); i++) {
      sum += meas[i];
    }
    average = meas.size() > 0 ? sum / meas.size() : 0;
  }
  return Irms;
}
