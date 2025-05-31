#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <HTTPClient.h>

// LoRa pins
#define SS      10
#define RST     9
#define DIO0    2

// WiFi credentials
#define WIFI_SSID "pithree"
#define WIFI_PASSWORD "password"

// ThingSpeak API
#define THINGSPEAK_API_KEY "VLIL83OS19EN18YA"
#define THINGSPEAK_URL "http://api.thingspeak.com/update"

// Sensor data placeholders
float temperature = 0.0;
float tds = 0.0;
float turbidity = 0.0;
float ph = 0.0;
float waterLevel = 0.0;

void setup() {
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  // Initialize LoRa
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Check connections.");
    while (1);
  }
  Serial.println("LoRa Receiver ready");
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    Serial.println("Received: " + incoming);
    parseAndSendToThingSpeak(cleanData(incoming));
  }
}

String cleanData(String raw) {
  // Remove all spaces
  raw.replace(" ", "");

  // Remove units
  raw.replace("C", "");
  raw.replace("ppm", "");
  raw.replace("NTU", "");
  raw.replace("cm", "");

  // Replace field labels to match parser
  raw.replace("Temp:", "TEMP:");
  raw.replace("TDS:", "TDS:");
  raw.replace("Turb:", "TURB:");
  raw.replace("pH:", "PH:");
  raw.replace("Level:", "LEVEL:");

  return raw;
}

void parseAndSendToThingSpeak(String data) {
  // Expected format after cleaning: "TEMP:27.25,TDS:310.13,TURB:94.53,PH:5.65,LEVEL:209.29"

  int tIndex = data.indexOf("TEMP:");
  int dIndex = data.indexOf("TDS:");
  int turbIndex = data.indexOf("TURB:");
  int phIndex = data.indexOf("PH:");
  int levelIndex = data.indexOf("LEVEL:");

  if (tIndex == -1 || dIndex == -1 || turbIndex == -1 || phIndex == -1 || levelIndex == -1) {
    Serial.println("Invalid packet format.");
    return;
  }

  // Extract values
  temperature = data.substring(tIndex + 5, dIndex - 1).toFloat();
  tds = data.substring(dIndex + 4, turbIndex - 1).toFloat();
  turbidity = data.substring(turbIndex + 5, phIndex - 1).toFloat();
  ph = data.substring(phIndex + 3 + 1, levelIndex - 1).toFloat();  // careful with PH
  waterLevel = data.substring(levelIndex + 6).toFloat();

  // Debug
  Serial.println("Parsed:");
  Serial.println("Temperature: " + String(temperature));
  Serial.println("Turbidity: " + String(turbidity));
  Serial.println("TDS: " + String(tds));
  Serial.println("Water Level: " + String(waterLevel));
  Serial.println("pH: " + String(ph));

  // Send to ThingSpeak
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = String(THINGSPEAK_URL) + "?api_key=" + THINGSPEAK_API_KEY +
                 "&field1=" + String(temperature, 1) +
                 "&field2=" + String(turbidity, 2) +
                 "&field3=" + String(tds, 2) +
                 "&field4=" + String(waterLevel, 2) +
                 "&field5=" + String(ph, 2);

    Serial.println("Sending to ThingSpeak: " + url);

    http.begin(url);
    http.setTimeout(10000);  // 10 seconds timeout
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      Serial.print("Data sent to ThingSpeak. Response Code: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("Error sending data: ");
      Serial.println(httpResponseCode);
    }

    http.end();
  } else {
    Serial.println("WiFi Disconnected. Reconnecting...");
    WiFi.disconnect();
    WiFi.reconnect();
  }

  delay(5000);  // ThingSpeak minimum update interval
}