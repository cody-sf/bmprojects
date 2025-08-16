#ifndef BM_BLUETOOTH_HANDLER_H
#define BM_BLUETOOTH_HANDLER_H

#include <Arduino.h>
#include <ArduinoBLE.h>
#include <functional>

// BLE Feature Constants (from your existing code)
#define BLE_FEATURE_POWER 0x01
#define BLE_FEATURE_BRIGHTNESS 0x04
#define BLE_FEATURE_SPEED 0x05
#define BLE_FEATURE_DIRECTION 0x06
#define BLE_FEATURE_ORIGIN 0x07
#define BLE_FEATURE_PALETTE 0x08
#define BLE_FEATURE_SPEEDOMETER 0x09
#define BLE_FEATURE_EFFECT 0x0A
#define BLE_FEATURE_WAVE_WIDTH 0x0B
#define BLE_FEATURE_METEOR_COUNT 0x0C
#define BLE_FEATURE_TRAIL_LENGTH 0x0D
#define BLE_FEATURE_HEAT_VARIANCE 0x0E
#define BLE_FEATURE_MIRROR_COUNT 0x0F
#define BLE_FEATURE_COMET_COUNT 0x10
#define BLE_FEATURE_DROP_RATE 0x11
#define BLE_FEATURE_CLOUD_SCALE 0x12
#define BLE_FEATURE_BLOB_COUNT 0x13
#define BLE_FEATURE_WAVE_COUNT 0x14
#define BLE_FEATURE_FLASH_INTENSITY 0x15
#define BLE_FEATURE_FLASH_FREQUENCY 0x16
#define BLE_FEATURE_EXPLOSION_SIZE 0x17
#define BLE_FEATURE_SPIRAL_ARMS 0x18
#define BLE_FEATURE_COLOR 0x19

// Defaults Management Features
#define BLE_FEATURE_GET_DEFAULTS 0x1A
#define BLE_FEATURE_SET_DEFAULTS 0x1B
#define BLE_FEATURE_SAVE_CURRENT_AS_DEFAULTS 0x1C
#define BLE_FEATURE_RESET_TO_FACTORY 0x1D
#define BLE_FEATURE_SET_MAX_BRIGHTNESS 0x1E
#define BLE_FEATURE_SET_DEVICE_OWNER 0x1F
#define BLE_FEATURE_SET_AUTO_ON 0x20

// GPS Speed Configuration Features
#define BLE_FEATURE_SET_GPS_LOW_SPEED 0x21
#define BLE_FEATURE_SET_GPS_TOP_SPEED 0x22
#define BLE_FEATURE_SET_GPS_LIGHTSHOW_SPEED_ENABLED 0x23

// Generic Device Configuration Features  
#define BLE_FEATURE_SET_OWNER 0x30
#define BLE_FEATURE_SET_DEVICE_TYPE 0x31
#define BLE_FEATURE_CONFIGURE_LED_STRIP 0x32
#define BLE_FEATURE_GET_CONFIGURATION 0x33
#define BLE_FEATURE_RESET_TO_DEFAULTS 0x34

class BMBluetoothHandler {
public:
    BMBluetoothHandler(const char* deviceName, const char* serviceUUID, 
                       const char* featuresUUID,
                       const char* statusUUID);
    
    // Initialization
    bool begin();
    void poll();
    
    // Callback registration
    void setFeatureCallback(std::function<void(uint8_t feature, const uint8_t* data, size_t length)> callback);
    void setConnectionCallback(std::function<void(bool connected)> callback);
    
    // Status updates
    void sendStatusUpdate(const String& status);
    
    // Device name management
    void setDeviceName(const char* deviceName);
    
    // Connection state
    bool isConnected() const { return deviceConnected_; }
    unsigned long getLastSyncTime() const { return lastBluetoothSync_; }
    void updateSyncTime() { lastBluetoothSync_ = millis(); }
    
private:
    // BLE objects
    BLEService* service_;
    BLECharacteristic* featuresCharacteristic_;
    BLECharacteristic* statusCharacteristic_;
    
    // Device info
    String deviceName_;
    String serviceUUID_;
    String featuresUUID_;
    String statusUUID_;
    
    // Connection state
    bool deviceConnected_;
    bool initialized_;
    unsigned long lastBluetoothSync_;
    
    // Callbacks
    std::function<void(uint8_t, const uint8_t*, size_t)> featureCallback_;
    std::function<void(bool)> connectionCallback_;
    
    // Static callbacks for BLE events
    static void onBLEConnected(BLEDevice central);
    static void onBLEDisconnected(BLEDevice central);
    static void onFeatureWritten(BLEDevice central, BLECharacteristic characteristic);
    
    // Static instance pointer for callbacks
    static BMBluetoothHandler* instance_;
    
    // Helper methods
    void processFeatureData(const uint8_t* buffer, size_t length);
};

#endif // BM_BLUETOOTH_HANDLER_H 