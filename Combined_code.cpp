// Transmitter side



#include <OneWire.h>
#include <DallasTemperature.h>
#include <SPI.h>
#include <LoRa.h>

// === Pin Definitions ===
#define ONE_WIRE_BUS   17   // DS18B20
#define TDS_PIN        35   // TDS analog
#define TURBIDITY_PIN  36   // Turbidity analog
#define PH_PIN         39   // pH analog
#define TRIG_PIN       4    // Ultrasonic Trig
#define ECHO_PIN       14    // Ultrasonic Echo

// LoRa Pins
#define LORA_SS    10
#define LORA_RST   9
#define LORA_DIO0  12

// Sensor setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  Serial.begin(115200);

  // DS18B20
  sensors.begin();
  Serial.println("Sensors Initialized");

  // ADC setup
  analogReadResolution(12);           // 12-bit ADC
  analogSetAttenuation(ADC_11db);     // 0-3.3V range

  // Ultrasonic pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // LoRa setup
  Serial.println("Initializing LoRa...");
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed!");
    while (1);
  }
  Serial.println("LoRa Initialized.");
}

float getUltrasonicDistanceCM() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // timeout in 30ms
  float distance = duration * 0.034 / 2;  // speed of sound = 0.034 cm/us
  return distance;
}

void loop() {
  // === Temperature Reading ===
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);

  // === TDS Reading ===
  int tdsRaw = analogRead(TDS_PIN);
  float tdsVoltage = tdsRaw * (3.3 / 4095.0);
  float tdsValue = (133.42 * tdsVoltage * tdsVoltage * tdsVoltage
                  - 255.86 * tdsVoltage * tdsVoltage
                  + 857.39 * tdsVoltage) * 0.5;

  // === Turbidity Reading ===
  int turbidityRaw = analogRead(TURBIDITY_PIN);
  float turbidityVoltage = turbidityRaw * (3.3 / 4095.0);
  float turbidityNTU = turbidityVoltage * 100.0;  // adjust as per calibration

  // === pH Reading ===
  int phRaw = analogRead(PH_PIN);
  float phVoltage = phRaw * (3.3 / 4095.0);
  float phValue = 3.5 * phVoltage;  // basic approximation

  // === Ultrasonic Distance Reading ===
  float distanceCM = getUltrasonicDistanceCM();

  // === LoRa Packet Send ===
  LoRa.beginPacket();
  LoRa.print("Temp: "); LoRa.print(temperature); LoRa.print(" C, ");
  LoRa.print("TDS: "); LoRa.print(tdsValue); LoRa.print(" ppm, ");
  LoRa.print("Turb: "); LoRa.print(turbidityNTU); LoRa.print(" NTU, ");
  LoRa.print("pH: "); LoRa.print(phValue); LoRa.print(", ");
  LoRa.print("Level: "); LoRa.print(distanceCM); LoRa.println(" cm");
  LoRa.endPacket();

  // === Debug Output ===
  Serial.print("Sent → Temp: "); Serial.print(temperature);
  Serial.print(" C, TDS: "); Serial.print(tdsValue);
  Serial.print(" ppm, Turb: "); Serial.print(turbidityNTU);
  Serial.print(" NTU, pH: "); Serial.print(phValue);
  Serial.print(", Distance: "); Serial.print(distanceCM);
  Serial.println(" cm");

  delay(3000);
}








// Receiver side



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





