// #include <FastLED.h>
#include <Adafruit_NeoPixel.h>
#include "LSM6DS3.h"
#include "Wire.h"
#include <ArduinoBLE.h>
// #include <NP_LightShow.h>
// #include "Helpers.h"
// #include <DeviceRoles.h>

// BT Setup
BLEService hatService("7955cd3f-ea93-48da-9d64-dfe273b9ddad");
// create switch characteristic and allow remote device to read and write
BLECharacteristic featuresCharacteristic("ef399903-6234-4ba8-894b-6b537bc6739b", BLERead | BLEWrite, 512);

#define LED_OUTPUT_PIN D0
#define NUM_LEDS 350
#define COLOR_ORDER GRB

// LED positions
const int BACK_LED_START = 1;
const int LEFT_MID_LED = 35;
const int FRONT_LED_CENTER = 78;
const int RIGHT_MID_LED = 118;
const int BACK_LED_END = 167;

uint16_t current_hue = 0; // in each loop call, the hue of the led strip color will be increased by 1

// CRGB leds[NUM_LEDS];
// static CLEDController &led_controller_1 = FastLED.addLeds<WS2812B, LED_OUTPUT_PIN, COLOR_ORDER>(leds, NUM_LEDS);
// static std::vector<CLEDController *> led_controllers = { &led_controller_1};
// static LightShow light_show(led_controllers);
Adafruit_NeoPixel pixels(NUM_LEDS, LED_OUTPUT_PIN, NEO_GRB + NEO_KHZ800);
LSM6DS3 hatIMU(I2C_MODE, 0x6A);

// variables
int brightness = 25;
uint16_t speed = 100;
bool reverseStrip = true;
bool power = true;
// AvailablePalettes AP_palette = cool;

int numLights = 0;

void setup()
{
  Serial.begin(115200);

  while (!Serial)
    ;
  // Call .begin() to configure the IMUs
  if (hatIMU.begin() != 0)
  {
    Serial.println("Device error");
  }
  else
  {
    Serial.println("Device OK!");
  }

  if (!BLE.begin())
  {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1)
      ;
  }

  BLE.setLocalName("CowboyHat-AC");
  BLE.setAdvertisedService(hatService);
  hatService.addCharacteristic(featuresCharacteristic);
  BLE.addService(hatService);

  // assign event handlers for connected, disconnected to peripheral
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  // assign event handlers for characteristic
  featuresCharacteristic.setEventHandler(BLEWritten, featureWritten);
  // set an initial value for the characteristic
  // featuresCharacteristic.setValue(0);
  // featuresCharacteristic.setCallbacks(new FeaturesCallbacks());

  // start advertising
  BLE.advertise();

  pixels.begin();
}

void loop()
{
  BLE.poll();

  // float x = hatIMU.readFloatAccelX();
  // float y = hatIMU.readFloatAccelY();
  // float z = hatIMU.readFloatAccelZ();

  // float gy_x = hatIMU.readFloatGyroX();
  // float gy_y = hatIMU.readFloatGyroY();
  // float gy_z = hatIMU.readFloatGyroZ();

  // Map x, y values to a range suitable for hue and brightness
  // This example assumes x, y values are between -10 and +10, adjust as needed
  // long hue = map(x, -10, 10, 0, 255); // Map x-axis to hue (0-65535)
  // int brightness = map(y, -10, 10, 0, 255);

  // for (int i = 0; i < NUM_LEDS; ++i) { // iterate over all pixels
  //   pixels.setPixelColor(i, pixels.ColorHSV(current_hue, 255, brightness)); // set color of pixel
  // }

  // Serial.print("hue: ");
  // Serial.println(hue);

  // Serial.print("Accel X: ");
  // Serial.println(x);

  // Serial.print("Gyro X: ");
  // Serial.println(gy_x);

  // Serial.print("Brightness: ");
  // Serial.println(brightness);

  // Serial.print("Accel Y: ");
  // Serial.println(y);

  // Serial.print("Gyro Y: ");
  // Serial.println(gy_y);

  // Serial.print("Accel Z: ");
  // Serial.println(z);

  // Serial.print("Gyro Z: ");
  // Serial.println(gy_z);
  if(!power) {
    pixels.clear();
  }
  pixels.show();
}

void blePeripheralConnectHandler(BLEDevice central)
{
  // central connected event handler
  Serial.print("Connected event, central: ");
  Serial.println(central.address());
}

