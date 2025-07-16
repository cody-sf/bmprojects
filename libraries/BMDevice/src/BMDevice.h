#ifndef BM_DEVICE_H
#define BM_DEVICE_H

#include <Arduino.h>
#include <FastLED.h>
#include <LightShow.h>
#include <LocationService.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <vector>
#include <functional>

#include "BMDeviceState.h"
#include "BMBluetoothHandler.h"
#include "BMDeviceDefaults.h"

#define DEFAULT_BT_REFRESH_INTERVAL 5000
#define DEFAULT_GPS_BAUD 9600

class BMDevice {
public:
    BMDevice(const char* deviceName, const char* serviceUUID, const char* featuresUUID, const char* statusUUID);
    BMDevice(const char* serviceUUID, const char* featuresUUID, const char* statusUUID); // Constructor for dynamic naming
    ~BMDevice();
    
    // LED Strip Management
    template<template<uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER>
    void addLEDStrip(CRGB* ledArray, int numLeds) {
        CLEDController& controller = FastLED.addLeds<CHIPSET, DATA_PIN, RGB_ORDER>(ledArray, numLeds);
        lightShow_.add_led_controller(&controller);
        Serial.print("[BMDevice] Added LED strip: ");
        Serial.print(numLeds);
        Serial.print(" LEDs on pin ");
        Serial.println(DATA_PIN);
    }
    
    // GPS Integration (optional)
    void enableGPS(int rxPin = 16, int txPin = 17, int baud = DEFAULT_GPS_BAUD);
    void setLocationService(LocationService* locationService);
    
    // Device lifecycle
    bool begin();
    void loop();
    
    // State access
    BMDeviceState& getState() { return deviceState_; }
    LightShow& getLightShow() { return lightShow_; }
    BMBluetoothHandler& getBluetoothHandler() { return bluetoothHandler_; }
    LocationService* getLocationService() { return locationService_; }
    BMDeviceDefaults& getDefaults() { return defaults_; }
    
    // Configuration
    void setStatusUpdateInterval(unsigned long interval) { statusUpdateInterval_ = interval; }
    void setBrightness(int brightness);
    void setEffect(LightSceneID effect);
    void setPalette(AvailablePalettes palette);
    
    // Defaults management
    bool loadDefaults();
    bool saveCurrentAsDefaults();
    bool resetToFactoryDefaults();
    void applyDefaults();
    void setMaxBrightness(int maxBrightness);
    void setDeviceOwner(const String& owner);
    
    // Callbacks for custom behavior
    void setCustomFeatureHandler(std::function<bool(uint8_t feature, const uint8_t* data, size_t length)> handler);
    void setCustomConnectionHandler(std::function<void(bool connected)> handler);
    
private:
    // Core components
    BMDeviceState deviceState_;
    BMBluetoothHandler bluetoothHandler_;
    LightShow lightShow_;
    Clock deviceClock_;
    BMDeviceDefaults defaults_;
    
    // GPS components (optional)
    bool gpsEnabled_;
    bool ownGPSSerial_;
    TinyGPSPlus* gps_;
    HardwareSerial* gpsSerial_;
    LocationService* locationService_;
    
    // Timing
    unsigned long lastBluetoothSync_;
    unsigned long statusUpdateInterval_;
    
    // Custom handlers
    std::function<bool(uint8_t, const uint8_t*, size_t)> customFeatureHandler_;
    std::function<void(bool)> customConnectionHandler_;
    
    // LED strip management
    CRGB* ledArrays_[MAX_LED_STRIPS];
    bool dynamicNaming_;
    
    // Internal methods
    void handleFeatureCommand(uint8_t feature, const uint8_t* buffer, size_t length);
    void handleConnectionChange(bool connected);
    void updateGPS();
    void updateLightShow();
    void sendStatusUpdate();
    
    // Feature handlers
    void handlePowerFeature(const uint8_t* buffer, size_t length);
    void handleBrightnessFeature(const uint8_t* buffer, size_t length);
    void handleSpeedFeature(const uint8_t* buffer, size_t length);
    void handleDirectionFeature(const uint8_t* buffer, size_t length);
    void handleOriginFeature(const uint8_t* buffer, size_t length);
    void handlePaletteFeature(const uint8_t* buffer, size_t length);
    void handleEffectFeature(const uint8_t* buffer, size_t length);
    void handleEffectParameterFeature(uint8_t feature, const uint8_t* buffer, size_t length);
    void handleColorFeature(const uint8_t* buffer, size_t length);
    
    // Defaults feature handlers
    void handleGetDefaultsFeature(const uint8_t* buffer, size_t length);
    void handleSetDefaultsFeature(const uint8_t* buffer, size_t length);
    void handleSaveCurrentAsDefaultsFeature(const uint8_t* buffer, size_t length);
    void handleResetToFactoryFeature(const uint8_t* buffer, size_t length);
    void handleSetMaxBrightnessFeature(const uint8_t* buffer, size_t length);
    void handleSetDeviceOwnerFeature(const uint8_t* buffer, size_t length);
    void handleSetAutoOnFeature(const uint8_t* buffer, size_t length);
    
    // Generic device configuration handlers
    void handleSetDeviceTypeFeature(const uint8_t* buffer, size_t length);
    void handleConfigureLEDStripFeature(const uint8_t* buffer, size_t length);
    void handleGetConfigurationFeature(const uint8_t* buffer, size_t length);
    void handleResetToDefaultsFeature(const uint8_t* buffer, size_t length);
    
    // LED strip management
    void addLEDStripByPin(int pin, CRGB* ledArray, int numLeds, int colorOrder);
    void initializeLEDStrips();
};

#endif // BM_DEVICE_H 