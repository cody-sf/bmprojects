#include <BMDevice.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>

// === HARDWARE CONFIGURATION ===
// LED Configuration - 8 LED strips for umbrella
#define NUM_STRIPS 8
#define LEDS_PER_STRIP 38
#define COLOR_ORDER GRB

// BLE Configuration - Same UUIDs as original
#define SERVICE_UUID "87748abc-e491-43a1-92bd-20b94ba20df4"
#define FEATURES_UUID "e06544bc-1989-4c0b-9ada-8cd4491d23a5"
#define STATUS_UUID "8d78c901-7dc0-4e1b-9ac9-34731a1ccf49"

// Sound Analysis Configuration
#define DEFAULT_SAMPLES 256
#define DEFAULT_SAMPLING_FREQ 46000
#define I2S_PORT I2S_NUM_0
#define I2S_SCK_PIN GPIO_NUM_26
#define I2S_WS_PIN GPIO_NUM_22
#define I2S_SD_PIN GPIO_NUM_21

// === LED SETUP ===
// Individual LED strip arrays for BMDevice
CRGB leds0[LEDS_PER_STRIP], leds1[LEDS_PER_STRIP], leds2[LEDS_PER_STRIP], leds3[LEDS_PER_STRIP];
CRGB leds4[LEDS_PER_STRIP], leds5[LEDS_PER_STRIP], leds6[LEDS_PER_STRIP], leds7[LEDS_PER_STRIP];

// 2D array for easy sound analysis access
CRGB* leds[NUM_STRIPS] = {leds0, leds1, leds2, leds3, leds4, leds5, leds6, leds7};

// === SOUND ANALYSIS VARIABLES ===
double* vReal = nullptr;
double* vImag = nullptr;
ArduinoFFT<double>* FFT = nullptr;

// === PALETTE MANAGEMENT ===
// Simple, clean palette variables (like original Umbrella.ino)
CRGBPalette16 primaryPalette;
CRGBPalette16 secondaryPalette;
AvailablePalettes currentSecondaryPaletteId = AvailablePalettes::earth;
bool secondaryPaletteOff = false;  // Start with secondary palette ON for proper visualization

// === SOUND SETTINGS ===
struct SoundSettings {
    // Basic settings
    bool soundSensitive = true;
    int amplitude = 1000;
    int noiseThreshold = 500;
    float reference = log10(50.0);
    bool decay = false;
    int decayRate = 100;
    
    // Visual modes
    bool barMode = true;
    bool rainbowMode = false;
    int colorSpeed = 50;
    bool reverseDirection = false;
    
    // Advanced features
    bool peakHold = false;
    int peakHoldTime = 500;
    bool smoothing = true;
    float smoothingFactor = 0.3;
    bool intensityMapping = false;
    
    // Beat detection
    bool beatDetection = false;
    int beatSensitivity = 70;
    bool strobeOnBeat = false;
    bool pulseOnBeat = false;
    
    // Frequency controls
    int bassEmphasis = 50;
    int midEmphasis = 50;
    int trebleEmphasis = 50;
    bool logarithmicMapping = true;
    
    // Auto features
    bool autoGain = false;
    bool ambientCompensation = false;
    int stripMapping = 0;
    bool individualDirections = false;
    
    // Advanced audio settings
    int samplingFrequency = DEFAULT_SAMPLING_FREQ;
    int sampleCount = DEFAULT_SAMPLES;
    float gainMultiplier = 1.0;
    int minLEDs = 0;
    int maxLEDs = LEDS_PER_STRIP;
    int frequencyMin = 50;
    int frequencyMax = 8000;
    bool doubleHeight = false;
    float ledMultiplier = 1.0;
    bool fillFromCenter = false;
};

SoundSettings soundSettings;

// === TRACKING VARIABLES ===
float peakValues[NUM_STRIPS] = {0};
unsigned long peakTimes[NUM_STRIPS] = {0};
float smoothedValues[NUM_STRIPS] = {0};
float previousAverage = 0;
unsigned long lastBeatTime = 0;
bool beatDetected = false;
float recentMax = 0;
unsigned long lastGainAdjust = 0;

