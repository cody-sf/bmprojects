#include "BMDevice.h"

BMDevice::BMDevice(const char* deviceName, const char* serviceUUID, const char* featuresUUID, const char* statusUUID)
    : bluetoothHandler_(deviceName, serviceUUID, featuresUUID, statusUUID), lightShow_(std::vector<CLEDController*>(), deviceClock_),
      gpsEnabled_(false), ownGPSSerial_(false), gps_(nullptr), gpsSerial_(nullptr), 
      locationService_(nullptr), lastBluetoothSync_(0), statusUpdateInterval_(DEFAULT_BT_REFRESH_INTERVAL) {
    
    // Set up callbacks
    bluetoothHandler_.setFeatureCallback([this](uint8_t feature, const uint8_t* data, size_t length) {
        this->handleFeatureCommand(feature, data, length);
    });
    
    bluetoothHandler_.setConnectionCallback([this](bool connected) {
        this->handleConnectionChange(connected);
    });
}

BMDevice::BMDevice(const char* serviceUUID, const char* featuresUUID, const char* statusUUID)
    : bluetoothHandler_("", serviceUUID, featuresUUID, statusUUID), lightShow_(std::vector<CLEDController*>(), deviceClock_),
      gpsEnabled_(false), ownGPSSerial_(false), gps_(nullptr), gpsSerial_(nullptr), 
      locationService_(nullptr), lastBluetoothSync_(0), statusUpdateInterval_(DEFAULT_BT_REFRESH_INTERVAL),
      dynamicNaming_(true) {
    
    // Initialize LED arrays
    for (int i = 0; i < MAX_LED_STRIPS; i++) {
        ledArrays_[i] = nullptr;
    }
    
    // Set up callbacks
    bluetoothHandler_.setFeatureCallback([this](uint8_t feature, const uint8_t* data, size_t length) {
        this->handleFeatureCommand(feature, data, length);
    });
    
    bluetoothHandler_.setConnectionCallback([this](bool connected) {
        this->handleConnectionChange(connected);
    });
}

BMDevice::~BMDevice() {
    if (ownGPSSerial_ && gpsSerial_) {
        delete gpsSerial_;
    }
    if (gps_) {
        delete gps_;
    }
    
    // Clean up LED arrays
    for (int i = 0; i < MAX_LED_STRIPS; i++) {
        if (ledArrays_[i]) {
            delete[] ledArrays_[i];
            ledArrays_[i] = nullptr;
        }
    }
}

void BMDevice::enableGPS(int rxPin, int txPin, int baud) {
    gpsEnabled_ = true;
    ownGPSSerial_ = true;
    
    gps_ = new TinyGPSPlus();
    gpsSerial_ = new HardwareSerial(2);
    gpsSerial_->begin(baud, SERIAL_8N1, rxPin, txPin);
    
    Serial.print("[BMDevice] GPS enabled on pins RX:");
    Serial.print(rxPin);
    Serial.print(" TX:");
    Serial.print(txPin);
    Serial.print(" @ ");
    Serial.print(baud);
    Serial.println(" baud");
}

void BMDevice::setLocationService(LocationService* locationService) {
    locationService_ = locationService;
    gpsEnabled_ = true;
    ownGPSSerial_ = false;
    
    Serial.println("[BMDevice] Using external LocationService for GPS");
}

bool BMDevice::begin() {
    Serial.begin(115200);
    delay(2000);
    
    // Initialize defaults system
    if (!defaults_.begin()) {
        Serial.println("[BMDevice] Failed to initialize defaults!");
        return false;
    }
    
    // Load and apply defaults
    if (loadDefaults()) {
        Serial.println("[BMDevice] Loaded and applied defaults");
    } else {
        Serial.println("[BMDevice] Using factory defaults");
    }
    
    // Handle dynamic naming
    if (dynamicNaming_) {
        DeviceDefaults currentDefaults = defaults_.getCurrentDefaults();
        String deviceName;
        if (currentDefaults.owner.length() > 0) {
            deviceName = "BMDevice - " + currentDefaults.owner;
        } else {
            deviceName = "BMDevice - New";
        }
        
        Serial.print("[BMDevice] Dynamic device name: ");
        Serial.println(deviceName);
        
        // Update the Bluetooth handler with the new name
        bluetoothHandler_.setDeviceName(deviceName.c_str());
    }
    
    // Initialize LED strips from configuration (if using dynamic constructor)
    if (dynamicNaming_) {
        initializeLEDStrips();
    }
    
    // Initialize Bluetooth
    if (!bluetoothHandler_.begin()) {
        return false;
    }
    
    // Set initial brightness (may be overridden by defaults)
    lightShow_.brightness(deviceState_.brightness * 2.55);
    
    // Update light show with initial state
    updateLightShow();
    
    Serial.println("[BMDevice] Setup complete");
    return true;
}