void blePeripheralDisconnectHandler(BLEDevice central)
{
  // central disconnected event handler
  Serial.print("Disconnected event, central: ");
  Serial.println(central.address());
}

void featureWritten(BLEDevice central, BLECharacteristic characteristic)
{
  uint8_t *data = (uint8_t *)featuresCharacteristic.value();      // Get the data buffer
  unsigned int dataLength = featuresCharacteristic.valueLength(); // Get the data length

  if (dataLength > 0)
  {
    uint8_t feature = data[0]; // The first byte is the feature code

    switch (feature)
    {
    case 0x01:
      if (dataLength >= 2)
      {
        bool powerState = data[1];
        power = powerState;
        Serial.println(powerState ? "Power ON" : "Power OFF");
      }
      break;
    case 0x02:
      if (dataLength >= 5)
      {
        int numLights;
        memcpy(&numLights, data + 1, sizeof(int));
        Serial.print("Number of Lights: ");
        Serial.println(numLights);
        showLightNum(numLights);
      }
      break;
      // Add cases for other features as needed
    case 0x03:
      if (dataLength > 0) {
        // Convert the data from buffer to a C-string
        char temp[dataLength + 1]; // Create a temporary char array
        memcpy(temp, data + 1, dataLength); // Copy the data to the temp array
        temp[dataLength] = '\0'; // Null-terminate the string


            Serial.print("Received String: ");
        Serial.println(temp);

        // Extract hue, saturation, and value from the string
        int hue, saturation, value;
        sscanf(temp, "%d,%d,%d;", &hue, &saturation, &value);
        setSolidHSVColor(hue, saturation, value);
        Serial.print("Hue: ");
        Serial.print(hue);
        Serial.print(", Saturation: ");
        Serial.print(saturation);
        Serial.print(", Value: ");
        Serial.println(value);
      }
      break;
    }

  }
}

void showLightNum(uint32_t numLights)
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    if (i <= numLights)
    {
      pixels.setPixelColor(i, pixels.ColorHSV(255, 255, 255));
    }
    else
    {
      pixels.setPixelColor(i, pixels.ColorHSV(0, 0, 0));
    }
  }
}

void setSolidHSVColor(uint32_t hue, uint32_t saturation, uint32_t value){
  // Scale hue from 0-360 to 0-65535
  uint16_t scaledHue = (uint16_t)((float)hue * 182.0416666667); // 65535 / 360 = 182.0416666667

  for (int i = 0; i < NUM_LEDS; i++){
    pixels.setPixelColor(i, pixels.ColorHSV(scaledHue, saturation, value));
  }
  pixels.show();
}
void updateColorsBasedOnTilt(float x, float y)
{
  // Determine tilt quadrant and magnitude (simplified calculation)
  int quadrant = 0; // 0: front, 1: right, 2: back, 3: left
  if (x > 0.1)
    quadrant = 1; // Tilt to the right
  else if (x < -0.1)
    quadrant = 3; // Tilt to the left
  if (y > 0.1)
    quadrant = 0; // Tilt forward
  else if (y < -0.1)
    quadrant = 2; // Tilt backward

  // Adjust brightness or hue based on quadrant and tilt magnitude for simplicity
  for (int i = 0; i < NUM_LEDS; i++)
  {
    int hue = (255 * i) / NUM_LEDS; // Simplified color pattern, adjust as needed
    int brightness = 255;           // Max brightness, adjust based on tilt if desired

    // Adjust hue or brightness based on quadrant
    switch (quadrant)
    {
    case 0: // Forward tilt
      if (i >= LEFT_MID_LED && i <= RIGHT_MID_LED)
        brightness = 255;
      else
        brightness = 128; // Dim other LEDs
      break;
    case 1: // Right tilt
      if (i >= FRONT_LED_CENTER && i <= BACK_LED_END)
        brightness = 255;
      else
        brightness = 128; // Dim other LEDs
      break;
    case 2: // Backward tilt
      if (i <= LEFT_MID_LED || i >= RIGHT_MID_LED)
        brightness = 255;
      else
        brightness = 128; // Dim other LEDs
      break;
    case 3: // Left tilt
      if (i >= BACK_LED_START && i <= FRONT_LED_CENTER)
        brightness = 255;
      else
        brightness = 128; // Dim other LEDs
      break;
    }

    pixels.setPixelColor(i, pixels.ColorHSV(hue, 255, brightness));
  }
  pixels.show();
}
