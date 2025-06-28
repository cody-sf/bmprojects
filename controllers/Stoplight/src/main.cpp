#include <GlobalDefaults.h>
#include <DeviceRoles.h>
#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// Stoplight States - Define enums first
enum StoplightState {
  STATE_OFF = 0,
  STATE_RED = 1,
  STATE_RED_YELLOW = 2,  // European style only
  STATE_GREEN = 3,
  STATE_YELLOW = 4
};

// Stoplight Modes
enum StoplightMode {
  MODE_AMERICAN = 0,    // Red -> Green -> Yellow -> Red
  MODE_EUROPEAN = 1     // Red -> Red+Yellow -> Green -> Yellow -> Red
};

// Forward declarations
void updateStoplightState();
void changeToState(StoplightState newState);
void blePeripheralConnectHandler(BLEDevice central);
void blePeripheralDisconnectHandler(BLEDevice central);
void sendStatusUpdate();
void sendConfigStatusUpdate();
void syncBluetoothStatus();
void syncBluetoothSettings();
void saveSettings();
void loadSettings();
void restoreDefaults();
void setLights(bool red, bool yellow, bool green);

static Device stoplight;

// Pin definitions for relay control
#define RED_RELAY_PIN 18
#define YELLOW_RELAY_PIN 19
#define GREEN_RELAY_PIN 21

// BLE Service and Characteristics
#define SERVICE_UUID "fbf35ec3-2ea1-471f-8814-763b47468683"
#define COMMAND_UUID "6108c3ed-5b10-45bf-92ae-72d8901959c9" // Commands TO device
#define STATUS_UUID "88a4f153-c7dc-4987-a784-03adb1e09ce9"  // Status FROM device

Preferences preferences;

BLEService stoplightService(SERVICE_UUID);
BLECharacteristic commandCharacteristic(COMMAND_UUID, BLEWrite, 512);
BLECharacteristic statusCharacteristic(STATUS_UUID, BLERead | BLENotify, 512);
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastStatusSync = 0;
unsigned long lastConfigSync = 0;

// Factory Defaults (all times in milliseconds)
const int FACTORY_RED_DURATION = 30000;      // 30 seconds
const int FACTORY_YELLOW_DURATION = 3000;    // 3 seconds
const int FACTORY_GREEN_DURATION = 25000;    // 25 seconds
const int FACTORY_RED_YELLOW_DURATION = 2000; // 2 seconds (European only)
const StoplightMode FACTORY_MODE = MODE_AMERICAN;
const bool FACTORY_AUTO_CYCLE = true;

// Active Settings
int redDuration;
int yellowDuration;
int greenDuration;
int redYellowDuration;
StoplightMode stoplightMode;
bool autoCycle;

// State Management
StoplightState currentState = STATE_OFF;
StoplightState nextState = STATE_OFF;
unsigned long stateChangeTime = 0;
bool stoplightActive = true;

// Light status for feedback
bool redOn = false;
bool yellowOn = false;
bool greenOn = false;