void BMDevice::loop() {
    bluetoothHandler_.poll();
    
    // Update GPS if enabled
    if (gpsEnabled_) {
        updateGPS();
    }
    
    // Send status updates
    if ((millis() - lastBluetoothSync_) > statusUpdateInterval_ && bluetoothHandler_.isConnected()) {
        sendStatusUpdate();
        lastBluetoothSync_ = millis();
    }
    
    // Handle power state
    if (!deviceState_.power) {
        FastLED.clear();
        FastLED.show();
        return;
    }
    
    // Render light show
    lightShow_.render();
}

void BMDevice::setBrightness(int brightness) {
    deviceState_.brightness = constrain(brightness, 1, 255);
    lightShow_.brightness(deviceState_.brightness);
}

void BMDevice::setEffect(LightSceneID effect) {
    deviceState_.currentEffect = effect;
    updateLightShow();
}

void BMDevice::setPalette(AvailablePalettes palette) {
    deviceState_.currentPalette = palette;
    updateLightShow();
}

void BMDevice::setCustomFeatureHandler(std::function<bool(uint8_t, const uint8_t*, size_t)> handler) {
    customFeatureHandler_ = handler;
}

void BMDevice::setCustomConnectionHandler(std::function<void(bool)> handler) {
    customConnectionHandler_ = handler;
}

void BMDevice::handleFeatureCommand(uint8_t feature, const uint8_t* buffer, size_t length) {
    // Allow custom handler to override
    if (customFeatureHandler_ && customFeatureHandler_(feature, buffer, length)) {
        return;
    }
    
    // Handle standard features
    switch (feature) {
        case BLE_FEATURE_POWER:
            handlePowerFeature(buffer, length);
            break;
        case BLE_FEATURE_BRIGHTNESS:
            handleBrightnessFeature(buffer, length);
            break;
        case BLE_FEATURE_SPEED:
            handleSpeedFeature(buffer, length);
            break;
        case BLE_FEATURE_DIRECTION:
            handleDirectionFeature(buffer, length);
            break;
        case BLE_FEATURE_ORIGIN:
            handleOriginFeature(buffer, length);
            break;
        case BLE_FEATURE_PALETTE:
            handlePaletteFeature(buffer, length);
            break;
        case BLE_FEATURE_EFFECT:
            handleEffectFeature(buffer, length);
            break;
        case BLE_FEATURE_COLOR:
            handleColorFeature(buffer, length);
            break;
        case BLE_FEATURE_WAVE_WIDTH:
        case BLE_FEATURE_METEOR_COUNT:
        case BLE_FEATURE_TRAIL_LENGTH:
        case BLE_FEATURE_HEAT_VARIANCE:
        case BLE_FEATURE_MIRROR_COUNT:
        case BLE_FEATURE_COMET_COUNT:
        case BLE_FEATURE_DROP_RATE:
        case BLE_FEATURE_CLOUD_SCALE:
        case BLE_FEATURE_BLOB_COUNT:
        case BLE_FEATURE_WAVE_COUNT:
        case BLE_FEATURE_FLASH_INTENSITY:
        case BLE_FEATURE_FLASH_FREQUENCY:
        case BLE_FEATURE_EXPLOSION_SIZE:
        case BLE_FEATURE_SPIRAL_ARMS:
            handleEffectParameterFeature(feature, buffer, length);
            break;
        
        // Defaults Management Features
        case BLE_FEATURE_GET_DEFAULTS:
            handleGetDefaultsFeature(buffer, length);
            break;
        case BLE_FEATURE_SET_DEFAULTS:
            handleSetDefaultsFeature(buffer, length);
            break;
        case BLE_FEATURE_SAVE_CURRENT_AS_DEFAULTS:
            handleSaveCurrentAsDefaultsFeature(buffer, length);
            break;
        case BLE_FEATURE_RESET_TO_FACTORY:
            handleResetToFactoryFeature(buffer, length);
            break;
        case BLE_FEATURE_SET_MAX_BRIGHTNESS:
            handleSetMaxBrightnessFeature(buffer, length);
            break;
        case BLE_FEATURE_SET_DEVICE_OWNER:
            handleSetDeviceOwnerFeature(buffer, length);
            break;
        case BLE_FEATURE_SET_AUTO_ON:
            handleSetAutoOnFeature(buffer, length);
            break;
        
        // Generic device configuration commands
        case BLE_FEATURE_SET_OWNER:
            handleSetDeviceOwnerFeature(buffer, length);
            break;
        case BLE_FEATURE_SET_DEVICE_TYPE:
            handleSetDeviceTypeFeature(buffer, length);
            break;
        case BLE_FEATURE_CONFIGURE_LED_STRIP:
            handleConfigureLEDStripFeature(buffer, length);
            break;
        case BLE_FEATURE_GET_CONFIGURATION:
            handleGetConfigurationFeature(buffer, length);
            break;
        case BLE_FEATURE_RESET_TO_DEFAULTS:
            handleResetToDefaultsFeature(buffer, length);
            break;
            
        default:
            Serial.print("[BMDevice] Unknown feature: 0x");
            Serial.println(feature, HEX);
            break;
    }
}

