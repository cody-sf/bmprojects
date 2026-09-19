#ifndef BM_DEVICE_DEFAULTS_H
#define BM_DEVICE_DEFAULTS_H

#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <LightShow.h>

#define MAX_LED_STRIPS 8

struct LEDStripConfig {
    int pin;
    int numLeds;
    int colorOrder; // 0=GRB, 1=RGB, 2=BRG, 3=BGR, 4=RBG, 5=GBR
    bool enabled;
    
    LEDStripConfig() {
        pin = 2;
        numLeds = 30;
        colorOrder = 0; // GRB
        enabled = false;
    }
};

#define DEFAULTS_NAMESPACE "bmdefaults"
#define DEFAULTS_VERSION 1

/// Longest name a person can give a custom palette.
#define CUSTOM_PALETTE_NAME_MAX 16

/// One of the device's custom palette slots: a name plus the colours the phone
/// sampled off the gradient, one per CRGBPalette16 entry.
struct CustomPalette {
    bool used;
    char name[CUSTOM_PALETTE_NAME_MAX + 1];
    uint8_t rgb[CUSTOM_PALETTE_ENTRIES * 3];

    CustomPalette() {
        clear();
    }

    void clear() {
        used = false;
        name[0] = '\0';
        memset(rgb, 0, sizeof(rgb));
    }
};

/// What one slot occupies in NVRAM: the name (fixed width, always terminated)
/// followed by the colours.
#define CUSTOM_PALETTE_BLOB_SIZE (CUSTOM_PALETTE_NAME_MAX + 1 + CUSTOM_PALETTE_ENTRIES * 3)

// Default setting keys
#define PREF_BRIGHTNESS "brightness"
#define PREF_MAX_BRIGHTNESS "maxBrightness"
#define PREF_SPEED "speed"
#define PREF_PALETTE "palette"
#define PREF_EFFECT "effect"
#define PREF_DIRECTION "direction"
#define PREF_OWNER "owner"
#define PREF_DEVICE_NAME "deviceName"
#define PREF_AUTO_ON "autoOn"
#define PREF_STATUS_INTERVAL "statusInterval"
#define PREF_EFFECT_COLOR_R "effectColorR"
#define PREF_EFFECT_COLOR_G "effectColorG"
#define PREF_EFFECT_COLOR_B "effectColorB"
#define PREF_GPS_ENABLED "gpsEnabled"
#define PREF_VERSION "version"
#define PREF_DEVICE_TYPE "deviceType"
#define PREF_LED_COUNT "ledCount"
#define PREF_LED_PINS "ledPins"
#define PREF_COLOR_ORDERS "colorOrders"
#define PREF_GPS_LOW_SPEED "gpsLowSpeed"
#define PREF_GPS_TOP_SPEED "gpsTopSpeed"
#define PREF_GPS_LIGHTSHOW_SPEED_ENABLED "gpsLightshowSpeedEnabled"
#define PREF_SYNC_ENABLED "syncEnabled"
// One key per custom palette slot: "cpal0" ... "cpal3".
#define PREF_CUSTOM_PALETTE_PREFIX "cpal"
#define PREF_MARQUEE_TEXT "marquee"
#define PREF_MATRIX_BITMAP "matrixBmp"
#define PREF_TEXT_STYLE "textStyle"
#define PREF_TEXT_FILL "textFill"
#define PREF_MATRIX_DISPLAY "matrixDisp"
#define PREF_MATRIX_SPEED "matrixMs"
// Animation frame blobs live under "animF0".."animF7".
#define PREF_ANIM_FRAME_PREFIX "animF"

struct DeviceDefaults {
    // Core settings
    int brightness;
    int maxBrightness;
    int speed;
    AvailablePalettes palette;
    LightSceneID effect;
    bool reverseDirection;
    
    // Device identity
    String owner;
    String deviceName;
    String deviceType;
    
    // LED configuration
    LEDStripConfig ledStrips[MAX_LED_STRIPS];
    int activeLEDStrips;
    
    // Behavior settings
    bool autoOn;
    unsigned long statusUpdateInterval;
    CRGB effectColor;
    bool gpsEnabled;
    
    // GPS Speed settings
    float gpsLowSpeed;  // km/h - speed for maximum lightshow delay
    float gpsTopSpeed;  // km/h - speed for minimum lightshow delay
    bool gpsLightshowSpeedEnabled;
    
    // Sync settings
    bool syncEnabled;
    
    // Version for migration
    int version;
    
    DeviceDefaults() {
        setFactoryDefaults();
    }
    
    void setFactoryDefaults() {
        brightness = 50;
        maxBrightness = 100;
        speed = 100;
        palette = AvailablePalettes::cool;
        effect = LightSceneID::palette_stream;
        reverseDirection = true;
        owner = "New";
        deviceName = "BMDevice";
        deviceType = "Generic";
        autoOn = true;
        
        // Initialize LED strips
        activeLEDStrips = 1;
        for (int i = 0; i < MAX_LED_STRIPS; i++) {
            ledStrips[i] = LEDStripConfig();
            ledStrips[i].pin = 2 + i;
            ledStrips[i].enabled = (i == 0); // Only first strip enabled by default
        }
        statusUpdateInterval = 5000;
        effectColor = CRGB::Green;
        gpsEnabled = false;
        
        // GPS Speed defaults
        gpsLowSpeed = 5.0;   // 5 km/h - walking speed for max delay
        gpsTopSpeed = 25.0;  // 25 km/h - biking speed for min delay
        gpsLightshowSpeedEnabled = false;
        
        // Sync defaults
        syncEnabled = true;    // Enable sync by default
        
        version = DEFAULTS_VERSION;
    }
};