// === BMDevice ===
BMDevice device("Umbrella-CL", SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

// === FUNCTION DECLARATIONS ===
void initializeFFT(int sampleCount, int samplingFreq);
void reinitializeI2S(int samplingFreq);
void updatePalettesFromBMDevice();
void handleSoundVisualization();
void sendUmbrellaStatus();

// === PALETTE MANAGEMENT ===
void updatePalettesFromBMDevice() {
    LightShow& lightShow = device.getLightShow();
    BMDeviceState& state = device.getState();
    
    // Update LightShow's primary palette based on BMDevice's current palette
    lightShow.setPrimaryPalette(state.currentPalette);
    
    // Update LightShow's secondary palette based on our secondary palette setting
    if (!secondaryPaletteOff) {
        lightShow.setSecondaryPalette(currentSecondaryPaletteId);
    }
    
    // Now get the updated palettes from LightShow
    std::pair<CRGBPalette16, CRGBPalette16> palettes = lightShow.getPrimarySecondaryPalettes();
    primaryPalette = palettes.first;
    secondaryPalette = palettes.second;
    
    Serial.printf("🎨 [PALETTE UPDATE] Primary: %s, Secondary: %s (Off: %s)\n",
                 LightShow::paletteIdToName(state.currentPalette),
                 LightShow::paletteIdToName(currentSecondaryPaletteId),
                 secondaryPaletteOff ? "true" : "false");
}

// === SOUND FEATURE HANDLER ===
bool handleSoundFeatures(uint8_t feature, const uint8_t* data, size_t length) {
    Serial.printf("🔧 [SOUND FEATURE] Received: 0x%02X, length: %d\n", feature, length);
    
    // Let BMDevice handle standard features - CORRECTED FEATURE CODES
    if (feature == 0x01 || feature == 0x04 || feature == 0x05 || 
        feature == 0x06 || feature == 0x08 || feature == 0x0A) {
        return false; // BMDevice handles these
    }
    
    // Handle secondary palette
    if (feature == 0x09) {
        if (length > 1) {
            std::string paletteName(reinterpret_cast<const char*>(data + 1), length - 1);
            Serial.printf("🎨 [SECONDARY PALETTE] Setting to: %s\n", paletteName.c_str());
            
            if (paletteName == "off") {
                secondaryPaletteOff = true;
                Serial.println("🎨 [SECONDARY PALETTE] Set to OFF mode");
            } else {
                secondaryPaletteOff = false;
                AvailablePalettes paletteEnum = LightShow::paletteNameToId(paletteName.c_str());
                currentSecondaryPaletteId = paletteEnum;
                // Update LightShow's secondary palette and get the updated palettes
                updatePalettesFromBMDevice();
            }
            return true;
        }
        return false;
    }
    
    // Handle sound-specific features
    switch (feature) {
        case 0x03: // Sensitivity
            if (length >= sizeof(int)) {
                int sensitivity = 0;
                memcpy(&sensitivity, data + 1, sizeof(int));
                soundSettings.reference = log10(sensitivity);
                Serial.printf("✅ Sensitivity: %d\n", sensitivity);
                return true;
            }
            break;
            
        case 0x07: // Sound Sensitivity on/off
            if (length >= 2) {
                soundSettings.soundSensitive = data[1] != 0;
                Serial.printf("✅ Sound Mode: %s\n", soundSettings.soundSensitive ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x0B: // Amplitude
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.amplitude, data + 1, sizeof(int));
                Serial.printf("✅ Amplitude: %d\n", soundSettings.amplitude);
                return true;
            }
            break;
            
        case 0x0C: // Noise Threshold
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.noiseThreshold, data + 1, sizeof(int));
                Serial.printf("✅ Noise Threshold: %d\n", soundSettings.noiseThreshold);
                return true;
            }
            break;
            
        case 0x0D: // Decay
            if (length >= 2) {
                soundSettings.decay = data[1] != 0;
                Serial.printf("✅ Decay: %s\n", soundSettings.decay ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x0E: // Decay Rate
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.decayRate, data + 1, sizeof(int));
                Serial.printf("✅ Decay Rate: %d\n", soundSettings.decayRate);
                return true;
            }
            break;
            
        case 0x0F: // Peak Hold On/Off
            if (length >= 2) {
                soundSettings.peakHold = data[1] != 0;
                Serial.printf("✅ Peak Hold: %s\n", soundSettings.peakHold ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x10: // Peak Hold Time
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.peakHoldTime, data + 1, sizeof(int));
                Serial.printf("✅ Peak Hold Time: %d\n", soundSettings.peakHoldTime);
                return true;
            }
            break;
            
        case 0x11: // Smoothing On/Off
            if (length >= 2) {
                soundSettings.smoothing = data[1] != 0;
                Serial.printf("✅ Smoothing: %s\n", soundSettings.smoothing ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x12: // Smoothing Factor
            if (length >= sizeof(float) + 1) {
                memcpy(&soundSettings.smoothingFactor, data + 1, sizeof(float));
                Serial.printf("✅ Smoothing Factor: %.2f\n", soundSettings.smoothingFactor);
                return true;
            }
            break;
            
        case 0x13: // Bar Mode
            if (length >= 2) {
                soundSettings.barMode = data[1] != 0;
                Serial.printf("✅ Visual Mode: %s\n", soundSettings.barMode ? "Bars" : "Dots");
                return true;
            }
            break;
            
        case 0x14: // Rainbow Mode
            if (length >= 2) {
                soundSettings.rainbowMode = data[1] != 0;
                Serial.printf("✅ Rainbow Mode: %s\n", soundSettings.rainbowMode ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x15: // Color Speed
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.colorSpeed, data + 1, sizeof(int));
                Serial.printf("✅ Color Speed: %d\n", soundSettings.colorSpeed);
                return true;
            }
            break;
            
        case 0x16: // Intensity Mapping
            if (length >= 2) {
                soundSettings.intensityMapping = data[1] != 0;
                Serial.printf("✅ Intensity Mapping: %s\n", soundSettings.intensityMapping ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x17: // Beat Detection
            if (length >= 2) {
                soundSettings.beatDetection = data[1] != 0;
                Serial.printf("✅ Beat Detection: %s\n", soundSettings.beatDetection ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x18: // Beat Sensitivity
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.beatSensitivity, data + 1, sizeof(int));
                Serial.printf("✅ Beat Sensitivity: %d\n", soundSettings.beatSensitivity);
                return true;
            }
            break;
            
        case 0x19: // Strobe on Beat
            if (length >= 2) {
                soundSettings.strobeOnBeat = data[1] != 0;
                Serial.printf("✅ Strobe on Beat: %s\n", soundSettings.strobeOnBeat ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x21: // Pulse on Beat
            if (length >= 2) {
                soundSettings.pulseOnBeat = data[1] != 0;
                Serial.printf("✅ Pulse on Beat: %s\n", soundSettings.pulseOnBeat ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x22: // Bass Emphasis
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.bassEmphasis, data + 1, sizeof(int));
                Serial.printf("✅ Bass Emphasis: %d\n", soundSettings.bassEmphasis);
                return true;
            }
            break;
            
        case 0x23: // Mid Emphasis
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.midEmphasis, data + 1, sizeof(int));
                Serial.printf("✅ Mid Emphasis: %d\n", soundSettings.midEmphasis);
                return true;
            }
            break;
            
        case 0x24: // Treble Emphasis
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.trebleEmphasis, data + 1, sizeof(int));
                Serial.printf("✅ Treble Emphasis: %d\n", soundSettings.trebleEmphasis);
                return true;
            }
            break;
            
        case 0x25: // Logarithmic Mapping
            if (length >= 2) {
                soundSettings.logarithmicMapping = data[1] != 0;
                Serial.printf("✅ Logarithmic Mapping: %s\n", soundSettings.logarithmicMapping ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x26: // Auto Gain
            if (length >= 2) {
                soundSettings.autoGain = data[1] != 0;
                Serial.printf("✅ Auto Gain: %s\n", soundSettings.autoGain ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x27: // Ambient Compensation
            if (length >= 2) {
                soundSettings.ambientCompensation = data[1] != 0;
                Serial.printf("✅ Ambient Compensation: %s\n", soundSettings.ambientCompensation ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x28: // Strip Mapping
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.stripMapping, data + 1, sizeof(int));
                Serial.printf("✅ Strip Mapping: %d\n", soundSettings.stripMapping);
                return true;
            }
            break;
            
        case 0x29: // Individual Directions
            if (length >= 2) {
                soundSettings.individualDirections = data[1] != 0;
                Serial.printf("✅ Individual Directions: %s\n", soundSettings.individualDirections ? "ON" : "OFF");
                return true;
            }
            break;
            
        // NEW ADVANCED FEATURES
        case 0x2A: // Sampling Frequency
            if (length >= sizeof(int) + 1) {
                int newFreq = 0;
                memcpy(&newFreq, data + 1, sizeof(int));
                newFreq = constrain(newFreq, 8000, 96000);
                soundSettings.samplingFrequency = newFreq;
                Serial.printf("✅ Sampling Frequency: %d\n", newFreq);
                reinitializeI2S(newFreq);
                return true;
            }
            break;
            
        case 0x2B: // Sample Count
            if (length >= sizeof(int) + 1) {
                int newCount = 0;
                memcpy(&newCount, data + 1, sizeof(int));
                newCount = constrain(newCount, 64, 2048);
                // Round to nearest power of 2
                int powerOf2 = 1;
                while (powerOf2 < newCount) powerOf2 <<= 1;
                soundSettings.sampleCount = powerOf2;
                Serial.printf("✅ Sample Count: %d\n", powerOf2);
                initializeFFT(soundSettings.sampleCount, soundSettings.samplingFrequency);
                return true;
            }
            break;
            
        case 0x2C: // Gain Multiplier
            if (length >= sizeof(float) + 1) {
                memcpy(&soundSettings.gainMultiplier, data + 1, sizeof(float));
                soundSettings.gainMultiplier = constrain(soundSettings.gainMultiplier, 0.1f, 10.0f);
                Serial.printf("✅ Gain Multiplier: %.2f\n", soundSettings.gainMultiplier);
                return true;
            }
            break;
            
        case 0x2D: // Min LEDs
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.minLEDs, data + 1, sizeof(int));
                soundSettings.minLEDs = constrain(soundSettings.minLEDs, 0, LEDS_PER_STRIP);
                Serial.printf("✅ Min LEDs: %d\n", soundSettings.minLEDs);
                return true;
            }
            break;
            
        case 0x2E: // Max LEDs
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.maxLEDs, data + 1, sizeof(int));
                soundSettings.maxLEDs = constrain(soundSettings.maxLEDs, 1, LEDS_PER_STRIP);
                Serial.printf("✅ Max LEDs: %d\n", soundSettings.maxLEDs);
                return true;
            }
            break;
            
        case 0x2F: // Frequency Min
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.frequencyMin, data + 1, sizeof(int));
                soundSettings.frequencyMin = constrain(soundSettings.frequencyMin, 20, 10000);
                Serial.printf("✅ Frequency Min: %d\n", soundSettings.frequencyMin);
                return true;
            }
            break;
            
        case 0x30: // Frequency Max
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.frequencyMax, data + 1, sizeof(int));
                soundSettings.frequencyMax = constrain(soundSettings.frequencyMax, 100, 20000);
                Serial.printf("✅ Frequency Max: %d\n", soundSettings.frequencyMax);
                return true;
            }
            break;
            
        case 0x31: // Double Height
            if (length >= 2) {
                soundSettings.doubleHeight = data[1] != 0;
                Serial.printf("✅ Double Height: %s\n", soundSettings.doubleHeight ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x32: // LED Multiplier
            if (length >= sizeof(float) + 1) {
                memcpy(&soundSettings.ledMultiplier, data + 1, sizeof(float));
                soundSettings.ledMultiplier = constrain(soundSettings.ledMultiplier, 0.1f, 5.0f);
                Serial.printf("✅ LED Multiplier: %.2f\n", soundSettings.ledMultiplier);
                return true;
            }
            break;
            
        case 0x33: // Fill From Center
            if (length >= 2) {
                soundSettings.fillFromCenter = data[1] != 0;
                Serial.printf("✅ Fill From Center: %s\n", soundSettings.fillFromCenter ? "ON" : "OFF");
                return true;
            }
            break;
            
        default:
            Serial.printf("❓ Unknown sound feature: 0x%02X\n", feature);
            return false;
    }
    
    Serial.printf("❌ Invalid data length for feature 0x%02X\n", feature);
    return false;
}