void BMDevice::handleConnectionChange(bool connected) {
    if (connected) {
        sendStatusUpdate();
    }
    
    if (customConnectionHandler_) {
        customConnectionHandler_(connected);
    }
}

void BMDevice::updateGPS() {
    if (locationService_) {
        // Use external LocationService
        locationService_->update_position();
        if (locationService_->is_current_position_available()) {
            deviceState_.currentPosition = locationService_->current_position();
            deviceState_.positionAvailable = true;
            deviceState_.currentSpeed = locationService_->current_speed();
        }
    } else if (gpsSerial_ && gps_) {
        // Use internal GPS
        while (gpsSerial_->available() > 0) {
            if (gps_->encode(gpsSerial_->read())) {
                if (gps_->location.isValid()) {
                    deviceState_.currentPosition = Position(gps_->location.lat(), gps_->location.lng());
                    deviceState_.positionAvailable = true;
                    
                    if (gps_->speed.isValid()) {
                        deviceState_.currentSpeed = gps_->speed.mph();
                    }
                }
            }
        }
    }
}

void BMDevice::updateLightShow() {
    // Map LightSceneID to LightShow effect
    switch (deviceState_.currentEffect) {
        case LightSceneID::palette_stream:
            lightShow_.palette_stream(deviceState_.speed, deviceState_.currentPalette, deviceState_.reverseStrip);
            break;
        case LightSceneID::pulse_wave:
            lightShow_.pulse_wave(deviceState_.speed, deviceState_.waveWidth, deviceState_.currentPalette);
            break;
        case LightSceneID::meteor_shower:
            lightShow_.meteor_shower(deviceState_.speed, deviceState_.meteorCount, deviceState_.trailLength, deviceState_.currentPalette);
            break;
        case LightSceneID::fire_plasma:
            lightShow_.fire_plasma(deviceState_.speed, deviceState_.heatVariance, deviceState_.currentPalette);
            break;
        case LightSceneID::kaleidoscope:
            lightShow_.kaleidoscope(deviceState_.speed, deviceState_.mirrorCount, deviceState_.currentPalette);
            break;
        case LightSceneID::rainbow_comet:
            lightShow_.rainbow_comet(deviceState_.speed, deviceState_.cometCount, deviceState_.trailLength);
            break;
        case LightSceneID::matrix_rain:
            lightShow_.matrix_rain(deviceState_.speed, deviceState_.dropRate, deviceState_.effectColor);
            break;
        case LightSceneID::plasma_clouds:
            lightShow_.plasma_clouds(deviceState_.speed, deviceState_.cloudScale, deviceState_.currentPalette);
            break;
        case LightSceneID::lava_lamp:
            lightShow_.lava_lamp(deviceState_.speed, deviceState_.blobCount, deviceState_.currentPalette);
            break;
        case LightSceneID::aurora_borealis:
            lightShow_.aurora_borealis(deviceState_.speed, deviceState_.waveCount, deviceState_.currentPalette);
            break;
        case LightSceneID::lightning_storm:
            lightShow_.lightning_storm(deviceState_.speed, deviceState_.flashIntensity, deviceState_.flashFrequency);
            break;
        case LightSceneID::color_explosion:
            lightShow_.color_explosion(deviceState_.speed, deviceState_.explosionSize, deviceState_.currentPalette);
            break;
        case LightSceneID::spiral_galaxy:
            lightShow_.spiral_galaxy(deviceState_.speed, deviceState_.spiralArms, deviceState_.currentPalette);
            break;
        default:
            lightShow_.palette_stream(deviceState_.speed, deviceState_.currentPalette, deviceState_.reverseStrip);
            break;
    }
}

