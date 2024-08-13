#include <FastLED.h>
#include <GlobalDefaults.h>
#include <Clock.h>
#include <LightShow.h>
#include <DeviceRoles.h>
#include <ArduinoBLE.h>
#include "Helpers.h"
#include <WhiteTemps.h>

bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

// UUID Setup
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "e2328b59-7cc4-4fd2-a15e-79218ad539a6"
#define CONSOLE_MESSAGE_UUID "c93f6913-48f8-43bc-9f64-3733c9020308"
#define FEATURES_UUID "929446f8-8f9b-4a13-9181-abb0582b5516"

// BT Setup
BLEService patioService(SERVICE_UUID);
// create switch characteristic and allow remote device to read and write
BLECharacteristic featuresCharacteristic(FEATURES_UUID, BLERead | BLEWrite, 512);


#define LED_OUTPUT_PIN_1 27
#define NUM_LEDS_1 900
#define LED_OUTPUT_PIN_2 25
#define NUM_LEDS_2 600
#define COLOR_ORDER GRB
static Device patio;

// LED Strips/Lightshow
CRGB leds1[NUM_LEDS_1];
CRGB leds2[NUM_LEDS_2];


static CLEDController &led_controller_1 = FastLED.addLeds<WS2812B, LED_OUTPUT_PIN_1, COLOR_ORDER>(leds1, NUM_LEDS_1);
static CLEDController &led_controller_2 = FastLED.addLeds<WS2812B, LED_OUTPUT_PIN_2, COLOR_ORDER>(leds2, NUM_LEDS_2);
static std::vector<CLEDController *> led_controllers = { &led_controller_1, &led_controller_2};
static LightShow light_show(led_controllers);

std::pair<CRGBPalette16, CRGBPalette16> palettes = light_show.getPrimarySecondaryPalettes();
CRGBPalette16 primaryPalette = palettes.first;
CRGBPalette16 secondaryPalette = palettes.second;

int colorTemperature = 128; // Starting value (midpoint)


// variables
int whiteTemp = 0;
int brightness = 25;
uint16_t speed = 100;
bool reverseStrip = true;
bool power = true;
AvailablePalettes AP_palette = cool;

std::string toLowerCase(const std::string &input) {
  std::string output = input;
  for (auto &c : output) c = tolower(c);
  return output;
}

#define candle 0xFF9329
#define sunlight 0xFFFFFF

CRGB interpolateColor(CRGB color1, CRGB color2, uint8_t amount) {
    return blend(color1, color2, amount);
}

CRGB getColorTemperature(uint8_t sliderValue) {
    return interpolateColor(candle, sunlight, sliderValue);
}

// TODO clean this up!
void setMode(std::string show) {
  Serial.print("Changing mode");

  if(show == "pstream") {   
    // control_center.change_light_show_mode("palette-stream");
    return;
  }

  if(show == "pcycle") {   
    // control_center.change_light_show_mode("palette-cycle");
    return;
  }

  if(show == "solid"){
    // control_center.change_light_show_mode("solid-color");
    return;
  }
}

void featureCallback(BLEDevice central, BLECharacteristic characteristic)
{
  Serial.println("Received a thing");
  const uint8_t* buffer = featuresCharacteristic.value();      // Get the data buffer
  unsigned int dataLength = featuresCharacteristic.valueLength(); // Get the data length
  
  // Create a string from buffer
  std::string value((char*)buffer, dataLength);
    if (!value.empty()) {
      uint8_t feature = value[0];
      Serial.print("Feature: ");
      Serial.println(feature);

      // On/Off
      if (feature == 0x01) {
        power = value[1] != 0;
        Serial.print("Power: ");
        Serial.println(power ? "On" : "Off");
        return;
      }

      // White Temp
      if (feature == 0x02) {
        int wt = 0;
        memcpy(&wt, value.data() + 1, sizeof(int));
        Serial.print("Whiteness: ");
        Serial.println(wt);
        whiteTemp = wt;
        int colorTemperature = constrain(whiteTemp, 0, 255);
        CRGB colorTemp = interpolateColor(candle, sunlight, colorTemperature);
        light_show.solid(colorTemp);
        return;
      }

      // Brightness
      if (feature == 0x03) {
        int b = 0;
        memcpy(&b, value.data() + 1, sizeof(int));
        Serial.print("Brightness: ");
        Serial.println(b);
        brightness = b;
        light_show.brightness(brightness);
        return;
      }

      // Mode
      if (feature == 0x04) {
        Serial.println("Mode: ");
        std::string strValue = value.substr(1);
        std::transform(strValue.begin(), strValue.end(), strValue.begin(), ::tolower); 
        Serial.print(strValue.c_str());
        setMode(strValue);
        return;
      }

      // Primary Palette
      if (feature == 0x05) {
        Serial.println("Primary Palette: ");
        std::string strValue = toLowerCase(value.substr(1));
        AP_palette = stringToPalette(strValue.c_str());
        Serial.print(strValue.c_str());
        light_show.setPrimaryPalette(AP_palette);
        // light_show.palette_stream(speed, AP_palette, reverseStrip);
        primaryPalette = light_show.getPrimarySecondaryPalettes().first;
        return;
      }
       
      // Speed
      if (feature == 0x06) {
        int s = 0;
        memcpy(&s, value.data() + 1, sizeof(int));
        Serial.print("Speed: ");
        Serial.println(s);
        speed = s;
        light_show.palette_stream(speed, AP_palette, reverseStrip);
        return;
      }


      // Direction
      if (feature == 0x07) {
        reverseStrip = value[1] != 0;
        light_show.palette_stream(speed, AP_palette, reverseStrip);
        Serial.print("Received direction: ");
        Serial.println(reverseStrip ? "True" : "False");
        return;
      }
    }
};





void setup() {
  Serial.begin(115200);

  if (!BLE.begin())
  {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1)
      ;
  }

  BLE.setLocalName("Patio-AC");
  BLE.setAdvertisedService(patioService);
  patioService.addCharacteristic(featuresCharacteristic);
  BLE.addService(patioService);

  // assign event handlers for connected, disconnected to peripheral
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  // assign event handlers for characteristic
  featuresCharacteristic.setEventHandler(BLEWritten, featureCallback);
  // set an initial value for the characteristic
  // featuresCharacteristic.setValue(0);
  // featuresCharacteristic.setCallbacks(new FeaturesCallbacks());

  // start advertising
  BLE.advertise();
  light_show.brightness(brightness);
  //light_show.palette_stream(speed, AP_palette);
  light_show.solid(CRGB::White);
}

void loop() {
  BLE.poll();

  // TODO sync?
  if (!power) {
    FastLED.clear();
    FastLED.show();
    return;
  }

  light_show.render();
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