// === CONNECTION HANDLER ===
void handleConnectionChange(bool connected) {
    Serial.printf("📡 [CONNECTION] %s\n", connected ? "CONNECTED" : "DISCONNECTED");
    
    if (connected) {
        updatePalettesFromBMDevice();
        sendUmbrellaStatus();
    }
}

// === FFT INITIALIZATION ===
void initializeFFT(int sampleCount, int samplingFreq) {
    if (FFT) {
        delete FFT;
        delete[] vReal;
        delete[] vImag;
    }
    
    vReal = new double[sampleCount];
    vImag = new double[sampleCount];
    FFT = new ArduinoFFT<double>(vReal, vImag, sampleCount, samplingFreq);
    
    Serial.printf("🎵 [FFT] Initialized: %d samples at %d Hz\n", sampleCount, samplingFreq);
}

// === I2S REINITIALIZATION ===
void reinitializeI2S(int samplingFreq) {
    // Uninstall existing I2S driver
    i2s_driver_uninstall(I2S_PORT);
    
    // I2S Microphone Configuration with new sampling frequency
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = (uint32_t)samplingFreq,
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
        Serial.printf("❌ I2S driver reinstall failed: %s\n", esp_err_to_name(err));
        return;
    }
    i2s_set_pin(I2S_PORT, &pin_config);
    
    // Update current frequency and reinitialize FFT
    soundSettings.samplingFrequency = samplingFreq;
    initializeFFT(soundSettings.sampleCount, samplingFreq);
    
    Serial.printf("🎤 I2S reinitialized with sampling frequency: %d Hz\n", samplingFreq);
}

