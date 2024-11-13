#include <FastLED.h>
#include <GlobalDefaults.h>
#include <Clock.h>
#include <LightShow.h>
#include <DeviceRoles.h>
#include <ArduinoBLE.h>
#include "Helpers.h"
#include "SyncController.h"
#include <ArduinoJson.h>

// BT Devices
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;
unsigned long lastBluetoothSync = 0;

// Rotary encoder pins
#define ENCODER_PIN_A 16
#define ENCODER_PIN_B 17
#define ENCODER_BUTTON_PIN 18
int encoderState = 0;
volatile long encoderPosition = 0;
volatile int lastEncoded = 0;
void IRAM_ATTR handleEncoder()
{
  int MSB = digitalRead(ENCODER_PIN_A);   // Most significant bit
  int LSB = digitalRead(ENCODER_PIN_B);   // Least significant bit
  int encoded = (MSB << 1) | LSB;         // Convert the 2 pin values to a single number
  int sum = (lastEncoded << 2) | encoded; // Add the previous encoded value
  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011)
  {
    encoderState++;
  }

  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000)
  {
    encoderState--;
  }
  if (encoderState == 4)
  {
    encoderPosition++; // Increment position after a full cycle
    encoderState = 0;  // Reset state
  }
  else if (encoderState == -4)
  {
    encoderPosition--; // Decrement position after a full cycle
    encoderState = 0;  // Reset state
  }
  lastEncoded = encoded; // Store this value for next time
}
// Button Press Stuff
int lastState = LOW; // the previous state from the input pin
int currentState;    // the current reading from the input pin
unsigned long pressedTime = 0;
unsigned long releasedTime = 0;
const int SHORT_PRESS_TIME = 1000;
const int OFF_TIME = 5000;

// UUID Setup
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "e75d21d2-2482-4ba2-bed5-c59bc8d7aa2c"
#define CONSOLE_MESSAGE_UUID "5a11f1fe-619c-487b-966c-4996d71c3e90"
#define FEATURES_UUID "e2d56095-0da0-45f3-9494-2369305334c1"

// BT Setup
BLEService bikeService(SERVICE_UUID);
// create switch characteristic and allow remote device to read and write
BLECharacteristic featuresCharacteristic(FEATURES_UUID, BLERead | BLEWrite | BLENotify, 512);

#define LED_OUTPUT_PIN_MAIN 27
#define NUM_LEDS_MAIN 90

#define LED_OUTPUT_PIN_BASKET 25
#define NUM_LEDS_BASKET 200

#define LED_OUTPUT_PIN_FENDER 23
#define NUM_LEDS_FENDER 75


// Brandon/Generic
// #define LED_OUTPUT_PIN_MAIN 27
// #define NUM_LEDS_MAIN 150

// #define LED_OUTPUT_PIN_BASKET 25
// #define NUM_LEDS_BASKET 150

// CHANGE
#define COLOR_ORDER GRB 
// #define COLOR_ORDER RGB
static Device bike;

// LED Strips/Lightshow
CRGB leds1[NUM_LEDS_MAIN];
CRGB leds2[NUM_LEDS_BASKET];
CRGB leds3[NUM_LEDS_FENDER];

static CLEDController &led_controller_1 = FastLED.addLeds<WS2812B, LED_OUTPUT_PIN_MAIN, RGB>(leds1, NUM_LEDS_MAIN);
static CLEDController &led_controller_2 = FastLED.addLeds<WS2812B, LED_OUTPUT_PIN_BASKET, RGB>(leds2, NUM_LEDS_BASKET);
static CLEDController &led_controller_3 = FastLED.addLeds<WS2812B, LED_OUTPUT_PIN_FENDER, COLOR_ORDER>(leds3, NUM_LEDS_FENDER);
static std::vector<CLEDController *> led_controllers = {&led_controller_1, &led_controller_2, &led_controller_3};
// static std::vector<CLEDController *> led_controllers = {&led_controller_1, &led_controller_2};
static LightShow light_show(led_controllers);

std::pair<CRGBPalette16, CRGBPalette16> palettes = light_show.getPrimarySecondaryPalettes();
CRGBPalette16 primaryPalette = palettes.first;
CRGBPalette16 secondaryPalette = palettes.second;

// Sync Controller
SyncController syncController(light_show, CURRENT_USER, bike);

// variables
int brightness = 25;
uint16_t speed = 100;
bool reverseStrip = true;
bool power = true;
AvailablePalettes AP_palette = cool;

