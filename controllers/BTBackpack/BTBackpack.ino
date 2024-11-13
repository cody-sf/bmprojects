// #include <math.h>
#include <FastLED.h>
#include <GlobalDefaults.h>
#include <LightShow.h>
#include <LocationService.h>
#include <Position.h>
#include <DeviceRoles.h>
#include <ArduinoBLE.h>
#include "Helpers.h"
#include <ArduinoJson.h>
#include "SyncController.h"

// 26 for AShton/Vinny
// 27 for Cody/ Aleg
#define LED_OUTPUT_PIN 26
#define NUM_LEDS 192
#define COLOR_ORDER GRB

//Button pin
// Vinny/Ashton 2
// Cody Aleg 18
#define BUTTON_PIN 2
static Device backpack;

// BT Setup
// UUIDS
// https://www.uuidgenerator.net/
#define SERVICE_UUID "be03096f-9322-4360-bc84-0f977c5c3c10"
#define FEATURES_UUID "24dcb43c-d457-4de0-a968-9cdc9d60392c"
#define STATUS_UUID "71a0cb09-7998-4774-83b5-1a5f02f205fb"
BLEService backpackService(SERVICE_UUID);
BLECharacteristic featuresCharacteristic(FEATURES_UUID, BLERead | BLEWrite | BLENotify, 512);
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastBluetoothSync = 0;

static Clock backpackClock;
static CRGB leds[NUM_LEDS];

static CLEDController &led_controller_1 = FastLED.addLeds<WS2812B, LED_OUTPUT_PIN, COLOR_ORDER>(leds, NUM_LEDS);
static std::vector<CLEDController *> led_controllers = {&led_controller_1};
static LightShow light_show(led_controllers);

static LocationService location_service;
unsigned long lastGpsPolledTIme = 0;
// Rewrite control center to manage location service and light show
// static ControlCenter control_center(location_service, light_show, backpack);
SyncController syncController(light_show, CURRENT_USER, backpack);

// Variables
LightSceneID currentScene = palette_stream;
bool power = true;
std::pair<CRGBPalette16, CRGBPalette16> palettes = light_show.getPrimarySecondaryPalettes();
CRGBPalette16 primaryPalette = palettes.first;
AvailablePalettes AP_palette = cool;
uint16_t speed = 100;
int brightness = 10;
bool reverseStrip = true;
CRGB speedoColor1 = CRGB::Red;
CRGB speedoColor2 = CRGB::Blue;

// Button Setup
// Button Press Stuff
#define DEBOUNCE_DELAY 50
int lastState = LOW; // the previous state from the input pin
int currentState;    // the current reading from the input pin
unsigned long pressedTime = 0;
unsigned long releasedTime = 0;
const int SHORT_PRESS_TIME = 1000;
const int OFF_TIME = 5000;

// Status Templates
void sendStatusUpdate()
{
  StaticJsonDocument<512> doc;
  LightScene deviceScene = light_show.getCurrentScene();

  // Add all the settings and their values
  doc["power"] = power;
  doc["currentSceneId"] = currentScene; // You may want to convert this to a string if it's an enum
  doc["brightness"] = deviceScene.brightness;
  doc["speed"] = deviceScene.speed;
  doc["direction"] = reverseStrip;
  doc["palette"] = paletteToString(deviceScene.primary_palette); // Assume you have a function to convert palette to string
  doc["position_available"] = location_service.is_current_position_available();
  doc["current_speed"] = location_service.current_speed();
  JsonObject positionObject = doc.createNestedObject("current_position");
  location_service.current_position().toJson(positionObject);
  String output;
  serializeJson(doc, output);

  // Send the JSON string via the BLE characteristic
  Serial.print("Sending notify status: ");
  Serial.println(output.c_str());
  featuresCharacteristic.setValue(output.c_str());
}

