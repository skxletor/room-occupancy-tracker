#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>
#include <WiFi.h>

const char* ssid     = "PAWS-Secure";
const char* identity = "jpr99061";        // just your MyID, e.g. "jr01234"
const char* username = "jpr99061";        // same as identity
const char* password = "UniGAJR123$$";


const char* serverURL = "url for the esp occupancy";

// put function declarations here:
VL53L1X sensor1;
VL53L1X sensor2;

const int SDA_PIN= 21;
const int SCL_PIN= 22;
const int XSHUT1_PIN= 23;
const int XSHUT2_PIN= 32;
uint16_t initDist;

enum State{
  IDLE_SHUT,
  IDLE_OPEN,
  DETECTING,
  PROCESSING
};
State currentState = IDLE_SHUT;
State prevState = IDLE_SHUT;


void setup() {
  // put your setup code here, to run once:
  
  //wifi section
  Serial.begin(115200);
  Serial.println("Connecting to PAWS-Secure...");
  WiFi.disconnect(true);
  WiFi.begin(ssid, WPA2_AUTH_PEAP, identity, username, password);

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    counter++;
    if (counter >= 60) {
      Serial.println("\nTimed out. Restarting...");
      ESP.restart();
    }
  }

  Serial.println("\nConnected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

//pin setup, wire setup, etc.

  pinMode(XSHUT1_PIN, OUTPUT);
  pinMode(XSHUT2_PIN, OUTPUT);
  digitalWrite(XSHUT1_PIN, LOW);
  digitalWrite(XSHUT2_PIN, LOW);
  Wire.begin(SDA_PIN, SCL_PIN);
  digitalWrite(XSHUT1_PIN, HIGH);
  delay(10);
  sensor1.setAddress(0x31);
  sensor1.init();
  sensor1.startContinuous(50);
  digitalWrite(XSHUT2_PIN, HIGH);
  delay(10);
  sensor2.setAddress(0x32);
  sensor2.init();
  sensor2.startContinuous(50);
  
  sensor1.setDistanceMode(VL53L1X::Medium);
  sensor2.setDistanceMode(VL53L1X::Medium);
  initDist = sensor1.read();
}

void loop() {
  // put your main code here, to run repeatedly:
  uint16_t dist1 = sensor1.read();
  uint16_t dist2 = sensor2.read();
}

// put function definitions here:
