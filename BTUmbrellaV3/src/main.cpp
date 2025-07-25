#include <BMDevice.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>
#include <Preferences.h>

// === HARDWARE CONFIGURATION ===
// LED Configuration - 8 LED strips for umbrella
#define NUM_STRIPS 8
#define LEDS_PER_STRIP 38
#define COLOR_ORDER GRB

// BLE Configuration - Same UUIDs as original
#define SERVICE_UUID "87748abc-e491-43a1-92bd-20b94ba20df4"
#define FEATURES_UUID "e06544bc-1989-4c0b-9ada-8cd4491d23a5"
#define STATUS_UUID "0b95cc9e-288e-49d2-a2aa-7230ed489ec8"

// Sound Analysis Configuration
#define DEFAULT_SAMPLES 256
#define DEFAULT_SAMPLING_FREQ 46000
#define I2S_PORT I2S_NUM_0
#define I2S_SCK_PIN GPIO_NUM_26
#define I2S_WS_PIN GPIO_NUM_22
#define I2S_SD_PIN GPIO_NUM_21

// === PREFERENCES CONFIGURATION ===
Preferences preferences;
const char* PREFS_NAMESPACE = "umbrella";
const char* PREFS_SOUND_KEY = "sound";
const char* PREFS_UMBRELLA_KEY = "umbrella";
const char* PREFS_VERSION_KEY = "version";
const int CURRENT_SETTINGS_VERSION = 1;

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

// === PREFERENCES FUNCTION DECLARATIONS ===
void saveUmbrellaSettings();
void loadUmbrellaSettings();
void saveAllSettings();
void loadAllSettings();
void restoreFactoryDefaults();
void verifyAllSettings();
void debugStructLayout();

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

// === PREFERENCES MANAGEMENT ===
void saveUmbrellaSettings() {
    Serial.println("💾 [SAVE UMBRELLA] Starting umbrella settings save...");
    Serial.printf("💾 [SAVE UMBRELLA] Namespace: %s\n", PREFS_NAMESPACE);
    
    bool success = preferences.begin(PREFS_NAMESPACE, false);
    if (!success) {
        Serial.println("❌ [SAVE UMBRELLA] Failed to begin preferences!");
        return;
    }
    Serial.println("✅ [SAVE UMBRELLA] Preferences opened successfully");
    
    // Save settings version
    size_t versionBytes = preferences.putInt(PREFS_VERSION_KEY, CURRENT_SETTINGS_VERSION);
    Serial.printf("💾 [SAVE UMBRELLA] Version save: %d bytes (key: %s, value: %d)\n", 
                 versionBytes, PREFS_VERSION_KEY, CURRENT_SETTINGS_VERSION);
    
    // Save umbrella-specific settings
    size_t palOffBytes = preferences.putBool("secPalOff", secondaryPaletteOff);
    size_t palIdBytes = preferences.putUChar("secPalId", (uint8_t)currentSecondaryPaletteId);
    Serial.printf("💾 [SAVE UMBRELLA] SecPalOff: %d bytes, SecPalId: %d bytes\n", palOffBytes, palIdBytes);
    
    // Save all sound settings as a blob
    size_t soundBytes = preferences.putBytes(PREFS_SOUND_KEY, &soundSettings, sizeof(soundSettings));
    Serial.printf("💾 [SAVE UMBRELLA] Sound blob: %d bytes (expected: %d)\n", soundBytes, sizeof(soundSettings));
    
    // ALSO save critical settings individually for debugging
    size_t rainbowBytes = preferences.putBool("rainbowMode", soundSettings.rainbowMode);
    size_t soundSensBytes = preferences.putBool("soundSensitive", soundSettings.soundSensitive);
    size_t ampBytes = preferences.putInt("amplitude", soundSettings.amplitude);
    Serial.printf("💾 [SAVE UMBRELLA] Individual: rainbow=%d, sound=%d, amp=%d bytes\n", 
                 rainbowBytes, soundSensBytes, ampBytes);
    
    preferences.end();
    
    if (versionBytes > 0 && palOffBytes > 0 && soundBytes > 0) {
        Serial.println("✅ [PREFERENCES] Umbrella settings saved successfully");
    } else {
        Serial.println("❌ [PREFERENCES] Some umbrella settings failed to save!");
    }
    
    Serial.printf("💾 [SAVE DEBUG] Saved rainbowMode: %s\n", soundSettings.rainbowMode ? "ON" : "OFF");
    Serial.printf("💾 [SAVE DEBUG] Saved secondaryPaletteOff: %s\n", secondaryPaletteOff ? "true" : "false");
}

