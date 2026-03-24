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
void scanI2C();

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
  
  Serial.begin(115200);

  // //wifi section
  // Serial.println("Connecting to PAWS-Secure...");
  // WiFi.disconnect(true);
  // WiFi.begin(ssid, WPA2_AUTH_PEAP, identity, username, password);

  // int counter = 0;
  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print(".");
  //   counter++;
  //   if (counter >= 60) {
  //     Serial.println("\nTimed out. Restarting...");
  //     ESP.restart();
  //   }
  // }

  // Serial.println("\nConnected!");
  // Serial.print("IP address: ");
  // Serial.println(WiFi.localIP());

//pin setup, wire setup, etc.

  pinMode(XSHUT1_PIN, OUTPUT);
  pinMode(XSHUT2_PIN, OUTPUT);
  digitalWrite(XSHUT1_PIN, LOW);
  digitalWrite(XSHUT2_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  delay(50);

  // Bring up sensor 1 and assign address
  digitalWrite(XSHUT1_PIN, HIGH);
  delay(10);
  sensor1.setTimeout(500);
  if (sensor1.init()) {
    sensor1.setAddress(0x31);
    sensor1.setDistanceMode(VL53L1X::Medium);
    sensor1.setMeasurementTimingBudget(50000);
    sensor1.startContinuous(50);
    Serial.print("Sensor 1 connected at address 0x");
    Serial.println(sensor1.getAddress(), HEX);
  } else {
    Serial.println("ERROR: Sensor 1 failed to initialize!");
  }

  // Bring up sensor 2 and assign address
  digitalWrite(XSHUT2_PIN, HIGH);
  delay(10);
  sensor2.setTimeout(500);
  if (sensor2.init()) {
    sensor2.setAddress(0x32);
    sensor2.setDistanceMode(VL53L1X::Medium);
    sensor2.setMeasurementTimingBudget(50000);
    sensor2.startContinuous(50);
    Serial.print("Sensor 2 connected at address 0x");
    Serial.println(sensor2.getAddress(), HEX);
  } else {
    Serial.println("ERROR: Sensor 2 failed to initialize!");
  }


  initDist = sensor1.read();
  Serial.print("Initial distance: ");
  Serial.println(initDist);
}

void loop() {
  uint16_t dist1 = sensor1.read();
  if (sensor1.timeoutOccurred()) {
    Serial.println("Sensor 1 TIMEOUT");
  }

  uint16_t dist2 = sensor2.read();
  if (sensor2.timeoutOccurred()) {
    Serial.println("Sensor 2 TIMEOUT");
  }


  if ((dist1 > 2050) && (dist2 > 2050))
  {
    currentState=IDLE_SHUT;
    Serial.print(currentState);
  }
  else if (dist1 <2050)
  {
    if (dist2<2050)
    {
      prevState=IDLE_SHUT;
      currentState=PROCESSING;
    Serial.print(currentState);
    Serial.println("prev state:");
    Serial.print(prevState);
    }
    
  }
  
  

//printing the distance (for testing)
//----------------------------------------------------

  // Serial.print("Sensor 1: ");
  // Serial.print(dist1);
  // Serial.print(" mm  |  Sensor 2: ");
  // Serial.print(dist2-30);
  // Serial.println(" mm");

//----------------------------------------------------



  delay(10);
}

// put function definitions here:
