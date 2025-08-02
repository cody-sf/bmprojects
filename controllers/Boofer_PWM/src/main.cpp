#include <GlobalDefaults.h>
#include <DeviceRoles.h>
#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// Forward declarations
void warmupIgnition();
void handleButtonPress();
void updateWarmupProcess();
void handlePressureSensor();
void blePeripheralConnectHandler(BLEDevice central);
void blePeripheralDisconnectHandler(BLEDevice central);
void sendStatusUpdate();
void sendConfigStatusUpdate();
void syncBluetoothStatus();
void syncBluetoothSettings();
void syncQuickStatus();
void saveSettings();
void loadSettings();
void restoreDefaults();
void checkValveTimer();

static Device boofer;
#define SOLENOID_PIN 18
#define IGNITION_PIN 25
#define PRESSURE_PIN 34
#define BUTTON_PIN 16
#define SERVICE_UUID "17504d91-b22e-4455-a466-1521d6c4c4af"
#define COMMAND_UUID "8ce6e2d0-093b-456a-9c39-322b268e1bc0" // Commands TO device
#define STATUS_UUID "7f3a0b45-8d2c-4789-a1b6-3e4f5c6d7e8f"  // Status FROM device

Preferences preferences;

BLEService booferService(SERVICE_UUID);
BLECharacteristic commandCharacteristic(COMMAND_UUID, BLEWrite, 512);
BLECharacteristic statusCharacteristic(STATUS_UUID, BLERead | BLENotify, 512);
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastStatusSync = 0;
unsigned long lastConfigSync = 0;
unsigned long lastQuickStatusSync = 0;

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

// Button Variables
const int SHORT_PRESS_TIME = 100;
const int OFF_TIME = 1000;
const int LONG_PRESS_TIME = 10000;  // 10 seconds for ignition toggle

// Button state tracking
bool buttonPressed = false;
bool lastButtonState = HIGH;  // Since we're using INPUT_PULLUP, released = HIGH
unsigned long buttonPressStart = 0;
bool longPressTriggered = false;
bool valveOpenedByButton = false;
bool valveClosedByTimeout = false;
unsigned long valveTimeoutTime = 0;

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

