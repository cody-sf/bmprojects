#include <math.h>
#include <FastLED.h>
#include <GlobalDefaults.h>
#include <LightShow.h>
#include <LocationService.h>
#include <Position.h>
#include <DeviceRoles.h>
#include <ControlCenter.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "Helpers.h"
#include <ArduinoJson.h>

#define LED_OUTPUT_PIN 26
#define NUM_LEDS 192
#define COLOR_ORDER GRB
#define BUTTON_PIN 2
static Device backpack;


// BT
BLEServer* pServer = NULL;
BLECharacteristic* featuresCharacteristic = NULL;
BLECharacteristic* statusCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// UUIDS
// https://www.uuidgenerator.net/
#define SERVICE_UUID  "be03096f-9322-4360-bc84-0f977c5c3c10"
#define FEATURES_UUID "24dcb43c-d457-4de0-a968-9cdc9d60392c"
#define STATUS_UUID "71a0cb09-7998-4774-83b5-1a5f02f205fb"

static Clock backpackClock;
static CRGB leds[NUM_LEDS];

static CLEDController &led_controller_1 = FastLED.addLeds<WS2812B, LED_OUTPUT_PIN, COLOR_ORDER>(leds, NUM_LEDS);
static std::vector<CLEDController *> led_controllers = { &led_controller_1};
static LightShow light_show(led_controllers);

static LocationService location_service;
static ControlCenter control_center(location_service, light_show, backpack);

// Variables
bool power = true;
std::pair<CRGBPalette16, CRGBPalette16> palettes = light_show.getPrimarySecondaryPalettes();
CRGBPalette16 currentPalette = palettes.first;
AvailablePalettes AP_palette = cool;
int speed = 100;
int brightness = 50;
bool reverseStrip = true;