void BMDevice::sendStatusUpdate() {
    // Get current defaults for additional status info
    DeviceDefaults defaults = defaults_.getCurrentDefaults();
    
    // Start with the basic device state JSON
    StaticJsonDocument<768> doc;
    
    // Basic device state
    doc["pwr"] = deviceState_.power;
    doc["bri"] = deviceState_.brightness;
    doc["spd"] = deviceState_.speed;
    doc["dir"] = deviceState_.reverseStrip;
    
    const char* effectName = LightShow::effectIdToName(deviceState_.currentEffect);
    Serial.print("[BMDevice] sendStatusUpdate: Current effect ID: ");
    Serial.print((uint8_t)deviceState_.currentEffect);
    Serial.print(" (");
    Serial.print(effectName);
    Serial.println(")");
    
    doc["fx"] = effectName;
    doc["pal"] = LightShow::paletteIdToName(deviceState_.currentPalette);
    
    // GPS/Position data
    doc["posAvail"] = deviceState_.positionAvailable;
    doc["spdCur"] = deviceState_.currentSpeed;
    
    if (deviceState_.positionAvailable) {
        Position& currentPos = const_cast<Position&>(deviceState_.currentPosition);
        JsonObject posObj = doc.createNestedObject("pos");
        posObj["lat"] = currentPos.latitude();
        posObj["lon"] = currentPos.longitude();
    }
    
    // Add defaults information
    doc["maxBri"] = defaults.maxBrightness;
    doc["owner"] = defaults.owner;
    doc["deviceName"] = defaults.deviceName;
    
    String status;
    serializeJson(doc, status);
    Serial.print("[BMDevice] sendStatusUpdate: Sending status: ");
    Serial.println(status);
    bluetoothHandler_.sendStatusUpdate(status);
}

// Feature handler implementations
void BMDevice::handlePowerFeature(const uint8_t* buffer, size_t length) {
    if (length >= 2) {
        deviceState_.power = buffer[1] != 0;
        Serial.print("[BMDevice] Power set to: ");
        Serial.println(deviceState_.power ? "On" : "Off");
    }
}

void BMDevice::handleBrightnessFeature(const uint8_t* buffer, size_t length) {
    if (length >= 5) {
        int b = 0;
        memcpy(&b, buffer + 1, sizeof(int));
        setBrightness(b);
        Serial.print("[BMDevice] Brightness set to: ");
        Serial.println(deviceState_.brightness);
    }
}

void BMDevice::handleSpeedFeature(const uint8_t* buffer, size_t length) {
    if (length >= 5) {
        int s = 0;
        memcpy(&s, buffer + 1, sizeof(int));
        deviceState_.speed = constrain(s, 5, 200);
        Serial.print("[BMDevice] Speed set to: ");
        Serial.println(deviceState_.speed);
        updateLightShow();
    }
}

void BMDevice::handleDirectionFeature(const uint8_t* buffer, size_t length) {
    if (length >= 2) {
        deviceState_.reverseStrip = buffer[1] != 0;
        Serial.print("[BMDevice] Direction set to: ");
        Serial.println(deviceState_.reverseStrip ? "Up" : "Down");
        updateLightShow();
    }
}

void BMDevice::handleOriginFeature(const uint8_t* buffer, size_t length) {
    if (length == 9) {
        float latitude, longitude;
        memcpy(&latitude, buffer + 1, sizeof(float));
        memcpy(&longitude, buffer + 5, sizeof(float));
        deviceState_.origin = Position(latitude, longitude);
        Serial.print("[BMDevice] Origin set to: ");
        Serial.print(latitude, 6);
        Serial.print(", ");
        Serial.println(longitude, 6);
    }
}

void BMDevice::handlePaletteFeature(const uint8_t* buffer, size_t length) {
    if (length > 1) {
        if (length == 2) { // ID
            uint8_t paletteId = buffer[1];
            if (paletteId <= (uint8_t)AvailablePalettes::moltenmetal) {
                setPalette((AvailablePalettes)paletteId);
                Serial.print("[BMDevice] Palette set to ID: ");
                Serial.println(paletteId);
            }
        } else { // String
            char paletteStr[32] = {0};
            memcpy(paletteStr, buffer + 1, min(length - 1, sizeof(paletteStr) - 1));
            AvailablePalettes palette = LightShow::paletteNameToId(paletteStr);
            setPalette(palette);
            Serial.print("[BMDevice] Palette set to: ");
            Serial.println(paletteStr);
        }
    }
}

void BMDevice::handleEffectFeature(const uint8_t* buffer, size_t length) {
    if (length > 1) {
        if (length == 2) { // ID
            uint8_t effectId = buffer[1];
            if (effectId <= (uint8_t)LightSceneID::spiral_galaxy) {
                Serial.print("[BMDevice] handleEffectFeature: Received effect ID: ");
                Serial.println(effectId);
                setEffect((LightSceneID)effectId);
                Serial.print("[BMDevice] Effect set to ID: ");
                Serial.println(effectId);
            }
        } else { // String
            char effectStr[32] = {0};
            memcpy(effectStr, buffer + 1, min(length - 1, sizeof(effectStr) - 1));
            Serial.print("[BMDevice] handleEffectFeature: Received effect string: '");
            Serial.print(effectStr);
            Serial.println("'");
            
            LightSceneID effect = LightShow::effectNameToId(effectStr);
            Serial.print("[BMDevice] handleEffectFeature: Converted to effect ID: ");
            Serial.print((uint8_t)effect);
            Serial.print(" (");
            Serial.print(LightShow::effectIdToName(effect));
            Serial.println(")");
            
            setEffect(effect);
            Serial.print("[BMDevice] Effect set to: ");
            Serial.println(effectStr);
        }
    }
}

