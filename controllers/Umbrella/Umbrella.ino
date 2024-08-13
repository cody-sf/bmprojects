#include "arduinoFFT.h"
#include <FastLED.h>
#include <GlobalDefaults.h>
#include <Clock.h>
#include <LightShow.h>
#include <DeviceRoles.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "Helpers.h"

// Bluetooth
BLEServer *pServer = NULL;
BLECharacteristic *consoleMessageCharacteristic = NULL;
BLECharacteristic *featuresCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

// UUID Setup
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "87748abc-e491-43a1-92bd-20b94ba20df4"
#define CONSOLE_MESSAGE_UUID "ecb19d40-d8ef-437b-a208-a56d18deb84e"
#define FEATURES_UUID "e06544bc-1989-4c0b-9ada-8cd4491d23a5"


// Umbrella Variables
#define SAMPLES 256          // Number of samples for FFT
#define SAMPLING_FREQ 36000  // Sampling frequency
#define FREQUENCY_BANDS 8    // Number of frequency bands (and LED strips)
#define LEDS_PER_STRIP 37    // Number of LEDs per strip
#define NUM_STRIPS 8         // Number of LED strips
#define LED_TYPE WS2812B     // Type of LED strip
#define COLOR_ORDER GRB      // Color order for the LED strip
#define ANALOG_PIN 34        // Analog pin for audio input (adjust as needed)

// LED Strips/Lightshow
CRGB leds[NUM_STRIPS][LEDS_PER_STRIP];  // Array to hold LED data
int ledState[NUM_STRIPS][LEDS_PER_STRIP];

static CLEDController &led_controller_1 = FastLED.addLeds<LED_TYPE, 13, COLOR_ORDER>(leds[0], LEDS_PER_STRIP);
static CLEDController &led_controller_2 = FastLED.addLeds<LED_TYPE, 12, COLOR_ORDER>(leds[1], LEDS_PER_STRIP);
static CLEDController &led_controller_3 = FastLED.addLeds<LED_TYPE, 14, COLOR_ORDER>(leds[2], LEDS_PER_STRIP);
static CLEDController &led_controller_4 = FastLED.addLeds<LED_TYPE, 27, COLOR_ORDER>(leds[3], LEDS_PER_STRIP);
static CLEDController &led_controller_5 = FastLED.addLeds<LED_TYPE, 26, COLOR_ORDER>(leds[4], LEDS_PER_STRIP);
static CLEDController &led_controller_6 = FastLED.addLeds<LED_TYPE, 25, COLOR_ORDER>(leds[5], LEDS_PER_STRIP);
static CLEDController &led_controller_7 = FastLED.addLeds<LED_TYPE, 33, COLOR_ORDER>(leds[6], LEDS_PER_STRIP);
static CLEDController &led_controller_8 = FastLED.addLeds<LED_TYPE, 32, COLOR_ORDER>(leds[7], LEDS_PER_STRIP);
static std::vector<CLEDController *> led_controllers = { &led_controller_1, &led_controller_2, &led_controller_3, &led_controller_4, &led_controller_5, &led_controller_6, &led_controller_7, &led_controller_8 };
// static Clock umbrellaClock;
static LightShow light_show(led_controllers);


std::pair<CRGBPalette16, CRGBPalette16> palettes = light_show.getPrimarySecondaryPalettes();
CRGBPalette16 primaryPalette = palettes.first;
CRGBPalette16 secondaryPalette = palettes.second;

// Sound Samples
double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQ);


// Umbrella variables
int brightness = 25;
int speed = 100;
float reference = log10(50.0);  // Adjust this based on your setup
bool decay = 0;
int decayRate = 100;
bool reverseStrip = true;
bool power = true;
bool soundSensitive = true;
AvailablePalettes AP_palette;

std::string toLowerCase(const std::string &input) {
  std::string output = input;
  for (auto &c : output) c = tolower(c);
  return output;
}

// BT Callbacks
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    BLEDevice::startAdvertising();
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
  }
};

class FeaturesCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    std::string value = pCharacteristic->getValue();


    if (!value.empty()) {
      uint8_t feature = value[0];

      // Primary Palette
      if (feature == 0x01) {
        Serial.println("Primary Palette: ");
        std::string strValue = toLowerCase(value.substr(1));
        AP_palette = stringToPalette(strValue.c_str());
        Serial.print(strValue.c_str());
        light_show.setPrimaryPalette(AP_palette);
        light_show.palette_stream(speed, AP_palette, reverseStrip);
        primaryPalette = light_show.getPrimarySecondaryPalettes().first;
        return;
      }

      // Secondary Palette
      if (feature == 0x02) {
        Serial.println("Secondary Palette: ");
        std::string strValue = toLowerCase(value.substr(1));
        AvailablePalettes palette = stringToPalette(strValue.c_str());
        Serial.print(strValue.c_str());
        light_show.setSecondaryPalette(palette);
        secondaryPalette = light_show.getPrimarySecondaryPalettes().second;
        return;
      }

      // Sensitivity
      if (feature == 0x03) {
        int sensitivity = 0;
        memcpy(&sensitivity, value.data() + 1, sizeof(int));
        Serial.print("Sensitivity: ");
        Serial.println(sensitivity);
        reference = log10(sensitivity);
        return;
      }

      // Direction
      if (feature == 0x05) {
        reverseStrip = value[1] != 0;
        light_show.palette_stream(speed, AP_palette, reverseStrip);
        Serial.print("Received direction: ");
        Serial.println(reverseStrip ? "True" : "False");
        return;
      }

      // On/Off
      if (feature == 0x06) {
        power = value[1] != 0;
        Serial.print("Power: ");
        Serial.println(power ? "On" : "Off");
        return;
      }

      // Sound Sensitivity on/off
      if (feature == 0x07) {
        soundSensitive = value[1] != 0;
        Serial.print("Sound Sensitive: ");
        Serial.println(soundSensitive ? "On" : "Off");
        light_show.apply_scene_updates(palette_stream);
        return;
      }

      // Speed
      if (feature == 0x08) {
        int s = 0;
        memcpy(&s, value.data() + 1, sizeof(int));
        Serial.print("Speed: ");
        Serial.println(s);
        speed = s;
        light_show.palette_stream(speed, AP_palette, reverseStrip);
        return;
      }

      // Brightness
      if (feature == 0x09) {
        int b = 0;
        memcpy(&b, value.data() + 1, sizeof(int));
        Serial.print("Brightness: ");
        Serial.println(b);
        brightness = b;
        light_show.brightness(brightness);
        return;
      }
    }
  }
};


class ConsoleMessageCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    std::string value = pCharacteristic->getValue();
    if (!value.empty()) {
      uint8_t dataType = value[0];  // Get the first byte indicating the data type

      // Check if the data type is a string (0x01)
      if (dataType == 0x01) {
        Serial.println("Console: ");

        // Optional: Extract the string from the received data, skipping the first byte
        std::string strValue = value.substr(1);
        Serial.print(strValue.c_str());

        // Add your logic here to handle the string data
        // For example, update some internal state or trigger another action
      }
    }
  }
};



void setup() {
  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("Umbrella-CL");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create BLE Characteristics
  consoleMessageCharacteristic = pService->createCharacteristic(
    CONSOLE_MESSAGE_UUID,
    BLECharacteristic::PROPERTY_WRITE);
  featuresCharacteristic = pService->createCharacteristic(
    FEATURES_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);


  consoleMessageCharacteristic->addDescriptor(new BLE2902());
  featuresCharacteristic->addDescriptor(new BLE2902());





  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  consoleMessageCharacteristic->setCallbacks(new ConsoleMessageCallbacks());
  featuresCharacteristic->setCallbacks(new FeaturesCallbacks());

  // Start the service
  pService->start();
  BLEDevice::startAdvertising();
  light_show.brightness(brightness);
  FastLED.setBrightness(brightness);
  for (int i = 0; i < NUM_STRIPS; i++) {
    for (int j = 0; j < LEDS_PER_STRIP; j++) {
      ledState[i][j] = 0;
    }
  }
}

void loop() {
  // notify changed value
  if (deviceConnected) {
    delay(10);  // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);                   // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising();  // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  if (!power) {
    FastLED.clear();
    FastLED.show();
    return;
  }

  if (!soundSensitive) {
    light_show.render();
    return;
  }

  // Handle Lights
  // Take samples
  for (int i = 0; i < SAMPLES; i++) {
    vReal[i] = analogRead(ANALOG_PIN);
    vImag[i] = 0;
    delayMicroseconds(1000000 / SAMPLING_FREQ);
  }

  // Compute FFT
  FFT.dcRemoval();
  FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  FFT.compute(FFT_FORWARD);
  FFT.complexToMagnitude();

  double median[FREQUENCY_BANDS];
  double max[FREQUENCY_BANDS];
  int index = 0;
  double hzPerSample = (1.0 * SAMPLING_FREQ) / SAMPLES;
  double hz = 0;
  double sum = 0;
  int count = 0;
  for (int i = 2; i < (SAMPLES / 2); i++) {
    count++;
    sum += vReal[i];
    if (vReal[i] > max[index]) {
      max[index] = vReal[i];
    }
    if (hz > (index + 1) * (SAMPLING_FREQ / 2.0) / FREQUENCY_BANDS) {
      median[index] = sum / count;
      sum = 0.0;
      count = 0;
      index++;
    }
    hz += hzPerSample;
  }

  // Update LED strips based on FFT results
  if (decay) {
    for (int band = 0; band < FREQUENCY_BANDS; band++) {
      int newHeight = (median[band] > 0) ? 20.0 * (log10(median[band]) - reference) : 1;
      newHeight = constrain(newHeight, 1, LEDS_PER_STRIP);

      for (int i = 0; i < LEDS_PER_STRIP; i++) {
        if (i < newHeight) {
          uint8_t paletteIndex = map(i, 0, LEDS_PER_STRIP - 1, 0, 255);
          leds[band][i] = ColorFromPalette(primaryPalette, paletteIndex);
        } else {
          ledState[band][i] = ledState[band][i] - decayRate > 0 ? ledState[band][i] - decayRate : 0;
          int hue = (i * 255 / LEDS_PER_STRIP) + (band * 30);
          leds[band][i] = CHSV(hue, 255, ledState[band][i]);
        }
      }
    }
  }

  // No Decay
  else {
    for (int band = 0; band < FREQUENCY_BANDS; band++) {
      int newHeight = (median[band] > 0) ? 20.0 * (log10(median[band]) - reference) : 1;
      newHeight = constrain(newHeight, 1, LEDS_PER_STRIP);

      for (int i = 0; i < LEDS_PER_STRIP; i++) {
        uint8_t onPaletteIndex = map(i, 0, LEDS_PER_STRIP - 1, 0, 255);
        uint8_t offPaletteIndex = map(i, 0, LEDS_PER_STRIP - 1, 0, 255);
        leds[band][i] = (i < newHeight) ? ColorFromPalette(primaryPalette, onPaletteIndex) : ColorFromPalette(secondaryPalette, offPaletteIndex);
      }
    }
  }

  if (reverseStrip) {
    reverseLEDs();
  }

  FastLED.setBrightness(brightness);
  FastLED.show();
}

void reverseLEDs() {
  for (int i = 0; i < NUM_STRIPS; i++) {
    std::reverse(&leds[i][0], &leds[i][LEDS_PER_STRIP]);
  }
}
