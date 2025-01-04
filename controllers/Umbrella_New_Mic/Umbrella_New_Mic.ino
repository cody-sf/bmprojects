#include "arduinoFFT.h"
#include <FastLED.h>
#include <GlobalDefaults.h>
#include <Clock.h>
#include <LightShow.h>
#include <DeviceRoles.h>
#include <ArduinoBLE.h>
#include "Helpers.h"
#include "SyncController.h"
#include <driver/i2s.h>

// Bluetooth
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint32_t value = 0;

static Device umbrella;

// UUID Setup
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "87748abc-e491-43a1-92bd-20b94ba20df4"
#define CONSOLE_MESSAGE_UUID "ecb19d40-d8ef-437b-a208-a56d18deb84e"
#define FEATURES_UUID "e06544bc-1989-4c0b-9ada-8cd4491d23a5"
BLEService umbrellaService(SERVICE_UUID);
// create switch characteristic and allow remote device to read and write
BLECharacteristic featuresCharacteristic(FEATURES_UUID, BLERead | BLEWrite | BLENotify, 512);
// Umbrella Variables
#define SAMPLES 256         // Number of samples for FFT
#define SAMPLING_FREQ 36000 // Sampling frequency
#define FREQUENCY_BANDS 8   // Number of frequency bands (and LED strips)
#define LEDS_PER_STRIP 30   // Number of LEDs per strip
#define NUM_STRIPS 8        // Number of LED strips
#define LED_TYPE WS2812B    // Type of LED strip
#define COLOR_ORDER GRB     // Color order for the LED strip


#define I2S_PORT I2S_NUM_0     // I2S Port
#define I2S_SCK_PIN     GPIO_NUM_26   // Bit Clock
#define I2S_WS_PIN      GPIO_NUM_22   // Word Select (LRCLK)
#define I2S_SD_PIN      GPIO_NUM_21   // Data Input

// LED Strips/Lightshow
CRGB leds[NUM_STRIPS][LEDS_PER_STRIP]; // Array to hold LED data
int ledState[NUM_STRIPS][LEDS_PER_STRIP];

static CLEDController &led_controller_1 = FastLED.addLeds<LED_TYPE, 13, COLOR_ORDER>(leds[0], LEDS_PER_STRIP);
static CLEDController &led_controller_2 = FastLED.addLeds<LED_TYPE, 12, COLOR_ORDER>(leds[1], LEDS_PER_STRIP);
static CLEDController &led_controller_3 = FastLED.addLeds<LED_TYPE, 14, COLOR_ORDER>(leds[2], LEDS_PER_STRIP);
static CLEDController &led_controller_4 = FastLED.addLeds<LED_TYPE, 27, COLOR_ORDER>(leds[3], LEDS_PER_STRIP);
static CLEDController &led_controller_5 = FastLED.addLeds<LED_TYPE, 16, COLOR_ORDER>(leds[4], LEDS_PER_STRIP);
static CLEDController &led_controller_6 = FastLED.addLeds<LED_TYPE, 25, COLOR_ORDER>(leds[5], LEDS_PER_STRIP);
static CLEDController &led_controller_7 = FastLED.addLeds<LED_TYPE, 33, COLOR_ORDER>(leds[6], LEDS_PER_STRIP);
static CLEDController &led_controller_8 = FastLED.addLeds<LED_TYPE, 32, COLOR_ORDER>(leds[7], LEDS_PER_STRIP);
static std::vector<CLEDController *> led_controllers = {&led_controller_1, &led_controller_2, &led_controller_3, &led_controller_4, &led_controller_5, &led_controller_6, &led_controller_7, &led_controller_8};
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
float reference = log10(50.0); // Adjust this based on your setup
bool decay = 0;
int decayRate = 100;
bool reverseStrip = true;
bool power = true;
bool soundSensitive = true;
AvailablePalettes AP_palette;