void BMDevice::handleEffectParameterFeature(uint8_t feature, const uint8_t* buffer, size_t length) {
    if (length >= 5) {
        int value = 0;
        memcpy(&value, buffer + 1, sizeof(int));
        
        switch (feature) {
            case BLE_FEATURE_WAVE_WIDTH:
                deviceState_.waveWidth = constrain(value, 1, 50);
                Serial.print("[BMDevice] Wave width set to: ");
                Serial.println(deviceState_.waveWidth);
                break;
            case BLE_FEATURE_METEOR_COUNT:
                deviceState_.meteorCount = constrain(value, 1, 20);
                Serial.print("[BMDevice] Meteor count set to: ");
                Serial.println(deviceState_.meteorCount);
                break;
            case BLE_FEATURE_TRAIL_LENGTH:
                deviceState_.trailLength = constrain(value, 1, 30);
                Serial.print("[BMDevice] Trail length set to: ");
                Serial.println(deviceState_.trailLength);
                break;
            // Add more cases as needed...
        }
        updateLightShow();
    }
}

void BMDevice::handleColorFeature(const uint8_t* buffer, size_t length) {
    if (length >= 4) {
        uint8_t r = buffer[1], g = buffer[2], b = buffer[3];
        deviceState_.effectColor = CRGB(r, g, b);
        Serial.print("[BMDevice] Effect color set to RGB(");
        Serial.print(r); Serial.print(","); Serial.print(g); Serial.print(","); Serial.print(b);
        Serial.println(")");
        updateLightShow();
    }
}

// Defaults Management Methods
bool BMDevice::loadDefaults() {
    DeviceDefaults defaults = defaults_.getCurrentDefaults();
    applyDefaults();
    return true;
}

bool BMDevice::saveCurrentAsDefaults() {
    DeviceDefaults newDefaults;
    
    // Copy current state to defaults
    newDefaults.brightness = deviceState_.brightness;
    newDefaults.speed = deviceState_.speed;
    newDefaults.palette = deviceState_.currentPalette;
    newDefaults.effect = deviceState_.currentEffect;
    newDefaults.reverseDirection = deviceState_.reverseStrip;
    newDefaults.effectColor = deviceState_.effectColor;
    
    // Keep existing identity and behavior settings
    DeviceDefaults currentDefaults = defaults_.getCurrentDefaults();
    newDefaults.maxBrightness = currentDefaults.maxBrightness;
    newDefaults.owner = currentDefaults.owner;
    newDefaults.deviceName = currentDefaults.deviceName;
    newDefaults.autoOn = currentDefaults.autoOn;
    newDefaults.statusUpdateInterval = currentDefaults.statusUpdateInterval;
    newDefaults.gpsEnabled = currentDefaults.gpsEnabled;
    newDefaults.version = currentDefaults.version;
    
    bool success = defaults_.saveDefaults(newDefaults);
    if (success) {
        Serial.println("[BMDevice] Current state saved as defaults");
    } else {
        Serial.println("[BMDevice] Failed to save current state as defaults");
    }
    
    return success;
}

bool BMDevice::resetToFactoryDefaults() {
    bool success = defaults_.resetToFactory();
    if (success) {
        applyDefaults();
        Serial.println("[BMDevice] Reset to factory defaults and applied");
    } else {
        Serial.println("[BMDevice] Failed to reset to factory defaults");
    }
    return success;
}

void BMDevice::applyDefaults() {
    DeviceDefaults defaults = defaults_.getCurrentDefaults();
    
    // Apply defaults to current state
    setBrightness(defaults.brightness);
    setEffect(defaults.effect);
    setPalette(defaults.palette);
    deviceState_.speed = defaults.speed;
    deviceState_.reverseStrip = defaults.reverseDirection;
    deviceState_.effectColor = defaults.effectColor;
    deviceState_.power = defaults.autoOn;
    
    // Apply status update interval
    statusUpdateInterval_ = defaults.statusUpdateInterval;
    
    // Update light show
    updateLightShow();
    
    Serial.println("[BMDevice] Applied defaults to current state");
}

void BMDevice::setMaxBrightness(int maxBrightness) {
    bool success = defaults_.setMaxBrightness(maxBrightness);
    if (success) {
        // If current brightness exceeds new max, adjust it
        DeviceDefaults currentDefaults = defaults_.getCurrentDefaults();
        if (deviceState_.brightness > currentDefaults.maxBrightness) {
            setBrightness(currentDefaults.maxBrightness);
        }
        Serial.print("[BMDevice] Max brightness set to: ");
        Serial.println(currentDefaults.maxBrightness);
    }
}