// Status Templates
void sendStatus(const char* key, bool value) {
    StaticJsonDocument<200> doc;
    doc[key] = value;
    
    String output;
    serializeJson(doc, output);
    
    statusCharacteristic->setValue(output.c_str());
    statusCharacteristic->notify();
}
void sendStatus(const char* key, const char* value) {
    StaticJsonDocument<200> doc;
    doc[key] = value;
    
    String output;
    serializeJson(doc, output);
    
    statusCharacteristic->setValue(output.c_str());
    statusCharacteristic->notify();
}
void sendStatus(const char* key, int value) {
    StaticJsonDocument<200> doc;
    doc[key] = value;
    
    String output;
    serializeJson(doc, output);
    
    statusCharacteristic->setValue(output.c_str());
    statusCharacteristic->notify();
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      BLEDevice::startAdvertising();
      //sendStatus("pal", getPaketteNamepalette);
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class FeaturesCallbacks: public BLECharacteristicCallbacks {
  void setMode(std::string show) {
    if(show == "pstream") {   
      control_center.change_light_show_mode("palette-stream");
      return;
    }

    if(show == "pcycle") {   
      control_center.change_light_show_mode("palette-cycle");
      return;
    }

    if(show == "speedo"){
      control_center.change_light_show_mode("color-speedometer");
      return;
    }

    if(show == "cwheel"){
      control_center.change_light_show_mode("color-wheel");
      return;
    }

    if(show == "cradial"){
      control_center.change_light_show_mode("color-radial");
      return;
    }

    if(show == "clock"){
      control_center.change_light_show_mode("color-radial");
      return;
    }

    if(show == "solid"){
      control_center.change_light_show_mode("solid-color");
      return;
    }

    if(show == "pstat"){
      control_center.change_light_show_mode("position-status");
      return;
    }
  }
  void onWrite(BLECharacteristic *pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
  
    
    if (!value.empty()) {
      uint8_t feature = value[0];

      // On/Off
      if(feature == 0x01) {
        power = value[1] != 0;
        Serial.print("Power: ");
        Serial.println(power ? "On" : "Off");
        return;
      }

      // Mode
      if (feature == 0x02) {
        Serial.println("Mode: ");
        std::string strValue = value.substr(1);
        std::transform(strValue.begin(), strValue.end(), strValue.begin(), ::tolower); 
        Serial.print(strValue.c_str());
        setMode(strValue);
        return;
      }

      // Palette
      if(feature == 0x03){
        Serial.println("Palette: ");
        std::string strValue = value.substr(1);
        std::transform(strValue.begin(), strValue.end(), strValue.begin(), ::tolower);
        Serial.print(strValue.c_str());
        control_center.change_palette(strValue.c_str());
        return;
      }

      //Brightness 
      if(feature == 0x04) {
        int b = 0;
        memcpy(&b, value.data() + 1, sizeof(int));
        Serial.print("Brightness: ");
        Serial.println(b);
        brightness = b;
        control_center.change_brightness(brightness);
        return;
      }

      // Speed
      if(feature == 0x05){
        int s = 0;
        memcpy(&s, value.data() + 1, sizeof(int));
        Serial.print("Speed: ");
        Serial.println(s);
        speed = s;
        control_center.change_speed(speed);
        return;
      }

      // Direction
      if(feature == 0x06){
        reverseStrip = value[1] != 0;
        control_center.change_direction(reverseStrip);
        Serial.print("Received direction: ");
        Serial.println(reverseStrip ? "True" : "False");
        return;
      }

      // Change Origin
      if(feature == 0x07){
        Serial.println("Origin: ");
        std::string strValue = value.substr(1);
        std::transform(strValue.begin(), strValue.end(), strValue.begin(), ::tolower);
        Serial.print(strValue.c_str());
        control_center.change_origin(strValue.c_str());
        return;
      }

      // Ping location and speed
      if(feature == 0x08){

        return;
      }
    }
  }
};

void setup()
{

    Serial.begin(115200);
    Serial.println("system up!");

    BLEDevice::init("Backpack-CL");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    BLEService *pService = pServer->createService(SERVICE_UUID);
    featuresCharacteristic = pService->createCharacteristic(
      FEATURES_UUID,
      BLECharacteristic::PROPERTY_READ   |
      BLECharacteristic::PROPERTY_WRITE  |
      BLECharacteristic::PROPERTY_NOTIFY |
      BLECharacteristic::PROPERTY_INDICATE
    );
    statusCharacteristic = pService->createCharacteristic(
      STATUS_UUID,
      BLECharacteristic::PROPERTY_READ   |
      BLECharacteristic::PROPERTY_WRITE  |
      BLECharacteristic::PROPERTY_NOTIFY |
      BLECharacteristic::PROPERTY_INDICATE
    );
    featuresCharacteristic->addDescriptor(new BLE2902());
    statusCharacteristic->addDescriptor(new BLE2902());

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinPreferred(0x0);

    featuresCharacteristic->setCallbacks(new FeaturesCallbacks());

    pService->start();
    BLEDevice::startAdvertising(); 

    // Setup Button
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    //light_show = new LightShow(led_controllers, backpackClock);
    location_service.start_tracking_position();
    control_center.update_lightshow(&light_show);
    light_show.brightness(brightness);
}

void loop()
{
  // notify changed value
  if (deviceConnected) {
      delay(10); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
      delay(500); // give the bluetooth stack the chance to get things ready
      pServer->startAdvertising(); // restart advertising
      Serial.println("start advertising");
      oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
      oldDeviceConnected = deviceConnected;
  }

  int buttonValue = digitalRead(BUTTON_PIN);
  if (buttonValue == LOW)
  {
    Serial.println("Button Pressed");
    control_center.button_press_cycle_lightshow();
    sendStatus("button", "FUCK ALEG!!");
    sendStatus("pal", "hfjksdhfkjsd");
    sendStatus("1", 345678);
    delay(300);
  }
  
  unsigned long now = backpackClock.now();
  location_service.update_position();
  control_center.update_state();


  if(!power){
    light_show.apply_scene_updates(off);
    light_show.render();
    return;
  }

  static bool is_position_known = false;
  if (is_position_known != location_service.is_current_position_available())
  {
    if (location_service.is_current_position_available())
    {
      Serial.println("Current Position Available");
      sendStatus("loc", true);
    }
    else
    {
      Serial.println("Current Position Not Available");
      sendStatus("loc", false);
    }

    is_position_known = location_service.is_current_position_available();
  }

  light_show.render();
}

