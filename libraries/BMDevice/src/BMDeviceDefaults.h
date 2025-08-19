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
    bool initialized_;
    
    // Helper methods
    void constrainValues(DeviceDefaults& defaults);
    bool writeString(const char* key, const String& value);
    String readString(const char* key, const String& defaultValue = "");
};

#endif // BM_DEVICE_DEFAULTS_H 