void loadUmbrellaSettings() {
    preferences.begin(PREFS_NAMESPACE, true); // read-only
    
    // Check settings version
    int version = preferences.getInt(PREFS_VERSION_KEY, 0);
    Serial.printf("🔍 [LOAD DEBUG] Settings version in flash: %d (expected: %d)\n", version, CURRENT_SETTINGS_VERSION);
    
    if (version != CURRENT_SETTINGS_VERSION) {
        Serial.printf("⚠️ [PREFERENCES] Settings version mismatch (found: %d, expected: %d), using defaults\n", 
                     version, CURRENT_SETTINGS_VERSION);
        preferences.end();
        return;
    }
    
    // Load umbrella-specific settings with debugging
    bool oldSecondaryPaletteOff = secondaryPaletteOff;
    AvailablePalettes oldSecondaryPaletteId = currentSecondaryPaletteId;
    
    secondaryPaletteOff = preferences.getBool("secPalOff", false);
    currentSecondaryPaletteId = (AvailablePalettes)preferences.getUChar("secPalId", (uint8_t)AvailablePalettes::earth);
    
    Serial.printf("🔍 [LOAD DEBUG] Secondary palette OFF: %s -> %s\n", 
                 oldSecondaryPaletteOff ? "true" : "false",
                 secondaryPaletteOff ? "true" : "false");
    Serial.printf("🔍 [LOAD DEBUG] Secondary palette ID: %s -> %s\n",
                 LightShow::paletteIdToName(oldSecondaryPaletteId),
                 LightShow::paletteIdToName(currentSecondaryPaletteId));
    
    // Load sound settings with debugging
    size_t settingsSize = preferences.getBytesLength(PREFS_SOUND_KEY);
    Serial.printf("🔍 [LOAD DEBUG] Sound settings blob size: %d bytes (expected: %d)\n", 
                 settingsSize, sizeof(soundSettings));
    
    if (settingsSize == sizeof(soundSettings)) {
        // Save old values for comparison
        bool oldRainbowMode = soundSettings.rainbowMode;
        bool oldSoundSensitive = soundSettings.soundSensitive;
        int oldAmplitude = soundSettings.amplitude;
        
        preferences.getBytes(PREFS_SOUND_KEY, &soundSettings, sizeof(soundSettings));
        
        Serial.printf("🔍 [LOAD DEBUG] Rainbow mode: %s -> %s\n",
                     oldRainbowMode ? "ON" : "OFF",
                     soundSettings.rainbowMode ? "ON" : "OFF");
        Serial.printf("🔍 [LOAD DEBUG] Sound mode: %s -> %s\n",
                     oldSoundSensitive ? "ON" : "OFF", 
                     soundSettings.soundSensitive ? "ON" : "OFF");
        Serial.printf("🔍 [LOAD DEBUG] Amplitude: %d -> %d\n",
                     oldAmplitude, soundSettings.amplitude);
        
        Serial.println("📂 [PREFERENCES] Sound settings loaded from flash");
        
        // COMPARE with individually saved settings for debugging
        bool individualRainbow = preferences.getBool("rainbowMode", false);
        bool individualSoundSensitive = preferences.getBool("soundSensitive", true);
        int individualAmplitude = preferences.getInt("amplitude", 1000);
        
        Serial.printf("🔍 [COMPARE DEBUG] Blob vs Individual:\n");
        Serial.printf("  Rainbow: blob=%s, individual=%s\n", 
                     soundSettings.rainbowMode ? "ON" : "OFF",
                     individualRainbow ? "ON" : "OFF");
        Serial.printf("  SoundSensitive: blob=%s, individual=%s\n",
                     soundSettings.soundSensitive ? "ON" : "OFF", 
                     individualSoundSensitive ? "ON" : "OFF");
        Serial.printf("  Amplitude: blob=%d, individual=%d\n",
                     soundSettings.amplitude, individualAmplitude);
        
        // FALLBACK: If blob and individual don't match, use individual values
        if (soundSettings.rainbowMode != individualRainbow || 
            soundSettings.soundSensitive != individualSoundSensitive ||
            soundSettings.amplitude != individualAmplitude) {
            Serial.println("⚠️ [FALLBACK] Blob and individual settings mismatch, using individual values");
            soundSettings.rainbowMode = individualRainbow;
            soundSettings.soundSensitive = individualSoundSensitive;
            soundSettings.amplitude = individualAmplitude;
        }
    } else {
        Serial.printf("⚠️ [PREFERENCES] Sound settings size mismatch (expected: %d, found: %d), using individual fallback\n", 
                     sizeof(soundSettings), settingsSize);
        
        // FALLBACK: Load critical settings individually
        soundSettings.rainbowMode = preferences.getBool("rainbowMode", false);
        soundSettings.soundSensitive = preferences.getBool("soundSensitive", true);
        soundSettings.amplitude = preferences.getInt("amplitude", 1000);
        Serial.println("📂 [FALLBACK] Loaded critical settings individually");
    }
    
    preferences.end();
    Serial.println("📂 [PREFERENCES] Umbrella settings loaded successfully");
}

// === STRUCT DEBUGGING ===
void debugStructLayout() {
    Serial.println("🔍 [STRUCT DEBUG] SoundSettings struct analysis:");
    Serial.printf("  Struct size: %d bytes\n", sizeof(SoundSettings));
    Serial.printf("  Expected size (rough): ~%d bytes\n", 
                 4*4 + 1*20 + 4*10 + 8*3); // int*4 + bool*20 + int*10 + float*3
    
    // Show current values
    Serial.println("🔍 [STRUCT DEBUG] Current SoundSettings values:");
    Serial.printf("  soundSensitive: %s (offset ~0)\n", soundSettings.soundSensitive ? "ON" : "OFF");
    Serial.printf("  amplitude: %d (offset ~1)\n", soundSettings.amplitude);
    Serial.printf("  rainbowMode: %s (offset ~17)\n", soundSettings.rainbowMode ? "ON" : "OFF");
    Serial.printf("  samplingFrequency: %d (offset ~88)\n", soundSettings.samplingFrequency);
}

void saveAllSettings() {
    Serial.println("💾 [PREFERENCES] Saving ALL settings...");
    
    // Save BMDevice settings using the correct API
    BMDeviceState& state = device.getState();
    
    // Log what BMDevice settings will be saved
    Serial.printf("📝 [BMDevice SETTINGS] Power: %s, Brightness: %d, Speed: %d\n", 
                 state.power ? "ON" : "OFF", 
                 state.brightness, 
                 state.speed);
    Serial.printf("📝 [BMDevice SETTINGS] Direction: %s, Effect: %s, Palette: %s\n",
                 state.reverseStrip ? "Reverse" : "Normal",
                 LightShow::effectIdToName(state.currentEffect),
                 LightShow::paletteIdToName(state.currentPalette));
    
    // Use BMDevice's built-in save method
    Serial.println("💾 [BMDevice] About to call device.saveCurrentAsDefaults()...");
    bool bmDeviceSaved = device.saveCurrentAsDefaults();
    Serial.printf("💾 [BMDevice] saveCurrentAsDefaults() returned: %s\n", bmDeviceSaved ? "true" : "false");
    
    if (bmDeviceSaved) {
        Serial.println("✅ [BMDevice] Settings saved successfully");
    } else {
        Serial.println("❌ [BMDevice] Failed to save settings");
    }
    
    // Save umbrella-specific settings with detailed logging
    Serial.printf("📝 [UMBRELLA SETTINGS] Secondary Palette: %s (Off: %s)\n",
                 LightShow::paletteIdToName(currentSecondaryPaletteId),
                 secondaryPaletteOff ? "true" : "false");
    
    // Log ALL sound settings being saved
    Serial.println("📝 [SOUND SETTINGS] Saving complete sound configuration:");
    Serial.printf("  Basic: SoundMode=%s, Amplitude=%d, NoiseThreshold=%d, Sensitivity=%d\n",
                 soundSettings.soundSensitive ? "ON" : "OFF",
                 soundSettings.amplitude,
                 soundSettings.noiseThreshold,
                 (int)pow(10, soundSettings.reference));
    
    Serial.printf("  Visual: BarMode=%s, Rainbow=%s, ColorSpeed=%d, Decay=%s, DecayRate=%d\n",
                 soundSettings.barMode ? "Bars" : "Dots",
                 soundSettings.rainbowMode ? "ON" : "OFF",
                 soundSettings.colorSpeed,
                 soundSettings.decay ? "ON" : "OFF",
                 soundSettings.decayRate);
    
    Serial.printf("  Effects: PeakHold=%s, PeakHoldTime=%d, Smoothing=%s, SmoothingFactor=%.2f\n",
                 soundSettings.peakHold ? "ON" : "OFF",
                 soundSettings.peakHoldTime,
                 soundSettings.smoothing ? "ON" : "OFF",
                 soundSettings.smoothingFactor);
    
    Serial.printf("  Beat: Detection=%s, Sensitivity=%d, Strobe=%s, Pulse=%s\n",
                 soundSettings.beatDetection ? "ON" : "OFF",
                 soundSettings.beatSensitivity,
                 soundSettings.strobeOnBeat ? "ON" : "OFF",
                 soundSettings.pulseOnBeat ? "ON" : "OFF");
    
    Serial.printf("  Frequency: Bass=%d, Mid=%d, Treble=%d, LogMapping=%s\n",
                 soundSettings.bassEmphasis,
                 soundSettings.midEmphasis,
                 soundSettings.trebleEmphasis,
                 soundSettings.logarithmicMapping ? "ON" : "OFF");
    
    Serial.printf("  Auto: Gain=%s, AmbientComp=%s, IntensityMapping=%s\n",
                 soundSettings.autoGain ? "ON" : "OFF",
                 soundSettings.ambientCompensation ? "ON" : "OFF",
                 soundSettings.intensityMapping ? "ON" : "OFF");
    
    Serial.printf("  Strips: Mapping=%d, IndividualDirections=%s\n",
                 soundSettings.stripMapping,
                 soundSettings.individualDirections ? "ON" : "OFF");
    
    Serial.printf("  Audio: SampleRate=%d Hz, SampleCount=%d, GainMultiplier=%.2f\n",
                 soundSettings.samplingFrequency,
                 soundSettings.sampleCount,
                 soundSettings.gainMultiplier);
    
    Serial.printf("  LEDs: Min=%d, Max=%d, LEDMultiplier=%.2f, DoubleHeight=%s, FillFromCenter=%s\n",
                 soundSettings.minLEDs,
                 soundSettings.maxLEDs,
                 soundSettings.ledMultiplier,
                 soundSettings.doubleHeight ? "ON" : "OFF",
                 soundSettings.fillFromCenter ? "ON" : "OFF");
    
    Serial.printf("  FreqRange: Min=%d Hz, Max=%d Hz\n",
                 soundSettings.frequencyMin,
                 soundSettings.frequencyMax);
    
    // Save umbrella-specific settings
    saveUmbrellaSettings();
    
    if (bmDeviceSaved) {
        Serial.printf("✅ [PREFERENCES] ALL 37 sound parameters + BMDevice settings + umbrella settings saved!\n");
        Serial.println("✅ [PREFERENCES] Complete settings backup created!");
    } else {
        Serial.println("⚠️ [PREFERENCES] Umbrella settings saved, but BMDevice settings failed");
    }
}

