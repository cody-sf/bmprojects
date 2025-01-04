#include <FastLED.h>
#include <GlobalDefaults.h>
#include <Clock.h>
#include <LightShow.h>
#include <DeviceRoles.h>
#include <ArduinoBLE.h>
#include "Helpers.h"
#include "SyncController.h"
#include <ArduinoJson.h>

// Bluetooth
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;
unsigned long lastBluetoothSync = 0;

// UUID Setup
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "a6b43832-0e6e-46df-a80b-e7ab5b4d7c99"
#define FEATURES_UUID "ef399903-6234-4ba8-894b-6b537bc6739b"
BLEService hatService(SERVICE_UUID);
BLECharacteristic featuresCharacteristic(FEATURES_UUID, BLERead | BLEWrite | BLENotify, 512);
// Xiao ESP32c3 - hold boot and reset button if not working
// Aleg D7, other C3 unites 7
#define LED_OUTPUT_PIN 7
#define NUM_LEDS 350
#define COLOR_ORDER GRB
static Device cowboyhat;

// LED Strips/Lightshow
CRGB leds[NUM_LEDS];

static CLEDController &led_controller_1 = FastLED.addLeds<WS2812B, LED_OUTPUT_PIN, COLOR_ORDER>(leds, NUM_LEDS);
static std::vector<CLEDController *> led_controllers = {&led_controller_1};
static LightShow light_show(led_controllers);

std::pair<CRGBPalette16, CRGBPalette16> palettes = light_show.getPrimarySecondaryPalettes();
CRGBPalette16 primaryPalette = palettes.first;

// variables
int brightness = 1;
uint16_t speed = 100;
bool reverseStrip = true;
bool power = true;
AvailablePalettes AP_palette = vivid;

// Sync Controller
SyncController syncController(light_show, CURRENT_USER, cowboyhat);

std::string toLowerCase(const std::string &input)
{
  std::string output = input;
  for (auto &c : output)
    c = tolower(c);
  return output;
}

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
  LightScene deviceScene = light_show.getCurrentScene();

  // Add all the settings and their values
  doc["power"] = power;
  doc["currentSceneId"] = deviceScene.scene_id; // You may want to convert this to a string if it's an enum
  doc["brightness"] = deviceScene.brightness;
  doc["speed"] = deviceScene.speed;
  doc["direction"] = reverseStrip;
  doc["palette"] = paletteToString(deviceScene.primary_palette); // Assume you have a function to convert palette to string

  String output;
  serializeJson(doc, output);

  // Send the JSON string via the BLE characteristic
  Serial.print("Sending notify status: ");
  Serial.println(output.c_str());
  featuresCharacteristic.setValue(output.c_str());
}

// TODO clean this up!
void setMode(std::string show)
{
  if (show == "pstream")
  {

    return;
  }

  if (show == "pcycle")
  {

    return;
  }

  if (show == "solid")
  {

    return;
  }
}

void featureCallback(BLEDevice central, BLECharacteristic characteristic)
{
  Serial.println("Received a thing");
  const uint8_t *buffer = featuresCharacteristic.value();         // Get the raw data buffer
  unsigned int dataLength = featuresCharacteristic.valueLength(); // Get the data length

  // Create a string from buffer
  std::string value((char *)buffer, dataLength);
  if (!value.empty())
  {
    uint8_t feature = value[0];
    Serial.print("Feature: ");
    Serial.println(feature);
    // Primary Palette
    if (feature == 0x01)
    {
      Serial.println("BTCommand: Primary Palette: ");
      std::string strValue = toLowerCase(value.substr(1));
      AP_palette = stringToPalette(strValue.c_str());
      Serial.print(strValue.c_str());
      primaryPalette = light_show.getPrimarySecondaryPalettes().first;
      syncController.setPalette(AP_palette);
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
    // Direction
    if (feature == 0x05)
    {
      reverseStrip = value[1] != 0;
      Serial.print("Received direction: ");
      Serial.println(reverseStrip ? "True" : "False");
      syncController.setDirection(reverseStrip);
      return;
    }

    // On/Off
    if (feature == 0x06)
    {
      power = value[1] != 0;
      Serial.print("Power: ");
      Serial.println(power ? "On" : "Off");
      return;
    }

    // Speed
    if (feature == 0x08)
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

    // Brightness
    if (feature == 0x09)
    {
      int b = 0;
      memcpy(&b, value.data() + 1, sizeof(int));
      Serial.print("Brightness: ");
      Serial.println(b);
      brightness = b;
      syncController.setBrightness(brightness);
      return;
    }
  }
};

void setup()
{
  Serial.begin(115200);

  if (!BLE.begin())
  {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1)
      ;
  }

  BLE.setLocalName("CowboyHat-AY");
  BLE.setAdvertisedService(hatService);
  hatService.addCharacteristic(featuresCharacteristic);
  BLE.addService(hatService);

  // assign event handlers for connected, disconnected to peripheral
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  // assign event handlers for characteristic
  featuresCharacteristic.setEventHandler(BLEWritten, featureCallback);

  // start advertising
  BLE.advertise();

  // Start Sync
  syncController.begin(CURRENT_USER);

  light_show.brightness(brightness);
  light_show.palette_stream(speed, AP_palette);
}

void loop()
{

  BLE.poll();
  syncBluetoothSettings();
  // TODO sync?
  if (!power)
  {
    FastLED.clear();
    FastLED.show();
    return;
  }

  light_show.render();
}

void syncBluetoothSettings()
{
  if ((millis() - lastBluetoothSync) > DEFAULT_BT_REFRESH_INTERVAL && deviceConnected)
  {
    sendStatusUpdate();
    lastBluetoothSync = millis();
  }
}