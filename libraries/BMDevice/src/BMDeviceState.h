#ifndef BM_DEVICE_STATE_H
#define BM_DEVICE_STATE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FastLED.h>
#include <LightShow.h>
#include <Position.h>

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