#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid     = "PAWS-Secure";
const char* identity = "jpr99061";        // just your MyID, e.g. "jr01234"
const char* username = "jpr99061";        // same as identity
const char* password = "UniGAJR123$$";


const char* serverURL = "https://esp32-projects-node.onrender.com/occupancy/event";

// put function declarations here:

VL53L1X sensor1;
VL53L1X sensor2;

const int SDA_PIN= 21;
const int SCL_PIN= 22;
const int XSHUT1_PIN= 23;
const int XSHUT2_PIN= 32;
uint16_t initDist;


unsigned long lastS1TimeoutPost = 0;
unsigned long lastS2TimeoutPost = 0;
const unsigned long TIMEOUT_COOLDOWN_MS = 5000; // only post once every 5 seconds

enum State{
  IDLE_SHUT,
  // IDLE_OPEN,
  ENTERING,
  ENTERING_DOOR,
  LEAVING,
  LEAVING_DOOR,
  WAIT_CLEAR
};

State currentState = IDLE_SHUT;


//0 or false is detecting (still detecting if people are entering) 1 or true is processing is finished
bool detectingTorF=true;

bool s1Covered=false;
bool s2Covered=false;

bool doorOpen=false;
bool personDone=false;
bool doorJustClosed=false;

//the distance to the wall, subject to change

int wallDist;
const int threshold=660;//26 inches

//i can reuse this for changing from detecting to finished
unsigned long stateStartTime = 0;

//time both sensors have been continuously clear in WAIT_CLEAR
unsigned long clearStartTime = 0;
const unsigned long CLEAR_DWELL_MS = 500;

//when you have the door distance (idle) put that in this variable
//doorDist=wallDist-(amt in mm)
int doorDist;

int count=0;

void postEvent(const char* message) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");

  // build the JSON body manually — simple enough we don't need a library
  String body = "{\"message\":\"";
  body += message;
  body += "\",\"count\":";
  body += count;
  body += "}";

  int httpCode = http.POST(body);
  if (httpCode == 200) {
    String response = http.getString();
    // find "count": in the response and parse the number after it
    int idx = response.indexOf("\"count\":");
    if (idx != -1) {
      count = response.substring(idx + 8).toInt();
    }
  }
  http.end();
}

void setup() {
  // put your setup code here, to run once:
  
  Serial.begin(115200);

  //wifi section
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
  wallDist=initDist-15;
  doorDist=wallDist-45;
  Serial.print("Initial distance: ");
  Serial.println(initDist);
}