void setMode(std::string show)
{
  if (show == "pstream")
  {
    Serial.println("Palette Stream");
    currentScene = palette_stream;
    syncController.changeMode(palette_stream);
    return;
  }

  if (show == "pcycle")
  {
    Serial.println("Palette Cycle");
    currentScene = palette_cycle;
    syncController.changeMode(palette_cycle);
    return;
  }

  if (show == "speedo")
  {
    Serial.println("Speedometer");
    currentScene = speedometer;
    syncController.speedometer(location_service, speedoColor1, speedoColor2);
    return;
  }

  if (show == "cwheel")
  {
    Serial.println("Color Wheel");
    currentScene = color_wheel;
    syncController.colorWheel(location_service);
    return;
  }

  if (show == "cradial")
  {
    Serial.println("Color Radial");
    currentScene = color_radial;
    syncController.colorRadial(location_service, CRGB::White, CRGB::Red);
    return;
  }

  if (show == "pstat")
  {
    Serial.println("Position Status");
    currentScene = position_status;
    syncController.positionStatus(location_service);
    return;
  }
}

std::string toLowerCase(const std::string &input)
{
  std::string output = input;
  for (auto &c : output)
    c = tolower(c);
  return output;
}

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
      power = value[1] != 0;
      Serial.print("Power: ");
      Serial.println(power ? "On" : "Off");
      return;
    }

    // Mode
    if (feature == 0x02)
    {
      Serial.println("Mode: ");
      std::string strValue = value.substr(1);
      std::transform(strValue.begin(), strValue.end(), strValue.begin(), ::tolower);
      Serial.print(strValue.c_str());
      setMode(strValue);
      return;
    }

    // Palette
    if (feature == 0x03)
    {
      Serial.println("Primary Palette: ");
      std::string strValue = toLowerCase(value.substr(1));
      AP_palette = stringToPalette(strValue.c_str());
      Serial.print(strValue.c_str());
      primaryPalette = light_show.getPrimarySecondaryPalettes().first;
      syncController.setPalette(AP_palette);
      return;
    }

    // Brightness
    if (feature == 0x04)
    {
      int b = 0;
      memcpy(&b, value.data() + 1, sizeof(int));
      Serial.print("BT Brightness: ");
      Serial.println(b);
      brightness = b;
      syncController.setBrightness(brightness);
      return;
    }

    // Speed
    if (feature == 0x05)
    {
      int s = 0;
      memcpy(&s, value.data() + 1, sizeof(int));
      Serial.print("Speed: ");
      Serial.println(s);
      speed = s;
      Serial.println("Speed sent from BT: ");
      Serial.print(speed);
      syncController.setSpeed(speed);
      return;
    }

    // Direction
    if (feature == 0x06)
    {
      reverseStrip = value[1] != 0;
      Serial.print("Received direction: ");
      Serial.println(reverseStrip ? "True" : "False");
      syncController.setDirection(reverseStrip);
      return;
    }

    // Change Origin
    if (feature == 0x07)
    {
      // This isn't working
      if (dataLength == 9) // Ensure 9 bytes are received
      {
        float latitude;
        float longitude;
        memcpy(&latitude, value.data() + 1, sizeof(float));  // Extract latitude
        memcpy(&longitude, value.data() + 5, sizeof(float)); // Extract longitude
        Serial.print("Received Data Length: ");
        Serial.println(dataLength);
        Serial.print("Received new origin - Latitude: ");
        Serial.print(latitude, 6); // Print with 6 decimal places
        Serial.print(", Longitude: ");
        Serial.println(longitude, 6); // Print with 6 decimal places

        Position newOrigin(latitude, longitude);
        syncController.setOrigin(newOrigin);
      }
      else
      {
        Serial.println("Invalid data length for origin update.");
      }
      return;
    }

    // Ping location and speed
    if (feature == 0x08)
    {

      return;
    }
    if (feature == 0x09)
    {
      Serial.println("Speedometer Colors: ");
      if (value.length() == 7)
      {                                                        // 1 byte for feature, 6 bytes for 2 colors
        String color1Hex = String(value.substr(1, 3).c_str()); // First 3 bytes for color1
        String color2Hex = String(value.substr(4, 3).c_str()); // Next 3 bytes for color2

        CRGB color1 = CRGB(value[1], value[2], value[3]); // Manual conversion to CRGB
        CRGB color2 = CRGB(value[4], value[5], value[6]);

        // Set the speedometer colors
        speedoColor1 = color1;
        speedoColor2 = color2;
        syncController.speedometer(location_service, speedoColor1, speedoColor2);
      }
      else
      {
        Serial.println("Invalid data length for speedometer colors.");
      }

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

  BLE.setLocalName("Backpack-AC");
  BLE.setAdvertisedService(backpackService);
  backpackService.addCharacteristic(featuresCharacteristic);
  BLE.addService(backpackService);

  // assign event handlers for connected, disconnected to peripheral
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
  featuresCharacteristic.setEventHandler(BLEWritten, featureCallback);

  // start advertising
  BLE.advertise();

  // Setup Button
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Start Sync
  syncController.begin(CURRENT_USER);

  location_service.start_tracking_position();
  light_show.brightness(brightness);
  light_show.palette_stream(speed, AP_palette);
}

