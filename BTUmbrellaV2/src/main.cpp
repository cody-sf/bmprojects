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

// Enhanced Sound Settings with all the cool new features
struct SoundSettings {
    // Basic settings (existing)
    bool soundSensitive = true;
    int amplitude = 1000;
    int noiseThreshold = 500;
    float reference = log10(50.0);
    bool decay = false;
    int decayRate = 100;
    
    // Peak Hold settings
    bool peakHold = false;
    int peakHoldTime = 500;  // milliseconds
    
    // Smoothing
    bool smoothing = true;
    float smoothingFactor = 0.3;  // 0.0 = no smoothing, 1.0 = maximum smoothing
    
    // Visual modes
    bool barMode = true;  // true = bars, false = dots
    bool rainbowMode = false;  // override palettes with rainbow
    
    // Color dynamics
    int colorSpeed = 50;  // 0-100, how fast colors cycle
    bool intensityMapping = false;  // map volume to color intensity instead of height
    
    // Beat detection
    bool beatDetection = false;
    int beatSensitivity = 70;  // 0-100
    bool strobeOnBeat = false;
    bool pulseOnBeat = false;
    
    // Frequency control
    int bassEmphasis = 50;    // 0-100
    int midEmphasis = 50;     // 0-100  
    int trebleEmphasis = 50;  // 0-100
    bool logarithmicMapping = true;  // true = log, false = linear
    
    // Auto adjustment
    bool autoGain = false;
    bool ambientCompensation = false;
    
    // Strip configuration
    bool individualDirections = false;  // each strip can have own direction
    int stripMapping = 0;  // 0=normal, 1=bass-to-treble, 2=treble-to-bass, 3=center-out
};

SoundSettings soundSettings;

// Palette variables (like original)
CRGBPalette16 primaryPalette;
CRGBPalette16 secondaryPalette;

// Track current secondary palette name for status updates
AvailablePalettes currentSecondaryPaletteId = AvailablePalettes::cool;