SyncController syncController(light_show, CURRENT_USER, umbrella);

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

  // Create a string from buffer
  std::string value((char *)buffer, dataLength);
  if (!value.empty())
  {
    uint8_t feature = value[0];

    // Primary Palette
    if (feature == 0x01)
    {
      Serial.println("BTCommand: Primary Palette: ");
      std::string strValue = toLowerCase(value.substr(1));
      AP_palette = stringToPalette(strValue.c_str());
      Serial.print(strValue.c_str());
      syncController.setPalette(AP_palette);
      primaryPalette = light_show.getPrimarySecondaryPalettes().first;
      return;
    }

    // Secondary Palette
    if (feature == 0x02)
    {
      Serial.println("Secondary Palette: ");
      std::string strValue = toLowerCase(value.substr(1));
      AvailablePalettes palette = stringToPalette(strValue.c_str());
      Serial.print(strValue.c_str());
      light_show.setSecondaryPalette(palette);
      secondaryPalette = light_show.getPrimarySecondaryPalettes().second;
      return;
    }

    // Sensitivity
    if (feature == 0x03)
    {
      int sensitivity = 0;
      memcpy(&sensitivity, value.data() + 1, sizeof(int));
      Serial.print("Sensitivity: ");
      Serial.println(sensitivity);
      reference = log10(sensitivity);
      return;
    }

    // Direction
    if (feature == 0x05)
    {
      reverseStrip = value[1] != 0;
      light_show.palette_stream(speed, AP_palette, reverseStrip);
      Serial.print("Received direction: ");
      Serial.println(reverseStrip ? "True" : "False");
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

    // Sound Sensitivity on/off
    if (feature == 0x07)
    {
      soundSensitive = value[1] != 0;
      Serial.print("Sound Sensitive: ");
      Serial.println(soundSensitive ? "On" : "Off");
      light_show.apply_scene_updates(palette_stream);
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
  delay(1000);
  if (!BLE.begin())
  {
    Serial.println("starting Bluetooth® Low Energy module failed!");
    while (1)
      ;
  }
    BLE.setLocalName("Umbrella-CL");
  BLE.setAdvertisedService(umbrellaService);
  umbrellaService.addCharacteristic(featuresCharacteristic);
  BLE.addService(umbrellaService);

  // assign event handlers for connected, disconnected to peripheral
  BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
  BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);

  // assign event handlers for characteristic
  featuresCharacteristic.setEventHandler(BLEWritten, featureCallback);

  // start advertising
  BLE.advertise();

  syncController.begin(CURRENT_USER);

  i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = SAMPLING_FREQ,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 1024,
      .use_apll = false
  };

  i2s_pin_config_t pin_config = {
      .bck_io_num = I2S_SCK_PIN,
      .ws_io_num = I2S_WS_PIN,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = I2S_SD_PIN
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
      Serial.printf("I2S driver install failed: %s\n", esp_err_to_name(err));
      while (true);
  }
  i2s_set_pin(I2S_PORT, &pin_config);

  
  for (int i = 0; i < NUM_STRIPS; i++)
  {
    for (int j = 0; j < LEDS_PER_STRIP; j++)
    {
      ledState[i][j] = 0;
    }
  }
  light_show.brightness(brightness);
}


  void loop()
{
    BLE.poll();

    if (!power)
    {
        FastLED.clear();
        FastLED.show();
        return;
    }

    if (!soundSensitive)
    {
        light_show.render();
        return;
    }

    // Handle Lights
    // Take samples
    for (int i = 0; i < SAMPLES; i++)
    {
        int16_t sample;
        size_t bytesRead = 0;
        i2s_read(I2S_NUM_0, &sample, sizeof(sample), &bytesRead, portMAX_DELAY);
        if (bytesRead > 0)
        {
            vReal[i] = (double)sample;
            vImag[i] = 0.0;
        }
    }

    // Compute FFT
    FFT.dcRemoval();
    FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(FFT_FORWARD);
    FFT.complexToMagnitude();

    double median[FREQUENCY_BANDS] = {0};
    double max[FREQUENCY_BANDS] = {0};
    int index = 0;
    double hzPerSample = (1.0 * SAMPLING_FREQ) / SAMPLES;
    double hz = 0;
    double sum = 0;
    int count = 0;
    for (int i = 2; i < (SAMPLES / 2); i++)
    {
        count++;
        sum += vReal[i];
        if (vReal[i] > max[index])
        {
            max[index] = vReal[i];
        }
        if (hz > (index + 1) * (SAMPLING_FREQ / 2.0) / FREQUENCY_BANDS)
        {
            median[index] = sum / count;
            sum = 0.0;
            count = 0;
            index++;
        }
        hz += hzPerSample;
    }

    // Update LED strips based on FFT results
    if (decay)
    {
        for (int band = 0; band < FREQUENCY_BANDS; band++)
        {
            int newHeight = (median[band] > 0) ? 20.0 * (log10(median[band]) - reference) : 1;
            newHeight = constrain(newHeight, 1, LEDS_PER_STRIP);

            for (int i = 0; i < LEDS_PER_STRIP; i++)
            {
                if (i < newHeight)
                {
                    uint8_t paletteIndex = map(i, 0, LEDS_PER_STRIP - 1, 0, 255);
                    leds[band][i] = ColorFromPalette(primaryPalette, paletteIndex);
                }
                else
                {
                    ledState[band][i] = ledState[band][i] - decayRate > 0 ? ledState[band][i] - decayRate : 0;
                    int hue = (i * 255 / LEDS_PER_STRIP) + (band * 30);
                    leds[band][i] = CHSV(hue, 255, ledState[band][i]);
                }
            }
        }
    }
    else
    {
        for (int band = 0; band < FREQUENCY_BANDS; band++)
        {
            int newHeight = (median[band] > 0) ? 20.0 * (log10(median[band]) - reference) : 1;
            newHeight = constrain(newHeight, 1, LEDS_PER_STRIP);

            for (int i = 0; i < LEDS_PER_STRIP; i++)
            {
                uint8_t onPaletteIndex = map(i, 0, LEDS_PER_STRIP - 1, 0, 255);
                uint8_t offPaletteIndex = map(i, 0, LEDS_PER_STRIP - 1, 0, 255);
                leds[band][i] = (i < newHeight) ? ColorFromPalette(primaryPalette, onPaletteIndex) : ColorFromPalette(secondaryPalette, offPaletteIndex);
            }
        }
    }

    if (reverseStrip)
    {
        reverseLEDs();
    }

    light_show.render();
}

void reverseLEDs()
{
  for (int i = 0; i < NUM_STRIPS; i++)
  {
    std::reverse(&leds[i][0], &leds[i][LEDS_PER_STRIP]);
  }
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

