#include <BMDevice.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>

// LED Configuration - 8 LED strips for umbrella
#define NUM_STRIPS 8
#define LEDS_PER_STRIP 38
#define COLOR_ORDER GRB

// BLE Configuration - Using umbrella-specific UUIDs
#define SERVICE_UUID "87748abc-e491-43a1-92bd-20b94ba20df4"
#define FEATURES_UUID "e06544bc-1989-4c0b-9ada-8cd4491d23a5"
#define STATUS_UUID "8d78c901-7dc0-4e1b-9ac9-34731a1ccf49"

// Sound Analysis Configuration
#define SAMPLES 256             // Must be a power of 2
#define SAMPLING_FREQ 46000   // Hz, max frequency analyzed = sampleF/2
#define I2S_PORT I2S_NUM_0      // I2S Port
#define I2S_SCK_PIN GPIO_NUM_26 // Bit Clock
#define I2S_WS_PIN GPIO_NUM_22  // Word Select (LRCLK)
#define I2S_SD_PIN GPIO_NUM_21  // Data Input

// LED Setup - Individual strips for BMDevice
CRGB leds0[LEDS_PER_STRIP];
CRGB leds1[LEDS_PER_STRIP];
CRGB leds2[LEDS_PER_STRIP];
CRGB leds3[LEDS_PER_STRIP];
CRGB leds4[LEDS_PER_STRIP];
CRGB leds5[LEDS_PER_STRIP];
CRGB leds6[LEDS_PER_STRIP];
CRGB leds7[LEDS_PER_STRIP];

// 2D array for sound analysis (points to individual strips)
CRGB* leds[NUM_STRIPS] = {leds0, leds1, leds2, leds3, leds4, leds5, leds6, leds7};

// Sound Analysis Variables
double vReal[SAMPLES];
double vImag[SAMPLES];
ArduinoFFT<double> FFT = ArduinoFFT<double>(vReal, vImag, SAMPLES, SAMPLING_FREQ);

// Sound Settings (stored in defaults)
struct SoundSettings {
    bool soundSensitive = true;
    int amplitude = 1000;
    int noiseThreshold = 500;
    float reference = log10(50.0);
    bool decay = false;
    int decayRate = 100;
};

SoundSettings soundSettings;

// Palette variables (like original)
CRGBPalette16 primaryPalette;
CRGBPalette16 secondaryPalette;

