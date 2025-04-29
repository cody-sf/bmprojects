#include <GlobalDefaults.h>
#include <DeviceRoles.h>
#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "Helpers.h"

static Device boofer;
#define SOLENOID_PIN 18
#define IGNITION_PIN 25 // The pin connected to TRIG/PWM on the MOSFET module
#define PRESSURE_PIN 34
#define SERVICE_UUID "17504d91-b22e-4455-a466-1521d6c4c4af"
#define FEATURES_UUID "8ce6e2d0-093b-456a-9c39-322b268e1bc0"

Preferences preferences;

BLEService booferService(SERVICE_UUID);
BLECharacteristic featuresCharacteristic(FEATURES_UUID, BLERead | BLEWrite | BLENotify, 512);
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastStatusSync = 0;
unsigned long lastConfigSync = 0;

// Variables
// Factory Defaults
const int FACTORY_STAGE_DELAY = 5000;
const int FACTORY_STAGE1_START = 140;
const int FACTORY_STAGE1_END = 215;
const int FACTORY_STAGE1_WARMUP = 500;
const int FACTORY_STAGE2_START = 216;
const int FACTORY_STAGE2_END = 225;
const int FACTORY_STAGE2_WARMUP = 2000;
const int FACTORY_STAGE3_START = 226;
const int FACTORY_STAGE3_END = 240;
const int FACTORY_STAGE3_WARMUP = 2500;
// Pressure Sensor Variables
const float FACTORY_VMin = 0.215;        // Voltage at 0 PSI
const float FACTORY_VMax = 2.217;        // Voltage at 60 PSI
const float FACTORY_PRESSURE_MAX = 60.0; // Max pressure in PSI
const int FACTORY_ADC_RES = 4095;        // ADC resolution for ESP32
float currentVoltage = 0.0;
float pressure = 0.0;

// Active Settings
// Ignition Variables
int stageDelay;
int stage1Start;
int stage1End;
int stage1WI;
int stage2Start;
int stage2End;
int stage2WI;
int stage3Start;
int stage3End;
int stage3WI;
float VoutMin;
float VoutMax;
float pressureMax;
int adcResolution;

bool ignitionPower = false;
bool valveOpen = false;
int valveTimeout = 5000;
int shortBurst = 100;
int longBurst = 1000;
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

    // Handle features
    switch (feature)
    {
    case 0x01: // On/Off
      ignitionPower = value[1] != 0;

      if (value[1] == 1 && !booferReady)
      {
        Serial.println("Warmup Ignition");
        warmupIgnition();
      }
      break;

    case 0x02: // Short Boof
      Serial.print("Short Boof: ");
      Serial.println(shortBurst);
      digitalWrite(SOLENOID_PIN, HIGH);
      delay(shortBurst);
      digitalWrite(SOLENOID_PIN, LOW);
      break;

    case 0x03: // Long Boof
      Serial.print("Long Boof: ");
      Serial.println(longBurst);
      digitalWrite(SOLENOID_PIN, HIGH);
      delay(longBurst);
      digitalWrite(SOLENOID_PIN, LOW);
      break;

    case 0x04: // Short Boof Duration
    {
      int d = 0;
      memcpy(&d, value.data() + 1, sizeof(int));
      shortBurst = d;
      Serial.print("Short Boof Duration: ");
      Serial.println(shortBurst);
    }
    break;

    case 0x05: // Long Boof Duration
    {
      int l = 0;
      memcpy(&l, value.data() + 1, sizeof(int));
      longBurst = l;
      Serial.print("Long Boof Duration: ");
      Serial.println(longBurst);
    }
    break;

    case 0x07: // stage1Start
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stage1Start = v;
      Serial.print("Updated stage1Start: ");
      Serial.println(stage1Start);
    }
    break;

    case 0x08: // stage1End
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stage1End = v;
      stage2Start = v + 1;
      Serial.print("Updated stage1End: ");
      Serial.println(stage1End);
    }
    break;

    case 0x09: // stage1Interval
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stage1WI = v;
      Serial.print("Updated stage1WI: ");
      Serial.println(stage1WI);
    }
    break;

    case 0x0A: // stage2End
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stage2End = v;
      stage3Start = v + 1;
      Serial.print("Updated stage2End: ");
      Serial.println(stage2End);
    }
    break;

    case 0x0B: // stage2Interval
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stage2WI = v;
      Serial.print("Updated stage2WI: ");
      Serial.println(stage2WI);
    }
    break;

    case 0x0C: // stage3End
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stage3End = v;
      Serial.print("Updated stage3End: ");
      Serial.println(stage3End);
    }
    break;

    case 0x0D: // stage3Interval
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stage3WI = v;
      Serial.print("Updated stage3WI: ");
      Serial.println(stage3WI);
    }
    break;

    case 0x0E: // stageDelay
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stageDelay = v;
      Serial.print("Updated stageDelay: ");
      Serial.println(stageDelay);
    }
    break;

    case 0x0F: // Save Settings
      Serial.println("Save Settings Command");
      saveSettings();
      break;

    case 0x10: // Restore Defaults
      Serial.println("Restore Defaults Command");
      restoreDefaults();
      break;

    default:
      Serial.println("Unknown feature received!");
      break;
    }
  }
}