// Create BMDevice - Umbrella with no GPS support
BMDevice device("Umbrella-CL", SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

// Function declarations
void sendUmbrellaStatusUpdate();

// Peak hold tracking
float peakValues[NUM_STRIPS] = {0};
unsigned long peakTimes[NUM_STRIPS] = {0};

// Smoothing tracking
float smoothedValues[NUM_STRIPS] = {0};

// Beat detection
float previousAverage = 0;
unsigned long lastBeatTime = 0;
bool beatDetected = false;

// Auto-gain tracking
float recentMax = 0;
unsigned long lastGainAdjust = 0;

// Custom feature handler for sound-specific commands
bool handleSoundFeatures(uint8_t feature, const uint8_t* data, size_t length) {
    Serial.printf("🔧 [DEBUG] Received custom feature: 0x%02X, length: %d\n", feature, length);
    
    // Let BMDevice handle its standard features - CORRECTED FEATURE CODES
    if (feature == 0x01 || feature == 0x04 ||  // power, brightness
        feature == 0x05 || feature == 0x06 ||  // speed, direction
        feature == 0x08 || feature == 0x0A) {  // palette, effect
        Serial.printf("⚡ [PASSTHROUGH] Letting BMDevice handle feature: 0x%02X\n", feature);
        return false; // Let BMDevice handle these
    }
    
    // Handle secondary palette separately
    if (feature == 0x09) {  // Secondary palette
        Serial.printf("🎨 [SECONDARY PALETTE] Received secondary palette command! Length: %d\n", length);
        // Print raw data for debugging
        Serial.print("🎨 [SECONDARY PALETTE] Raw data: ");
        for (size_t i = 0; i < length; i++) {
            Serial.printf("0x%02X ", data[i]);
        }
        Serial.println();
        
        // Extract palette name from data
        if (length > 1) {
            std::string paletteName(reinterpret_cast<const char*>(data + 1), length - 1);
            Serial.printf("🎨 [SECONDARY PALETTE] Setting secondary palette to: %s\n", paletteName.c_str());
            
            // Convert string to AvailablePalettes enum
            AvailablePalettes paletteEnum = LightShow::paletteNameToId(paletteName.c_str());
            
            // Set the secondary palette in BMDevice
            LightShow& lightShow = device.getLightShow();
            lightShow.setSecondaryPalette(paletteEnum);
            
            // Store the current secondary palette ID for status updates
            currentSecondaryPaletteId = paletteEnum;
            
            // Update our local secondary palette
            std::pair<CRGBPalette16, CRGBPalette16> palettes = lightShow.getPrimarySecondaryPalettes();
            primaryPalette = palettes.first;
            secondaryPalette = palettes.second;
            
            Serial.printf("✅ Secondary palette set successfully\n");
            
            // Send immediate status update to show the change
            sendUmbrellaStatusUpdate();
            
            return true;
        }
        Serial.println("❌ Secondary palette: Invalid length");
        return false;
    }
    
    switch (feature) {
        case 0x03: // Sensitivity
            if (length >= sizeof(int)) {
                int sensitivity = 0;
                memcpy(&sensitivity, data + 1, sizeof(int)); // Fix: skip feature byte
                soundSettings.reference = log10(sensitivity);
                Serial.print("✅ Sound Sensitivity: ");
                Serial.println(sensitivity);
                return true;
            }
            Serial.println("❌ Sensitivity: Invalid length");
            break;
            
        case 0x07: // Sound Sensitivity on/off
            if (length >= 2) { // Fix: need at least 2 bytes (feature + data)
                soundSettings.soundSensitive = data[1] != 0; // Fix: read data[1], not data[0]
                Serial.print("✅ Sound Sensitive: ");
                Serial.println(soundSettings.soundSensitive ? "On" : "Off");
                return true;
            }
            Serial.println("❌ Sound Sensitive: Invalid length");
            break;
            
        case 0x0A: // Save Settings - Add back
            Serial.println("✅ Save Settings command received");
            // TODO: Add save settings functionality if needed
            return true;
            
        case 0x0B: // Amplitude
            if (length >= sizeof(int) + 1) { // Fix: account for feature byte
                memcpy(&soundSettings.amplitude, data + 1, sizeof(int)); // Fix: skip feature byte
                Serial.print("✅ Amplitude: ");
                Serial.println(soundSettings.amplitude);
                return true;
            }
            Serial.println("❌ Amplitude: Invalid length");
            break;
            
        case 0x0C: // Noise Threshold
            if (length >= sizeof(int) + 1) { // Fix: account for feature byte
                memcpy(&soundSettings.noiseThreshold, data + 1, sizeof(int)); // Fix: skip feature byte
                Serial.print("✅ Noise Threshold: ");
                Serial.println(soundSettings.noiseThreshold);
                return true;
            }
            Serial.println("❌ Noise Threshold: Invalid length");
            break;
            
        case 0x0D: // Decay
            if (length >= 2) { // Fix: need at least 2 bytes
                soundSettings.decay = data[1] != 0; // Fix: read data[1]
                Serial.print("✅ Decay: ");
                Serial.println(soundSettings.decay ? "On" : "Off");
                return true;
            }
            Serial.println("❌ Decay: Invalid length");
            break;
            
        case 0x0E: // Decay Rate
            if (length >= sizeof(int) + 1) { // Fix: account for feature byte
                memcpy(&soundSettings.decayRate, data + 1, sizeof(int)); // Fix: skip feature byte
                Serial.print("✅ Decay Rate: ");
                Serial.println(soundSettings.decayRate);
                return true;
            }
            Serial.println("❌ Decay Rate: Invalid length");
            break;
            
        case 0x0F: // Peak Hold On/Off
            if (length >= 2) { // Fix: need at least 2 bytes
                soundSettings.peakHold = data[1] != 0; // Fix: read data[1]
                Serial.print("✅ Peak Hold: ");
                Serial.println(soundSettings.peakHold ? "On" : "Off");
                return true;
            }
            Serial.println("❌ Peak Hold: Invalid length");
            break;
            
        case 0x10: // Peak Hold Time
            if (length >= sizeof(int) + 1) { // Fix: account for feature byte
                memcpy(&soundSettings.peakHoldTime, data + 1, sizeof(int)); // Fix: skip feature byte
                Serial.print("✅ Peak Hold Time: ");
                Serial.println(soundSettings.peakHoldTime);
                return true;
            }
            Serial.println("❌ Peak Hold Time: Invalid length");
            break;
            
        case 0x11: // Smoothing On/Off
            if (length >= 2) { // Fix: need at least 2 bytes
                soundSettings.smoothing = data[1] != 0; // Fix: read data[1]
                Serial.print("✅ Smoothing: ");
                Serial.println(soundSettings.smoothing ? "On" : "Off");
                return true;
            }
            Serial.println("❌ Smoothing: Invalid length");
            break;
            
        case 0x12: // Smoothing Factor
            if (length >= sizeof(float) + 1) { // Fix: account for feature byte
                memcpy(&soundSettings.smoothingFactor, data + 1, sizeof(float)); // Fix: skip feature byte
                Serial.print("✅ Smoothing Factor: ");
                Serial.println(soundSettings.smoothingFactor);
                return true;
            }
            Serial.println("❌ Smoothing Factor: Invalid length");
            break;
            
        case 0x13: // Bar Mode
            if (length >= 2) { // Fix: need at least 2 bytes
                soundSettings.barMode = data[1] != 0; // Fix: read data[1]
                Serial.print("✅ Visual Mode: ");
                Serial.println(soundSettings.barMode ? "Bars" : "Dots");
                return true;
            }
            Serial.println("❌ Bar Mode: Invalid length");
            break;
            
        case 0x14: // Rainbow Mode
            if (length >= 2) { // Fix: need at least 2 bytes
                soundSettings.rainbowMode = data[1] != 0; // Fix: read data[1]
                Serial.print("✅ Rainbow Mode: ");
                Serial.println(soundSettings.rainbowMode ? "On" : "Off");
                return true;
            }
            Serial.println("❌ Rainbow Mode: Invalid length");
            break;
            
        case 0x15: // Color Speed
            if (length >= sizeof(int) + 1) { // Fix: account for feature byte
                memcpy(&soundSettings.colorSpeed, data + 1, sizeof(int)); // Fix: skip feature byte
                Serial.print("✅ Color Speed: ");
                Serial.println(soundSettings.colorSpeed);
                return true;
            }
            Serial.println("❌ Color Speed: Invalid length");
            break;
            
        case 0x16: // Intensity Mapping
            if (length >= 2) { // Fix: need at least 2 bytes
                soundSettings.intensityMapping = data[1] != 0; // Fix: read data[1]
                Serial.print("✅ Intensity Mapping: ");
                Serial.println(soundSettings.intensityMapping ? "On" : "Off");
                return true;
            }
            Serial.println("❌ Intensity Mapping: Invalid length");
            break;
            
        case 0x17: // Beat Detection
            if (length >= 2) { // Fix: need at least 2 bytes
                soundSettings.beatDetection = data[1] != 0; // Fix: read data[1]
                Serial.print("✅ Beat Detection: ");
                Serial.println(soundSettings.beatDetection ? "On" : "Off");
                return true;
            }
            Serial.println("❌ Beat Detection: Invalid length");
            break;
            
        case 0x18: // Beat Sensitivity
            if (length >= sizeof(int) + 1) { // Fix: account for feature byte
                memcpy(&soundSettings.beatSensitivity, data + 1, sizeof(int)); // Fix: skip feature byte
                Serial.print("✅ Beat Sensitivity: ");
                Serial.println(soundSettings.beatSensitivity);
                return true;
            }
            Serial.println("❌ Beat Sensitivity: Invalid length");
            break;
            
        case 0x19: // Strobe on Beat
            if (length >= 2) { // Fix: need at least 2 bytes
                soundSettings.strobeOnBeat = data[1] != 0; // Fix: read data[1]
                Serial.print("✅ Strobe on Beat: ");
                Serial.println(soundSettings.strobeOnBeat ? "On" : "Off");
                return true;
            }
            Serial.println("❌ Strobe on Beat: Invalid length");
            break;
            
        case 0x21: // Pulse on Beat
            if (length >= 2) { // Fix: need at least 2 bytes
                soundSettings.pulseOnBeat = data[1] != 0; // Fix: read data[1]
                Serial.print("✅ Pulse on Beat: ");
                Serial.println(soundSettings.pulseOnBeat ? "On" : "Off");
                return true;
            }
            Serial.println("❌ Pulse on Beat: Invalid length");
            break;
            
        case 0x22: // Bass Emphasis
            if (length >= sizeof(int) + 1) { // Fix: account for feature byte
                memcpy(&soundSettings.bassEmphasis, data + 1, sizeof(int)); // Fix: skip feature byte
                Serial.print("✅ Bass Emphasis: ");
                Serial.println(soundSettings.bassEmphasis);
                return true;
            }
            Serial.println("❌ Bass Emphasis: Invalid length");
            break;
            
        case 0x23: // Mid Emphasis
            if (length >= sizeof(int) + 1) { // Fix: account for feature byte
                memcpy(&soundSettings.midEmphasis, data + 1, sizeof(int)); // Fix: skip feature byte
                Serial.print("✅ Mid Emphasis: ");
                Serial.println(soundSettings.midEmphasis);
                return true;
            }
            Serial.println("❌ Mid Emphasis: Invalid length");
            break;
            
        case 0x24: // Treble Emphasis
            if (length >= sizeof(int) + 1) { // Fix: account for feature byte
                memcpy(&soundSettings.trebleEmphasis, data + 1, sizeof(int)); // Fix: skip feature byte
                Serial.print("✅ Treble Emphasis: ");
                Serial.println(soundSettings.trebleEmphasis);
                return true;
            }
            Serial.println("❌ Treble Emphasis: Invalid length");
            break;
            
        case 0x25: // Logarithmic Mapping
            if (length >= 2) { // Fix: need at least 2 bytes
                soundSettings.logarithmicMapping = data[1] != 0; // Fix: read data[1]
                Serial.print("✅ Logarithmic Mapping: ");
                Serial.println(soundSettings.logarithmicMapping ? "On" : "Off");
                return true;
            }
            Serial.println("❌ Logarithmic Mapping: Invalid length");
            break;
            
        case 0x26: // Auto Gain
            if (length >= 2) { // Fix: need at least 2 bytes
                soundSettings.autoGain = data[1] != 0; // Fix: read data[1]
                Serial.print("✅ Auto Gain: ");
                Serial.println(soundSettings.autoGain ? "On" : "Off");
                return true;
            }
            Serial.println("❌ Auto Gain: Invalid length");
            break;
            
        case 0x27: // Ambient Compensation
            if (length >= 2) { // Fix: need at least 2 bytes
                soundSettings.ambientCompensation = data[1] != 0; // Fix: read data[1]
                Serial.print("✅ Ambient Compensation: ");
                Serial.println(soundSettings.ambientCompensation ? "On" : "Off");
                return true;
            }
            Serial.println("❌ Ambient Compensation: Invalid length");
            break;
            
        case 0x28: // Strip Mapping
            if (length >= sizeof(int) + 1) { // Fix: account for feature byte
                memcpy(&soundSettings.stripMapping, data + 1, sizeof(int)); // Fix: skip feature byte
                Serial.print("✅ Strip Mapping: ");
                Serial.println(soundSettings.stripMapping);
                return true;
            }
            Serial.println("❌ Strip Mapping: Invalid length");
            break;
            
        case 0x29: // Individual Directions
            if (length >= 2) { // Fix: need at least 2 bytes
                soundSettings.individualDirections = data[1] != 0; // Fix: read data[1]
                Serial.print("✅ Individual Directions: ");
                Serial.println(soundSettings.individualDirections ? "On" : "Off");
                return true;
            }
            Serial.println("❌ Individual Directions: Invalid length");
            break;
            
        default:
            Serial.printf("❓ Unknown custom feature: 0x%02X\n", feature);
            return false; // Not handled
    }
    return false; // Not handled
}

// Connection handler to update palettes when device connects/disconnects
void handleConnectionChange(bool connected) {
    Serial.printf("📡 [CONNECTION] Device %s\n", connected ? "CONNECTED" : "DISCONNECTED");
    
    if (connected) {
        // Update palettes when connected
        LightShow& lightShow = device.getLightShow();
        std::pair<CRGBPalette16, CRGBPalette16> palettes = lightShow.getPrimarySecondaryPalettes();
        primaryPalette = palettes.first;
        secondaryPalette = palettes.second;
        
        // Initialize secondary palette tracking
        BMDeviceState& state = device.getState();
        currentSecondaryPaletteId = AvailablePalettes::cool; // Default to 'cool' palette
        
        Serial.println("🎨 Palettes updated from BMDevice");
        Serial.printf("🎨 Primary palette initialized, Secondary palette set to: %s\n", 
                     LightShow::paletteIdToName(currentSecondaryPaletteId));
        
        // Send custom umbrella status update with all sound settings
        sendUmbrellaStatusUpdate();
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

    // Update palettes from BMDevice
    LightShow& lightShow = device.getLightShow();
    std::pair<CRGBPalette16, CRGBPalette16> palettes = lightShow.getPrimarySecondaryPalettes();
    primaryPalette = palettes.first;
    secondaryPalette = palettes.second;
    
    // Get current state for direction setting
    BMDeviceState& state = device.getState();
    bool reverseDirection = state.reverseStrip;

    // Calculate overall volume for beat detection and auto-gain
    float totalVolume = 0;
    for (int i = 2; i < (SAMPLES / 2); i++) {
        totalVolume += vReal[i];
    }
    float averageVolume = totalVolume / (SAMPLES / 2 - 2);
    
    // Beat detection
    beatDetected = false;
    if (soundSettings.beatDetection) {
        float beatThreshold = previousAverage * (1.0 + soundSettings.beatSensitivity / 100.0);
        unsigned long currentTime = millis();
        
        if (averageVolume > beatThreshold && (currentTime - lastBeatTime) > 100) { // 100ms minimum between beats
            beatDetected = true;
            lastBeatTime = currentTime;
            Serial.println("Beat detected!");
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
                Serial.print("Auto-gain adjusted amplitude to: ");
                Serial.println(soundSettings.amplitude);
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
    for (int i = 2; i < (SAMPLES / 2); i++) {
        if (vReal[i] > soundSettings.noiseThreshold) {
            int bandIndex;
            
            if (soundSettings.logarithmicMapping) {
                // Logarithmic mapping for more musical response
                float logPos = log(i - 1) / log(SAMPLES / 2 - 2);
                bandIndex = (int)(logPos * NUM_STRIPS);
            } else {
                // Linear mapping
                bandIndex = map(i, 2, SAMPLES / 2, 0, NUM_STRIPS - 1);
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
        int barHeight = rawBandValues[strip] / soundSettings.amplitude;
        if (barHeight > LEDS_PER_STRIP) barHeight = LEDS_PER_STRIP;
        
        // Beat effects
        bool applyBeatEffect = beatDetected && (soundSettings.strobeOnBeat || soundSettings.pulseOnBeat);
        
        for (int i = 0; i < LEDS_PER_STRIP; i++) {
            CRGB color = CRGB::Black;
            
            // Determine if this LED should be lit
            bool shouldLight = false;
            if (soundSettings.barMode) {
                // Bar mode - fill from bottom
                shouldLight = (i < barHeight);
            } else {
                // Dot mode - only peak
                shouldLight = (i == barHeight - 1) && (barHeight > 0);
            }
            
            if (shouldLight || applyBeatEffect) {
                if (soundSettings.rainbowMode) {
                    // Rainbow mode overrides palettes
                    uint8_t hue = map(strip, 0, NUM_STRIPS - 1, 0, 255);
                    hue += (millis() * soundSettings.colorSpeed / 100) % 256; // Color cycling
                    color = CHSV(hue, 255, 255);
                } else {
                    // Use palettes
                    uint8_t paletteIndex;
                    
                    if (soundSettings.intensityMapping) {
                        // Map volume to color intensity instead of height
                        paletteIndex = map(rawBandValues[strip] / soundSettings.amplitude, 0, LEDS_PER_STRIP, 0, 255);
                    } else {
                        // Normal palette mapping by position
                        paletteIndex = map(i, 0, LEDS_PER_STRIP - 1, 0, 255);
                        paletteIndex += (millis() * soundSettings.colorSpeed / 500) % 256; // Color cycling
                    }
                    
                    if (shouldLight) {
                        color = ColorFromPalette(primaryPalette, paletteIndex);
                    } else {
                        color = ColorFromPalette(secondaryPalette, paletteIndex);
                    }
                }
                
                // Beat effects
                if (applyBeatEffect) {
                    if (soundSettings.strobeOnBeat) {
                        color = CRGB::White; // Flash white on beat
                    } else if (soundSettings.pulseOnBeat) {
                        color.fadeToBlackBy(128); // Dim on beat for pulse effect
                    }
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

// Custom status update function that includes all umbrella sound settings
void sendUmbrellaStatusUpdate() {
    // Get basic BMDevice state
    BMDeviceState& state = device.getState();
    BMDeviceDefaults& defaults = device.getDefaults();
    DeviceDefaults currentDefaults = defaults.getCurrentDefaults();
    LightShow& lightShow = device.getLightShow();
    
    // Create comprehensive status JSON including sound settings
    StaticJsonDocument<1536> doc; // Increased size for all sound settings
    
    // Basic device state (from BMDevice)
    doc["pwr"] = state.power;
    doc["bri"] = state.brightness;
    doc["spd"] = state.speed;
    doc["dir"] = state.reverseStrip;
    
    const char* effectName = LightShow::effectIdToName(state.currentEffect);
    doc["fx"] = effectName;
    doc["pal"] = LightShow::paletteIdToName(state.currentPalette);
    
    // Debug secondary palette
    const char* secPalName = LightShow::paletteIdToName(currentSecondaryPaletteId);
    Serial.printf("🎨 [DEBUG] Secondary palette ID: %d, Name: %s\n", (int)currentSecondaryPaletteId, secPalName ? secPalName : "NULL");
    doc["secPal"] = secPalName;
    
    // GPS/Position data (umbrella doesn't use GPS but BMDevice includes these)
    doc["posAvail"] = false;
    doc["spdCur"] = 0;
    
    // Add defaults information
    doc["maxBri"] = currentDefaults.maxBrightness;
    doc["owner"] = currentDefaults.owner;
    doc["deviceName"] = currentDefaults.deviceName;
    
    // ✅ ADD ALL SOUND SETTINGS
    doc["sound"] = soundSettings.soundSensitive;  // ✅ FIXED: changed from "soundMode" to "sound"
    doc["amplitude"] = soundSettings.amplitude;
    doc["noiseThreshold"] = soundSettings.noiseThreshold;
    doc["sensitivity"] = (int)pow(10, soundSettings.reference); // Convert back from log
    
    // Decay settings
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
    
    String status;
    serializeJson(doc, status);
    Serial.print("🌂 [UMBRELLA STATUS] Sending comprehensive status: ");
    Serial.println(status);
    
    // Send via BMDevice's bluetooth handler
    BMBluetoothHandler& bluetoothHandler = device.getBluetoothHandler();
    bluetoothHandler.sendStatusUpdate(status);
}

void setup() {
    Serial.begin(115200);
    Serial.println("🌂 ================================");
    Serial.println("🌂 Starting Umbrella Device v2.0");
    Serial.println("🌂 ================================");
    
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
        Serial.println("❌ Failed to initialize BMDevice!");
        while (1);
    }
    
    // Set umbrella-specific defaults
    BMDeviceDefaults& defaults = device.getDefaults();
    
    // Initialize palettes
    LightShow& lightShow = device.getLightShow();
    std::pair<CRGBPalette16, CRGBPalette16> palettes = lightShow.getPrimarySecondaryPalettes();
    primaryPalette = palettes.first;
    secondaryPalette = palettes.second;
    
    Serial.println("✅ Umbrella BMDevice setup complete!");
    Serial.printf("🎵 Initial sound mode: %s\n", soundSettings.soundSensitive ? "ON" : "OFF");
    defaults.printCurrentDefaults();
}

void loop() {
    // Handle BMDevice operations (BLE, state management, etc.) but skip automatic status updates
    // We'll handle status updates manually to include sound settings
    
    // Poll Bluetooth manually (like BMDevice does)
    BMBluetoothHandler& bluetoothHandler = device.getBluetoothHandler();
    bluetoothHandler.poll();
    
    // Get current state
    BMDeviceState& state = device.getState();
    
    // Handle our custom status updates with sound settings
    static unsigned long lastStatusUpdate = 0;
    unsigned long statusUpdateInterval = 5000; // 5 seconds like BMDevice default
    
    if ((millis() - lastStatusUpdate) > statusUpdateInterval && bluetoothHandler.isConnected()) {
        sendUmbrellaStatusUpdate(); // Send comprehensive status including sound settings
        lastStatusUpdate = millis();
    }
    
    // Debug: Print power state changes
    static bool lastPowerState = true;
    if (state.power != lastPowerState) {
        Serial.printf("🔋 [POWER DEBUG] Power changed: %s -> %s\n", 
                     lastPowerState ? "ON" : "OFF", 
                     state.power ? "ON" : "OFF");
        lastPowerState = state.power;
    }
    
    // Only proceed if device is powered on
    if (!state.power) {
        FastLED.clear();
        FastLED.show();
        return;
    }
    
    // Debug: Print mode information occasionally
    static unsigned long lastDebugPrint = 0;
    if (millis() - lastDebugPrint > 5000) {  // Every 5 seconds
        Serial.printf("🎵 [STATUS] Power: %s, SoundMode: %s\n", 
                     state.power ? "ON" : "OFF",
                     soundSettings.soundSensitive ? "ON" : "OFF");
        lastDebugPrint = millis();
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
// Basic Sound Controls:
// 0x03 - Set sensitivity (int)
// 0x07 - Set sound sensitivity on/off (bool)
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
// Your Umbrella Music Visualizer now has:
// - Full BMDevice integration with shared palettes and light shows
// - Advanced FFT sound analysis for 8 LED strips
// - Beat detection with strobe and pulse effects
// - Peak hold with configurable decay times
// - Smoothing to reduce flicker
// - Multiple visual modes (bars vs dots)
// - Rainbow mode with cycling colors
// - Frequency emphasis controls (bass/mid/treble)
// - Auto-gain and ambient noise compensation
// - Multiple strip mapping patterns
// - Color intensity mapping
// - Individual strip directions
// - All standard BLE commands from BMDevice
// - 22 custom sound-specific BLE commands
// - Persistent defaults that survive reboots
// - Automatic status reporting
// - Power management
// - Complete parameter control via Bluetooth! 