// === I2S INITIALIZATION ===
void initializeI2S() {
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = (uint32_t)DEFAULT_SAMPLING_FREQ,
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
        Serial.printf("❌ I2S driver install failed: %s\n", esp_err_to_name(err));
        while (true);
    }
    i2s_set_pin(I2S_PORT, &pin_config);
    Serial.println("🎤 I2S microphone initialized");
}

// === ADVANCED SOUND VISUALIZATION ===
void handleSoundVisualization() {
    // Ensure FFT is initialized
    if (!FFT || !vReal || !vImag) {
        Serial.println("❌ FFT not initialized, skipping analysis");
        return;
    }

    // Reset FFT arrays
    for (int i = 0; i < soundSettings.sampleCount; i++) {
        vReal[i] = 0;
        vImag[i] = 0;
    }

    // Sample audio from INMP441
    for (int i = 0; i < soundSettings.sampleCount; i++) {
        int16_t sample;
        size_t bytes_read;
        i2s_read(I2S_PORT, &sample, sizeof(sample), &bytes_read, portMAX_DELAY);
        if (bytes_read > 0) {
            // Apply gain multiplier early in the chain
            vReal[i] = (double)sample * soundSettings.gainMultiplier;
        }
    }

    // Compute FFT
    FFT->dcRemoval();
    FFT->windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
    FFT->compute(FFT_FORWARD);
    FFT->complexToMagnitude();

    // Get lightShow reference for rendering
    LightShow& lightShow = device.getLightShow();
    
    // Get current state for direction setting
    BMDeviceState& state = device.getState();
    bool reverseDirection = state.reverseStrip;

    // Calculate frequency range for analysis based on settings
    int minFreqBin = (soundSettings.frequencyMin * soundSettings.sampleCount) / soundSettings.samplingFrequency;
    int maxFreqBin = (soundSettings.frequencyMax * soundSettings.sampleCount) / soundSettings.samplingFrequency;
    minFreqBin = constrain(minFreqBin, 2, soundSettings.sampleCount / 2);
    maxFreqBin = constrain(maxFreqBin, minFreqBin + 1, soundSettings.sampleCount / 2);
    
    // Calculate overall volume for beat detection and auto-gain
    float totalVolume = 0;
    for (int i = minFreqBin; i < maxFreqBin; i++) {
        totalVolume += vReal[i];
    }
    float averageVolume = totalVolume / (maxFreqBin - minFreqBin);
    
    // Beat detection
    beatDetected = false;
    if (soundSettings.beatDetection) {
        float beatThreshold = previousAverage * (1.0 + soundSettings.beatSensitivity / 100.0);
        unsigned long currentTime = millis();
        
        if (averageVolume > beatThreshold && (currentTime - lastBeatTime) > 100) { // 100ms minimum between beats
            beatDetected = true;
            lastBeatTime = currentTime;
            Serial.println("🎵 Beat detected!");
        }
        previousAverage = averageVolume * 0.9 + previousAverage * 0.1; // Smooth average
    }
    
    // Auto-gain adjustment
    if (soundSettings.autoGain) {
        if (averageVolume > recentMax) {
            recentMax = averageVolume;
        }
        
        unsigned long currentTime = millis();
        if (currentTime - lastGainAdjust > 2000) { // Adjust every 2 seconds
            if (recentMax > 0) {
                float targetAmplitude = 32000; // Target for good visual range
                float gainAdjustment = targetAmplitude / recentMax;
                soundSettings.amplitude = (int)(soundSettings.amplitude * gainAdjustment);
                soundSettings.amplitude = constrain(soundSettings.amplitude, 100, 10000);
                Serial.printf("🔧 Auto-gain adjusted amplitude to: %d\n", soundSettings.amplitude);
            }
            recentMax *= 0.8; // Decay recent max
            lastGainAdjust = currentTime;
        }
    }
    
    // Ambient noise compensation
    if (soundSettings.ambientCompensation) {
        float ambientLevel = averageVolume * 0.1; // Estimate ambient as 10% of current
        soundSettings.noiseThreshold = (int)(ambientLevel * 1.5);
    }

    // Initialize band values
    float rawBandValues[NUM_STRIPS] = {0};
    
    // Calculate frequency ranges for each strip based on mapping mode
    int stripOrder[NUM_STRIPS];
    switch (soundSettings.stripMapping) {
        case 0: // Normal (0,1,2,3,4,5,6,7)
            for (int i = 0; i < NUM_STRIPS; i++) stripOrder[i] = i;
            break;
        case 1: // Bass to Treble (low freq to high freq)
            for (int i = 0; i < NUM_STRIPS; i++) stripOrder[i] = i;
            break;
        case 2: // Treble to Bass (high freq to low freq)
            for (int i = 0; i < NUM_STRIPS; i++) stripOrder[i] = NUM_STRIPS - 1 - i;
            break;
        case 3: // Center out (mid frequencies in center)
            int centerOut[8] = {3, 4, 2, 5, 1, 6, 0, 7};
            for (int i = 0; i < NUM_STRIPS; i++) stripOrder[i] = centerOut[i];
            break;
    }

    // Map FFT results to LED strips with frequency emphasis
    for (int i = minFreqBin; i < maxFreqBin; i++) {
        if (vReal[i] > soundSettings.noiseThreshold) {
            int bandIndex;
            
            if (soundSettings.logarithmicMapping) {
                // Logarithmic mapping for more musical response
                float logPos = log(i - minFreqBin + 1) / log(maxFreqBin - minFreqBin);
                bandIndex = (int)(logPos * NUM_STRIPS);
            } else {
                // Linear mapping
                bandIndex = map(i, minFreqBin, maxFreqBin - 1, 0, NUM_STRIPS - 1);
            }
            
            bandIndex = constrain(bandIndex, 0, NUM_STRIPS - 1);
            
            // Apply frequency emphasis
            float emphasis = 1.0;
            if (bandIndex < NUM_STRIPS / 3) {
                // Bass range
                emphasis = soundSettings.bassEmphasis / 50.0;
            } else if (bandIndex < 2 * NUM_STRIPS / 3) {
                // Mid range  
                emphasis = soundSettings.midEmphasis / 50.0;
            } else {
                // Treble range
                emphasis = soundSettings.trebleEmphasis / 50.0;
            }
            
            rawBandValues[stripOrder[bandIndex]] += vReal[i] * emphasis;
        }
    }

    // Apply smoothing
    for (int strip = 0; strip < NUM_STRIPS; strip++) {
        if (soundSettings.smoothing) {
            smoothedValues[strip] = smoothedValues[strip] * (1.0 - soundSettings.smoothingFactor) + 
                                   rawBandValues[strip] * soundSettings.smoothingFactor;
            rawBandValues[strip] = smoothedValues[strip];
        }
    }
    
    // Apply peak hold
    unsigned long currentTime = millis();
    for (int strip = 0; strip < NUM_STRIPS; strip++) {
        if (soundSettings.peakHold) {
            if (rawBandValues[strip] > peakValues[strip]) {
                peakValues[strip] = rawBandValues[strip];
                peakTimes[strip] = currentTime;
            } else if (currentTime - peakTimes[strip] > soundSettings.peakHoldTime) {
                peakValues[strip] *= 0.95; // Gradual decay
            }
            rawBandValues[strip] = max(rawBandValues[strip], peakValues[strip]);
        }
    }

    // Update each LED strip with all the visual modes
    for (int strip = 0; strip < NUM_STRIPS; strip++) {
        // Calculate bar height with new enhancement features
        float rawHeight = rawBandValues[strip] / soundSettings.amplitude;
        
        // Apply LED multiplier for more sensitivity
        rawHeight *= soundSettings.ledMultiplier;
        
        // Apply double height if enabled
        if (soundSettings.doubleHeight) {
            rawHeight *= 2.0;
        }
        
        int barHeight = (int)rawHeight;
        
        // Apply min/max LED constraints
        if (barHeight > 0) {
            barHeight = max(barHeight, soundSettings.minLEDs);
        }
        barHeight = min(barHeight, soundSettings.maxLEDs);
        
        // Beat effects
        bool applyBeatEffect = beatDetected && (soundSettings.strobeOnBeat || soundSettings.pulseOnBeat);
        
        for (int i = 0; i < LEDS_PER_STRIP; i++) {
            CRGB color = CRGB::Black;
            
            // Calculate if this LED is part of the sound visualization
            bool isInSoundVisualization = false;
            float soundIntensity = 0.0; // 0.0 = background, 1.0 = full sound intensity
            
            if (soundSettings.barMode) {
                if (soundSettings.fillFromCenter) {
                    // Fill from center outward
                    int center = LEDS_PER_STRIP / 2;
                    int halfHeight = barHeight / 2;
                    isInSoundVisualization = (i >= (center - halfHeight) && i <= (center + halfHeight));
                    if (isInSoundVisualization) {
                        // Calculate intensity based on distance from center
                        int distanceFromCenter = abs(i - center);
                        soundIntensity = 1.0 - ((float)distanceFromCenter / (halfHeight + 1));
                    }
                } else {
                    // Bar mode - fill from bottom
                    isInSoundVisualization = (i < barHeight);
                    if (isInSoundVisualization) {
                        // Calculate intensity - higher LEDs get more intensity
                        soundIntensity = (float)(barHeight - i) / barHeight;
                    }
                }
            } else {
                // Dot mode - only peak
                if (soundSettings.fillFromCenter) {
                    int center = LEDS_PER_STRIP / 2;
                    isInSoundVisualization = (i == center) && (barHeight > 0);
                    soundIntensity = isInSoundVisualization ? 1.0 : 0.0;
                } else {
                    isInSoundVisualization = (i == barHeight - 1) && (barHeight > 0);
                    soundIntensity = isInSoundVisualization ? 1.0 : 0.0;
                }
            }
            
            // Calculate palette index for gradient
            uint8_t paletteIndex;
            if (soundSettings.intensityMapping && isInSoundVisualization) {
                // Map volume to color intensity instead of height
                paletteIndex = map(rawBandValues[strip] / soundSettings.amplitude, 0, LEDS_PER_STRIP, 0, 255);
            } else {
                // Normal palette mapping by position
                paletteIndex = map(i, 0, LEDS_PER_STRIP - 1, 0, 255);
                paletteIndex += (millis() * soundSettings.colorSpeed / 500) % 256; // Color cycling
            }
            
            if (soundSettings.rainbowMode) {
                // Rainbow mode overrides palettes
                uint8_t hue = map(strip, 0, NUM_STRIPS - 1, 0, 255);
                hue += (millis() * soundSettings.colorSpeed / 100) % 256; // Color cycling
                if (isInSoundVisualization) {
                    color = CHSV(hue, 255, 255 * soundIntensity); // Intensity based on sound level
                } else {
                    color = CHSV(hue, 255, 64);  // Dimmed for background
                }
            } else {
                // SOUND VISUALIZATION ALWAYS USES PRIMARY PALETTE
                if (isInSoundVisualization) {
                    // Get primary palette color and adjust brightness based on sound intensity
                    color = ColorFromPalette(primaryPalette, paletteIndex);
                    if (soundIntensity < 1.0) {
                        // Fade the primary palette color based on sound intensity
                        color.fadeToBlackBy(255 * (1.0 - soundIntensity));
                    }
                } else {
                    // Background uses secondary palette or black
                    if (secondaryPaletteOff) {
                        color = CRGB::Black;
                    } else {
                        color = ColorFromPalette(secondaryPalette, paletteIndex);
                        color.fadeToBlackBy(192); // Dim background to 25% brightness
                    }
                }
            }
            
            // Beat effects
            if (applyBeatEffect) {
                if (soundSettings.strobeOnBeat) {
                    color = CRGB::White; // Flash white on beat
                } else if (soundSettings.pulseOnBeat && isInSoundVisualization) {
                    color.fadeToBlackBy(128); // Dim on beat for pulse effect
                }
            }
            
            leds[strip][i] = color;
        }
    }

    // Apply individual directions if enabled
    if (soundSettings.individualDirections) {
        // For now, alternate direction every other strip
        for (int i = 1; i < NUM_STRIPS; i += 2) {
            std::reverse(leds[i], leds[i] + LEDS_PER_STRIP);
        }
    } else if (reverseDirection) {
        // Apply global direction
        for (int i = 0; i < NUM_STRIPS; i++) {
            std::reverse(leds[i], leds[i] + LEDS_PER_STRIP);
        }
    }

    // Let BMDevice handle the rendering
    lightShow.render();
}

