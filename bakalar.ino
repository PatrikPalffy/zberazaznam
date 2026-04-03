#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// ==========================
// WIFI + FIREBASE SETTINGS
// ==========================
const char* WIFI_SSID = "WIFI-NAME";
const char* WIFI_PASSWORD = "WIFI-PASSWORD";

const char* FIREBASE_HOST = "bakalar-database-default-rtdb.europe-west1.firebasedatabase.app";

// Leave empty if your rules allow writes
String FIREBASE_AUTH = "";

// Send every 1 seconds
const unsigned long SEND_INTERVAL_MS = 1000;

// ==========================
// PIN SETTINGS
// ==========================
#define I2C_SDA 21
#define I2C_SCL 22

#define GPS_RX 16   // ESP32 receives from GPS TX
#define GPS_TX 17   // ESP32 sends to GPS RX

// ==========================
// SENSOR ADDRESSES
// ==========================
const uint8_t MPU6500_ADDR = 0x68;
const uint8_t BMP280_ADDR_1 = 0x76;
const uint8_t BMP280_ADDR_2 = 0x77;

// ==========================
// GLOBAL OBJECTS
// ==========================
Adafruit_BMP280 bmp;
TinyGPSPlus gps;
HardwareSerial gpsSerial(2);

unsigned long lastSend = 0;
bool bmpFound = false;

// ==========================
// MPU6500 HELPERS
// ==========================
void mpuWriteByte(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

bool mpuReadBytes(uint8_t reg, uint8_t* buffer, size_t len) {
  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  if (Wire.requestFrom((int)MPU6500_ADDR, (int)len) != len) {
    return false;
  }

  for (size_t i = 0; i < len; i++) {
    buffer[i] = Wire.read();
  }

  return true;
}

bool initMPU6500() {
  mpuWriteByte(0x6B, 0x00); // wake up
  delay(100);

  mpuWriteByte(0x1B, 0x00); // gyro ±250 dps
  mpuWriteByte(0x1C, 0x00); // accel ±2g

  uint8_t whoami = 0;
  if (!mpuReadBytes(0x75, &whoami, 1)) {
    return false;
  }

  Serial.print("MPU6500 WHO_AM_I: 0x");
  Serial.println(whoami, HEX);

  return true;
}

bool readMPU6500(
  float& ax_g, float& ay_g, float& az_g,
  float& gx_dps, float& gy_dps, float& gz_dps,
  float& temp_c
) {
  uint8_t data[14];

  if (!mpuReadBytes(0x3B, data, 14)) {
    return false;
  }

  int16_t ax = (int16_t)((data[0] << 8) | data[1]);
  int16_t ay = (int16_t)((data[2] << 8) | data[3]);
  int16_t az = (int16_t)((data[4] << 8) | data[5]);
  int16_t t  = (int16_t)((data[6] << 8) | data[7]);
  int16_t gx = (int16_t)((data[8] << 8) | data[9]);
  int16_t gy = (int16_t)((data[10] << 8) | data[11]);
  int16_t gz = (int16_t)((data[12] << 8) | data[13]);

  ax_g = ax / 16384.0f;
  ay_g = ay / 16384.0f;
  az_g = az / 16384.0f;

  gx_dps = gx / 131.0f;
  gy_dps = gy / 131.0f;
  gz_dps = gz / 131.0f;

  temp_c = (t / 333.87f) + 21.0f;

  return true;
}

// ==========================
// WIFI
// ==========================
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

// ==========================
// FIREBASE SEND
// ==========================
bool sendToFirebase(const String& jsonPayload) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;

  String url = "https://";
  url += FIREBASE_HOST;
  url += "/sensorData/latest.json";

  if (FIREBASE_AUTH.length() > 0) {
    url += "?auth=" + FIREBASE_AUTH;
  }

  if (!https.begin(client, url)) {
    Serial.println("HTTPS begin failed");
    return false;
  }

  https.addHeader("Content-Type", "application/json");

  int httpCode = https.PUT(jsonPayload);
  String response = https.getString();

  Serial.print("HTTP code: ");
  Serial.println(httpCode);
  Serial.print("Response: ");
  Serial.println(response);

  https.end();

  return (httpCode >= 200 && httpCode < 300);
}

