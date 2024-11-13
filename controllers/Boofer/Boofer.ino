#include <GlobalDefaults.h>
#include <DeviceRoles.h>
#include <ArduinoBLE.h>
#include "Helpers.h"
#include <ArduinoJson.h>

static Device boofer;
#define SOLENOID_PIN 27
#define RELAY_PIN 18
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
int shortBurst = 500;
int longBurst = 1000;
int warmupTime = 10000;

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

      Serial.print("Ignition: ");
      Serial.println(ignitionPower ? "On" : "Off");

      if (value[1] == 1) {
        Serial.println("Running if statement");
        warmupIgnition();
      }

      Serial.println("If statement over");
      
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

    // Ignition warmup time
    if (feature == 0x06) 
    {
      int w = 0;
      memcpy(&w, value.data() + 1, sizeof(int));
      warmupTime = w;
      Serial.print("Warmup Time: ");
      Serial.println(warmupTime);
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
  
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(IGNITION_PIN, OUTPUT);
  pinMode(SOLENOID_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(IGNITION_PIN, LOW);
  digitalWrite(SOLENOID_PIN, LOW);
}

void loop()
{
  BLE.poll();

  if (ignitionPower)
  {
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(IGNITION_PIN, HIGH); 
  }
  else
  {
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(IGNITION_PIN, LOW);
  }
}

// Warmup ignition with battery pack
void warmupIgnition() {
  Serial.println("Warmup in progress");
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(IGNITION_PIN, HIGH);
  delay(warmupTime);
  Serial.println("Warmup complete, starting USB");
  digitalWrite(RELAY_PIN, HIGH);
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