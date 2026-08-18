#include "BMBluetoothHandler.h"

// Static instance pointer
BMBluetoothHandler* BMBluetoothHandler::instance_ = nullptr;

BMBluetoothHandler::BMBluetoothHandler(const char* deviceName, const char* serviceUUID, 
                                       const char* featuresUUID, const char* statusUUID)
    : deviceName_(deviceName), serviceUUID_(serviceUUID), featuresUUID_(featuresUUID), 
      statusUUID_(statusUUID), deviceConnected_(false), connectedCount_(0),
      advertisingRestartPending_(false), initialized_(false), lastBluetoothSync_(0),
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
    
    initialized_ = true;
    
    Serial.print("[BMBluetoothHandler] BLE initialized for device: ");
    Serial.println(deviceName_);
    
    return true;
}

void BMBluetoothHandler::poll() {
    BLE.poll();

    // A connectable advertisement stops the instant a link comes up, and
    // ArduinoBLE never turns it back on - that, not any peer limit, is what
    // hides the device from a second central once the phone has hold of it.
    // Restart it here rather than in onBLEConnected: BLE.advertise() busy-waits
    // inside HCI.poll() for its command-complete, so issuing it from a BLE event
    // handler re-enters the packet parser mid-dispatch.
    if (advertisingRestartPending_) {
        advertisingRestartPending_ = false;
        if (initialized_ && connectedCount_ < BM_MAX_BLE_CENTRALS) {
            BLE.advertise();
        }
    }
}

void BMBluetoothHandler::setFeatureCallback(std::function<void(uint8_t, const uint8_t*, size_t)> callback) {
    featureCallback_ = callback;
}

void BMBluetoothHandler::setConnectionCallback(std::function<void(bool)> callback) {
    connectionCallback_ = callback;
}

void BMBluetoothHandler::setDeviceName(const char* deviceName) {
    deviceName_ = String(deviceName);
    
    // Only update BLE if it's already initialized
    if (initialized_) {
        // Update the BLE local name
        BLE.setLocalName(deviceName_.c_str());
        
        // Stop and restart advertising to update the name
        BLE.stopAdvertise();
        BLE.advertise();
    }
    
    Serial.print("[BMBluetoothHandler] Device name set to: ");
    Serial.println(deviceName_);
}

bool BMBluetoothHandler::isSubscribed() {
    return statusCharacteristic_ && statusCharacteristic_->subscribed();
}

void BMBluetoothHandler::sendStatusUpdate(const String& status) {
    // Skip the notify entirely when nobody is listening - a status burst is
    // several hundred bytes of JSON plus the serial trace behind it.
    if (deviceConnected_ && isSubscribed()) {
        BM_LOGV("[BMBluetoothHandler] Sending status update: %s\n", status.c_str());
        statusCharacteristic_->setValue(status.c_str());
    }
}

void BMBluetoothHandler::startAdvertising() {
    if (initialized_) {
        BLE.advertise();
        Serial.println("[BMBluetoothHandler] Advertising started");
    }
}

void BMBluetoothHandler::stopAdvertising() {
    if (initialized_) {
        BLE.stopAdvertise();
        Serial.println("[BMBluetoothHandler] Advertising stopped");
    }
}

void BMBluetoothHandler::onBLEConnected(BLEDevice central) {
    if (instance_) {
        Serial.print("[BMBluetoothHandler] Connected to central: ");
        Serial.println(central.address());
        if (instance_->connectedCount_ < BM_MAX_BLE_CENTRALS) {
            instance_->connectedCount_++;
        }
        instance_->deviceConnected_ = true;
        // Stay findable so the next central (watch alongside phone) can join.
        instance_->advertisingRestartPending_ = true;
        if (instance_->connectionCallback_) {
            instance_->connectionCallback_(true);
        }
    }
}

void BMBluetoothHandler::onBLEDisconnected(BLEDevice central) {
    if (instance_) {
        Serial.print("[BMBluetoothHandler] Disconnected from central: ");
        Serial.println(central.address());
        if (instance_->connectedCount_ > 0) {
            instance_->connectedCount_--;
        }
        // Only the last central leaving counts as a disconnect - the callback
        // tears down status state that any remaining central still needs.
        instance_->deviceConnected_ = (instance_->connectedCount_ > 0);
        // A slot just freed up, so advertise again even if we were at the cap.
        instance_->advertisingRestartPending_ = true;
        if (instance_->connectionCallback_) {
            instance_->connectionCallback_(instance_->deviceConnected_);
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
#if BM_VERBOSE_LOGS
    Serial.printf("[BMBluetoothHandler] Data length: %u\n", (unsigned)length);
    Serial.print("[BMBluetoothHandler] Raw buffer: ");
    for (size_t i = 0; i < length; ++i) {
        Serial.print("0x");
        Serial.print(buffer[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
#endif

    if (length > 0) {
        uint8_t feature = buffer[0];
        BM_LOGV("[BMBluetoothHandler] Received feature: 0x%02X\n", feature);

        if (featureCallback_) {
            featureCallback_(feature, buffer, length);
        }
    } else {
        Serial.println("[BMBluetoothHandler] No data received!");
    }
}