// ==========================
// SETUP
// ==========================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("System starting...");

  Wire.begin(I2C_SDA, I2C_SCL);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  connectWiFi();

  if (!initMPU6500()) {
    Serial.println("MPU6500 init failed");
    while (true) {
      delay(1000);
    }
  }
  Serial.println("MPU6500 OK");

  if (bmp.begin(BMP280_ADDR_1)) {
    bmpFound = true;
    Serial.println("BMP280 found at 0x76");
  } else if (bmp.begin(BMP280_ADDR_2)) {
    bmpFound = true;
    Serial.println("BMP280 found at 0x77");
  } else {
    bmpFound = false;
    Serial.println("BMP280 not found - continuing without it");
  }
}

// ==========================
// LOOP
// ==========================
void loop() {
  // Continuously feed GPS parser
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }

  if (millis() - lastSend < SEND_INTERVAL_MS) {
    return;
  }

  lastSend = millis();

  // ---------- MPU6500 ----------
  float ax, ay, az, gx, gy, gz, mpuTemp;
  bool mpuOk = readMPU6500(ax, ay, az, gx, gy, gz, mpuTemp);

  // ---------- BMP280 ----------
  float bmpTemp = NAN;
  float bmpPressure = NAN;
  float bmpAltitude = NAN;

  if (bmpFound) {
    bmpTemp = bmp.readTemperature();
    bmpPressure = bmp.readPressure() / 100.0f; // hPa
    bmpAltitude = bmp.readAltitude(1013.25);
  }

  // ---------- GPS ----------
  bool gpsValid = gps.location.isValid();
  double latitude = gps.location.lat();
  double longitude = gps.location.lng();
  double gpsAltitude = gps.altitude.meters();
  double gpsSpeed = gps.speed.kmph();
  int satellites = gps.satellites.value();

  // ---------- Build JSON ----------
  String json = "{";

  json += "\"timestamp_ms\":" + String(millis()) + ",";

  json += "\"mpu6500\":{";
  json += "\"connected\":" + String(mpuOk ? "true" : "false") + ",";
  json += "\"temperature_c\":" + String(mpuTemp, 2) + ",";
  json += "\"accel_g\":{";
  json += "\"x\":" + String(ax, 4) + ",";
  json += "\"y\":" + String(ay, 4) + ",";
  json += "\"z\":" + String(az, 4);
  json += "},";
  json += "\"gyro_dps\":{";
  json += "\"x\":" + String(gx, 2) + ",";
  json += "\"y\":" + String(gy, 2) + ",";
  json += "\"z\":" + String(gz, 2);
  json += "}";
  json += "},";

  json += "\"bmp280\":{";
  json += "\"connected\":" + String(bmpFound ? "true" : "false") + ",";
  if (bmpFound) {
    json += "\"temperature_c\":" + String(bmpTemp, 2) + ",";
    json += "\"pressure_hpa\":" + String(bmpPressure, 2) + ",";
    json += "\"altitude_m\":" + String(bmpAltitude, 2);
  } else {
    json += "\"temperature_c\":null,";
    json += "\"pressure_hpa\":null,";
    json += "\"altitude_m\":null";
  }
  json += "},";

  json += "\"gps\":{";
  json += "\"valid\":" + String(gpsValid ? "true" : "false") + ",";
  if (gpsValid) {
    json += "\"latitude\":" + String(latitude, 6) + ",";
    json += "\"longitude\":" + String(longitude, 6) + ",";
    json += "\"altitude_m\":" + String(gpsAltitude, 2) + ",";
    json += "\"speed_kmh\":" + String(gpsSpeed, 2) + ",";
    json += "\"satellites\":" + String(satellites);
  } else {
    json += "\"latitude\":null,";
    json += "\"longitude\":null,";
    json += "\"altitude_m\":null,";
    json += "\"speed_kmh\":null,";
    json += "\"satellites\":" + String(satellites);
  }
  json += "}";

  json += "}";

  Serial.println("Sending:");
  Serial.println(json);

  bool ok = sendToFirebase(json);
  Serial.println(ok ? "Upload OK" : "Upload FAILED");
  Serial.println();
}