void BMDevice::setDeviceOwner(const String& owner) {
    bool success = defaults_.setOwner(owner);
    if (success) {
        Serial.print("[BMDevice] Device owner set to: ");
        Serial.println(owner);
    }
}

// Defaults Feature Handlers
void BMDevice::handleGetDefaultsFeature(const uint8_t* buffer, size_t length) {
    String defaultsJson = defaults_.defaultsToJSON();
    
    // Send as status notification (you might want a separate characteristic for this)
    bluetoothHandler_.sendStatusUpdate(defaultsJson);
    
    Serial.println("[BMDevice] Sent defaults over BLE");
    Serial.print("Defaults JSON: ");
    Serial.println(defaultsJson);
}

void BMDevice::handleSetDefaultsFeature(const uint8_t* buffer, size_t length) {
    if (length > 1) {
        char jsonStr[1024] = {0};
        size_t jsonLength = min(length - 1, sizeof(jsonStr) - 1);
        memcpy(jsonStr, buffer + 1, jsonLength);
        
        bool success = defaults_.defaultsFromJSON(String(jsonStr));
        if (success) {
            Serial.println("[BMDevice] Defaults updated from JSON");
        } else {
            Serial.println("[BMDevice] Failed to update defaults from JSON");
        }
    }
}

void BMDevice::handleSaveCurrentAsDefaultsFeature(const uint8_t* buffer, size_t length) {
    bool success = saveCurrentAsDefaults();
    
    // Send confirmation via status
    String response = success ? "{\"defaultsSaved\":true}" : "{\"defaultsSaved\":false}";
    bluetoothHandler_.sendStatusUpdate(response);
    
    Serial.println(success ? "[BMDevice] Current state saved as defaults" : "[BMDevice] Failed to save current state as defaults");
}

void BMDevice::handleResetToFactoryFeature(const uint8_t* buffer, size_t length) {
    bool success = resetToFactoryDefaults();
    
    // Send confirmation via status
    String response = success ? "{\"factoryReset\":true}" : "{\"factoryReset\":false}";
    bluetoothHandler_.sendStatusUpdate(response);
    
    Serial.println(success ? "[BMDevice] Reset to factory defaults" : "[BMDevice] Failed to reset to factory defaults");
}

void BMDevice::handleSetMaxBrightnessFeature(const uint8_t* buffer, size_t length) {
    if (length >= 5) {
        int maxBrightness = 0;
        memcpy(&maxBrightness, buffer + 1, sizeof(int));
        setMaxBrightness(maxBrightness);
    }
}

void BMDevice::handleSetDeviceOwnerFeature(const uint8_t* buffer, size_t length) {
    if (length > 1) {
        char ownerStr[33] = {0};
        size_t ownerLength = min(length - 1, sizeof(ownerStr) - 1);
        memcpy(ownerStr, buffer + 1, ownerLength);
        setDeviceOwner(String(ownerStr));
    }
}

void BMDevice::handleSetAutoOnFeature(const uint8_t* buffer, size_t length) {
    if (length >= 2) {
        bool autoOn = buffer[1] != 0;
        bool success = defaults_.setAutoOn(autoOn);
        if (success) {
            Serial.print("[BMDevice] Auto-on set to: ");
            Serial.println(autoOn ? "true" : "false");
        }
    }
}

void BMDevice::handleSetDeviceTypeFeature(const uint8_t* buffer, size_t length) {
    if (length > 1) {
        String deviceType = String((char*)(buffer + 1), length - 1);
        bool success = defaults_.setDeviceType(deviceType);
        if (success) {
            Serial.print("[BMDevice] Device type set to: ");
            Serial.println(deviceType);
        }
    }
}

void BMDevice::handleConfigureLEDStripFeature(const uint8_t* buffer, size_t length) {
    if (length >= 6) { // stripIndex(1) + pin(1) + numLeds(2) + colorOrder(1) + enabled(1)
        int stripIndex = buffer[1];
        int pin = buffer[2];
        int numLeds = (buffer[3] << 8) | buffer[4];
        int colorOrder = buffer[5];
        bool enabled = length > 6 ? buffer[6] > 0 : true;
        
        bool success = defaults_.setLEDStripConfig(stripIndex, pin, numLeds, colorOrder, enabled);
        if (success) {
            Serial.printf("[BMDevice] LED strip %d configured: Pin %d, %d LEDs, Color order %d, %s\n", 
                         stripIndex, pin, numLeds, colorOrder, enabled ? "enabled" : "disabled");
        }
    }
}

