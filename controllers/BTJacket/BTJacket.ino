#include <FastLED.h>
#include <GlobalDefaults.h>
#include <LightShow.h>
#include <DeviceRoles.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "Helpers.h"

#define LED_OUTPUT_PIN 14
#define NUM_LEDS 215
#define COLOR_ORDER GRB
#define BUTTON_PIN 18

#define SERVICE_UUID  "1cfbcb17-6a53-4c6f-a7fd-7a8e480d7a5f"
#define FEATURES_UUID "8a1e8fec-98ec-4763-a884-dde07dacf905"

// Valiables
// std::pair<CRGBPalette16, CRGBPalette16> palettes = light_show.getPrimarySecondaryPalettes();
// CRGBPalette16 currentPalette = palettes.first;
AvailablePalettes current_palette = cool;
bool controlBrightness = true;
bool power = true;
uint32_t maxBrightness = 75;
bool movementEnabled = true;
uint32_t speed = 100;

// BT
BLEServer* pServer = NULL;
BLECharacteristic* featuresCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

// static Clock jacketClock;
static Device coat;
static CRGB leds[NUM_LEDS];
static CLEDController &led_controller_main = FastLED.addLeds<WS2811, LED_OUTPUT_PIN, COLOR_ORDER>(leds, NUM_LEDS);
static std::vector<CLEDController *> led_controllers = {&led_controller_main};
static LightShow light_show(led_controllers);

//Gyroscope
Adafruit_MPU6050 mpu1; 
Adafruit_MPU6050 mpu2;
Adafruit_Sensor *mpu_temp1, *mpu_accel1, *mpu_gyro1, *mpu_temp2, *mpu_accel2, *mpu_gyro2;
double pitch1,roll1,direction1,pitch2,roll2,direction2;

// Button Press Stuff
int lastState = LOW;  // the previous state from the input pin
int currentState;     // the current reading from the input pin
unsigned long pressedTime  = 0;
unsigned long releasedTime = 0;
const int SHORT_PRESS_TIME = 2000; 
bool disconnect = false;


// Lightscenes
int sceneIndex = 0;
#define NUM_SCENES 11
enum AvailableScenes : uint8_t
{
  coat_gyro_1,
  coat_gyro_2,
  coat_gyro_3,
  coat_gyro_4,
  coat_gyro_5,
  coat_gyro_6,
  coat_gyro_7,
  coat_gyro_8,
  coat_gyro_9,
  coat_gyro_10,
  coat_gyro_11,
  turnoff,
};
typedef void (*SceneFunction)(); // This is a function pointer type
SceneFunction currentScene = nullptr; 
std::string toLowerCase(const std::string &input) {
  std::string output = input;
  for (auto &c : output) c = tolower(c);
  return output;
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

      // Palette
      if(feature == 0x02){
        Serial.println("Primary Palette: ");
        std::string strValue = toLowerCase(value.substr(1));
        current_palette = stringToPalette(strValue.c_str());
        Serial.print(strValue.c_str());
        // light_show.setPrimaryPalette(AP_palette);
        // light_show.palette_stream(speed, AP_palette, reverseStrip);
        // primaryPalette = light_show.getPrimarySecondaryPalettes().first;
        cool_program(current_palette, controlBrightness);
        return;
      }

      // control brightness
      if(feature == 0x03){
        controlBrightness = value[1] != 0;
        Serial.print("Control Brightness: ");
        Serial.println(controlBrightness ? "True" : "False");
        cool_program(current_palette, controlBrightness);
        return;
      }

      // Set Brightness
      if (feature == 0x04) {
        int b = 0;
        memcpy(&b, value.data() + 1, sizeof(int));
        Serial.print("Brightness: ");
        Serial.println(b);
        maxBrightness = b;
        return;
      }

      // Set Mode
      if(feature == 0x05){
        movementEnabled = value[1] != 0;
        Serial.print("Control Brightness: ");
        Serial.println(movementEnabled ? "True" : "False");
        // cool_program(current_palette, controlBrightness);
        return;
      }

    }
  }
};
void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Wire.begin();
  Serial.println("Starting!");
  Serial.println("MPU6050 1 test!");
  if (!mpu1.begin(0x69)) {
    Serial.println("Failed to find MPU6050 1 chip");
  } else {
    Serial.println("MPU6050 1 Found!");
  }

  // MPU 2
  Serial.println("MPU6050 2 test!");
  if (!mpu2.begin(0x68)) {
    Serial.println("Failed to find MPU6050 2 chip");
  } else {
    Serial.println("MPU6050 2 Found!"); 
  }
  

    // Create the BLE Device
  BLEDevice::init("Coat-CL");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristics
  featuresCharacteristic = pService->createCharacteristic(
    FEATURES_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);


  featuresCharacteristic->addDescriptor(new BLE2902());

    // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  featuresCharacteristic->setCallbacks(new FeaturesCallbacks());

  // Start the service
  pService->start();
  BLEDevice::startAdvertising();

  light_show.brightness(maxBrightness);
}