// === CHUNKED STATUS UPDATES ===
enum StatusUpdateState {
    STATUS_IDLE,
    STATUS_START_BASIC,
    STATUS_BASIC_SENT,
    STATUS_SOUND_SENT,
    STATUS_ADVANCED_SENT,
    STATUS_SUPER_ADVANCED_SENT
};
StatusUpdateState statusUpdateState = STATUS_IDLE;
unsigned long statusUpdateTimer = 0;
const unsigned long STATUS_UPDATE_DELAY = 25; // 25ms delay between chunks

// Function to send basic device status (core BMDevice info)
void sendBasicStatus() {
    BMDeviceState& state = device.getState();
    BMDeviceDefaults& defaults = device.getDefaults();
    DeviceDefaults currentDefaults = defaults.getCurrentDefaults();
    
    // Create basic status JSON (under 512 bytes)
    StaticJsonDocument<512> doc;
    
    // Essential device state
    doc["pwr"] = state.power;
    doc["bri"] = state.brightness;
    doc["spd"] = state.speed;
    doc["dir"] = state.reverseStrip;
    
    const char* effectName = LightShow::effectIdToName(state.currentEffect);
    doc["fx"] = effectName;
    doc["pal"] = LightShow::paletteIdToName(state.currentPalette);
    
    // Secondary palette
    if (secondaryPaletteOff) {
        doc["secPal"] = "off";
    } else {
        const char* secPalName = LightShow::paletteIdToName(currentSecondaryPaletteId);
        doc["secPal"] = secPalName;
    }
    
    // Standard BMDevice fields
    doc["maxBri"] = currentDefaults.maxBrightness;
    doc["owner"] = currentDefaults.owner;
    doc["deviceName"] = currentDefaults.deviceName;
    
    // Essential sound mode indicator
    doc["sound"] = soundSettings.soundSensitive;
    
    String basicStatus;
    serializeJson(doc, basicStatus);
    Serial.printf("🌂 [BASIC STATUS] %s\n", basicStatus.c_str());
    
    BMBluetoothHandler& bluetoothHandler = device.getBluetoothHandler();
    bluetoothHandler.sendStatusUpdate(basicStatus);
}