// Create BMDevice - Umbrella with no GPS support
BMDevice device("Umbrella-CL", SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

// Custom feature handler for sound-specific commands
bool handleSoundFeatures(uint8_t feature, const uint8_t* data, size_t length) {
    switch (feature) {
        case 0x03: // Sensitivity
            if (length >= sizeof(int)) {
                int sensitivity = 0;
                memcpy(&sensitivity, data, sizeof(int));
                soundSettings.reference = log10(sensitivity);
                Serial.print("Sound Sensitivity: ");
                Serial.println(sensitivity);
                return true;
            }
            break;
            
        case 0x07: // Sound Sensitivity on/off
            if (length >= 1) {
                soundSettings.soundSensitive = data[0] != 0;
                Serial.print("Sound Sensitive: ");
                Serial.println(soundSettings.soundSensitive ? "On" : "Off");
                return true;
            }
            break;
            
        case 0x0B: // Amplitude
            if (length >= sizeof(int)) {
                memcpy(&soundSettings.amplitude, data, sizeof(int));
                Serial.print("Amplitude: ");
                Serial.println(soundSettings.amplitude);
                return true;
            }
            break;
            
        case 0x0C: // Noise Threshold
            if (length >= sizeof(int)) {
                memcpy(&soundSettings.noiseThreshold, data, sizeof(int));
                Serial.print("Noise Threshold: ");
                Serial.println(soundSettings.noiseThreshold);
                return true;
            }
            break;
            
        case 0x0D: // Decay
            if (length >= 1) {
                soundSettings.decay = data[0] != 0;
                Serial.print("Decay: ");
                Serial.println(soundSettings.decay ? "On" : "Off");
                return true;
            }
            break;
            
        case 0x0E: // Decay Rate
            if (length >= sizeof(int)) {
                memcpy(&soundSettings.decayRate, data, sizeof(int));
                Serial.print("Decay Rate: ");
                Serial.println(soundSettings.decayRate);
                return true;
            }
            break;
    }
    return false; // Not handled
}

// Connection handler to update palettes when device connects/disconnects
void handleConnectionChange(bool connected) {
    if (connected) {
        // Update palettes when connected
        LightShow& lightShow = device.getLightShow();
        std::pair<CRGBPalette16, CRGBPalette16> palettes = lightShow.getPrimarySecondaryPalettes();
        primaryPalette = palettes.first;
        secondaryPalette = palettes.second;
    }
}

void initializeI2S() {
    // I2S Microphone Configuration
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLING_FREQ,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
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
    Serial.println("I2S microphone initialized");
}

void handleSoundAnalysis() {
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
        if (bytes_read > 0) {
            vReal[i] = (double)sample;
        }
    }

    // Compute FFT
    FFT.dcRemoval();
    FFT.windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT.compute(FFT_FORWARD);
    FFT.complexToMagnitude();

    // Update palettes from BMDevice if needed
    LightShow& lightShow = device.getLightShow();
    std::pair<CRGBPalette16, CRGBPalette16> palettes = lightShow.getPrimarySecondaryPalettes();
    primaryPalette = palettes.first;
    secondaryPalette = palettes.second;
    
    // Get current state for direction setting
    BMDeviceState& state = device.getState();
    bool reverseDirection = state.reverseStrip;

    int bandValues[NUM_STRIPS] = {0};

    // Map FFT results to LED strips
    for (int i = 2; i < (SAMPLES / 2); i++) {
        if (vReal[i] > soundSettings.noiseThreshold) {
            int bandIndex = map(i, 2, SAMPLES / 2, 0, NUM_STRIPS - 1);
            bandValues[bandIndex] += (int)vReal[i];
        }
    }

    // Update each LED strip
    for (int strip = 0; strip < NUM_STRIPS; strip++) {
        int barHeight = bandValues[strip] / soundSettings.amplitude;
        if (barHeight > LEDS_PER_STRIP) barHeight = LEDS_PER_STRIP;

        for (int i = 0; i < LEDS_PER_STRIP; i++) {
            uint8_t onPaletteIndex = map(i, 0, LEDS_PER_STRIP - 1, 0, 255);
            uint8_t offPaletteIndex = map(i, 0, LEDS_PER_STRIP - 1, 0, 255);
            
            if (i < barHeight) {
                leds[strip][i] = ColorFromPalette(primaryPalette, onPaletteIndex);
            } else {
                leds[strip][i] = ColorFromPalette(secondaryPalette, offPaletteIndex);
            }
        }
    }

    // Apply direction if reversed
    if (reverseDirection) {
        for (int i = 0; i < NUM_STRIPS; i++) {
            std::reverse(leds[i], leds[i] + LEDS_PER_STRIP);
        }
    }

    // Let BMDevice handle the rendering (reuse existing lightShow reference)
    lightShow.render();
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting Umbrella device...");
    
    // Add all 8 LED strips using individual arrays
    device.addLEDStrip<WS2812B, 32, COLOR_ORDER>(leds0, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 33, COLOR_ORDER>(leds1, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 27, COLOR_ORDER>(leds2, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 14, COLOR_ORDER>(leds3, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 12, COLOR_ORDER>(leds4, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 13, COLOR_ORDER>(leds5, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 18, COLOR_ORDER>(leds6, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 5, COLOR_ORDER>(leds7, LEDS_PER_STRIP);

    
    // Set custom feature handler for sound-specific commands
    device.setCustomFeatureHandler(handleSoundFeatures);
    
    // Set connection handler to update palettes
    device.setCustomConnectionHandler(handleConnectionChange);
    
    // Initialize I2S microphone
    initializeI2S();
    
    // Start the BMDevice
    if (!device.begin()) {
        Serial.println("Failed to initialize BMDevice!");
        while (1);
    }
    
    // Set umbrella-specific defaults
    BMDeviceDefaults& defaults = device.getDefaults();
    
    // Initialize palettes
    LightShow& lightShow = device.getLightShow();
    std::pair<CRGBPalette16, CRGBPalette16> palettes = lightShow.getPrimarySecondaryPalettes();
    primaryPalette = palettes.first;
    secondaryPalette = palettes.second;
    
    Serial.println("Umbrella BMDevice setup complete!");
    defaults.printCurrentDefaults();
}

void loop() {
    // Handle BMDevice operations (BLE, state management, etc.)
    device.loop();
    
    // Get current state
    BMDeviceState& state = device.getState();
    
    // Only proceed if device is powered on
    if (!state.power) {
        FastLED.clear();
        FastLED.show();
        return;
    }
    
    // Handle sound-sensitive mode
    if (soundSettings.soundSensitive) {
        handleSoundAnalysis();
        return;
    }
    else {
        // Use regular BMDevice light show - let BMDevice handle rendering
        LightShow& lightShow = device.getLightShow();
        lightShow.render();
        return;
    }
}

// Available BLE Commands (inherited from BMDevice + custom):
// Standard BMDevice commands:
// 0x01 - Set primary palette
// 0x02 - Set lighting mode/scene
// 0x05 - Set direction
// 0x06 - Set power on/off
// 0x08 - Set speed
// 0x09 - Set brightness
// 0x1A - Get current defaults
// 0x1B - Set defaults from JSON
// 0x1C - Save current state as defaults
// 0x1D - Reset to factory defaults
// 0x1E - Set max brightness limit
// 0x1F - Set device owner
// 0x20 - Set auto-on behavior
//
// Custom Umbrella sound commands:
// 0x03 - Set sensitivity (int)
// 0x07 - Set sound sensitivity on/off (bool)
// 0x0B - Set amplitude (int)
// 0x0C - Set noise threshold (int)
// 0x0D - Set decay on/off (bool)
// 0x0E - Set decay rate (int)
//
// Your Umbrella device now has:
// - Full BMDevice integration with shared palettes and light shows
// - Sound analysis with FFT for 8 LED strips
// - All standard BLE commands from BMDevice
// - Custom sound-specific BLE commands
// - Persistent defaults that survive reboots
// - Automatic status reporting
// - Power management
// - All parameter controls (brightness, speed, effects, direction, etc.) 