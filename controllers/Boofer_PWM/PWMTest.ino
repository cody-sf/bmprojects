#include <GlobalDefaults.h>
#include <DeviceRoles.h>
#include <ArduinoBLE.h>
#include "Helpers.h"
#include <ArduinoJson.h>

static Device boofer;
#define SOLENOID_PIN 18
#define IGNITION_PIN 25 // The pin connected to TRIG/PWM on the MOSFET module
#define SERVICE_UUID "17504d91-b22e-4455-a466-1521d6c4c4af"
#define FEATURES_UUID "8ce6e2d0-093b-456a-9c39-322b268e1bc0"

BLEService booferService(SERVICE_UUID);
BLECharacteristic featuresCharacteristic(FEATURES_UUID, BLERead | BLEWrite | BLENotify, 512);
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastBluetoothSync = 0;

// Variables
bool ignitionPower = false;
bool valveOpen = false;
int valveTimeout = 5000;
int shortBurst = 100;
int longBurst = 1000;

int stageDelay = 5000;

int stage1Start = 140;
int stage1End = 215;
int stage1WarumupInterval = 500;

int stage2Start = 216;
int stage2End = 225;
int stage2WarumupInterval = 2000;

int stage3Start = 226;
int stage3End = 240;
int stage3WarumupInterval = 2500;

int currentIgnitionPower = 0;
bool booferReady = false;

void featureCallback(BLEDevice central, BLECharacteristic characteristic)
{

  const uint8_t *buffer = featuresCharacteristic.value();
  unsigned int dataLength = featuresCharacteristic.valueLength();
  std::string value((char *)buffer, dataLength);

  if (!value.empty())
  {
    uint8_t feature = value[0];
    Serial.print("Feature: ");
    Serial.println(feature);

    // On/Off
    if (feature == 0x01)
    {
      ignitionPower = value[1] != 0;

      if (value[1] == 1 && !booferReady) {
        Serial.println("Warmup Ignition");
        warmupIgnition();
      }
      return;
    }

    // Short Boof
    if (feature == 0x02)
    {
      Serial.print("Short Boof: ");
      Serial.println(shortBurst);
      digitalWrite(SOLENOID_PIN, HIGH);
      delay(shortBurst);
      digitalWrite(SOLENOID_PIN, LOW);
      return;
    }

    // Long Boof
    if (feature == 0x03)
    {
      Serial.print("Long Boof: ");
      Serial.println(longBurst);
      digitalWrite(SOLENOID_PIN, HIGH);
      delay(longBurst);
      digitalWrite(SOLENOID_PIN, LOW);
      return;
    }

    // Short Boof Duration
    if (feature == 0x04)
    {
      int d = 0;
      memcpy(&d, value.data() + 1, sizeof(int));
      shortBurst = d;
      Serial.print("Short Boof Duration: ");
      Serial.println(shortBurst);
      return;
    }

    // Long Boof Duration
    if (feature == 0x05)
    {
      int l = 0;
      memcpy(&l, value.data() + 1, sizeof(int));
      longBurst = l;
      Serial.print("Long Boof Duration: ");
      Serial.println(longBurst);
      return;
    }
  }
};

void setup()
{

  Serial.begin(115200);
  delay(2000);

  if (!BLE.begin())
  {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1)
      ;
  }

  BLE.setLocalName("Boofer-CL");
  BLE.setAdvertisedService(booferService);
  booferService.addCharacteristic(featuresCharacteristic);
  BLE.addService(booferService);

  // assign event handlers for connected, disconnected to peripheral
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
  featuresCharacteristic.setEventHandler(BLEWritten, featureCallback);

  // start advertising
  BLE.advertise();
  
  pinMode(IGNITION_PIN, OUTPUT);
  pinMode(SOLENOID_PIN, OUTPUT);

  digitalWrite(IGNITION_PIN, LOW);
  digitalWrite(SOLENOID_PIN, LOW);
}

void loop()
{
  BLE.poll();

  if (ignitionPower)
  {
    booferReady = true;
    analogWrite(IGNITION_PIN, 240); 
  }
  else
  {
    booferReady = false;
    currentIgnitionPower = 0;
    analogWrite(IGNITION_PIN, 0);
  }

  syncBluetoothSettings();
}

// Warmup ignition with battery pack
void warmupIgnition() {

  for (int dutyCycle = stage1Start; dutyCycle <= stage2End; dutyCycle += 1) {
    Serial.print("Currently power at: ");
    Serial.println(dutyCycle);
    currentIgnitionPower = dutyCycle;
    analogWrite(IGNITION_PIN, dutyCycle);   
    delay(stage1WarumupInterval); // Adjust the delay for how quickly you ramp up
    BLE.poll();
    syncBluetoothSettings();
  }
  delay(stageDelay);
  
  for (int dutyCycle = stage2Start; dutyCycle <= stage2End; dutyCycle += 1) {
    Serial.print("Currently power at: ");
    Serial.println(dutyCycle);
    currentIgnitionPower = dutyCycle;
    analogWrite(IGNITION_PIN, dutyCycle);   
    delay(stage2WarumupInterval); // Adjust the delay for how quickly you ramp up
    BLE.poll();
    syncBluetoothSettings();
  }
  delay(stageDelay);
  
  for (int dutyCycle = stage3Start; dutyCycle <= stage3End; dutyCycle += 1) {
    Serial.print("Currently power at: ");
    Serial.println(dutyCycle);
    currentIgnitionPower = dutyCycle;
    analogWrite(IGNITION_PIN, dutyCycle);   
    delay(stage3WarumupInterval); // Adjust the delay for how quickly you ramp up
    BLE.poll();
    syncBluetoothSettings();
  }

  booferReady = true;
}

// BLE helpers
void blePeripheralConnectHandler(BLEDevice central)
{
  // central connected event handler
  Serial.print("Connected event, central: ");
  Serial.println(central.address());
  deviceConnected = true;
}

void blePeripheralDisconnectHandler(BLEDevice central)
{
  // central disconnected event handler
  Serial.print("Disconnected event, central: ");
  Serial.println(central.address());
  deviceConnected = false;
}

void sendStatusUpdate()
{
  StaticJsonDocument<512> doc;

  // Add all the settings and their values
  doc["power"] = ignitionPower;
  doc["powerLevel"] = currentIgnitionPower;

  doc["shortDur"] = shortBurst;
  doc["longDur"] = longBurst;
  doc["ready"] = booferReady;
  doc["sDelay"] = stageDelay;

  doc["s1Start"] = stage1Start;
  doc["s1End"] = stage1End;
  doc["s1int"] = stage1WarumupInterval;

  doc["s2End"] = stage2End;
  doc["s2Int"] = stage2WarumupInterval;

  doc["s3End"] = stage3End;
  doc["s3Int"] = stage3WarumupInterval;



  String output;
  serializeJson(doc, output);

  // Send the JSON string via the BLE characteristic
  Serial.print("Sending notify status: ");
  Serial.println(output.c_str());
  featuresCharacteristic.setValue(output.c_str());
}

void syncBluetoothSettings()
{
  if ((millis() - lastBluetoothSync) > DEFAULT_BT_REFRESH_INTERVAL && deviceConnected)
  {
    sendStatusUpdate();
    lastBluetoothSync = millis();
  }
}