#include "BMBluetoothHandler.h"

// Static instance pointer
BMBluetoothHandler* BMBluetoothHandler::instance_ = nullptr;

BMBluetoothHandler::BMBluetoothHandler(const char* deviceName, const char* serviceUUID, 
                                       const char* featuresUUID, const char* statusUUID)
    : deviceName_(deviceName), serviceUUID_(serviceUUID), featuresUUID_(featuresUUID), 
      statusUUID_(statusUUID), deviceConnected_(false), lastBluetoothSync_(0),
      service_(nullptr), featuresCharacteristic_(nullptr), statusCharacteristic_(nullptr) {
    instance_ = this;
}

bool BMBluetoothHandler::begin() {
    if (!BLE.begin()) {
        Serial.println("[BMBluetoothHandler] Starting Bluetooth® Low Energy module failed!");
        return false;
    }
    
    // Create service and characteristics
    service_ = new BLEService(serviceUUID_.c_str());
    featuresCharacteristic_ = new BLECharacteristic(featuresUUID_.c_str(), BLERead | BLEWrite, 512);
    statusCharacteristic_ = new BLECharacteristic(statusUUID_.c_str(), BLERead | BLENotify, 512);
    
    // Configure BLE
    BLE.setLocalName(deviceName_.c_str());
    BLE.setAdvertisedService(*service_);
    service_->addCharacteristic(*featuresCharacteristic_);
    service_->addCharacteristic(*statusCharacteristic_);
    BLE.addService(*service_);
    
    // Set BLE event handlers
    BLE.setEventHandler(BLEConnected, onBLEConnected);
    BLE.setEventHandler(BLEDisconnected, onBLEDisconnected);
    featuresCharacteristic_->setEventHandler(BLEWritten, onFeatureWritten);
    
    // Start advertising
    BLE.advertise();
    
    Serial.print("[BMBluetoothHandler] BLE initialized for device: ");
    Serial.println(deviceName_);
    
    return true;
}

void BMBluetoothHandler::poll() {
    BLE.poll();
}

void BMBluetoothHandler::setFeatureCallback(std::function<void(uint8_t, const uint8_t*, size_t)> callback) {
    featureCallback_ = callback;
}

void BMBluetoothHandler::setConnectionCallback(std::function<void(bool)> callback) {
    connectionCallback_ = callback;
}

void BMBluetoothHandler::sendStatusUpdate(const String& status) {
    if (deviceConnected_ && statusCharacteristic_) {
        Serial.print("[BMBluetoothHandler] Sending status update: ");
        Serial.println(status);
        statusCharacteristic_->setValue(status.c_str());
    }
}

void BMBluetoothHandler::onBLEConnected(BLEDevice central) {
    if (instance_) {
        Serial.print("[BMBluetoothHandler] Connected to central: ");
        Serial.println(central.address());
        instance_->deviceConnected_ = true;
        if (instance_->connectionCallback_) {
            instance_->connectionCallback_(true);
        }
    }
}

void BMBluetoothHandler::onBLEDisconnected(BLEDevice central) {
    if (instance_) {
        Serial.print("[BMBluetoothHandler] Disconnected from central: ");
        Serial.println(central.address());
        instance_->deviceConnected_ = false;
        if (instance_->connectionCallback_) {
            instance_->connectionCallback_(false);
        }
    }
}

void BMBluetoothHandler::onFeatureWritten(BLEDevice central, BLECharacteristic characteristic) {
    if (instance_ && instance_->featuresCharacteristic_) {
        const uint8_t* buffer = instance_->featuresCharacteristic_->value();
        size_t length = instance_->featuresCharacteristic_->valueLength();
        instance_->processFeatureData(buffer, length);
    }
}

void BMBluetoothHandler::processFeatureData(const uint8_t* buffer, size_t length) {
    Serial.print("[BMBluetoothHandler] Data length: ");
    Serial.println(length);
    Serial.print("[BMBluetoothHandler] Raw buffer: ");
    for (size_t i = 0; i < length; ++i) {
        Serial.print("0x"); 
        Serial.print(buffer[i], HEX); 
        Serial.print(" ");
    }
    Serial.println();
    
    if (length > 0) {
        uint8_t feature = buffer[0];
        Serial.print("[BMBluetoothHandler] Received feature: 0x");
        Serial.println(feature, HEX);
        
        if (featureCallback_) {
            featureCallback_(feature, buffer, length);
        }
    } else {
        Serial.println("[BMBluetoothHandler] No data received!");
    }
} 