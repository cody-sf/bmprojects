#include <FastLED.h>
#include <GlobalDefaults.h>
#include <LightShow.h>
#include <DeviceRoles.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <ArduinoBLE.h>
#include "Helpers.h"
#include "SyncController.h"

#define LED_OUTPUT_PIN 14
#define NUM_LEDS 215
#define COLOR_ORDER GRB
#define BUTTON_PIN 18

#define SERVICE_UUID "1cfbcb17-6a53-4c6f-a7fd-7a8e480d7a5f"
#define FEATURES_UUID "8a1e8fec-98ec-4763-a884-dde07dacf905"

#define OFF_TIME 5000
// BT
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;
BLEService coatService(SERVICE_UUID);
// create switch characteristic and allow remote device to read and write
BLECharacteristic featuresCharacteristic(FEATURES_UUID, BLERead | BLEWrite, 512);

static Device coat;
static CRGB leds[NUM_LEDS];
static CLEDController &led_controller_main = FastLED.addLeds<WS2811, LED_OUTPUT_PIN, COLOR_ORDER>(leds, NUM_LEDS);
static std::vector<CLEDController *> led_controllers = {&led_controller_main};
static LightShow light_show(led_controllers);

// Gyroscope
Adafruit_MPU6050 mpu1;
Adafruit_MPU6050 mpu2;
Adafruit_Sensor *mpu_temp1, *mpu_accel1, *mpu_gyro1, *mpu_temp2, *mpu_accel2, *mpu_gyro2;
double pitch1, roll1, direction1, pitch2, roll2, direction2;

// Button Press Stuff
int lastState = LOW; // the previous state from the input pin
int currentState;    // the current reading from the input pin
unsigned long pressedTime = 0;
unsigned long releasedTime = 0;
const int SHORT_PRESS_TIME = 2000;
bool disconnect = false;

// Valiables
AvailablePalettes current_palette = cool;
bool controlBrightness = true;
bool power = true;
uint32_t maxBrightness = 75;
bool movementEnabled = true;
uint32_t speed = 100;
bool syncOtherDevices = false;

// Sync Controller
SyncController syncController(light_show, CURRENT_USER, coat);

std::string toLowerCase(const std::string &input)
{
  std::string output = input;
  for (auto &c : output)
    c = tolower(c);
  return output;
}

void featureCallback(BLEDevice central, BLECharacteristic characteristic)
{
  Serial.println("Received a thing");
  const uint8_t *buffer = featuresCharacteristic.value();         // Get the data buffer
  unsigned int dataLength = featuresCharacteristic.valueLength(); // Get the data length
  std::string value((char *)buffer, dataLength);

  if (!value.empty())
  {
    uint8_t feature = value[0];

    // On/Off
    if (feature == 0x01)
    {
      power = value[1] != 0;
      Serial.print("Power: ");
      Serial.println(power ? "On" : "Off");
      return;
    }

    // Palette
    if (feature == 0x02)
    {
      Serial.println("Primary Palette: ");
      std::string strValue = toLowerCase(value.substr(1));
      current_palette = stringToPalette(strValue.c_str());
      Serial.print(strValue.c_str());
      syncController.setPalette(current_palette);
      return;
    }

    // control brightness
    if (feature == 0x03)
    {
      controlBrightness = value[1] != 0;
      Serial.print("Control Brightness: ");
      Serial.println(controlBrightness ? "True" : "False");
      // cool_program(current_palette, controlBrightness);
      return;
    }

    // Set Brightness
    if (feature == 0x04)
    {
      int b = 0;
      memcpy(&b, value.data() + 1, sizeof(int));
      Serial.print("Brightness: ");
      Serial.println(b);
      maxBrightness = b;
      syncController.setBrightness(b);
      return;
    }

    // Set Mode
    if (feature == 0x05)
    {
      movementEnabled = value[1] != 0;
      Serial.print("Control Brightness: ");
      Serial.println(movementEnabled ? "True" : "False");
      // cool_program(current_palette, controlBrightness);
      return;
    }
  }
};

void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Wire.begin();
  Serial.println("Starting!");
  Serial.println("MPU6050 1 test!");
  if (!mpu1.begin(0x69))
  {
    Serial.println("Failed to find MPU6050 1 chip");
  }
  else
  {
    Serial.println("MPU6050 1 Found!");
  }

  // MPU 2
  Serial.println("MPU6050 2 test!");
  if (!mpu2.begin(0x68))
  {
    Serial.println("Failed to find MPU6050 2 chip");
  }
  else
  {
    Serial.println("MPU6050 2 Found!");
  }

  if (!BLE.begin())
  {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1)
      ;
  }
  BLE.setLocalName("Coat-CL");
  BLE.setAdvertisedService(coatService);
  coatService.addCharacteristic(featuresCharacteristic);
  BLE.addService(coatService);

  // assign event handlers for connected, disconnected to peripheral
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  // assign event handlers for characteristic
  featuresCharacteristic.setEventHandler(BLEWritten, featureCallback);

  // start advertising
  BLE.advertise();

  // Start Sync
  syncController.begin(CURRENT_USER);
  light_show.palette_stream(speed, current_palette);
  light_show.brightness(maxBrightness);
}

void loop()
{
  BLE.poll();
  handleButton();

  if (!power)
  {
    FastLED.clear();
    FastLED.show();
    return;
  }

  // if (movementEnabled)
  // {
  //   movement_program();
  //   light_show.render();
  //   delay(100);
  //   return;
  // }
  // else
  // {
  // }

  light_show.render();
}

void movement_program()
{
  CRGBPalette16 current_palette = light_show.getPalette(light_show.getCurrentScene().primary_palette);
  getPitch(1);
  getPitch(2);

  // Map roll1 from range [-90, 90] to [0, 255]
  int hueIndex = constrain(mapValue(roll1, -90, 90, 0, 255), 0, 255);

  // Map pitch1 from range [-90, 90] to [0, 255] for brightness control
  int brightness = constrain(mapValue(pitch1, -90, 90, 0, 255), 0 ,255);

  Serial.print("Hue: ");
  Serial.println(hueIndex);

  Serial.print("BRightness: ");
  Serial.println(brightness);
  // Fetch the color from the palette
  CRGB color = ColorFromPalette(current_palette, hueIndex);
  if (controlBrightness)
  {
    controlBrightnessWithGyroAccel();
  }
  syncController.jacketDance(color);
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

void handleButton()
{
  // Check if the encoder button is pressed
  currentState = digitalRead(BUTTON_PIN);

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
      // sendStatusUpdate();
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