void setup()
{

  Serial.begin(115200);
  loadSettings();

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

  // Set the resolution of the ADC to 12 bits
  analogReadResolution(12);
}

void loop()
{
  BLE.poll();
  handlePressureSensor();
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
  syncBluetoothStatus();
  syncBluetoothSettings();
}

// Warmup ignition with battery pack
void warmupIgnition()
{

  for (int dutyCycle = stage1Start; dutyCycle <= stage2End; dutyCycle += 1)
  {
    Serial.print("Currently power at: ");
    Serial.println(dutyCycle);
    currentIgnitionPower = dutyCycle;
    analogWrite(IGNITION_PIN, dutyCycle);
    delay(stage1WI); // Adjust the delay for how quickly you ramp up
    BLE.poll();
    syncBluetoothStatus();
  }
  delay(stageDelay);

  for (int dutyCycle = stage2Start; dutyCycle <= stage2End; dutyCycle += 1)
  {
    Serial.print("Currently power at: ");
    Serial.println(dutyCycle);
    currentIgnitionPower = dutyCycle;
    analogWrite(IGNITION_PIN, dutyCycle);
    delay(stage2WI); // Adjust the delay for how quickly you ramp up
    BLE.poll();
    syncBluetoothStatus();
  }
  delay(stageDelay);

  for (int dutyCycle = stage3Start; dutyCycle <= stage3End; dutyCycle += 1)
  {
    Serial.print("Currently power at: ");
    Serial.println(dutyCycle);
    currentIgnitionPower = dutyCycle;
    analogWrite(IGNITION_PIN, dutyCycle);
    delay(stage3WI); // Adjust the delay for how quickly you ramp up
    BLE.poll();
    syncBluetoothStatus();
  }

  booferReady = true;
}

void handlePressureSensor()
{
  int sensorValue = analogRead(PRESSURE_PIN);
  currentVoltage = (sensorValue / float(adcResolution)) * 3.3;
  pressure = ((currentVoltage - VoutMin) / (VoutMax - VoutMin)) * pressureMax;
  pressure = max(0.0f, pressure);
  // Serial.print("Sensor Value: ");
  // Serial.print(sensorValue);
  // Serial.print(", Voltage: ");
  // Serial.print(currentVoltage, 3); // Print voltage with 3 decimal places
  // Serial.print(" V, Pressure: ");
  // Serial.print(pressure, 2); // Print pressure with 2 decimal places
  // Serial.println(" PSI");
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
  doc["pLev"] = currentIgnitionPower;
  doc["ready"] = booferReady;
  doc["vol"] = currentVoltage;
  doc["pres"] = pressure;
  String output;
  serializeJson(doc, output);

  // Send the JSON string via the BLE characteristic
  Serial.print("Sending notify status: ");
  Serial.println(output.c_str());
  featuresCharacteristic.setValue(output.c_str());
}