// Function to send sound settings as separate update
void sendSoundSettings() {
    // Create sound settings JSON (under 512 bytes)
    StaticJsonDocument<512> doc;
    
    // Mark this as extended sound data
    doc["type"] = "soundSettings";
    
    // Basic sound controls
    doc["amplitude"] = soundSettings.amplitude;
    doc["noiseThreshold"] = soundSettings.noiseThreshold;
    doc["sensitivity"] = (int)pow(10, soundSettings.reference);
    doc["decay"] = soundSettings.decay;
    doc["decayRate"] = soundSettings.decayRate;
    
    // Visual effects
    doc["peakHold"] = soundSettings.peakHold;
    doc["peakHoldTime"] = soundSettings.peakHoldTime;
    doc["smoothing"] = soundSettings.smoothing;
    doc["smoothingFactor"] = soundSettings.smoothingFactor;
    doc["barMode"] = soundSettings.barMode;
    doc["rainbowMode"] = soundSettings.rainbowMode;
    doc["colorSpeed"] = soundSettings.colorSpeed;
    doc["intensityMapping"] = soundSettings.intensityMapping;
    
    String soundStatus;
    serializeJson(doc, soundStatus);
    Serial.printf("🔊 [SOUND SETTINGS] %s\n", soundStatus.c_str());
    
    BMBluetoothHandler& bluetoothHandler = device.getBluetoothHandler();
    bluetoothHandler.sendStatusUpdate(soundStatus);
}

// Function to send advanced sound settings
void sendAdvancedSoundSettings() {
    // Create advanced sound settings JSON (under 512 bytes)
    StaticJsonDocument<512> doc;
    
    // Mark this as extended advanced sound data
    doc["type"] = "advancedSoundSettings";
    
    // Beat detection
    doc["beatDetection"] = soundSettings.beatDetection;
    doc["beatSensitivity"] = soundSettings.beatSensitivity;
    doc["strobeOnBeat"] = soundSettings.strobeOnBeat;
    doc["pulseOnBeat"] = soundSettings.pulseOnBeat;
    
    // Frequency controls
    doc["bassEmphasis"] = soundSettings.bassEmphasis;
    doc["midEmphasis"] = soundSettings.midEmphasis;
    doc["trebleEmphasis"] = soundSettings.trebleEmphasis;
    doc["logarithmicMapping"] = soundSettings.logarithmicMapping;
    
    // Auto adjustments
    doc["autoGain"] = soundSettings.autoGain;
    doc["ambientCompensation"] = soundSettings.ambientCompensation;
    
    // Strip configuration
    doc["stripMapping"] = soundSettings.stripMapping;
    doc["individualDirections"] = soundSettings.individualDirections;
    
    String advancedStatus;
    serializeJson(doc, advancedStatus);
    Serial.printf("🎛️ [ADVANCED SOUND] %s\n", advancedStatus.c_str());
    
    BMBluetoothHandler& bluetoothHandler = device.getBluetoothHandler();
    bluetoothHandler.sendStatusUpdate(advancedStatus);
}