// Non-blocking warmup state machine
enum WarmupState
{
  WARMUP_IDLE,
  WARMUP_STAGE1,
  WARMUP_DELAY1,
  WARMUP_STAGE2,
  WARMUP_DELAY2,
  WARMUP_STAGE3,
  WARMUP_COMPLETE
};
WarmupState warmupState = WARMUP_IDLE;
unsigned long warmupLastUpdate = 0;
int warmupCurrentDutyCycle = 0;
int shortBurst = 100;
int longBurst = 1000;
int currentIgnitionPower = 0;
bool booferReady = false;
bool ignitionPower = false;
bool valveOpen = false;
unsigned long valveOpenTime = 0;
int valveTimeout = 5000; // Default 5 seconds - used for timed releases and safety
int valveActiveDuration = 0; // 0 = indefinite (button hold), >0 = timed release

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
    case 0x01: // On/Off
      if (dataLength >= 2)
      {
        Serial.print("Ignition command - value[1]: ");
        Serial.println(value[1]);
        ignitionPower = value[1] != 0;

        if (value[1] == 1 && !booferReady)
        {
          Serial.println("Warmup Ignition");
          warmupIgnition();
        }
      }
      else
      {
        Serial.println("Invalid ignition command - insufficient data");
      }
      break;

    case 0x02: // Short Boof
      Serial.print("Short Boof: ");
      Serial.println(shortBurst);
      digitalWrite(SOLENOID_PIN, HIGH);
      valveOpen = true;
      valveOpenTime = millis();
      valveActiveDuration = shortBurst;
      break;

    case 0x03: // Long Boof
      Serial.print("Long Boof: ");
      Serial.println(longBurst);
      digitalWrite(SOLENOID_PIN, HIGH);
      valveOpen = true;
      valveOpenTime = millis();
      valveActiveDuration = longBurst;
      break;

    case 0x35: // Short Boof Duration
    {
      int d = 0;
      memcpy(&d, value.data() + 1, sizeof(int));
      shortBurst = d;
      Serial.print("Short Boof Duration: ");
      Serial.println(shortBurst);
    }
    break;

    case 0x36: // Long Boof Duration
    {
      int l = 0;
      memcpy(&l, value.data() + 1, sizeof(int));
      longBurst = l;
      Serial.print("Long Boof Duration: ");
      Serial.println(longBurst);
    }
    break;

    case 0x37: // stage1Start
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stage1Start = v;
      Serial.print("Updated stage1Start: ");
      Serial.println(stage1Start);
    }
    break;

    case 0x42: // stage1End
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stage1End = v;
      stage2Start = v + 1;
      Serial.print("Updated stage1End: ");
      Serial.println(stage1End);
    }
    break;

    case 0x38: // stage1Interval
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stage1WI = v;
      Serial.print("Updated stage1WI: ");
      Serial.println(stage1WI);
    }
    break;

    case 0x39: // stage2End
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stage2End = v;
      stage3Start = v + 1;
      Serial.print("Updated stage2End: ");
      Serial.println(stage2End);
    }
    break;

    case 0x3A: // stage2Interval
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stage2WI = v;
      Serial.print("Updated stage2WI: ");
      Serial.println(stage2WI);
    }
    break;

    case 0x3B: // stage3End
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stage3End = v;
      Serial.print("Updated stage3End: ");
      Serial.println(stage3End);
    }
    break;

    case 0x3C: // stage3Interval
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stage3WI = v;
      Serial.print("Updated stage3WI: ");
      Serial.println(stage3WI);
    }
    break;

    case 0x3D: // stageDelay
    {
      int v = 0;
      memcpy(&v, value.data() + 1, sizeof(int));
      stageDelay = v;
      Serial.print("Updated stageDelay: ");
      Serial.println(stageDelay);
    }
    break;

    case 0x1C: // Save Settings
      Serial.println("Save Settings Command");
      saveSettings();
      break;

    case 0x34: // Restore Defaults
    {
      Serial.println("Restore Defaults Command");
      restoreDefaults();
    }
    break;

    case 0x3E: // handle release
    {
      if (dataLength >= 5)
      { // Need at least 1 byte for command + 4 bytes for int
        int release = 0;
        memcpy(&release, value.data() + 1, sizeof(int));
        Serial.print("Release command - Starting non-blocking release for ");
        Serial.print(release);
        Serial.println(" ms");

        // Start non-blocking release (prevents BLE corruption)
        digitalWrite(SOLENOID_PIN, HIGH);
        valveOpen = true;
        valveOpenTime = millis();
        valveActiveDuration = release;
      }
      else
      {
        Serial.print("Invalid release command - insufficient data. Length: ");
        Serial.println(dataLength);
      }
      break;
    }

    case 0x3F: // Open Valve
    {
      Serial.println("Opening valve");
      digitalWrite(SOLENOID_PIN, HIGH);
      valveOpen = true;
      valveOpenTime = millis();
      valveActiveDuration = valveTimeout; // Use safety timeout for manual open
      Serial.print("Valve opened. Timeout in ");
      Serial.print(valveTimeout);
      Serial.println(" ms");
      break;
    }

    case 0x40: // Close Valve
    {
      Serial.println("Closing valve");
      digitalWrite(SOLENOID_PIN, LOW);
      valveOpen = false;
      valveOpenTime = 0;
      valveActiveDuration = 0;
      break;
    }

    case 0x41: // Set Valve Timeout
    {
      int timeout = 0;
      memcpy(&timeout, value.data() + 1, sizeof(int));
      valveTimeout = timeout;
      Serial.print("Valve timeout set to: ");
      Serial.print(valveTimeout);
      Serial.println(" ms");
      break;
    }
    case 0x30: // set_owner
    {
      Serial.println("Setting owner");
      break;
    }

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
  booferService.addCharacteristic(commandCharacteristic);
  booferService.addCharacteristic(statusCharacteristic);
  BLE.addService(booferService);

  // assign event handlers for connected, disconnected to peripheral
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
  commandCharacteristic.setEventHandler(BLEWritten, featureCallback);

  // start advertising
  BLE.advertise();

  pinMode(IGNITION_PIN, OUTPUT);
  pinMode(SOLENOID_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  digitalWrite(IGNITION_PIN, LOW);
  digitalWrite(SOLENOID_PIN, LOW);

  // Set the resolution of the ADC to 12 bits
  analogReadResolution(12);
}

