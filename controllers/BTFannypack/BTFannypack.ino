#include <FastLED.h>
#include <GlobalDefaults.h>
#include <Clock.h>
#include <LightShow.h>
#include <DeviceRoles.h>
#include <ArduinoBLE.h>
#include "Helpers.h"
#include "SyncController.h"

// Bluetooth
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;
// UUID Setup
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "ac34717f-b5d0-45b5-8bbb-7020974bea2f"
#define CONSOLE_MESSAGE_UUID "ca6b1a3e-7746-4e80-88fe-9b9df934a0bd"
#define FEATURES_UUID "dc23128c-6bc0-406e-9725-075e44e73a2d"
// BT Setup
BLEService fpService(SERVICE_UUID);
// create switch characteristic and allow remote device to read and write
BLECharacteristic featuresCharacteristic(FEATURES_UUID, BLERead | BLEWrite, 512);

#define LED_OUTPUT_PIN 27
#define NUM_LEDS 20
#define COLOR_ORDER GRB
static Device fannypack;

// LED Strips/Lightshow
CRGB leds[NUM_LEDS];

static CLEDController &led_controller_1 = FastLED.addLeds<WS2812B, LED_OUTPUT_PIN, COLOR_ORDER>(leds, NUM_LEDS);
static std::vector<CLEDController *> led_controllers = {&led_controller_1};
static LightShow light_show(led_controllers);

std::pair<CRGBPalette16, CRGBPalette16> palettes = light_show.getPrimarySecondaryPalettes();
CRGBPalette16 primaryPalette = palettes.first;
CRGBPalette16 secondaryPalette = palettes.second;

// variables
int brightness = 25;
uint16_t speed = 100;
bool reverseStrip = true;
bool power = true;
AvailablePalettes AP_palette = cool;

// Sync Controller
SyncController syncController(light_show, CURRENT_USER, fannypack);

std::string toLowerCase(const std::string &input)
{
  std::string output = input;
  for (auto &c : output)
    c = tolower(c);
  return output;
}

// TODO clean this up!
void featureCallback(BLEDevice central, BLECharacteristic characteristic)
{
  Serial.println("Received a thing");
  const uint8_t *buffer = featuresCharacteristic.value();         // Get the data buffer
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

    // Direction
    if (feature == 0x05)
    {
      reverseStrip = value[1] != 0;
      light_show.palette_stream(speed, AP_palette, reverseStrip);
      Serial.print("Received direction: ");
      Serial.println(reverseStrip ? "True" : "False");
      //syncController.setDirection(reverseStrip);
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
      Serial.print("BT Speed: ");
      Serial.println(s);
      speed = s;
      syncController.setSpeed(speed);
      return;
    }

    // Brightness
    if (feature == 0x09)
    {
      int b = 0;
      memcpy(&b, value.data() + 1, sizeof(int));
      Serial.print("BT Brightness: ");
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
  BLE.setLocalName("Fannypack-CL");
  BLE.setAdvertisedService(fpService);
  fpService.addCharacteristic(featuresCharacteristic);
  BLE.addService(fpService);

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

  // TODO sync?
  if (!power)
  {
    FastLED.clear();
    FastLED.show();
    return;
  }

  light_show.render();
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