void loop() {

  // read the state of the switch/button:
  currentState = digitalRead(BUTTON_PIN);

  // button is pressed
  if (lastState == HIGH && currentState == LOW) {
    pressedTime = millis();
    Serial.println("Button Pressed!!");
  }    

  // else if(lastState == LOW && currentState == HIGH) { 
  //   releasedTime = millis();
  //   long pressDuration = releasedTime - pressedTime;

  //   if( pressDuration < SHORT_PRESS_TIME ) {
  //     // Short Press
  //     Serial.println("Short Press");
  //     switch (static_cast<AvailableScenes>(sceneIndex)) {
        
  //       case coat_gyro_1:
  //         Serial.println("Coat Gyro");
  //         currentScene = program1;
  //         break;
  //       case coat_gyro_2:
  //         currentScene = program2;
  //         break;
  //       case coat_gyro_3:
  //         currentScene = program3;
  //         break;
  //       case coat_gyro_4:
  //         currentScene = program4;
  //         break;
  //       case coat_gyro_5:
  //         Serial.println("Coat Gyro 5");
  //         currentScene = program5;
  //         break;
  //       case coat_gyro_6:
  //         Serial.println("Coat Gyro 6");
  //         currentScene = program6;
  //         break;
  //       case coat_gyro_7:
  //         Serial.println("Coat Gyro 7");
  //         currentScene = program7;
  //         break;
  //       case coat_gyro_8:
  //         Serial.println("Coat Gyro 8");
  //         currentScene = program8;
  //         break;
  //       case coat_gyro_9:
  //         Serial.println("Coat Gyro 9");
  //         currentScene = program9;
  //         break;
  //       case coat_gyro_10:
  //         Serial.println("Coat Gyro 10");
  //         currentScene = program10;
  //         break;
  //       case coat_gyro_11:
  //         Serial.println("Coat Gyro 11");
  //         currentScene = program11;
  //         break;
  //       case turnoff:
  //         light_show.solid(CRGB::Black);
  //         break;
  //       default:
  //         break;
  //     }
  //     sceneIndex++;  // Move to the next scene
  //     sceneIndex %= NUM_SCENES;  // Wrap around to the beginning of the list if necessary

  //     // Call the scene function
  //   } else {
  //     // Long Press
  //     Serial.println("Long Press");
  //     disconnect = !disconnect;
  //   }
  // }
  // save the the last state
  lastState = currentState;

  // if(!disconnect){
  //   if (message_size == sizeof(NetworkMessage)) {
  //     Serial.println("Network connecting");
  //       if (message_buffer.selected_devices & device_role) {
  //         currentScene = nullptr;
  //           switch (message_buffer.type) {
  //               case NetworkMessage::MessageType::light_scene:
  //                   light_show.import_scene(&message_buffer.messages.light_scene);
  //                   break;

  //               case NetworkMessage::MessageType::time_reference:
  //                   clock.synchronize(message_buffer.messages.time_reference);
  //                   break;

  //               default:
  //                   break;
  //           }
  //       }
  //   }
  // }
  if (!power) {
    FastLED.clear();
    FastLED.show();
    return;
  }
  
  if (movementEnabled) {
    cool_program(current_palette, controlBrightness);
    light_show.render();
    delay(50);
    return;
  }

  light_show.palette_stream(200, current_palette);
  light_show.render();
}

void cool_program(AvailablePalettes palette, bool controlBrightness){
  getPitch(1);
  getPitch(2);

  // Map roll1 from range [-90, 90] to [0, 255]
  int hueIndex = mapValue(roll1, -90, 90, 0, 255);
  
  // Map pitch1 from range [-90, 90] to [0, 255] for brightness control
  int brightness = mapValue(pitch1, -90, 90, 0, 255);

  // Fetch the palette using getPalette method
  CRGBPalette16 selectedPalette = light_show.getPalette(palette);

  // Fetch the color from the palette
  CRGB color = ColorFromPalette(selectedPalette, hueIndex);
  
  // Apply the selected color to the light show using the solid method
  light_show.solid(color);

  if(controlBrightness) {
    controlBrightnessWithGyroAccel();
  }
}