void loop()
{
  BLE.poll();
  handleButton();
  syncBluetoothSettings();
  unsigned long now = backpackClock.now();
  location_service.update_position();

  if (!power)
  {
    FastLED.clear();
    FastLED.show();
    return;
  }

  handleBackpackFeatures();
  light_show.render();
}

// BLE Helpers
void blePeripheralConnectHandler(BLEDevice central)
{
  // central connected event handler
  Serial.print("Connected event, central: ");
  Serial.println(central.address());
  sendStatusUpdate();
  deviceConnected = true;
}

void blePeripheralDisconnectHandler(BLEDevice central)
{
  // central disconnected event handler
  Serial.print("Disconnected event, central: ");
  Serial.println(central.address());
  deviceConnected = false;
}

bool buttonInitialized = false;
void handleButton()
{
  // Check if the encoder button is pressed
  currentState = digitalRead(BUTTON_PIN);

  // Fix initial button press on boot
  if (!buttonInitialized)
  {
    lastState = currentState;
    buttonInitialized = !buttonInitialized;
    return;
  }
  if ((millis() - pressedTime) > DEBOUNCE_DELAY)
  {
    if (lastState == HIGH && currentState == LOW)
    {
      pressedTime = millis();
    }
    else if (lastState == LOW && currentState == HIGH)
    {
      releasedTime = millis();
      long pressDuration = releasedTime - pressedTime;

      if (pressDuration < SHORT_PRESS_TIME)
      {
        // Short Press
        Serial.println("Short Press");
        sendStatusUpdate();
        syncController.handleButtonShortPress();
        delay(100);
      }
      else if (pressDuration > SHORT_PRESS_TIME && pressDuration < OFF_TIME)
      {
        // Long Press
        Serial.println("Long Press");
        syncController.handleButtonLongPress();
        delay(100);
      }
      else
      {
        power = power ? 0 : 1;
      }
    }
  }
  lastState = currentState;
}

void handleBackpackFeatures()
{
  switch (currentScene)
  {

  case position_status:
    if ((millis() - lastGpsPolledTIme) > DEFAULT_POSITION_STATUS_POLL_INTERVAL)
    {
      syncController.positionStatus(location_service);
      lastGpsPolledTIme = millis();
    }
    break;

  case color_wheel:
    if ((millis() - lastGpsPolledTIme) > DEFAULT_POSITION_STATUS_POLL_INTERVAL)
    {
      syncController.colorWheel(location_service);
      lastGpsPolledTIme = millis();
    }
    break;
  case speedometer:
    if ((millis() - lastGpsPolledTIme) > DEFAULT_POSITION_STATUS_POLL_INTERVAL)
    {
      syncController.speedometer(location_service, speedoColor1, speedoColor2);
      lastGpsPolledTIme = millis();
    }
    break;
  }
}

void syncBluetoothSettings()
{
  if ((millis() - lastBluetoothSync) > DEFAULT_BT_REFRESH_INTERVAL && deviceConnected)
  {
    sendStatusUpdate();
    lastBluetoothSync = millis();
  }
}