#ifndef BM_DEVICE_STATE_H
#define BM_DEVICE_STATE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <LightShow.h>
#include <Position.h>

/**
 * Brightness crosses the wire as a percent (1-100) and lives internally as a
 * FastLED level (1-255), so it is converted on every write, every status
 * report, and every save.
 *
 * Both directions round rather than truncate. Two truncating conversions in a
 * row cost a point off almost every value: the app would send 50, the device
 * would store 127, and the next status burst would report 49 - so the slider
 * visibly snapped back a point after every adjustment. At the bottom it was
 * worse, with level 1 reporting as 0 and pushing the app's slider below its own
 * minimum.
 */
inline int brightnessPercentToLevel(int percent) {
    return (constrain(percent, 0, 100) * 255 + 50) / 100;
}

inline int brightnessLevelToPercent(int level) {
    return (constrain(level, 0, 255) * 100 + 127) / 255;
}

class BMDeviceState {
public:
    BMDeviceState();
    
    // Core device state
    bool power;
    int brightness;
    uint16_t speed;
    bool reverseStrip;
    
    // Effect state
    AvailablePalettes currentPalette;
    LightSceneID currentEffect;
    
    // Effect parameters
    int waveWidth;
    int meteorCount;
    int trailLength;
    int heatVariance;
    int mirrorCount;
    int cometCount;
    int dropRate;
    int cloudScale;
    int blobCount;
    int waveCount;
    int flashIntensity;
    int flashFrequency;
    int explosionSize;
    int spiralArms;
    CRGB effectColor;
    
    // Location state (optional)
    Position origin;
    Position currentPosition;
    bool positionAvailable;
    float currentSpeed;
    
    // GPS Speed control settings
    float gpsLowSpeed;
    float gpsTopSpeed;
    bool gpsLightshowSpeedEnabled;
    
    // GPS Speedometer colors
    CRGB gpsSlowColor;
    CRGB gpsFastColor;
    
    // State management
    String toJSON() const;
    void applyStateUpdate(const String& json);
    void reset();
    
    // Parameter validation and constraints
    void constrainBrightness(int min = 1, int max = 100);
    void constrainSpeed(int min = 5, int max = 200);
    void constrainEffectParameter(int& param, int min, int max);
    
private:
    void initializeDefaults();
};

#endif // BM_DEVICE_STATE_H 