void loop()
{
  BLE.poll();
  handleButtonPress();
  checkValveTimer();
  updateWarmupProcess();
  handlePressureSensor();
  if (ignitionPower)
  {
    // Don't set booferReady here - let warmup completion control it
    // booferReady will be set to true only when warmup completes
  }
  else
  {
    booferReady = false;
    currentIgnitionPower = 0;
    analogWrite(IGNITION_PIN, 0);
    // Reset warmup state when turned off
    warmupState = WARMUP_IDLE;
  }
  syncQuickStatus();
  syncBluetoothStatus();
  syncBluetoothSettings();
}

void handleButtonPress()
{
  bool currentButtonState = digitalRead(BUTTON_PIN);
  unsigned long currentTime = millis();
  
  // Detect button press (transition from HIGH to LOW)
  if (lastButtonState == HIGH && currentButtonState == LOW)
  {
    buttonPressed = true;
    buttonPressStart = currentTime;
    longPressTriggered = false;
    valveOpenedByButton = false;
    Serial.println("Button press detected");
    
    // If ignition is on and ready, open valve immediately
    if (ignitionPower && booferReady)
    {
      Serial.println("Ignition ready - Opening valve for button hold");
      digitalWrite(SOLENOID_PIN, HIGH);
      valveOpen = true;
      valveOpenTime = millis();
      valveActiveDuration = 0; // Indefinite - controlled by button release
      valveOpenedByButton = true;
    }
    else if (!ignitionPower)
    {
      Serial.println("Ignition off - Button press ignored");
    }
    else if (ignitionPower && !booferReady)
    {
      Serial.println("Ignition warming up - Button press ignored");
    }
  }
  
  // Handle button being held down
  if (buttonPressed && currentButtonState == LOW)
  {
    unsigned long pressDuration = currentTime - buttonPressStart;
    
    // Keep valve open while button is held (if ignition is ready)
    if (ignitionPower && booferReady && !valveOpenedByButton)
    {
      Serial.println("Ignition became ready - Opening valve");
      digitalWrite(SOLENOID_PIN, HIGH);
      valveOpen = true;
      valveOpenTime = millis();
      valveActiveDuration = 0; // Indefinite - controlled by button release
      valveOpenedByButton = true;
    }
    else if ((!ignitionPower || !booferReady) && valveOpenedByButton)
    {
      Serial.println("Ignition no longer ready - Closing valve");
      digitalWrite(SOLENOID_PIN, LOW);
      valveOpen = false;
      valveOpenTime = 0;
      valveActiveDuration = 0;
      valveOpenedByButton = false;
    }
    
    // Check for long press (10 seconds) - toggle ignition
    if (pressDuration >= LONG_PRESS_TIME && !longPressTriggered)
    {
      longPressTriggered = true;
      Serial.println("Long press detected (10s) - Toggling ignition");
      
      // Close valve if it was opened by button before toggling ignition
      if (valveOpenedByButton)
      {
        Serial.println("Closing valve before ignition toggle");
        digitalWrite(SOLENOID_PIN, LOW);
        valveOpen = false;
        valveOpenTime = 0;
        valveActiveDuration = 0;
        valveOpenedByButton = false;
      }
      
      // Toggle ignition like bluetooth command
      ignitionPower = !ignitionPower;
      
      if (ignitionPower && !booferReady)
      {
        Serial.println("Starting warmup from button press");
        warmupIgnition();
      }
      else if (!ignitionPower)
      {
        Serial.println("Turning off ignition from button press");
        // The main loop will handle turning off ignition and resetting warmup
      }
    }
  }
  
  // Detect button release (transition from LOW to HIGH)
  if (lastButtonState == LOW && currentButtonState == HIGH)
  {
    Serial.println("Button released");
    
    // Close valve if it was opened by button
    if (valveOpenedByButton)
    {
      Serial.println("Closing valve on button release");
      digitalWrite(SOLENOID_PIN, LOW);
      valveOpen = false;
      valveOpenTime = 0;
      valveActiveDuration = 0;
      valveOpenedByButton = false;
    }
    
    // Reset timeout tracking when button is released
    if (valveClosedByTimeout)
    {
      Serial.println("Button released - Canceling ignition shutoff timer");
      valveClosedByTimeout = false;
      valveTimeoutTime = 0;
    }
    
    // Handle quick tap (short press) - only if ignition didn't toggle
    if (buttonPressed && !longPressTriggered)
    {
      unsigned long pressDuration = currentTime - buttonPressStart;
      
      if (pressDuration >= SHORT_PRESS_TIME && pressDuration < LONG_PRESS_TIME)
      {
        Serial.println("Quick tap detected");
        
                 if (ignitionPower && booferReady)
         {
           // Ignition is ready - trigger short valve release
           Serial.println("Quick tap - Triggering short burst");
           digitalWrite(SOLENOID_PIN, HIGH);
           valveOpen = true;
           valveOpenTime = currentTime;
           valveActiveDuration = shortBurst;
         }
        else
        {
          Serial.println("Ignition not ready - Quick tap ignored");
        }
      }
    }
    
    // Reset button state
    buttonPressed = false;
    longPressTriggered = false;
  }
  
  lastButtonState = currentButtonState;
}
// Start non-blocking warmup process
void warmupIgnition()
{
  Serial.println("Starting non-blocking warmup process");
  warmupState = WARMUP_STAGE1;
  warmupCurrentDutyCycle = stage1Start;
  warmupLastUpdate = millis();
  currentIgnitionPower = warmupCurrentDutyCycle;
  analogWrite(IGNITION_PIN, warmupCurrentDutyCycle);
}