// Function to send super advanced sound settings (new features)
void sendSuperAdvancedSoundSettings() {
    // Create super advanced sound settings JSON (under 512 bytes)
    StaticJsonDocument<512> doc;
    
    // Mark this as extended super advanced sound data
    doc["type"] = "superAdvancedSoundSettings";
    
    // Advanced audio settings
    doc["samplingFrequency"] = soundSettings.samplingFrequency;
    doc["sampleCount"] = soundSettings.sampleCount;
    doc["gainMultiplier"] = soundSettings.gainMultiplier;
    doc["minLEDs"] = soundSettings.minLEDs;
    doc["maxLEDs"] = soundSettings.maxLEDs;
    
    // Frequency filtering
    doc["frequencyMin"] = soundSettings.frequencyMin;
    doc["frequencyMax"] = soundSettings.frequencyMax;
    
    // Visual enhancements
    doc["doubleHeight"] = soundSettings.doubleHeight;
    doc["ledMultiplier"] = soundSettings.ledMultiplier;
    doc["fillFromCenter"] = soundSettings.fillFromCenter;
    
    String superAdvancedStatus;
    serializeJson(doc, superAdvancedStatus);
    Serial.printf("🚀 [SUPER ADVANCED SOUND] %s\n", superAdvancedStatus.c_str());
    
    BMBluetoothHandler& bluetoothHandler = device.getBluetoothHandler();
    bluetoothHandler.sendStatusUpdate(superAdvancedStatus);
}

void sendUmbrellaStatus() {
    statusUpdateState = STATUS_START_BASIC;
    statusUpdateTimer = millis();
    Serial.println("🌂 [CHUNKED STATUS] Starting non-blocking chunked status update...");
}

void handleStatusUpdate() {
    if (statusUpdateState == STATUS_IDLE) {
        return;
    }
    
    unsigned long currentTime = millis();
    
    switch (statusUpdateState) {
        case STATUS_START_BASIC:
            sendBasicStatus();
            statusUpdateState = STATUS_BASIC_SENT;
            statusUpdateTimer = currentTime;
            break;
            
        case STATUS_BASIC_SENT:
            if (currentTime - statusUpdateTimer >= STATUS_UPDATE_DELAY) {
                sendSoundSettings();
                statusUpdateState = STATUS_SOUND_SENT;
                statusUpdateTimer = currentTime;
            }
            break;
            
        case STATUS_SOUND_SENT:
            if (currentTime - statusUpdateTimer >= STATUS_UPDATE_DELAY) {
                sendAdvancedSoundSettings();
                statusUpdateState = STATUS_ADVANCED_SENT;
                statusUpdateTimer = currentTime;
            }
            break;
            
        case STATUS_ADVANCED_SENT:
            if (currentTime - statusUpdateTimer >= STATUS_UPDATE_DELAY) {
                sendSuperAdvancedSoundSettings();
                statusUpdateState = STATUS_SUPER_ADVANCED_SENT;
                statusUpdateTimer = currentTime;
            }
            break;
            
        case STATUS_SUPER_ADVANCED_SENT:
            statusUpdateState = STATUS_IDLE;
            Serial.println("🌂 [CHUNKED STATUS] Completed non-blocking chunked status update!");
            break;
            
        default:
            break;
    }
}