void featureCallback(BLEDevice central, BLECharacteristic characteristic)
{
  const uint8_t *buffer = commandCharacteristic.value();
  unsigned int dataLength = commandCharacteristic.valueLength();
  std::string value((char *)buffer, dataLength);

  Serial.print("BLE Command Debug - Length: ");
  Serial.print(dataLength);
  Serial.print(", Raw bytes: ");
  for (int i = 0; i < dataLength; i++)
  {
    Serial.print("0x");
    Serial.print(buffer[i], HEX);
    Serial.print(" ");
  }
  Serial.println();

  if (!value.empty() && dataLength > 0)
  {
    uint8_t feature = value[0];
    Serial.print("Processing Feature: 0x");
    Serial.println(feature, HEX);

    // Handle features
    switch (feature)
    {
    case 0x01: // On/Off Auto Cycle
      if (dataLength >= 2)
      {
        Serial.print("Auto cycle command - value[1]: ");
        Serial.println(value[1]);
        autoCycle = value[1] != 0;
        
        if (autoCycle) {
          stoplightActive = true;
          changeToState(STATE_RED);
          Serial.println("Starting auto cycle");
        } else {
          stoplightActive = false;
          changeToState(STATE_OFF);
          Serial.println("Stopping auto cycle");
        }
      }
      else
      {
        Serial.println("Invalid auto cycle command - insufficient data");
      }
      break;

    case 0x02: // Manual Red Light
      Serial.println("Manual Red Light");
      stoplightActive = false;
      autoCycle = false;
      changeToState(STATE_RED);
      break;

    case 0x03: // Manual Yellow Light
      Serial.println("Manual Yellow Light");
      stoplightActive = false;
      autoCycle = false;
      changeToState(STATE_YELLOW);
      break;

    case 0x04: // Manual Green Light
      Serial.println("Manual Green Light");
      stoplightActive = false;
      autoCycle = false;
      changeToState(STATE_GREEN);
      break;

    case 0x05: // Manual Red+Yellow (European)
      Serial.println("Manual Red+Yellow Light");
      stoplightActive = false;
      autoCycle = false;
      changeToState(STATE_RED_YELLOW);
      break;

    case 0x06: // All Off
      Serial.println("All Lights Off");
      stoplightActive = false;
      autoCycle = false;
      changeToState(STATE_OFF);
      break;

    case 0x07: // Red Duration
    {
      int duration = 0;
      memcpy(&duration, value.data() + 1, sizeof(int));
      redDuration = duration;
      Serial.print("Updated Red Duration: ");
      Serial.println(redDuration);
    }
    break;

    case 0x08: // Yellow Duration
    {
      int duration = 0;
      memcpy(&duration, value.data() + 1, sizeof(int));
      yellowDuration = duration;
      Serial.print("Updated Yellow Duration: ");
      Serial.println(yellowDuration);
    }
    break;

    case 0x09: // Green Duration
    {
      int duration = 0;
      memcpy(&duration, value.data() + 1, sizeof(int));
      greenDuration = duration;
      Serial.print("Updated Green Duration: ");
      Serial.println(greenDuration);
    }
    break;

    case 0x0A: // Red+Yellow Duration (European)
    {
      int duration = 0;
      memcpy(&duration, value.data() + 1, sizeof(int));
      redYellowDuration = duration;
      Serial.print("Updated Red+Yellow Duration: ");
      Serial.println(redYellowDuration);
    }
    break;

    case 0x0B: // Stoplight Mode (American/European)
    {
      int mode = 0;
      memcpy(&mode, value.data() + 1, sizeof(int));
      stoplightMode = (StoplightMode)mode;
      Serial.print("Updated Stoplight Mode: ");
      Serial.println(stoplightMode == MODE_AMERICAN ? "American" : "European");
    }
    break;

    case 0x0F: // Save Settings
      Serial.println("Save Settings Command");
      saveSettings();
      break;

    case 0x10: // Restore Defaults
    {
      Serial.println("Restore Defaults Command");
      restoreDefaults();
    }
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

  // Set device name BEFORE setting up services
  BLE.setLocalName("Stoplight-XX");
  BLE.setDeviceName("Stoplight-XX");
  
  Serial.println("BLE Name set to: Stoplight-XX");

  // Set up the service and characteristics
  BLE.setAdvertisedService(stoplightService);
  stoplightService.addCharacteristic(commandCharacteristic);
  stoplightService.addCharacteristic(statusCharacteristic);
  BLE.addService(stoplightService);

  // assign event handlers for connected, disconnected to peripheral
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
  commandCharacteristic.setEventHandler(BLEWritten, featureCallback);

  // start advertising
  BLE.advertise();
  
  Serial.println("BLE advertising started with name: Stoplight-XX");
  Serial.print("Service UUID: ");
  Serial.println(SERVICE_UUID);

  // Initialize relay pins
  pinMode(RED_RELAY_PIN, OUTPUT);
  pinMode(YELLOW_RELAY_PIN, OUTPUT);
  pinMode(GREEN_RELAY_PIN, OUTPUT);

  // Start with all lights off initially
  setLights(false, false, false);
  
  // If auto-cycle is enabled, start the cycling process
  if (stoplightActive && autoCycle) {
    Serial.println("Auto-cycle enabled, starting stoplight sequence...");
    changeToState(STATE_RED);
  }
  
  Serial.println("Stoplight Controller Ready!");
}

void loop()
{
  BLE.poll();
  updateStoplightState();
  syncBluetoothStatus();
  syncBluetoothSettings();
}

void updateStoplightState()
{
  if (!stoplightActive || !autoCycle) {
    return;
  }

  unsigned long currentTime = millis();
  unsigned long stateDuration = 0;

  // Determine how long current state should last
  switch (currentState) {
    case STATE_RED:
      stateDuration = redDuration;
      break;
    case STATE_RED_YELLOW:
      stateDuration = redYellowDuration;
      break;
    case STATE_GREEN:
      stateDuration = greenDuration;
      break;
    case STATE_YELLOW:
      stateDuration = yellowDuration;
      break;
    default:
      return;
  }

  // Check if it's time to change state
  if (currentTime - stateChangeTime >= stateDuration) {
    StoplightState nextState;
    
    if (stoplightMode == MODE_AMERICAN) {
      // American sequence: Red -> Green -> Yellow -> Red
      switch (currentState) {
        case STATE_RED:
          nextState = STATE_GREEN;
          break;
        case STATE_GREEN:
          nextState = STATE_YELLOW;
          break;
        case STATE_YELLOW:
          nextState = STATE_RED;
          break;
        default:
          nextState = STATE_RED;
          break;
      }
    } else {
      // European sequence: Red -> Red+Yellow -> Green -> Yellow -> Red
      switch (currentState) {
        case STATE_RED:
          nextState = STATE_RED_YELLOW;
          break;
        case STATE_RED_YELLOW:
          nextState = STATE_GREEN;
          break;
        case STATE_GREEN:
          nextState = STATE_YELLOW;
          break;
        case STATE_YELLOW:
          nextState = STATE_RED;
          break;
        default:
          nextState = STATE_RED;
          break;
      }
    }
    
    changeToState(nextState);
  }
}

void changeToState(StoplightState newState)
{
  currentState = newState;
  stateChangeTime = millis();
  
  Serial.print("Changing to state: ");
  Serial.println(currentState);
  
  switch (currentState) {
    case STATE_OFF:
      setLights(false, false, false);
      break;
    case STATE_RED:
      setLights(true, false, false);
      break;
    case STATE_RED_YELLOW:
      setLights(true, true, false);
      break;
    case STATE_GREEN:
      setLights(false, false, true);
      break;
    case STATE_YELLOW:
      setLights(false, true, false);
      break;
  }
  
  // Send immediate status update when state changes
  if (deviceConnected) {
    Serial.println("State changed - sending immediate status update");
    sendStatusUpdate();
  }
}

void setLights(bool red, bool yellow, bool green)
{
  redOn = red;
  yellowOn = yellow;
  greenOn = green;
  
  digitalWrite(RED_RELAY_PIN, red ? HIGH : LOW);
  digitalWrite(YELLOW_RELAY_PIN, yellow ? HIGH : LOW);
  digitalWrite(GREEN_RELAY_PIN, green ? HIGH : LOW);
  
  Serial.print("Lights - Red: ");
  Serial.print(red ? "ON" : "OFF");
  Serial.print(", Yellow: ");
  Serial.print(yellow ? "ON" : "OFF");
  Serial.print(", Green: ");
  Serial.println(green ? "ON" : "OFF");
}

// BLE helpers
void blePeripheralConnectHandler(BLEDevice central)
{
  Serial.print("Connected event, central: ");
  Serial.println(central.address());
  deviceConnected = true;
  
  // Send initial status update immediately after connection
  delay(100); // Small delay to ensure connection is stable
  sendStatusUpdate();
  sendConfigStatusUpdate();
}

void blePeripheralDisconnectHandler(BLEDevice central)
{
  Serial.print("Disconnected event, central: ");
  Serial.println(central.address());
  deviceConnected = false;
}

void sendStatusUpdate()
{
  StaticJsonDocument<512> doc;
  doc["active"] = stoplightActive;
  doc["autoCycle"] = autoCycle;
  doc["state"] = currentState;
  doc["mode"] = stoplightMode;
  doc["redOn"] = redOn;
  doc["yellowOn"] = yellowOn;
  doc["greenOn"] = greenOn;
  doc["timeInState"] = millis() - stateChangeTime;
  
  String output;
  serializeJson(doc, output);

  Serial.print("Sending notify status: ");
  Serial.println(output.c_str());
  statusCharacteristic.writeValue(output.c_str());
}

void sendConfigStatusUpdate()
{
  StaticJsonDocument<512> doc;
  doc["redDur"] = redDuration;
  doc["yellowDur"] = yellowDuration;
  doc["greenDur"] = greenDuration;
  doc["redYellowDur"] = redYellowDuration;
  doc["mode"] = stoplightMode;
  doc["autoCycle"] = autoCycle;

  String output;
  serializeJson(doc, output);

  Serial.print("Sending config status: ");
  Serial.println(output.c_str());
  statusCharacteristic.writeValue(output.c_str());
}

void syncBluetoothStatus()
{
  if ((millis() - lastStatusSync) > 500 && deviceConnected)
  {
    Serial.println("Syncing status...");
    sendStatusUpdate();
    lastStatusSync = millis();
  }
}

void syncBluetoothSettings()
{
  if ((millis() - lastConfigSync) > 2000 && deviceConnected)  // Reduced from 5000ms to 2000ms for testing
  {
    Serial.println("Syncing config...");
    sendConfigStatusUpdate();
    lastConfigSync = millis();
  }
}

void restoreDefaults()
{
  redDuration = FACTORY_RED_DURATION;
  yellowDuration = FACTORY_YELLOW_DURATION;
  greenDuration = FACTORY_GREEN_DURATION;
  redYellowDuration = FACTORY_RED_YELLOW_DURATION;
  stoplightMode = FACTORY_MODE;
  autoCycle = FACTORY_AUTO_CYCLE;

  Serial.println("Settings restored to factory defaults.");
  saveSettings();
}

void saveSettings()
{
  preferences.begin("stoplight", false);

  preferences.putInt("redDur", redDuration);
  preferences.putInt("yellowDur", yellowDuration);
  preferences.putInt("greenDur", greenDuration);
  preferences.putInt("redYellowDur", redYellowDuration);
  preferences.putInt("mode", (int)stoplightMode);
  preferences.putBool("autoCycle", autoCycle);
  
  preferences.end();
  Serial.println("Settings saved!");
}

void loadSettings()
{
  preferences.begin("stoplight", true);

  redDuration = preferences.getInt("redDur", FACTORY_RED_DURATION);
  yellowDuration = preferences.getInt("yellowDur", FACTORY_YELLOW_DURATION);
  greenDuration = preferences.getInt("greenDur", FACTORY_GREEN_DURATION);
  redYellowDuration = preferences.getInt("redYellowDur", FACTORY_RED_YELLOW_DURATION);
  stoplightMode = (StoplightMode)preferences.getInt("mode", FACTORY_MODE);
  autoCycle = preferences.getBool("autoCycle", FACTORY_AUTO_CYCLE);

  preferences.end();
  Serial.println("Settings loaded!");
} 