void loadAllSettings() {
    Serial.println("📂 [PREFERENCES] Loading all settings...");
    
    // Load BMDevice settings first (BMDevice handles this automatically)
    // BMDevice will load its defaults and apply them to current state
    Serial.println("📂 [BMDevice] Loading BMDevice defaults...");
    
    // Load umbrella-specific settings
    loadUmbrellaSettings();
    
    // VALIDATE SETTINGS IMMEDIATELY AFTER LOADING
    Serial.println("🔍 [IMMEDIATE VALIDATION] Settings right after loading:");
    Serial.printf("  Secondary Palette: %s (Off: %s)\n",
                 LightShow::paletteIdToName(currentSecondaryPaletteId),
                 secondaryPaletteOff ? "true" : "false");
    Serial.printf("  Rainbow Mode: %s\n", soundSettings.rainbowMode ? "ON" : "OFF");
    Serial.printf("  Sound Mode: %s\n", soundSettings.soundSensitive ? "ON" : "OFF");
    Serial.printf("  Amplitude: %d\n", soundSettings.amplitude);
    
    // Log what was loaded
    BMDeviceState& state = device.getState();
    Serial.println("📂 [LOADED VERIFICATION] Settings successfully loaded:");
    Serial.printf("  BMDevice: Power=%s, Brightness=%d, Speed=%d, Direction=%s\n",
                 state.power ? "ON" : "OFF",
                 state.brightness,
                 state.speed,
                 state.reverseStrip ? "Reverse" : "Normal");
    Serial.printf("  BMDevice: Effect=%s, Palette=%s\n",
                 LightShow::effectIdToName(state.currentEffect),
                 LightShow::paletteIdToName(state.currentPalette));
    
    Serial.printf("  Sound Mode: %s, Amplitude: %d, Sensitivity: %d\n",
                 soundSettings.soundSensitive ? "ON" : "OFF",
                 soundSettings.amplitude,
                 (int)pow(10, soundSettings.reference));
    
    Serial.printf("  Audio Hardware: %d samples at %d Hz\n",
                 soundSettings.sampleCount,
                 soundSettings.samplingFrequency);
    
    Serial.printf("  Secondary Palette: %s (Off: %s)\n",
                 LightShow::paletteIdToName(currentSecondaryPaletteId),
                 secondaryPaletteOff ? "true" : "false");
    
    // Ensure palettes are updated after loading
    updatePalettesFromBMDevice();
    
    Serial.println("✅ [PREFERENCES] All settings loaded and verified!");
}

