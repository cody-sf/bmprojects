#include <FastLED.h>
#include <arduinoFFT.h>
// #include <EasyButton.h>
#include <driver/i2s.h>

#define SAMPLES         1024          // Must be a power of 2
#define SAMPLING_FREQ   40000.0       // Hz, determines the maximum frequency analyzed by the FFT Fmax=sampleF/2
#define AMPLITUDE       1000          // Adjust based on your microphone's sensitivity
#define BTN_PIN         4             // Push button pin for changing patterns
#define LONG_PRESS_MS   200           // Long press duration in milliseconds
#define COLOR_ORDER     GRB           // Color order for LED strips
#define CHIPSET         WS2812B       // LED strip type
#define MAX_MILLIAMPS   2000          // Max power for LEDs
const int BRIGHTNESS_SETTINGS[3] = {5, 70, 200}; // Brightness levels for long press
#define LED_VOLTS       5             // LED voltage
#define NUM_STRIPS      8             // Number of LED strips
#define LEDS_PER_STRIP  30            // Number of LEDs per strip
#define NOISE           500           // Noise filter threshold
#define I2S_PORT        I2S_NUM_0     // I2S port for the microphone

// LED strip pin configuration for ESP32 narrow version
const int LED_PINS[NUM_STRIPS] = {12, 13, 15, 16, 17, 21, 22, 23};  // Pin numbers for each LED strip

// FFT and Audio Sampling
unsigned int sampling_period_us;
double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQ); // Specify the template argument explicitly
unsigned long newTime;

// Button Logic
int buttonPushCounter = 0;
bool autoChangePatterns = false;
// EasyButton modeBtn(BTN_PIN);

// FastLED
CRGB leds[NUM_STRIPS][LEDS_PER_STRIP];
DEFINE_GRADIENT_PALETTE( purple_gp ) { 0, 0, 212, 255, 255, 179, 0, 255 };
DEFINE_GRADIENT_PALETTE( outrun_gp ) { 0, 141, 0, 100, 127, 255, 192, 0, 255, 0, 5, 255 };
DEFINE_GRADIENT_PALETTE( greenblue_gp ) { 0, 0, 255, 60, 64, 0, 236, 255, 128, 0, 5, 255, 192, 0, 236, 255, 255, 0, 255, 60 };
DEFINE_GRADIENT_PALETTE( redyellow_gp ) { 0, 200, 200, 200, 64, 255, 218, 0, 128, 231, 0, 0, 192, 255, 218, 0, 255, 200, 200, 200 };
CRGBPalette16 purplePal = purple_gp;
CRGBPalette16 outrunPal = outrun_gp;
CRGBPalette16 greenbluePal = greenblue_gp;
CRGBPalette16 heatPal = redyellow_gp;
uint8_t colorTimer = 0;

void setup() {
    Serial.begin(115200);

    // Initialize FastLED for all LED strips
    for (int i = 0; i < NUM_STRIPS; i++) {
        switch (i) {
            case 0: FastLED.addLeds<CHIPSET, 12, COLOR_ORDER>(leds[i], LEDS_PER_STRIP).setCorrection(TypicalSMD5050); break;
            case 1: FastLED.addLeds<CHIPSET, 13, COLOR_ORDER>(leds[i], LEDS_PER_STRIP).setCorrection(TypicalSMD5050); break;
            case 2: FastLED.addLeds<CHIPSET, 14, COLOR_ORDER>(leds[i], LEDS_PER_STRIP).setCorrection(TypicalSMD5050); break;
            case 3: FastLED.addLeds<CHIPSET, 15, COLOR_ORDER>(leds[i], LEDS_PER_STRIP).setCorrection(TypicalSMD5050); break;
            case 4: FastLED.addLeds<CHIPSET, 16, COLOR_ORDER>(leds[i], LEDS_PER_STRIP).setCorrection(TypicalSMD5050); break;
            case 5: FastLED.addLeds<CHIPSET, 17, COLOR_ORDER>(leds[i], LEDS_PER_STRIP).setCorrection(TypicalSMD5050); break;
            case 6: FastLED.addLeds<CHIPSET, 21, COLOR_ORDER>(leds[i], LEDS_PER_STRIP).setCorrection(TypicalSMD5050); break;
            case 7: FastLED.addLeds<CHIPSET, 22, COLOR_ORDER>(leds[i], LEDS_PER_STRIP).setCorrection(TypicalSMD5050); break;
        }
    }

    FastLED.setMaxPowerInVoltsAndMilliamps(LED_VOLTS, MAX_MILLIAMPS);
    FastLED.setBrightness(BRIGHTNESS_SETTINGS[1]);

    // Initialize Button
    // modeBtn.begin();
    // modeBtn.onPressed(changeMode);
    // modeBtn.onPressedFor(LONG_PRESS_MS, brightnessButton);
    // modeBtn.onSequence(3, 2000, startAutoMode);
    // modeBtn.onSequence(5, 2000, brightnessOff);
    
    // Setup I2S Microphone
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
        .bck_io_num = 14,    // Bit Clock pin
        .ws_io_num = 25,     // Word Select pin
        .data_out_num = I2S_PIN_NO_CHANGE,  // Not used
        .data_in_num = 32    // Data in pin
    };

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
}

void loop() {
    // Button handling
    // modeBtn.read();

    // Reset FFT arrays
    for (int i = 0; i < SAMPLES; i++) {
        vReal[i] = 0;
        vImag[i] = 0;
    }

    // Sample audio from INMP441
    for (int i = 0; i < SAMPLES; i++) {
        int16_t sample;
        size_t bytes_read;
        i2s_read(I2S_PORT, &sample, sizeof(sample), &bytes_read, portMAX_DELAY);
        vReal[i] = (double)sample;
    }

    // Compute FFT
    FFT.dcRemoval();
    FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(FFT_FORWARD);
    FFT.complexToMagnitude();

    // Process FFT results and update LED strips
    updateLEDStrips();

    FastLED.show();
}

void updateLEDStrips() {
    int bandValues[NUM_STRIPS] = {0};
    
    // Analyze FFT results and map them to LED strips
    for (int i = 2; i < (SAMPLES / 2); i++) {
        if (vReal[i] > NOISE) {
            int bandIndex = map(i, 2, SAMPLES / 2, 0, NUM_STRIPS - 1);
            bandValues[bandIndex] += (int)vReal[i];
        }
    }

    // Update each strip based on the FFT results
    for (int strip = 0; strip < NUM_STRIPS; strip++) {
        int barHeight = bandValues[strip] / AMPLITUDE;
        if (barHeight > LEDS_PER_STRIP) barHeight = LEDS_PER_STRIP;

        for (int i = 0; i < LEDS_PER_STRIP; i++) {
            if (i < barHeight) {
                leds[strip][i] = CHSV(strip * (255 / NUM_STRIPS), 255, 255);
            } else {
                leds[strip][i] = CRGB::Black;
            }
        }
    }
}

// Button handling functions
void changeMode() { /* Handle mode change */ }
void startAutoMode() { /* Start auto mode */ }
void brightnessButton() { /* Handle brightness change */ }
void brightnessOff() { FastLED.setBrightness(0); }