// === SETUP ===
void setup() {
    Serial.begin(115200);
    Serial.println("🌂 BTUmbrellaV3 Starting...");
    
    // Add LED strips to BMDevice (same pin order as original)
    device.addLEDStrip<WS2812B, 32, COLOR_ORDER>(leds0, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 33, COLOR_ORDER>(leds1, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 27, COLOR_ORDER>(leds2, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 14, COLOR_ORDER>(leds3, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 12, COLOR_ORDER>(leds4, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 13, COLOR_ORDER>(leds5, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 18, COLOR_ORDER>(leds6, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 5, COLOR_ORDER>(leds7, LEDS_PER_STRIP);
    
    // Set handlers
    device.setCustomFeatureHandler(handleSoundFeatures);
    device.setCustomConnectionHandler(handleConnectionChange);
    
    // Initialize hardware
    initializeI2S();
    initializeFFT(DEFAULT_SAMPLES, DEFAULT_SAMPLING_FREQ);
    
    // Start BMDevice
    if (!device.begin()) {
        Serial.println("❌ Failed to initialize BMDevice!");
        while (1);
    }
    
    // Initialize palettes - make sure LightShow palettes are synchronized with BMDevice
    updatePalettesFromBMDevice();
    
    Serial.println("✅ BTUmbrellaV3 Ready!");
    Serial.printf("🎵 Sound Mode: %s\n", soundSettings.soundSensitive ? "ON" : "OFF");
    Serial.printf("🎨 Primary Palette: %s\n", LightShow::paletteIdToName(device.getState().currentPalette));
    Serial.printf("🎨 Secondary Palette: %s (Off: %s)\n", 
                 LightShow::paletteIdToName(currentSecondaryPaletteId),
                 secondaryPaletteOff ? "true" : "false");
}

// === MAIN LOOP ===
void loop() {
    // Poll bluetooth
    BMBluetoothHandler& bluetoothHandler = device.getBluetoothHandler();
    bluetoothHandler.poll();
    
    // Handle chunked status updates
    handleStatusUpdate();

    // Get current state
    BMDeviceState& state = device.getState();

    // Send periodic status updates
    static unsigned long lastStatusUpdate = 0;
    unsigned long statusUpdateInterval = 5000; 
    
    if ((millis() - lastStatusUpdate) > statusUpdateInterval && bluetoothHandler.isConnected()) {
        sendUmbrellaStatus(); 
        lastStatusUpdate = millis();
    }
    
    // Debug power state changes
    static bool lastPowerState = true;
    if (state.power != lastPowerState) {
        Serial.printf("🔋 [POWER DEBUG] Power changed: %s -> %s\n", 
                     lastPowerState ? "ON" : "OFF", 
                     state.power ? "ON" : "OFF");
        lastPowerState = state.power;
    }
    
    // Check for palette updates
    static AvailablePalettes lastPrimaryPalette = state.currentPalette;
    if (state.currentPalette != lastPrimaryPalette) {
        Serial.printf("🎨 [PALETTE CHANGE] Primary: %s->%s\n",
                     LightShow::paletteIdToName(lastPrimaryPalette),
                     LightShow::paletteIdToName(state.currentPalette));
        
        updatePalettesFromBMDevice();
        lastPrimaryPalette = state.currentPalette;
    }
    
    // Handle power off
    if (!state.power) {
        FastLED.clear();
        FastLED.show();
        return;
    }
    
    // Debug status
    static unsigned long lastDebugPrint = 0;
    if (millis() - lastDebugPrint > 5000) {  // Every 5 seconds
        Serial.printf("🎵 [STATUS] Power: %s, SoundMode: %s\n", 
                     state.power ? "ON" : "OFF",
                     soundSettings.soundSensitive ? "ON" : "OFF");
        lastDebugPrint = millis();
    }
    
    // Main visualization logic
    if (soundSettings.soundSensitive) {
        handleSoundVisualization();
    } else {
        // Use BMDevice light shows when not in sound mode
        LightShow& lightShow = device.getLightShow();
        lightShow.render();
    }
}

// Available BLE Commands (inherited from BMDevice + custom):
// Standard BMDevice commands:
// 0x01 - Set primary palette
// 0x04 - Set brightness  
// 0x05 - Set speed
// 0x06 - Set power on/off
// 0x08 - Set direction
// 0x0A - Set lighting effect
//
// Custom Umbrella sound commands:
// Basic Sound Controls:
// 0x03 - Set sensitivity (int)
// 0x07 - Set sound sensitivity on/off (bool)
// 0x09 - Set secondary palette (string: palette name or "off")
// 0x0B - Set amplitude (int)
// 0x0C - Set noise threshold (int)
// 0x0D - Set decay on/off (bool)
// 0x0E - Set decay rate (int)
//
// Visual Effects:
// 0x0F - Peak hold on/off (bool)
// 0x10 - Peak hold time in ms (int)
// 0x11 - Smoothing on/off (bool)
// 0x12 - Smoothing factor 0.0-1.0 (float)
// 0x13 - Bar mode (true) vs Dot mode (false) (bool)
// 0x14 - Rainbow mode on/off (bool)
// 0x15 - Color speed 0-100 (int)
// 0x16 - Intensity mapping on/off (bool)
//
// Beat Detection & Effects:
// 0x17 - Beat detection on/off (bool)
// 0x18 - Beat sensitivity 0-100 (int)
// 0x19 - Strobe on beat on/off (bool)
// 0x21 - Pulse on beat on/off (bool)
//
// Frequency Controls:
// 0x22 - Bass emphasis 0-100 (int)
// 0x23 - Mid emphasis 0-100 (int)
// 0x24 - Treble emphasis 0-100 (int)
// 0x25 - Logarithmic mapping on/off (bool)
//
// Auto Adjustments:
// 0x26 - Auto gain on/off (bool)
// 0x27 - Ambient compensation on/off (bool)
//
// Strip Configuration:
// 0x28 - Strip mapping: 0=normal, 1=bass-treble, 2=treble-bass, 3=center-out (int)
// 0x29 - Individual directions on/off (bool)
//
// Advanced Audio Controls:
// 0x2A - Sampling frequency 8000-96000 Hz (int)
// 0x2B - Sample count 64-2048 (power of 2) (int)
// 0x2C - Gain multiplier 0.1-10.0 (float)
// 0x2D - Min LEDs 0-LEDS_PER_STRIP (int)
// 0x2E - Max LEDs 1-LEDS_PER_STRIP (int)
// 0x2F - Frequency min 20-10000 Hz (int)
// 0x30 - Frequency max 100-20000 Hz (int)
// 0x31 - Double height on/off (bool)
// 0x32 - LED multiplier 0.1-5.0 (float)
// 0x33 - Fill from center on/off (bool)
//
// BTUmbrellaV3 now includes ALL advanced sound analysis features:
// - Full BMDevice integration with shared palettes and light shows
// - Advanced FFT sound analysis for 8 LED strips with configurable sampling
// - Beat detection with strobe and pulse effects
// - Peak hold with configurable decay times
// - Smoothing to reduce flicker
// - Multiple visual modes (bars vs dots, center-fill)
// - Rainbow mode with cycling colors
// - Frequency emphasis controls (bass/mid/treble)
// - Auto-gain and ambient noise compensation
// - Multiple strip mapping patterns
// - Color intensity mapping
// - Individual strip directions
// - Secondary palette "off" mode for pure visualization
// - Enhanced sensitivity controls (gain multiplier, LED multiplier)
// - Configurable LED range (min/max LEDs)
// - Frequency filtering (customizable Hz ranges)
// - Double height mode for dramatic effect
// - All standard BLE commands from BMDevice
// - 31 custom sound-specific BLE commands (0x03-0x33)
// - Persistent defaults that survive reboots
// - Automatic 4-part chunked status reporting
// - Power management
// - Complete parameter control via Bluetooth! 