void BMDevice::handleGetConfigurationFeature(const uint8_t* buffer, size_t length) {
    // Send configuration as JSON via status notification
    StaticJsonDocument<2048> doc;
    DeviceDefaults defaults = defaults_.getCurrentDefaults();
    
    doc["owner"] = defaults.owner;
    doc["deviceType"] = defaults.deviceType;
    doc["activeLEDStrips"] = defaults.activeLEDStrips;
    
    JsonArray strips = doc.createNestedArray("ledStrips");
    for (int i = 0; i < defaults.activeLEDStrips; i++) {
        JsonObject strip = strips.createNestedObject();
        strip["pin"] = defaults.ledStrips[i].pin;
        strip["numLeds"] = defaults.ledStrips[i].numLeds;
        strip["colorOrder"] = defaults.ledStrips[i].colorOrder;
        strip["enabled"] = defaults.ledStrips[i].enabled;
    }
    
    String configJson;
    serializeJson(doc, configJson);
    
    bluetoothHandler_.sendStatusUpdate(configJson);
    Serial.println("[BMDevice] Configuration sent via BLE");
}

void BMDevice::handleResetToDefaultsFeature(const uint8_t* buffer, size_t length) {
    bool success = resetToFactoryDefaults();
    
    String response = success ? "{\"factoryReset\":true}" : "{\"factoryReset\":false}";
    bluetoothHandler_.sendStatusUpdate(response);
    
    Serial.println(success ? "[BMDevice] Reset to factory defaults" : "[BMDevice] Failed to reset to factory defaults");
}