// === SETTINGS VERIFICATION ===
void verifyAllSettings() {
    Serial.println("🔍 [VERIFICATION] Complete settings audit:");
    
    // Verify BMDevice current state and saved defaults
    BMDeviceState& state = device.getState();
    BMDeviceDefaults& defaults = device.getDefaults();
    DeviceDefaults currentDefaults = defaults.getCurrentDefaults();
    
    Serial.println("🔍 [BMDevice CURRENT STATE]:");
    Serial.printf("  Power: %s, Brightness: %d, Speed: %d\n",
                 state.power ? "ON" : "OFF",
                 state.brightness,
                 state.speed);
    Serial.printf("  Effect: %s, Palette: %s, Direction: %s\n",
                 LightShow::effectIdToName(state.currentEffect),
                 LightShow::paletteIdToName(state.currentPalette),
                 state.reverseStrip ? "Reverse" : "Normal");
    
    Serial.println("🔍 [BMDevice SAVED DEFAULTS]:");
    Serial.printf("  Brightness: %d (max: %d), Speed: %d\n",
                 currentDefaults.brightness,
                 currentDefaults.maxBrightness,
                 currentDefaults.speed);
    Serial.printf("  Effect: %s, Palette: %s, Direction: %s\n",
                 LightShow::effectIdToName(currentDefaults.effect),
                 LightShow::paletteIdToName(currentDefaults.palette),
                 currentDefaults.reverseDirection ? "Reverse" : "Normal");
    Serial.printf("  Device: Name=%s, Owner=%s, AutoOn=%s\n",
                 currentDefaults.deviceName.c_str(),
                 currentDefaults.owner.c_str(),
                 currentDefaults.autoOn ? "Yes" : "No");
    Serial.printf("  Behavior: StatusInterval=%lums, GPS=%s\n",
                 currentDefaults.statusUpdateInterval,
                 currentDefaults.gpsEnabled ? "Yes" : "No");
    Serial.printf("  EffectColor: RGB(%d,%d,%d)\n", 
                 currentDefaults.effectColor.r, 
                 currentDefaults.effectColor.g, 
                 currentDefaults.effectColor.b);
    
    // Verify ALL umbrella settings
    Serial.println("🔍 [UMBRELLA CONFIG]:");
    Serial.printf("  Secondary Palette: %s (Mode: %s)\n",
                 LightShow::paletteIdToName(currentSecondaryPaletteId),
                 secondaryPaletteOff ? "OFF" : "ON");
    
    // Verify ALL sound settings (every single parameter)
    Serial.println("🔍 [SOUND CONFIG] - ALL 37 PARAMETERS:");
    Serial.printf("  [01] soundSensitive: %s\n", soundSettings.soundSensitive ? "ON" : "OFF");
    Serial.printf("  [02] amplitude: %d\n", soundSettings.amplitude);
    Serial.printf("  [03] noiseThreshold: %d\n", soundSettings.noiseThreshold);
    Serial.printf("  [04] reference (sensitivity): %.2f (%d)\n", soundSettings.reference, (int)pow(10, soundSettings.reference));
    Serial.printf("  [05] decay: %s\n", soundSettings.decay ? "ON" : "OFF");
    Serial.printf("  [06] decayRate: %d\n", soundSettings.decayRate);
    Serial.printf("  [07] barMode: %s\n", soundSettings.barMode ? "Bars" : "Dots");
    Serial.printf("  [08] rainbowMode: %s\n", soundSettings.rainbowMode ? "ON" : "OFF");
    Serial.printf("  [09] colorSpeed: %d\n", soundSettings.colorSpeed);
    Serial.printf("  [10] reverseDirection: %s (unused)\n", soundSettings.reverseDirection ? "ON" : "OFF");
    Serial.printf("  [11] peakHold: %s\n", soundSettings.peakHold ? "ON" : "OFF");
    Serial.printf("  [12] peakHoldTime: %d ms\n", soundSettings.peakHoldTime);
    Serial.printf("  [13] smoothing: %s\n", soundSettings.smoothing ? "ON" : "OFF");
    Serial.printf("  [14] smoothingFactor: %.2f\n", soundSettings.smoothingFactor);
    Serial.printf("  [15] intensityMapping: %s\n", soundSettings.intensityMapping ? "ON" : "OFF");
    Serial.printf("  [16] beatDetection: %s\n", soundSettings.beatDetection ? "ON" : "OFF");
    Serial.printf("  [17] beatSensitivity: %d\n", soundSettings.beatSensitivity);
    Serial.printf("  [18] strobeOnBeat: %s\n", soundSettings.strobeOnBeat ? "ON" : "OFF");
    Serial.printf("  [19] pulseOnBeat: %s\n", soundSettings.pulseOnBeat ? "ON" : "OFF");
    Serial.printf("  [20] bassEmphasis: %d\n", soundSettings.bassEmphasis);
    Serial.printf("  [21] midEmphasis: %d\n", soundSettings.midEmphasis);
    Serial.printf("  [22] trebleEmphasis: %d\n", soundSettings.trebleEmphasis);
    Serial.printf("  [23] logarithmicMapping: %s\n", soundSettings.logarithmicMapping ? "ON" : "OFF");
    Serial.printf("  [24] autoGain: %s\n", soundSettings.autoGain ? "ON" : "OFF");
    Serial.printf("  [25] ambientCompensation: %s\n", soundSettings.ambientCompensation ? "ON" : "OFF");
    Serial.printf("  [26] stripMapping: %d\n", soundSettings.stripMapping);
    Serial.printf("  [27] individualDirections: %s\n", soundSettings.individualDirections ? "ON" : "OFF");
    Serial.printf("  [28] samplingFrequency: %d Hz\n", soundSettings.samplingFrequency);
    Serial.printf("  [29] sampleCount: %d\n", soundSettings.sampleCount);
    Serial.printf("  [30] gainMultiplier: %.2f\n", soundSettings.gainMultiplier);
    Serial.printf("  [31] minLEDs: %d\n", soundSettings.minLEDs);
    Serial.printf("  [32] maxLEDs: %d\n", soundSettings.maxLEDs);
    Serial.printf("  [33] frequencyMin: %d Hz\n", soundSettings.frequencyMin);
    Serial.printf("  [34] frequencyMax: %d Hz\n", soundSettings.frequencyMax);
    Serial.printf("  [35] doubleHeight: %s\n", soundSettings.doubleHeight ? "ON" : "OFF");
    Serial.printf("  [36] ledMultiplier: %.2f\n", soundSettings.ledMultiplier);
    Serial.printf("  [37] fillFromCenter: %s\n", soundSettings.fillFromCenter ? "ON" : "OFF");
    
    Serial.println("🔍 [SUMMARY] Total configurable parameters tracked:");
    Serial.printf("  - BMDevice current state: 6 runtime parameters\n");
    Serial.printf("  - BMDevice saved defaults: 12 persistent parameters\n");
    Serial.printf("  - Umbrella settings: 2 palette parameters\n");
    Serial.printf("  - Sound settings: 37 audio/visual parameters\n");
    Serial.printf("  - TOTAL CONFIGURABLE: 57 parameters (45 saved + 12 BMDevice)\n");
    Serial.println("✅ [VERIFICATION] Complete settings audit finished!");
}