class BMDeviceDefaults {
public:
    BMDeviceDefaults();
    ~BMDeviceDefaults();
    
    // Lifecycle
    bool begin();
    void end();
    
    // Load/Save operations
    bool loadDefaults(DeviceDefaults& defaults);
    bool saveDefaults(const DeviceDefaults& defaults);
    bool resetToFactory();
    
    // Individual setting operations
    bool setBrightness(int brightness);
    bool setMaxBrightness(int maxBrightness);
    bool setSpeed(int speed);
    bool setPalette(AvailablePalettes palette);
    bool setEffect(LightSceneID effect);
    bool setDirection(bool reverse);
    bool setOwner(const String& owner);
    bool setDeviceName(const String& name);
    bool setDeviceType(const String& deviceType);
    bool setAutoOn(bool autoOn);
    
    // LED strip configuration
    bool setLEDStripConfig(int stripIndex, int pin, int numLeds, int colorOrder, bool enabled);
    
    // Per-strip LED grouping, keyed by the order strips were *registered with
    // the show* (sketch-added and NVRAM-configured alike) - not by the config
    // rows above, which static targets like the bike never populate. 1 =
    // normal density, 6 = a 12 V glow strip ganging six LEDs per pixel.
    int getStripGroupSize(int stripIndex);
    bool setStripGroupSize(int stripIndex, int groupSize);
    // Per-strip brightness ceiling, keyed the same way. 255 = uncapped; the
    // show scales that strand's output by max/255 of the master brightness.
    int getStripMaxBrightness(int stripIndex);
    bool setStripMaxBrightness(int stripIndex, int maxBrightness);
    bool setActiveLEDStrips(int count);
    LEDStripConfig getLEDStripConfig(int stripIndex);
    int getActiveLEDStrips();
    bool setStatusInterval(unsigned long interval);
    bool setEffectColor(CRGB color);
    bool setGPSEnabled(bool enabled);
    
    // GPS Speed configuration
    bool setGPSLowSpeed(float speed);
    bool setGPSTopSpeed(float speed);
    bool setGPSLightshowSpeedEnabled(bool enabled);
    float getGPSLowSpeed();
    float getGPSTopSpeed();
    bool isGPSLightshowSpeedEnabled();
    
    // Sync configuration
    bool setSyncEnabled(bool enabled);
    bool isSyncEnabled();
    
    // Custom palettes. Kept off DeviceDefaults deliberately: that struct is
    // copied by value on every status chunk, and four palettes would add a
    // quarter-kilobyte to each of those copies. `rgb` is CUSTOM_PALETTE_ENTRIES
    // RGB triplets. Both setters write through to NVRAM immediately.
    bool setCustomPalette(int slot, const String& name, const uint8_t* rgb);
    bool clearCustomPalette(int slot);
    /// Null for an out-of-range slot; check `used` for an empty one.
    const CustomPalette* getCustomPalette(int slot) const;

    // Matrix display content, off DeviceDefaults for the same copied-by-value
    // reason as the palettes. The bitmap blob is opaque here:
    // [w][h][16*RGB][packed 4bpp pixels], exactly the BLE payload after the
    // feature byte. Setters write through to NVRAM immediately.
    bool setMarqueeText(const String& text);
    String getMarqueeText() const { return marqueeText_; }
    bool setTextStyle(uint8_t style);
    uint8_t getTextStyle() const { return textStyle_; }
    bool setTextFill(uint8_t fill);
    uint8_t getTextFill() const { return textFill_; }
    // What the display overlay shows (MATRIX_DISPLAY_*) and its own pace in
    // ms - the effect speed knob no longer touches the display.
    bool setMatrixDisplay(uint8_t mode);
    uint8_t getMatrixDisplay() const { return matrixDisplay_; }
    bool setMatrixSpeed(uint16_t ms);
    uint16_t getMatrixSpeed() const { return matrixSpeedMs_; }
    bool setMatrixBitmap(const uint8_t* blob, size_t length);
    bool clearMatrixBitmap();
    /// Bytes copied into `blob` (up to maxLength), 0 when nothing is stored.
    size_t getMatrixBitmap(uint8_t* blob, size_t maxLength) const;

    // Animation frames, one blob per slot in the same opaque bitmap format.
    bool setAnimFrame(uint8_t index, const uint8_t* blob, size_t length);
    bool clearAnimFrames();
    /// Bytes copied into `blob` for slot `index`, 0 when the slot is empty.
    size_t getAnimFrame(uint8_t index, uint8_t* blob, size_t maxLength) const;
    
    // Get current defaults
    DeviceDefaults getCurrentDefaults();
    
    // JSON operations
    String defaultsToJSON() const;
    bool defaultsFromJSON(const String& json);
    
    // Validation
    bool validateDefaults(const DeviceDefaults& defaults);
    
    // Migration
    bool migrateIfNeeded();
    
    // Status
    bool hasValidDefaults();
    void printCurrentDefaults();
    
private:
    Preferences preferences_;
    DeviceDefaults currentDefaults_;
    CustomPalette customPalettes_[CUSTOM_PALETTE_COUNT];
    String marqueeText_;
    uint8_t textStyle_ = 0;
    uint8_t textFill_ = 0;
    uint8_t matrixDisplay_ = 0;
    uint16_t matrixSpeedMs_ = 100;
    bool initialized_;
    
    // Helper methods
    void loadCustomPalettes();
    void constrainValues(DeviceDefaults& defaults);
    bool writeString(const char* key, const String& value);
    String readString(const char* key, const String& defaultValue = "");
};

#endif // BM_DEVICE_DEFAULTS_H 