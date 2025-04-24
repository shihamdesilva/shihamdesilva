// Updated water monitor code with Blynk MQTT publishing using SIM7020C NB-IoT (via AT commands)

#define BLYNK_TEMPLATE_ID     "TMPL6u2zq3FTN"
#define BLYNK_TEMPLATE_NAME   "water Monitor"
#define BLYNK_AUTH_TOKEN      "2-8DV2ntTAGpW_NCIcPQyi4pghRHt19K"  // Your actual Blynk device token

#define BLYNK_PRINT Serial

#include <SriLinkDevBoard.h>
#include <String>
#include <Arduino.h>

const int flowSensor1Pin = 4;
const int flowSensor2Pin = 5;

volatile int pulseCount1 = 0;
volatile int pulseCount2 = 0;
float flowRate1 = 0.0, flowRate2 = 0.0;
unsigned int totalMilliLitres1 = 0, totalMilliLitres2 = 0;
unsigned long oldTime = 0;

const float calibrationFactor = 7.5;

SriLinkDevBoard devboard;
Broker BLYNK_Broker;
bool connected;

void IRAM_ATTR pulseCounter1() { pulseCount1++; }
void IRAM_ATTR pulseCounter2() { pulseCount2++; }

void setup() {
  Serial.begin(115200);
  connected = false;

  Serial.println("Initializing NB-IoT modem...");
  if (!devboard.Init(115200)) {
    Serial.println("Modem initialized successfully.");
  } else {
    Serial.println("Modem initialization failed.");
  }

  if (!devboard.SET_APN("nbiot")) {
    Serial.println("APN set.");
  }

  Serial.println("Waiting for NB-IoT attachment...");
  long starttime = millis();
  delay(5);
  while (!devboard.IS_ATTACH() && (millis() - starttime < 60000));
  Serial.println("Connected to NB-IoT");

  if (!devboard.GET_IP()) 
  {
    Serial.println("IP fetch failed.");
  }

  // Setup Blynk MQTT broker details
  BLYNK_Broker.addr = "blynk.cloud";
  BLYNK_Broker.port = "9443";
  BLYNK_Broker.username = "BLYNK_AUTH_TOKEN";
  BLYNK_Broker.password = "";

  if (devboard.MQTT_SETUP(&BLYNK_Broker, BLYNK_Broker.addr, BLYNK_Broker.port))
   {
    delay(200);
    connected = devboard.MQTT_CONNECT(&BLYNK_Broker, "BLYNK_AUTH_TOKEN", "BLYNK_AUTH_TOKEN");

    if (connected) {
      Serial.println("MQTT connected to Blynk successfully.");
    } else {
      Serial.println("MQTT connection to Blynk failed.");
    }
  }

  // Initialize flow sensors
  pinMode(flowSensor1Pin, INPUT_PULLUP);
  pinMode(flowSensor2Pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(flowSensor1Pin), pulseCounter1, FALLING);
  attachInterrupt(digitalPinToInterrupt(flowSensor2Pin), pulseCounter2, FALLING);
  Serial.println("Flow sensors initialized.");
}

void loop() {
  unsigned long currentTime = millis();

  if (currentTime - oldTime > 1000) {
    detachInterrupt(digitalPinToInterrupt(flowSensor1Pin));
    detachInterrupt(digitalPinToInterrupt(flowSensor2Pin));

    flowRate1 = ((1000.0 / (currentTime - oldTime)) * pulseCount1) / calibrationFactor;
    flowRate2 = ((1000.0 / (currentTime - oldTime)) * pulseCount2) / calibrationFactor;

    unsigned int flowMilliLitres1 = (flowRate1 / 60) * 1000;
    unsigned int flowMilliLitres2 = (flowRate2 / 60) * 1000;

    totalMilliLitres1 += flowMilliLitres1;
    totalMilliLitres2 += flowMilliLitres2;

    bool leakDetected = abs((int)(totalMilliLitres1 - totalMilliLitres2)) > 50;

    Serial.print("Sensor 1 Flow Rate: ");
    Serial.print(flowRate1);
    Serial.print(" L/min  Total: ");
    Serial.print(totalMilliLitres1);
    Serial.println(" mL");

    Serial.print("Sensor 2 Flow Rate: ");
    Serial.print(flowRate2);
    Serial.print(" L/min  Total: ");
    Serial.print(totalMilliLitres2);
    Serial.println(" mL");

    Serial.println(leakDetected ? "Leakage Detected!" : "No Leakage Detected.");
    Serial.println("PH_level:7.3");

    if (connected) {
      // Send totalMilliLitres1 to V0
      String topic0 = String(BLYNK_AUTH_TOKEN) + "/v0";
      String msg0 = String(totalMilliLitres1);
      devboard.MQTT_PUB(&BLYNK_Broker, topic0, msg0, msg0.length(), 0, false, false, false);

      // Send totalMilliLitres2 to V1
      String topic1 = String(BLYNK_AUTH_TOKEN) + "/v1";
      String msg1 = String(totalMilliLitres2);
      devboard.MQTT_PUB(&BLYNK_Broker, topic1, msg1, msg1.length(), 0, false, false, false);

      // Send leak status to V2
      String topic2 = String(BLYNK_AUTH_TOKEN) + "/v2";
      String msg2 = leakDetected ? "Leakage Detected" : "No Leakage Detected";
      devboard.MQTT_PUB(&BLYNK_Broker, topic2, msg2, msg2.length(), 0, false, false, false);
    }

    pulseCount1 = 0;
    pulseCount2 = 0;
    oldTime = currentTime;

    attachInterrupt(digitalPinToInterrupt(flowSensor1Pin), pulseCounter1, FALLING);
    attachInterrupt(digitalPinToInterrupt(flowSensor2Pin), pulseCounter2, FALLING);
  }
}