// === SOUND FEATURE HANDLER ===
bool handleSoundFeatures(uint8_t feature, const uint8_t* data, size_t length) {
    Serial.printf("🔧 [SOUND FEATURE] Received: 0x%02X, length: %d\n", feature, length);
    
    // Handle save settings command (use new dedicated command)
    if (feature == 0x6A) {
        Serial.println("💾 [SAVE SETTINGS] Received save command");
        Serial.printf("💾 [SAVE DEBUG] About to call saveAllSettings() - Free heap: %d bytes\n", ESP.getFreeHeap());
        
        saveAllSettings();
        
        Serial.println("🔍 [POST-SAVE VERIFICATION] Verifying saved settings...");
        verifyAllSettings();
        
        // IMMEDIATE VERIFICATION: Try to read back what we just saved
        Serial.println("🔍 [IMMEDIATE READ-BACK TEST]");
        preferences.begin(PREFS_NAMESPACE, true); // read-only
        int savedVersion = preferences.getInt(PREFS_VERSION_KEY, -1);
        bool savedSecPalOff = preferences.getBool("secPalOff", false);
        Serial.printf("  Read-back version: %d (should be 1)\n", savedVersion);
        Serial.printf("  Read-back secPalOff: %s\n", savedSecPalOff ? "true" : "false");
        preferences.end();
        
        return true;
    }
    
    // Handle factory reset command
    if (feature == 0x65) {
        Serial.println("🏭 [FACTORY RESET] Received factory reset command");
        restoreFactoryDefaults();
        return true;
    }
    
    // Handle settings verification command
    if (feature == 0x66) {
        Serial.println("🔍 [VERIFICATION] Received verification command");
        verifyAllSettings();
        return true;
    }
    
    // Handle preferences test command
    if (feature == 0x67) {
        Serial.println("🧪 [PREFS TEST] Testing Preferences.h functionality...");
        
        // Test basic preferences functionality
        preferences.begin("test", false);
        
        // Write test data
        size_t intBytes = preferences.putInt("testInt", 12345);
        size_t boolBytes = preferences.putBool("testBool", true);
        size_t stringBytes = preferences.putString("testString", "hello");
        
        Serial.printf("🧪 [PREFS TEST] Write results: int=%d, bool=%d, string=%d bytes\n", 
                     intBytes, boolBytes, stringBytes);
        
        // Read test data back
        int readInt = preferences.getInt("testInt", 0);
        bool readBool = preferences.getBool("testBool", false);
        String readString = preferences.getString("testString", "");
        
        Serial.printf("🧪 [PREFS TEST] Read results: int=%d, bool=%s, string=%s\n",
                     readInt, readBool ? "true" : "false", readString.c_str());
        
        // Clear test data
        preferences.clear();
        preferences.end();
        
        if (readInt == 12345 && readBool == true && readString == "hello") {
            Serial.println("✅ [PREFS TEST] Preferences.h is working correctly!");
        } else {
            Serial.println("❌ [PREFS TEST] Preferences.h is NOT working!");
        }
        
        return true;
    }
    
    // Let BMDevice handle ALL its standard features now (no more conflicts!)
    // BMDevice handles: 0x01, 0x04-0x0A, 0x0B-0x20, 0x30-0x34
    // We only handle 0x40+ range now
    if (feature < 0x40) {
        return false; // Let BMDevice handle everything under 0x40
    }
    
    // Handle secondary palette (moved from 0x09 to 0x42)
    if (feature == 0x42) {
        if (length > 1) {
            std::string paletteName(reinterpret_cast<const char*>(data + 1), length - 1);
            Serial.printf("🎨 [SECONDARY PALETTE DEBUG] Raw data length: %d\n", length);
            Serial.printf("🎨 [SECONDARY PALETTE DEBUG] Raw palette name: '%s' (length: %d)\n", 
                         paletteName.c_str(), paletteName.length());
            Serial.printf("🎨 [SECONDARY PALETTE DEBUG] Before change - Off: %s, ID: %s\n",
                         secondaryPaletteOff ? "true" : "false",
                         LightShow::paletteIdToName(currentSecondaryPaletteId));
            
            if (paletteName == "off") {
                secondaryPaletteOff = true;
                Serial.println("🎨 [SECONDARY PALETTE] ✅ Set to OFF mode");
            } else {
                secondaryPaletteOff = false;
                AvailablePalettes paletteEnum = LightShow::paletteNameToId(paletteName.c_str());
                currentSecondaryPaletteId = paletteEnum;
                Serial.printf("🎨 [SECONDARY PALETTE] ✅ Set to palette: %s\n", paletteName.c_str());
                // Update LightShow's secondary palette and get the updated palettes
                updatePalettesFromBMDevice();
            }
            
            Serial.printf("🎨 [SECONDARY PALETTE DEBUG] After change - Off: %s, ID: %s\n",
                         secondaryPaletteOff ? "true" : "false",
                         LightShow::paletteIdToName(currentSecondaryPaletteId));
            return true;
        }
        Serial.println("🎨 [SECONDARY PALETTE] ❌ Invalid data length");
        return false;
    }
    
    // Handle sound-specific features (all remapped to 0x40+ range)
    switch (feature) {
        case 0x40: // Sensitivity (was 0x03)
            if (length >= sizeof(int)) {
                int sensitivity = 0;
                memcpy(&sensitivity, data + 1, sizeof(int));
                soundSettings.reference = log10(sensitivity);
                Serial.printf("✅ Sensitivity: %d\n", sensitivity);
                return true;
            }
            break;
            
        case 0x41: // Sound Sensitivity on/off (was 0x07)
            if (length >= 2) {
                soundSettings.soundSensitive = data[1] != 0;
                Serial.printf("✅ Sound Mode: %s\n", soundSettings.soundSensitive ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x43: // Amplitude (was 0x0B)
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.amplitude, data + 1, sizeof(int));
                Serial.printf("✅ Amplitude: %d\n", soundSettings.amplitude);
                return true;
            }
            break;
            
        case 0x44: // Noise Threshold (was 0x0C)
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.noiseThreshold, data + 1, sizeof(int));
                Serial.printf("✅ Noise Threshold: %d\n", soundSettings.noiseThreshold);
                return true;
            }
            break;
            
        case 0x45: // Decay (was 0x0D)
            if (length >= 2) {
                soundSettings.decay = data[1] != 0;
                Serial.printf("✅ Decay: %s\n", soundSettings.decay ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x46: // Decay Rate (was 0x0E)
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.decayRate, data + 1, sizeof(int));
                Serial.printf("✅ Decay Rate: %d\n", soundSettings.decayRate);
                return true;
            }
            break;
            
        case 0x47: // Peak Hold On/Off (was 0x0F)
            if (length >= 2) {
                soundSettings.peakHold = data[1] != 0;
                Serial.printf("✅ Peak Hold: %s\n", soundSettings.peakHold ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x48: // Peak Hold Time (was 0x10)
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.peakHoldTime, data + 1, sizeof(int));
                Serial.printf("✅ Peak Hold Time: %d\n", soundSettings.peakHoldTime);
                return true;
            }
            break;
            
        case 0x49: // Smoothing On/Off (was 0x11)
            if (length >= 2) {
                soundSettings.smoothing = data[1] != 0;
                Serial.printf("✅ Smoothing: %s\n", soundSettings.smoothing ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x4A: // Smoothing Factor (was 0x12)
            if (length >= sizeof(float) + 1) {
                memcpy(&soundSettings.smoothingFactor, data + 1, sizeof(float));
                Serial.printf("✅ Smoothing Factor: %.2f\n", soundSettings.smoothingFactor);
                return true;
            }
            break;
            
        case 0x4B: // Bar Mode (was 0x13)
            if (length >= 2) {
                soundSettings.barMode = data[1] != 0;
                Serial.printf("✅ Visual Mode: %s\n", soundSettings.barMode ? "Bars" : "Dots");
                return true;
            }
            break;
            
        case 0x4C: // Rainbow Mode (was 0x14)
            if (length >= 2) {
                soundSettings.rainbowMode = data[1] != 0;
                Serial.printf("✅ Rainbow Mode: %s\n", soundSettings.rainbowMode ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x4D: // Color Speed (was 0x15)
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.colorSpeed, data + 1, sizeof(int));
                Serial.printf("✅ Color Speed: %d\n", soundSettings.colorSpeed);
                return true;
            }
            break;
            
        case 0x4E: // Intensity Mapping (was 0x16)
            if (length >= 2) {
                soundSettings.intensityMapping = data[1] != 0;
                Serial.printf("✅ Intensity Mapping: %s\n", soundSettings.intensityMapping ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x4F: // Beat Detection (was 0x17)
            if (length >= 2) {
                soundSettings.beatDetection = data[1] != 0;
                Serial.printf("✅ Beat Detection: %s\n", soundSettings.beatDetection ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x50: // Beat Sensitivity (was 0x18)
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.beatSensitivity, data + 1, sizeof(int));
                Serial.printf("✅ Beat Sensitivity: %d\n", soundSettings.beatSensitivity);
                return true;
            }
            break;
            
        case 0x51: // Strobe on Beat (was 0x19)
            if (length >= 2) {
                soundSettings.strobeOnBeat = data[1] != 0;
                Serial.printf("✅ Strobe on Beat: %s\n", soundSettings.strobeOnBeat ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x52: // Pulse on Beat (was 0x21)
            if (length >= 2) {
                soundSettings.pulseOnBeat = data[1] != 0;
                Serial.printf("✅ Pulse on Beat: %s\n", soundSettings.pulseOnBeat ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x53: // Bass Emphasis (was 0x22)
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.bassEmphasis, data + 1, sizeof(int));
                Serial.printf("✅ Bass Emphasis: %d\n", soundSettings.bassEmphasis);
                return true;
            }
            break;
            
        case 0x54: // Mid Emphasis (was 0x23)
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.midEmphasis, data + 1, sizeof(int));
                Serial.printf("✅ Mid Emphasis: %d\n", soundSettings.midEmphasis);
                return true;
            }
            break;
            
        case 0x55: // Treble Emphasis (was 0x24)
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.trebleEmphasis, data + 1, sizeof(int));
                Serial.printf("✅ Treble Emphasis: %d\n", soundSettings.trebleEmphasis);
                return true;
            }
            break;
            
        case 0x56: // Logarithmic Mapping (was 0x25)
            if (length >= 2) {
                soundSettings.logarithmicMapping = data[1] != 0;
                Serial.printf("✅ Logarithmic Mapping: %s\n", soundSettings.logarithmicMapping ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x57: // Auto Gain (was 0x26)
            if (length >= 2) {
                soundSettings.autoGain = data[1] != 0;
                Serial.printf("✅ Auto Gain: %s\n", soundSettings.autoGain ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x58: // Ambient Compensation (was 0x27)
            if (length >= 2) {
                soundSettings.ambientCompensation = data[1] != 0;
                Serial.printf("✅ Ambient Compensation: %s\n", soundSettings.ambientCompensation ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x59: // Strip Mapping (was 0x28)
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.stripMapping, data + 1, sizeof(int));
                Serial.printf("✅ Strip Mapping: %d\n", soundSettings.stripMapping);
                return true;
            }
            break;
            
        case 0x5A: // Individual Directions (was 0x29)
            if (length >= 2) {
                soundSettings.individualDirections = data[1] != 0;
                Serial.printf("✅ Individual Directions: %s\n", soundSettings.individualDirections ? "ON" : "OFF");
                return true;
            }
            break;
            
        // ADVANCED FEATURES
        case 0x5B: // Sampling Frequency (was 0x2A)
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
            
        case 0x5C: // Sample Count (was 0x2B)
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
            
        case 0x5D: // Gain Multiplier (was 0x2C)
            if (length >= sizeof(float) + 1) {
                memcpy(&soundSettings.gainMultiplier, data + 1, sizeof(float));
                soundSettings.gainMultiplier = constrain(soundSettings.gainMultiplier, 0.1f, 10.0f);
                Serial.printf("✅ Gain Multiplier: %.2f\n", soundSettings.gainMultiplier);
                return true;
            }
            break;
            
        case 0x5E: // Min LEDs (was 0x2D)
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.minLEDs, data + 1, sizeof(int));
                soundSettings.minLEDs = constrain(soundSettings.minLEDs, 0, LEDS_PER_STRIP);
                Serial.printf("✅ Min LEDs: %d\n", soundSettings.minLEDs);
                return true;
            }
            break;
            
        case 0x5F: // Max LEDs (was 0x2E)
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.maxLEDs, data + 1, sizeof(int));
                soundSettings.maxLEDs = constrain(soundSettings.maxLEDs, 1, LEDS_PER_STRIP);
                Serial.printf("✅ Max LEDs: %d\n", soundSettings.maxLEDs);
                return true;
            }
            break;
            
        case 0x60: // Frequency Min (was 0x2F)
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.frequencyMin, data + 1, sizeof(int));
                soundSettings.frequencyMin = constrain(soundSettings.frequencyMin, 20, 10000);
                Serial.printf("✅ Frequency Min: %d\n", soundSettings.frequencyMin);
                return true;
            }
            break;
            
        case 0x61: // Frequency Max (was 0x30)
            if (length >= sizeof(int) + 1) {
                memcpy(&soundSettings.frequencyMax, data + 1, sizeof(int));
                soundSettings.frequencyMax = constrain(soundSettings.frequencyMax, 100, 20000);
                Serial.printf("✅ Frequency Max: %d\n", soundSettings.frequencyMax);
                return true;
            }
            break;
            
        case 0x62: // Double Height (was 0x31)
            if (length >= 2) {
                soundSettings.doubleHeight = data[1] != 0;
                Serial.printf("✅ Double Height: %s\n", soundSettings.doubleHeight ? "ON" : "OFF");
                return true;
            }
            break;
            
        case 0x63: // LED Multiplier (was 0x32)
            if (length >= sizeof(float) + 1) {
                memcpy(&soundSettings.ledMultiplier, data + 1, sizeof(float));
                soundSettings.ledMultiplier = constrain(soundSettings.ledMultiplier, 0.1f, 5.0f);
                Serial.printf("✅ LED Multiplier: %.2f\n", soundSettings.ledMultiplier);
                return true;
            }
            break;
            
        case 0x64: // Fill From Center (was 0x33)
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
        .sample_rate = (uint32_t)soundSettings.samplingFrequency,
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
    Serial.printf("🎤 I2S microphone initialized at %d Hz\n", soundSettings.samplingFrequency);
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
                        // Debug: occasionally log that we're using black background
                        static unsigned long lastSecondaryDebug = 0;
                        if (millis() - lastSecondaryDebug > 2000) { // Every 2 seconds
                            Serial.printf("🎨 [RENDER DEBUG] Using BLACK background (secondaryPaletteOff=true)\n");
                            lastSecondaryDebug = millis();
                        }
                    } else {
                        color = ColorFromPalette(secondaryPalette, paletteIndex);
                        color.fadeToBlackBy(192); // Dim background to 25% brightness
                        // Debug: occasionally log that we're using secondary palette
                        static unsigned long lastSecondaryDebug2 = 0;
                        if (millis() - lastSecondaryDebug2 > 2000) { // Every 2 seconds
                            Serial.printf("🎨 [RENDER DEBUG] Using secondary palette: %s (secondaryPaletteOff=false)\n",
                                         LightShow::paletteIdToName(currentSecondaryPaletteId));
                            lastSecondaryDebug2 = millis();
                        }
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
enum UmbrellaStatusUpdateState {
    UMBRELLA_STATUS_IDLE,
    UMBRELLA_STATUS_START_BASIC,
    UMBRELLA_STATUS_BASIC_SENT,
    UMBRELLA_STATUS_SOUND_SENT,
    UMBRELLA_STATUS_ADVANCED_SENT,
    UMBRELLA_STATUS_SUPER_ADVANCED_SENT
};
UmbrellaStatusUpdateState umbrellaStatusUpdateState = UMBRELLA_STATUS_IDLE;
unsigned long umbrellaStatusUpdateTimer = 0;
const unsigned long UMBRELLA_STATUS_UPDATE_DELAY = 25; // 25ms delay between chunks

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
    umbrellaStatusUpdateState = UMBRELLA_STATUS_START_BASIC;
    umbrellaStatusUpdateTimer = millis();
    Serial.println("🌂 [CHUNKED STATUS] Starting non-blocking chunked status update...");
}