void handlePressureSensor()
{
  int sensorValue = analogRead(PRESSURE_PIN);
  currentVoltage = (sensorValue / float(adcResolution)) * 3.3;
  pressure = ((currentVoltage - VoutMin) / (VoutMax - VoutMin)) * pressureMax;
  pressure = max(0.0f, pressure); 
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
  doc["ignPwr"] = ignitionPower;
  doc["pLev"] = currentIgnitionPower;
  doc["ready"] = booferReady;
  doc["vol"] = currentVoltage;
  doc["pres"] = pressure;
  doc["valveOpen"] = valveOpen;
  String output;
  serializeJson(doc, output);

  // Send the JSON string via the BLE characteristic
  Serial.print("Sending notify status: ");
  Serial.println(output.c_str());
  statusCharacteristic.setValue(output.c_str());
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
  doc["valveTimeout"] = valveTimeout;

  String output;
  serializeJson(doc, output);

  // Send the JSON string via the BLE characteristic
  Serial.print("Sending config status: ");
  Serial.println(output.c_str());
  statusCharacteristic.setValue(output.c_str());
}

void sendQuickStatusUpdate()
{
  StaticJsonDocument<128> doc;
  doc["ignPwr"] = ignitionPower;
  doc["pLev"] = currentIgnitionPower;
  doc["ready"] = booferReady;
  doc["pres"] = pressure;
  doc["valveOpen"] = valveOpen;
  String output;
  serializeJson(doc, output);
  Serial.print("Sending quick status: ");
  Serial.println(output.c_str());
  statusCharacteristic.setValue(output.c_str());
}
void syncBluetoothStatus()
{
  if ((millis() - lastStatusSync) > 10000 && deviceConnected)
  {
    sendStatusUpdate();
    lastStatusSync = millis();
  }
}

void syncBluetoothSettings()
{
  if ((millis() - lastConfigSync) > 10000 && deviceConnected)
  {
    sendConfigStatusUpdate();
    lastConfigSync = millis();
  }
}

