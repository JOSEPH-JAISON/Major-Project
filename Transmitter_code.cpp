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