void handleStatusUpdate() {
    if (umbrellaStatusUpdateState == UMBRELLA_STATUS_IDLE) {
        return;
    }
    
    unsigned long currentTime = millis();
    
    switch (umbrellaStatusUpdateState) {
        case UMBRELLA_STATUS_START_BASIC:
            sendBasicStatus();
            umbrellaStatusUpdateState = UMBRELLA_STATUS_BASIC_SENT;
            umbrellaStatusUpdateTimer = currentTime;
            break;
            
        case UMBRELLA_STATUS_BASIC_SENT:
            if (currentTime - umbrellaStatusUpdateTimer >= UMBRELLA_STATUS_UPDATE_DELAY) {
                sendSoundSettings();
                umbrellaStatusUpdateState = UMBRELLA_STATUS_SOUND_SENT;
                umbrellaStatusUpdateTimer = currentTime;
            }
            break;
            
        case UMBRELLA_STATUS_SOUND_SENT:
            if (currentTime - umbrellaStatusUpdateTimer >= UMBRELLA_STATUS_UPDATE_DELAY) {
                sendAdvancedSoundSettings();
                umbrellaStatusUpdateState = UMBRELLA_STATUS_ADVANCED_SENT;
                umbrellaStatusUpdateTimer = currentTime;
            }
            break;
            
        case UMBRELLA_STATUS_ADVANCED_SENT:
            if (currentTime - umbrellaStatusUpdateTimer >= UMBRELLA_STATUS_UPDATE_DELAY) {
                sendSuperAdvancedSoundSettings();
                umbrellaStatusUpdateState = UMBRELLA_STATUS_SUPER_ADVANCED_SENT;
                umbrellaStatusUpdateTimer = currentTime;
            }
            break;
            
        case UMBRELLA_STATUS_SUPER_ADVANCED_SENT:
            umbrellaStatusUpdateState = UMBRELLA_STATUS_IDLE;
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
    
    // Start BMDevice (this may load BMDevice settings)
    if (!device.begin()) {
        Serial.println("❌ Failed to initialize BMDevice!");
        while (1);
    }
    
    // Load saved settings from preferences (this must come before hardware initialization)
    loadAllSettings();
    
    // CHECKPOINT 1: Validate settings after loading
    Serial.println("🔍 [CHECKPOINT 1] Settings after loadAllSettings():");
    Serial.printf("  Secondary Palette: %s (Off: %s)\n",
                 LightShow::paletteIdToName(currentSecondaryPaletteId),
                 secondaryPaletteOff ? "true" : "false");
    Serial.printf("  Rainbow Mode: %s\n", soundSettings.rainbowMode ? "ON" : "OFF");
    
    // Initialize hardware with loaded settings
    initializeI2S();
    initializeFFT(soundSettings.sampleCount, soundSettings.samplingFrequency);
    
    // CHECKPOINT 2: Validate settings after hardware initialization
    Serial.println("🔍 [CHECKPOINT 2] Settings after hardware init:");
    Serial.printf("  Secondary Palette: %s (Off: %s)\n",
                 LightShow::paletteIdToName(currentSecondaryPaletteId),
                 secondaryPaletteOff ? "true" : "false");
    Serial.printf("  Rainbow Mode: %s\n", soundSettings.rainbowMode ? "ON" : "OFF");
    
    // Initialize palettes - make sure LightShow palettes are synchronized with BMDevice
    updatePalettesFromBMDevice();
    
    // CHECKPOINT 3: Validate settings after palette update
    Serial.println("🔍 [CHECKPOINT 3] Settings after updatePalettesFromBMDevice():");
    Serial.printf("  Secondary Palette: %s (Off: %s)\n",
                 LightShow::paletteIdToName(currentSecondaryPaletteId),
                 secondaryPaletteOff ? "true" : "false");
    Serial.printf("  Rainbow Mode: %s\n", soundSettings.rainbowMode ? "ON" : "OFF");
    
    Serial.println("✅ BTUmbrellaV3 Ready!");
    Serial.printf("🎵 Sound Mode: %s\n", soundSettings.soundSensitive ? "ON" : "OFF");
    Serial.printf("🎨 Primary Palette: %s\n", LightShow::paletteIdToName(device.getState().currentPalette));
    Serial.printf("🎨 Secondary Palette: %s (Off: %s)\n", 
                 LightShow::paletteIdToName(currentSecondaryPaletteId),
                 secondaryPaletteOff ? "true" : "false");
    Serial.printf("🎤 Audio Settings: %d samples at %d Hz\n", 
                 soundSettings.sampleCount, soundSettings.samplingFrequency);
    
    // Display complete settings verification on startup
    Serial.println("\n🔍 [STARTUP VERIFICATION] Current device configuration:");
    debugStructLayout();
    verifyAllSettings();
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
// Standard BMDevice commands (no longer conflicting!):
// 0x01 - Power on/off
// 0x04 - Set brightness  
// 0x05 - Set speed
// 0x06 - Set direction (forward/reverse)
// 0x07 - GPS origin setting (BMDevice)
// 0x08 - Set primary palette
// 0x09 - Speedometer (BMDevice)
// 0x0A - Set effect (BMDevice)
// 0x0B-0x19 - Effect parameters (BMDevice: wave width, meteor count, etc.)
// 0x1A-0x20 - Defaults management (BMDevice: get/set defaults, save current, factory reset, etc.)
// 0x30-0x34 - Generic device configuration (BMDevice: owner, device type, LED config, etc.)
//
// Custom Umbrella sound commands (remapped to avoid conflicts):
// Basic Sound Controls:
// 0x40 - Set sensitivity (int) [was 0x03]
// 0x41 - Set sound sensitivity on/off (bool) [was 0x07]
// 0x42 - Set secondary palette (string: palette name or "off") [was 0x09]
// 0x43 - Set amplitude (int) [was 0x0B]
// 0x44 - Set noise threshold (int) [was 0x0C]
// 0x45 - Set decay on/off (bool) [was 0x0D]
// 0x46 - Set decay rate (int) [was 0x0E]
//
// Visual Effects:
// 0x47 - Peak hold on/off (bool) [was 0x0F]
// 0x48 - Peak hold time in ms (int) [was 0x10]
// 0x49 - Smoothing on/off (bool) [was 0x11]
// 0x4A - Smoothing factor 0.0-1.0 (float) [was 0x12]
// 0x4B - Bar mode (true) vs Dot mode (false) (bool) [was 0x13]
// 0x4C - Rainbow mode on/off (bool) [was 0x14]
// 0x4D - Color speed 0-100 (int) [was 0x15]
// 0x4E - Intensity mapping on/off (bool) [was 0x16]
//
// Beat Detection & Effects:
// 0x4F - Beat detection on/off (bool) [was 0x17]
// 0x50 - Beat sensitivity 0-100 (int) [was 0x18]
// 0x51 - Strobe on beat on/off (bool) [was 0x19]
// 0x52 - Pulse on beat on/off (bool) [was 0x21]
//
// Frequency Controls:
// 0x53 - Bass emphasis 0-100 (int) [was 0x22]
// 0x54 - Mid emphasis 0-100 (int) [was 0x23]
// 0x55 - Treble emphasis 0-100 (int) [was 0x24]
// 0x56 - Logarithmic mapping on/off (bool) [was 0x25]
//
// Auto Adjustments:
// 0x57 - Auto gain on/off (bool) [was 0x26]
// 0x58 - Ambient compensation on/off (bool) [was 0x27]
//
// Strip Configuration:
// 0x59 - Strip mapping: 0=normal, 1=bass-treble, 2=treble-bass, 3=center-out (int) [was 0x28]
// 0x5A - Individual directions on/off (bool) [was 0x29]
//
// Advanced Audio Controls:
// 0x5B - Sampling frequency 8000-96000 Hz (int) [was 0x2A]
// 0x5C - Sample count 64-2048 (power of 2) (int) [was 0x2B]
// 0x5D - Gain multiplier 0.1-10.0 (float) [was 0x2C]
// 0x5E - Min LEDs 0-LEDS_PER_STRIP (int) [was 0x2D]
// 0x5F - Max LEDs 1-LEDS_PER_STRIP (int) [was 0x2E]
// 0x60 - Frequency min 20-10000 Hz (int) [was 0x2F]
// 0x61 - Frequency max 100-20000 Hz (int) [was 0x30]
// 0x62 - Double height on/off (bool) [was 0x31]
// 0x63 - LED multiplier 0.1-5.0 (float) [was 0x32]
// 0x64 - Fill from center on/off (bool) [was 0x33]
//
// Settings Management:
// 0x65 - Restore factory defaults (bool, any value triggers reset) [was 0x34]
// 0x66 - Verify and display all current settings (bool, any value triggers verification) [was 0x35]
// 0x67 - Test Preferences.h functionality (bool, any value triggers test) [was 0x36]
//
// Special Umbrella Commands:
// 0x6A - Save current settings as defaults (bool, any value triggers save) [was 0x0A override]
//
// BTUmbrellaV3 now has ZERO conflicts with BMDevice library!
// - Full BMDevice integration with shared palettes and light shows
// - All BMDevice commands work normally (0x01, 0x04-0x0A, 0x0B-0x20, 0x30-0x34)
// - All umbrella sound features remapped to safe 0x40-0x67 range
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
// - 40 custom sound-specific BLE commands (0x40-0x67)
// - Comprehensive persistent defaults via Preferences.h:
//   * BMDevice runtime state: 6 live parameters (power, brightness, speed, etc.)
//   * BMDevice saved defaults: 12 persistent parameters (device identity, limits, etc.)  
//   * Umbrella settings: 2 secondary palette configuration parameters
//   * Sound settings: 37 complete audio visualization parameters
//   * Total configurable: 57 parameters (39 saved + 18 BMDevice)
// - Save current settings as defaults (0x6A) with comprehensive verification
// - Factory reset capability (0x65) with complete restoration and verification
// - Complete settings verification (0x66) showing all 57 parameters organized by category
// - Automatic 4-part chunked status reporting
// - Power management
// - Complete parameter control via Bluetooth with NO conflicts! 

void restoreFactoryDefaults() {
    Serial.println("🏭 [PREFERENCES] Restoring factory defaults...");
    
    // Clear umbrella-specific preferences
    preferences.begin(PREFS_NAMESPACE, false);
    preferences.clear();
    preferences.end();
    
    // Reset BMDevice to factory defaults using correct API
    bool bmDeviceReset = device.resetToFactoryDefaults();
    if (bmDeviceReset) {
        Serial.println("✅ [BMDevice] Factory defaults restored successfully");
    } else {
        Serial.println("❌ [BMDevice] Failed to restore factory defaults");
    }
    
    // Reset umbrella-specific settings to defaults
    secondaryPaletteOff = false;
    currentSecondaryPaletteId = AvailablePalettes::earth;
    
    // Reset sound settings to defaults
    soundSettings = SoundSettings(); // Use default constructor values
    
    // Reinitialize with default values
    if (FFT) {
        initializeFFT(soundSettings.sampleCount, soundSettings.samplingFrequency);
        reinitializeI2S(soundSettings.samplingFrequency);
    }
    
    // Update palettes
    updatePalettesFromBMDevice();
    
    if (bmDeviceReset) {
        Serial.println("✅ [PREFERENCES] Factory defaults restored!");
    } else {
        Serial.println("⚠️ [PREFERENCES] Umbrella settings reset, but BMDevice factory reset failed");
    }
    Serial.println("🔍 [VERIFICATION] Running post-reset verification...");
    verifyAllSettings();
} 