void loop() {
  uint16_t dist1 = sensor1.read();
if (sensor1.timeoutOccurred()) {
  Serial.println("Sensor 1 TIMEOUT");
  if (millis() - lastS1TimeoutPost > TIMEOUT_COOLDOWN_MS) {
    postEvent("Sensor 1 TIMEOUT");
    lastS1TimeoutPost = millis();
  }
}

  uint16_t dist2 = sensor2.read();
if (sensor2.timeoutOccurred()) {
  Serial.println("Sensor 2 TIMEOUT");
  if (millis() - lastS2TimeoutPost > TIMEOUT_COOLDOWN_MS) {
    postEvent("Sensor 2 TIMEOUT");
    lastS2TimeoutPost = millis();
  }
}


 

  if (dist1<threshold)
  {
    s1Covered=true;
    // Serial.println("1 covered");
  }
  else{
    s1Covered=false;
  }
  if (dist2<threshold)
  {
    s2Covered=true;
    // Serial.println("2 covered");
  }
  else{
    s2Covered=false;
  }


//choosing state

  switch (currentState)
  {
  case IDLE_SHUT:
if (s1Covered&&!s2Covered&&!doorOpen) {
    // door not open yet, this is the door opening
    doorJustClosed=false;
    currentState = ENTERING_DOOR;
    stateStartTime = millis();
  } else if (s1Covered&&!s2Covered&&doorOpen&&!personDone) {
    // door open, person entering
    currentState = ENTERING;
    stateStartTime = millis();
  } else if (s2Covered&&!s1Covered&&doorOpen&&!personDone) {
    // door open, person leaving
    currentState = LEAVING;
    stateStartTime = millis();
  } else if (s2Covered&&!s1Covered&&doorOpen&&personDone) {
    // person done, this is the door closing (s2 first)
    currentState = LEAVING_DOOR;
    stateStartTime = millis();
  } else if (s1Covered&&!s2Covered&&doorOpen&&personDone) {
    // person done, door closing from s1 side (reverse of LEAVING_DOOR)
    Serial.println("DOOR CLOSE TICK (s1 first)");
    postEvent("DOOR CLOSE TICK (s1 first)");
    doorOpen=false;
    personDone=false;
    doorJustClosed=true;
    currentState=WAIT_CLEAR;
    stateStartTime = millis();
  }
  break;
  // case IDLE_OPEN:
  // // door is physically propped open, no door ticks needed
  // if (s1Covered&&!s2Covered) {
  //   currentState = ENTERING;
  //   stateStartTime = millis();
  // } else if (s2Covered&&!s1Covered) {
  //   currentState = LEAVING;
  //   stateStartTime = millis();
  // }
  // break;
  case ENTERING_DOOR:
    delay(10);
    if(s2Covered){
      Serial.println("DOOR ENTER TICK");
      postEvent("DOOR ENTER TICK");
      doorOpen=true;
      currentState=WAIT_CLEAR;
    } else if (millis() - stateStartTime > 2000) {
      if ((dist1>wallDist)||(dist2>wallDist)){
        currentState = IDLE_SHUT;}
      else{
        // currentState = IDLE_OPEN;
        Serial.println("this WOULD have set it to idle open");
        postEvent("this WOULD have set it to idle open");
        }
    }
  break;
  case ENTERING:
    delay(10);
    if(s2Covered){
      count++;
      Serial.println("ENTERING");
      postEvent("ENTERING");
      Serial.print("People in room: ");
      Serial.println(count);
      String countMsg = "People in room: " + String(count);
      postEvent(countMsg.c_str());
      personDone=true;
      currentState=WAIT_CLEAR;
    } else if (millis() - stateStartTime > 2000) {
      if ((dist1>wallDist)||(dist2>wallDist)){
        currentState = IDLE_SHUT;}
      else{
        // currentState = IDLE_OPEN;
        Serial.println("this WOULD have set it to idle open");
        postEvent("this WOULD have set it to idle open");
        }
    }
    break;
  case LEAVING:
    delay(10);
    if(s1Covered){
      if(count>0){
        count--;
      }
      Serial.println("LEAVING");
      postEvent("LEAVING");
      Serial.print("People in room: ");
      Serial.println(count);
      String countMsg = "People in room: " + String(count);
      postEvent(countMsg.c_str());
      personDone=true;
      currentState=WAIT_CLEAR;
    } else if (millis() - stateStartTime > 2000) {
      if ((dist1>wallDist)||(dist2>wallDist)){
        currentState = IDLE_SHUT;}
      else{
        // currentState = IDLE_OPEN;
        Serial.println("this WOULD have set it to idle open");
        postEvent("this WOULD have set it to idle open");
        }
    }
    break;
  case LEAVING_DOOR:
    delay(10);
    if(s1Covered){
      Serial.println("DOOR LEAVE TICK");
      postEvent("DOOR LEAVE TICK");
      doorOpen=false;
      personDone=false;
      doorJustClosed=true;
      currentState=IDLE_SHUT;
    } else if (millis() - stateStartTime > 2000) {
      doorOpen=false;
      personDone=false;
      if ((dist1>wallDist)||(dist2>wallDist)){
        currentState = IDLE_SHUT;}
      else{
        // currentState = IDLE_OPEN;
        Serial.println("this WOULD have set it to idle open");
        postEvent("this WOULD have set it to idle open");
        }
    }
    break;
  case WAIT_CLEAR:
    if(!s1Covered&&!s2Covered){
      // start or continue timing how long both sensors are clear
      if(clearStartTime==0) clearStartTime=millis();
      // wait for sensors to stay clear long enough for door to settle
      if(millis() - clearStartTime >= CLEAR_DWELL_MS){
        clearStartTime=0;
        if(doorOpen || doorJustClosed){
          // still tracking a door cycle or just closed, go to IDLE_SHUT
          doorJustClosed=false;
          currentState = IDLE_SHUT;
        } else if((dist1>wallDist)&&(dist2>wallDist)){
          currentState = IDLE_SHUT;
        } else {
          // door is propped open, no door ticks needed
          // currentState = IDLE_OPEN;
          Serial.println("this WOULD have set it to idle open");
          postEvent("this WOULD have set it to idle open");
        }
      }
    } else {
      // a sensor got re-covered (door bounce), reset the clear timer
      clearStartTime=0;
    }

    break;

  default:
    break;
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