void BMDevice::addLEDStripByPin(int pin, CRGB* ledArray, int numLeds, int colorOrder) {
    // Handle different pins at compile time due to FastLED template requirements
    switch (pin) {
        case 2:
            switch (colorOrder) {
                case 0: addLEDStrip<WS2812B, 2, GRB>(ledArray, numLeds); break;
                case 1: addLEDStrip<WS2812B, 2, RGB>(ledArray, numLeds); break;
                case 2: addLEDStrip<WS2812B, 2, BRG>(ledArray, numLeds); break;
                case 3: addLEDStrip<WS2812B, 2, BGR>(ledArray, numLeds); break;
                case 4: addLEDStrip<WS2812B, 2, RBG>(ledArray, numLeds); break;
                case 5: addLEDStrip<WS2812B, 2, GBR>(ledArray, numLeds); break;
                default: addLEDStrip<WS2812B, 2, GRB>(ledArray, numLeds); break;
            }
            break;
        case 4:
            switch (colorOrder) {
                case 0: addLEDStrip<WS2812B, 4, GRB>(ledArray, numLeds); break;
                case 1: addLEDStrip<WS2812B, 4, RGB>(ledArray, numLeds); break;
                case 2: addLEDStrip<WS2812B, 4, BRG>(ledArray, numLeds); break;
                case 3: addLEDStrip<WS2812B, 4, BGR>(ledArray, numLeds); break;
                case 4: addLEDStrip<WS2812B, 4, RBG>(ledArray, numLeds); break;
                case 5: addLEDStrip<WS2812B, 4, GBR>(ledArray, numLeds); break;
                default: addLEDStrip<WS2812B, 4, GRB>(ledArray, numLeds); break;
            }
            break;
        case 5:
            switch (colorOrder) {
                case 0: addLEDStrip<WS2812B, 5, GRB>(ledArray, numLeds); break;
                case 1: addLEDStrip<WS2812B, 5, RGB>(ledArray, numLeds); break;
                case 2: addLEDStrip<WS2812B, 5, BRG>(ledArray, numLeds); break;
                case 3: addLEDStrip<WS2812B, 5, BGR>(ledArray, numLeds); break;
                case 4: addLEDStrip<WS2812B, 5, RBG>(ledArray, numLeds); break;
                case 5: addLEDStrip<WS2812B, 5, GBR>(ledArray, numLeds); break;
                default: addLEDStrip<WS2812B, 5, GRB>(ledArray, numLeds); break;
            }
            break;
        case 16:
            switch (colorOrder) {
                case 0: addLEDStrip<WS2812B, 16, GRB>(ledArray, numLeds); break;
                case 1: addLEDStrip<WS2812B, 16, RGB>(ledArray, numLeds); break;
                case 2: addLEDStrip<WS2812B, 16, BRG>(ledArray, numLeds); break;
                case 3: addLEDStrip<WS2812B, 16, BGR>(ledArray, numLeds); break;
                case 4: addLEDStrip<WS2812B, 16, RBG>(ledArray, numLeds); break;
                case 5: addLEDStrip<WS2812B, 16, GBR>(ledArray, numLeds); break;
                default: addLEDStrip<WS2812B, 16, GRB>(ledArray, numLeds); break;
            }
            break;
        case 17:
            switch (colorOrder) {
                case 0: addLEDStrip<WS2812B, 17, GRB>(ledArray, numLeds); break;
                case 1: addLEDStrip<WS2812B, 17, RGB>(ledArray, numLeds); break;
                case 2: addLEDStrip<WS2812B, 17, BRG>(ledArray, numLeds); break;
                case 3: addLEDStrip<WS2812B, 17, BGR>(ledArray, numLeds); break;
                case 4: addLEDStrip<WS2812B, 17, RBG>(ledArray, numLeds); break;
                case 5: addLEDStrip<WS2812B, 17, GBR>(ledArray, numLeds); break;
                default: addLEDStrip<WS2812B, 17, GRB>(ledArray, numLeds); break;
            }
            break;
        case 18:
            switch (colorOrder) {
                case 0: addLEDStrip<WS2812B, 18, GRB>(ledArray, numLeds); break;
                case 1: addLEDStrip<WS2812B, 18, RGB>(ledArray, numLeds); break;
                case 2: addLEDStrip<WS2812B, 18, BRG>(ledArray, numLeds); break;
                case 3: addLEDStrip<WS2812B, 18, BGR>(ledArray, numLeds); break;
                case 4: addLEDStrip<WS2812B, 18, RBG>(ledArray, numLeds); break;
                case 5: addLEDStrip<WS2812B, 18, GBR>(ledArray, numLeds); break;
                default: addLEDStrip<WS2812B, 18, GRB>(ledArray, numLeds); break;
            }
            break;
        case 19:
            switch (colorOrder) {
                case 0: addLEDStrip<WS2812B, 19, GRB>(ledArray, numLeds); break;
                case 1: addLEDStrip<WS2812B, 19, RGB>(ledArray, numLeds); break;
                case 2: addLEDStrip<WS2812B, 19, BRG>(ledArray, numLeds); break;
                case 3: addLEDStrip<WS2812B, 19, BGR>(ledArray, numLeds); break;
                case 4: addLEDStrip<WS2812B, 19, RBG>(ledArray, numLeds); break;
                case 5: addLEDStrip<WS2812B, 19, GBR>(ledArray, numLeds); break;
                default: addLEDStrip<WS2812B, 19, GRB>(ledArray, numLeds); break;
            }
            break;
        case 21:
            switch (colorOrder) {
                case 0: addLEDStrip<WS2812B, 21, GRB>(ledArray, numLeds); break;
                case 1: addLEDStrip<WS2812B, 21, RGB>(ledArray, numLeds); break;
                case 2: addLEDStrip<WS2812B, 21, BRG>(ledArray, numLeds); break;
                case 3: addLEDStrip<WS2812B, 21, BGR>(ledArray, numLeds); break;
                case 4: addLEDStrip<WS2812B, 21, RBG>(ledArray, numLeds); break;
                case 5: addLEDStrip<WS2812B, 21, GBR>(ledArray, numLeds); break;
                default: addLEDStrip<WS2812B, 21, GRB>(ledArray, numLeds); break;
            }
            break;
        case 22:
            switch (colorOrder) {
                case 0: addLEDStrip<WS2812B, 22, GRB>(ledArray, numLeds); break;
                case 1: addLEDStrip<WS2812B, 22, RGB>(ledArray, numLeds); break;
                case 2: addLEDStrip<WS2812B, 22, BRG>(ledArray, numLeds); break;
                case 3: addLEDStrip<WS2812B, 22, BGR>(ledArray, numLeds); break;
                case 4: addLEDStrip<WS2812B, 22, RBG>(ledArray, numLeds); break;
                case 5: addLEDStrip<WS2812B, 22, GBR>(ledArray, numLeds); break;
                default: addLEDStrip<WS2812B, 22, GRB>(ledArray, numLeds); break;
            }
            break;
        default:
            Serial.printf("[BMDevice] Warning: Pin %d not supported. Using pin 2 as fallback.\n", pin);
            addLEDStrip<WS2812B, 2, GRB>(ledArray, numLeds);
            break;
    }
}

void BMDevice::initializeLEDStrips() {
    Serial.println("[BMDevice] Initializing LED strips...");
    
    DeviceDefaults defaults = defaults_.getCurrentDefaults();
    
    for (int i = 0; i < defaults.activeLEDStrips; i++) {
        if (!defaults.ledStrips[i].enabled) continue;
        
        int pin = defaults.ledStrips[i].pin;
        int numLeds = defaults.ledStrips[i].numLeds;
        int colorOrder = defaults.ledStrips[i].colorOrder;
        
        // Allocate LED array
        ledArrays_[i] = new CRGB[numLeds];
        
        // Add LED strip using our wrapper function
        addLEDStripByPin(pin, ledArrays_[i], numLeds, colorOrder);
        
        Serial.printf("[BMDevice] LED Strip %d: Pin %d, %d LEDs, Color Order %d\n", 
                     i, pin, numLeds, colorOrder);
    }
} 