std::string toLowerCase(const std::string &input)
{
  std::string output = input;
  for (auto &c : output)
    c = tolower(c);
  return output;
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
  Serial.print("Changing mode");

  if (show == "pstream")
  {
    // control_center.change_light_show_mode("palette-stream");
    return;
  }

  if (show == "pcycle")
  {
    // control_center.change_light_show_mode("palette-cycle");
    return;
  }

  if (show == "solid")
  {
    // control_center.change_light_show_mode("solid-color");
    return;
  }
}

void featureCallback(BLEDevice central, BLECharacteristic characteristic)
{
  Serial.println("Received a thing");
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

    if (feature == 0x02)
    {

      return;
    }

    // Brightness
    if (feature == 0x03)
    {
      int b = 0;
      memcpy(&b, value.data() + 1, sizeof(int));
      Serial.print("BT Brightness: ");
      Serial.println(b);
      brightness = b;
      syncController.setBrightness(brightness);
      return;
    }

    // Mode
    if (feature == 0x04)
    {
      Serial.println("Mode: ");
      std::string strValue = value.substr(1);
      std::transform(strValue.begin(), strValue.end(), strValue.begin(), ::tolower);
      Serial.print(strValue.c_str());
      setMode(strValue);
      return;
    }

    // Primary Palette
    if (feature == 0x05)
    {
      Serial.println("Primary Palette: ");
      std::string strValue = toLowerCase(value.substr(1));
      AP_palette = stringToPalette(strValue.c_str());
      Serial.print(strValue.c_str());
      // light_show.setPrimaryPalette(AP_palette);

      primaryPalette = light_show.getPrimarySecondaryPalettes().first;
      // light_show.palette_stream(speed, AP_palette, reverseStrip);

      syncController.setPalette(AP_palette);
      return;
    }

    // Speed
    if (feature == 0x06)
    {
      int s = 0;
      memcpy(&s, value.data() + 1, sizeof(int));
      Serial.print("Speed: ");
      Serial.println(s);
      speed = s;
      // light_show.palette_stream(speed, AP_palette, reverseStrip);
      Serial.println("Speed sent from BT: ");
      Serial.print(speed);
      syncController.setSpeed(speed);
      return;
    }

    // Direction
    if (feature == 0x07)
    {
      reverseStrip = value[1] != 0;
      light_show.palette_stream(speed, AP_palette, reverseStrip);
      Serial.print("Received direction: ");
      Serial.println(reverseStrip ? "True" : "False");
      return;
    }
  }
};

void setup()
{
  Serial.begin(115200);
  delay(2000);

  // Initialize pressedTime and releasedTime to avoid incorrect long press detection
  pressedTime = millis();
  releasedTime = millis();

  // Start Encoder
  pinMode(ENCODER_PIN_A, INPUT_PULLUP);
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), handleEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), handleEncoder, CHANGE);

  if (!BLE.begin())
  {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1)
      ;
  }

  BLE.setLocalName("Bike-CL");
  BLE.setAdvertisedService(bikeService);
  bikeService.addCharacteristic(featuresCharacteristic);
  BLE.addService(bikeService);

  // assign event handlers for connected, disconnected to peripheral
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
  featuresCharacteristic.setEventHandler(BLEWritten, featureCallback);

  // start advertising
  BLE.advertise();

  // Start Sync
  syncController.begin(CURRENT_USER);

  // Begin Lightshow
  light_show.brightness(brightness);
  light_show.palette_stream(speed, AP_palette);
}

void loop()
{
  BLE.poll();
  handleEncoderChange();
  yield();
  syncBluetoothSettings();
  if (!power)
  {
    FastLED.clear();
    FastLED.show();
    return;
  }

  light_show.render();
}

// Bluetooth Helpers
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

void syncBluetoothSettings()
{
  if ((millis() - lastBluetoothSync) > DEFAULT_BT_REFRESH_INTERVAL && deviceConnected)
  {
    sendStatusUpdate();
    lastBluetoothSync = millis();
  }
}

bool buttonInitialized = false;
void handleEncoderChange()
{
  int8_t dialDirection = 0;
  static long lastEncoderPosition = 0;

  // Check if the encoder position has changed
  if (encoderPosition != lastEncoderPosition)
  {
    if (encoderPosition > lastEncoderPosition)
    {
      dialDirection = 1;
    }
    else
    {
      dialDirection = -1;
    }
    lastEncoderPosition = encoderPosition;
    Serial.println("handle dial change");
    syncController.handleDialChange(dialDirection);
  }

  // Check if the encoder button is pressed
  currentState = digitalRead(ENCODER_BUTTON_PIN);

  // Fix initial button press on boot
  if (!buttonInitialized)
  {
    lastState = currentState;
    buttonInitialized = true;
    return;
  }

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
  lastState = currentState;
}
