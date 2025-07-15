#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <math.h>

// WiFi Setup
const char* ssid = "WE_431695";
const char* password = "YOUR_PASSWORD";
const char* host = "192.168.1.100";
const uint16_t port = 8000;
WiFiClient client;

// MPU6050 Setup
Adafruit_MPU6050 mpu;
float accelX, accelZ, shankAngle;
int i2cFails = 0;

// FSR Pins
const int Sensor_1 = A1;  // Heel
const int Sensor_2 = A2;  // Right Metatarsal
const int Sensor_3 = A3;  // Toe
const int Sensor_4 = A4;  // Left Metatarsal

// Gait Variables
int valHeel, valRMet, valToe, valLMet;
String gaitPhase = "Unknown";
float CoP_X = 0, CoP_Y = 0;
float heelSum = 0, toeSum = 0;
int symmetrySamples = 0;
unsigned long lastStepTime = 0;
float stepTime = 0, cadence = 0;
int stepCount = 0;
const int threshold = 100;

// Stability
#define ANGLE_WINDOW 25
float angleBuffer[ANGLE_WINDOW];
int angleIndex = 0;
bool bufferFull = false;
float stability = 0;
String stabilityStatus = "Unknown";

// For step detection (heel strike)
bool heelPreviouslyPressed = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();

  // WiFi connection
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  if (!client.connect(host, port)) {
    Serial.println("Connection to GUI server failed");
  } else {
    Serial.println("Connected to GUI");
  }

  if (!mpu.begin(0x68)) {
    Serial.println("MPU6050 not found!");
    while (1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  pinMode(Sensor_1, INPUT);
  pinMode(Sensor_2, INPUT);
  pinMode(Sensor_3, INPUT);
  pinMode(Sensor_4, INPUT);
}

void readFSRs() {
  valHeel = analogRead(Sensor_1);
  valRMet = analogRead(Sensor_2);
  valToe  = analogRead(Sensor_3);
  valLMet = analogRead(Sensor_4);
}

String detectGaitPhase() {
  if (valHeel > threshold && valRMet < threshold && valToe < threshold && valLMet < threshold)
    return "Heel Strike";
  else if (valHeel > threshold && (valRMet > threshold || valLMet > threshold) && valToe < threshold)
    return "Mid Stance";
  else if (valHeel < threshold && (valRMet > threshold || valLMet > threshold) && valToe < threshold)
    return "Terminal Stance";
  else if (valHeel < threshold && valRMet < threshold && valLMet < threshold && valToe > threshold)
    return "Pre-swing";
  else if (valHeel > threshold && valRMet > threshold && valLMet > threshold && valToe > threshold)
    return "Foot Flat";
  else if (valHeel < threshold && valRMet < threshold && valLMet < threshold && valToe < threshold)
    return "Swing";
  else
    return "Transition";
}

bool computeShankAngle() {
  sensors_event_t a, g;
  mpu.getEvent(&a, &g);

  accelX = a.acceleration.x;
  accelZ = a.acceleration.z;
  shankAngle = atan2(accelZ, accelX) * 180.0 / PI;
  i2cFails = 0;

  angleBuffer[angleIndex++] = shankAngle;
  if (angleIndex >= ANGLE_WINDOW) {
    angleIndex = 0;
    bufferFull = true;
  }
  return true;
}

void computeCoP() {
  float total = valHeel + valRMet + valLMet + valToe;
  if (total == 0) { CoP_X = 0.5; CoP_Y = 0.5; return; }
  CoP_X = ((valRMet * 1.0) + (valLMet * -1.0)) / total;
  CoP_Y = ((valToe * 1.0) + (valHeel * -1.0)) / total;
}

float computeSymmetry() {
  return (symmetrySamples == 0) ? 0 : toeSum / (heelSum + 1);
}

void computeStability() {
  if (!bufferFull) { stabilityStatus = "N/A"; return; }

  float sum = 0, mean, variance = 0;
  for (int i = 0; i < ANGLE_WINDOW; i++) sum += angleBuffer[i];
  mean = sum / ANGLE_WINDOW;
  for (int i = 0; i < ANGLE_WINDOW; i++) variance += pow(angleBuffer[i] - mean, 2);
  stability = sqrt(variance / ANGLE_WINDOW);

  if (stability < 2.0) stabilityStatus = "Stable";
  else if (stability < 4.0) stabilityStatus = "Low Stability";
  else if (stability < 6.0) stabilityStatus = "High Instability";
  else stabilityStatus = "Unstable";
}

void loop() {
  readFSRs();
  gaitPhase = detectGaitPhase();

  // Detect heel strike for step timing
  bool heelPressed = (valHeel > threshold);

  if (heelPressed && !heelPreviouslyPressed) {
    // Heel strike detected (rising edge)
    unsigned long currentTime = millis();

    if (lastStepTime != 0) {
      stepTime = (currentTime - lastStepTime) / 1000.0; // seconds
      cadence = 60.0 / stepTime;  // steps per minute
    }
    lastStepTime = currentTime;
    stepCount++;
  }
  heelPreviouslyPressed = heelPressed;

  if (!computeShankAngle()) {
    delay(200);
    return;
  }

  computeCoP();
  computeStability();

  // Prepare payload
  String payload = String(valHeel) + "," + valRMet + "," + valLMet + "," + valToe + "," +
                   gaitPhase + "," + String(shankAngle, 2) + "," + String(stepTime, 2) + "," +
                   String(cadence, 2) + "," + String(CoP_X, 2) + "," + String(CoP_Y, 2) + "," +
                   String(computeSymmetry(), 2) + "," + stabilityStatus + "\n";

  if (client.connected()) {
    client.print(payload);
  } else {
    client.connect(host, port);
  }

  delay(200);
}