void syncQuickStatus()
{
  if ((millis() - lastQuickStatusSync) > 2000 && deviceConnected)
  {
    sendQuickStatusUpdate();
    lastQuickStatusSync = millis();
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

  // Restore pressure sensor calibration values
  VoutMin = FACTORY_VMin;
  VoutMax = FACTORY_VMax;
  pressureMax = FACTORY_PRESSURE_MAX;
  adcResolution = FACTORY_ADC_RES;

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
  preferences.putInt("valveTimeout", valveTimeout);
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
  valveTimeout = preferences.getInt("valveTimeout", 5000); // Default 5 seconds

  preferences.end();
  Serial.println("Settings loaded!");
}

void checkValveTimer()
{
  if (valveOpen)
  {
    unsigned long currentTime = millis();
    unsigned long valveOpenDuration = currentTime - valveOpenTime;
    
    // Check if valve should close due to its specific duration
    bool shouldClose = false;
    if (valveActiveDuration > 0 && valveOpenDuration >= valveActiveDuration)
    {
      Serial.println("Valve timer complete - Closing valve");
      shouldClose = true;
    }
    // Always apply safety limit
    else if (valveOpenDuration >= valveTimeout)
    {
      Serial.print("Valve safety timeout (");
      Serial.print(valveTimeout);
      Serial.println("ms) - Closing valve");
      shouldClose = true;
      
      // If this was a button hold, start ignition shutoff timer
      if (valveOpenedByButton && buttonPressed)
      {
        Serial.println("Button still held after valve timeout - Starting ignition shutoff timer");
        valveClosedByTimeout = true;
        valveTimeoutTime = currentTime;
      }
    }
    
    if (shouldClose)
    {
      digitalWrite(SOLENOID_PIN, LOW);
      valveOpen = false;
      valveOpenTime = 0;
      valveActiveDuration = 0;
    }
  }
  
  // Check for ignition shutoff after valve timeout (button holds only)
  if (valveClosedByTimeout && buttonPressed)
  {
    unsigned long currentTime = millis();
    if (currentTime - valveTimeoutTime >= valveTimeout) // Another timeout period
    {
      Serial.print("Button held ");
      Serial.print(valveTimeout);
      Serial.println("ms after valve timeout - Turning off ignition");
      ignitionPower = false;
      valveClosedByTimeout = false;
      valveTimeoutTime = 0;
    }
  }
}

void updateWarmupProcess()
{
  if (warmupState == WARMUP_IDLE)
    return;

  unsigned long currentTime = millis();

  switch (warmupState)
  {
  case WARMUP_STAGE1:
    if (currentTime - warmupLastUpdate >= stage1WI)
    {
      warmupCurrentDutyCycle++;
      if (warmupCurrentDutyCycle <= stage1End)
      {
        Serial.print("Stage 1 - Power at: ");
        Serial.println(warmupCurrentDutyCycle);
        currentIgnitionPower = warmupCurrentDutyCycle;
        analogWrite(IGNITION_PIN, warmupCurrentDutyCycle);
        warmupLastUpdate = currentTime;
      }
      else
      {
        Serial.println("Stage 1 complete, starting delay");
        warmupState = WARMUP_DELAY1;
        warmupLastUpdate = currentTime;
      }
    }
    break;

  case WARMUP_DELAY1:
    if (currentTime - warmupLastUpdate >= stageDelay)
    {
      Serial.println("Starting Stage 2");
      warmupState = WARMUP_STAGE2;
      warmupCurrentDutyCycle = stage2Start;
      warmupLastUpdate = currentTime;
    }
    break;

  case WARMUP_STAGE2:
    if (currentTime - warmupLastUpdate >= stage2WI)
    {
      warmupCurrentDutyCycle++;
      if (warmupCurrentDutyCycle <= stage2End)
      {
        Serial.print("Stage 2 - Power at: ");
        Serial.println(warmupCurrentDutyCycle);
        currentIgnitionPower = warmupCurrentDutyCycle;
        analogWrite(IGNITION_PIN, warmupCurrentDutyCycle);
        warmupLastUpdate = currentTime;
      }
      else
      {
        Serial.println("Stage 2 complete, starting delay");
        warmupState = WARMUP_DELAY2;
        warmupLastUpdate = currentTime;
      }
    }
    break;

  case WARMUP_DELAY2:
    if (currentTime - warmupLastUpdate >= stageDelay)
    {
      Serial.println("Starting Stage 3");
      warmupState = WARMUP_STAGE3;
      warmupCurrentDutyCycle = stage3Start;
      warmupLastUpdate = currentTime;
    }
    break;

  case WARMUP_STAGE3:
    if (currentTime - warmupLastUpdate >= stage3WI)
    {
      warmupCurrentDutyCycle++;
      if (warmupCurrentDutyCycle <= stage3End)
      {
        Serial.print("Stage 3 - Power at: ");
        Serial.println(warmupCurrentDutyCycle);
        currentIgnitionPower = warmupCurrentDutyCycle;
        analogWrite(IGNITION_PIN, warmupCurrentDutyCycle);
        warmupLastUpdate = currentTime;
      }
      else
      {
        Serial.println("Warmup complete!");
        warmupState = WARMUP_COMPLETE;
        booferReady = true;
      }
    }
    break;

  case WARMUP_COMPLETE:
    warmupState = WARMUP_IDLE;
    break;

  default:
    break;
  }
}