void sendConfigStatusUpdate()
{
  StaticJsonDocument<512> doc;
  // Add all the settings and their values
  doc["sDur"] = shortBurst;
  doc["lDur"] = longBurst;
  doc["sDelay"] = stageDelay;
  doc["s1Start"] = stage1Start;
  doc["s1End"] = stage1End;
  doc["s1Int"] = stage1WI;
  doc["s2End"] = stage2End;
  doc["s2Int"] = stage2WI;
  doc["s3End"] = stage3End;
  doc["s3Int"] = stage3WI;
  doc["vmin"] = VoutMin;
  doc["vmax"] = VoutMax;
  doc["adcRes"] = adcResolution;

  String output;
  serializeJson(doc, output);

  // Send the JSON string via the BLE characteristic
  Serial.print("Sending config status: ");
  Serial.println(output.c_str());
  featuresCharacteristic.setValue(output.c_str());
}

void syncBluetoothStatus()
{
  if ((millis() - lastStatusSync) > 500 && deviceConnected)
  {
    sendStatusUpdate();
    lastStatusSync = millis();
  }
}

void syncBluetoothSettings()
{
  if ((millis() - lastConfigSync) > 5000 && deviceConnected)
  {
    sendConfigStatusUpdate();
    lastConfigSync = millis();
  }
}

// Preference Helpers
void restoreDefaults()
{
  stageDelay = FACTORY_STAGE_DELAY;

  stage1Start = FACTORY_STAGE1_START;
  stage1End = FACTORY_STAGE1_END;
  stage1WI = FACTORY_STAGE1_WARMUP;

  stage2Start = FACTORY_STAGE2_START;
  stage2End = FACTORY_STAGE2_END;
  stage2WI = FACTORY_STAGE2_WARMUP;

  stage3Start = FACTORY_STAGE3_START;
  stage3End = FACTORY_STAGE3_END;
  stage3WI = FACTORY_STAGE3_WARMUP;

  Serial.println("Settings restored to factory defaults.");
  saveSettings();
}

void saveSettings()
{
  preferences.begin("settings", false);

  preferences.putInt("stageDelay", stageDelay);
  preferences.putInt("stage1Start", stage1Start);
  preferences.putInt("stage1End", stage1End);
  preferences.putInt("stage1WI", stage1WI);
  preferences.putInt("stage2Start", stage2Start);
  preferences.putInt("stage2End", stage2End);
  preferences.putInt("stage2WI", stage2WI);
  preferences.putInt("stage3Start", stage3Start);
  preferences.putInt("stage3End", stage3End);
  preferences.putInt("stage3WI", stage3WI);
  preferences.putFloat("VoutMin", VoutMin);
  preferences.putFloat("VoutMax", VoutMax);
  preferences.putFloat("pressureMax", pressureMax);
  preferences.putInt("adcResolution", adcResolution);
  preferences.end();
  Serial.println("stage1WI saving: " + String(stage1WI));
  Serial.println("Settings saved!");
}

void loadSettings()
{
  preferences.begin("settings", true);

  stageDelay = preferences.getInt("stageDelay", FACTORY_STAGE_DELAY);
  stage1Start = preferences.getInt("stage1Start", FACTORY_STAGE1_START);
  stage1End = preferences.getInt("stage1End", FACTORY_STAGE1_END);
  stage1WI = preferences.getInt("stage1WI", FACTORY_STAGE1_WARMUP);
  stage2Start = preferences.getInt("stage2Start", FACTORY_STAGE2_START);
  stage2End = preferences.getInt("stage2End", FACTORY_STAGE2_END);
  stage2WI = preferences.getInt("stage2WI", FACTORY_STAGE2_WARMUP);
  stage3Start = preferences.getInt("stage3Start", FACTORY_STAGE3_START);
  stage3End = preferences.getInt("stage3End", FACTORY_STAGE3_END);
  stage3WI = preferences.getInt("stage3WI", FACTORY_STAGE3_WARMUP);
  VoutMin = preferences.getFloat("VoutMin", FACTORY_VMin);
  VoutMax = preferences.getFloat("VoutMax", FACTORY_VMax);
  pressureMax = preferences.getFloat("pressureMax", FACTORY_PRESSURE_MAX);
  adcResolution = preferences.getInt("adcResolution", FACTORY_ADC_RES);

  preferences.end();
  Serial.println("Settings loaded!");
}