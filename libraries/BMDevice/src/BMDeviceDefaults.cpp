#include "BMDeviceDefaults.h"

BMDeviceDefaults::BMDeviceDefaults() : initialized_(false) {
    // Constructor intentionally minimal
}

BMDeviceDefaults::~BMDeviceDefaults() {
    if (initialized_) {
        end();
    }
}

bool BMDeviceDefaults::begin() {
    if (initialized_) {
        return true;
    }
    
    bool success = preferences_.begin(DEFAULTS_NAMESPACE, false);
    if (success) {
        initialized_ = true;
        migrateIfNeeded();
        
        // Load current defaults from storage
        DeviceDefaults defaults;
        if (loadDefaults(defaults)) {
            currentDefaults_ = defaults;
            Serial.println("[BMDeviceDefaults] Loaded defaults from storage");
        } else {
            Serial.println("[BMDeviceDefaults] No stored defaults found, using factory defaults");
            currentDefaults_.setFactoryDefaults();
            saveDefaults(currentDefaults_);
        }
        
        printCurrentDefaults();
    } else {
        Serial.println("[BMDeviceDefaults] Failed to initialize preferences storage");
    }
    
    return success;
}

void BMDeviceDefaults::end() {
    if (initialized_) {
        preferences_.end();
        initialized_ = false;
    }
}

bool BMDeviceDefaults::loadDefaults(DeviceDefaults& defaults) {
    if (!initialized_) {
        return false;
    }
    
    // Check if we have stored defaults
    if (!preferences_.isKey(PREF_VERSION)) {
        return false;
    }
    
    // Load all values with fallbacks to current defaults
    defaults.brightness = preferences_.getInt(PREF_BRIGHTNESS, defaults.brightness);
    defaults.maxBrightness = preferences_.getInt(PREF_MAX_BRIGHTNESS, defaults.maxBrightness);
    defaults.speed = preferences_.getInt(PREF_SPEED, defaults.speed);
    defaults.palette = (AvailablePalettes)preferences_.getUChar(PREF_PALETTE, (uint8_t)defaults.palette);
    defaults.effect = (LightSceneID)preferences_.getUChar(PREF_EFFECT, (uint8_t)defaults.effect);
    defaults.reverseDirection = preferences_.getBool(PREF_DIRECTION, defaults.reverseDirection);
    defaults.owner = readString(PREF_OWNER, defaults.owner);
    defaults.deviceName = readString(PREF_DEVICE_NAME, defaults.deviceName);
    defaults.deviceType = readString(PREF_DEVICE_TYPE, defaults.deviceType);
    defaults.autoOn = preferences_.getBool(PREF_AUTO_ON, defaults.autoOn);
    defaults.activeLEDStrips = preferences_.getInt(PREF_LED_COUNT, defaults.activeLEDStrips);
    defaults.statusUpdateInterval = preferences_.getULong(PREF_STATUS_INTERVAL, defaults.statusUpdateInterval);
    defaults.gpsEnabled = preferences_.getBool(PREF_GPS_ENABLED, defaults.gpsEnabled);
    
    // Load GPS speed settings
    defaults.gpsLowSpeed = preferences_.getFloat(PREF_GPS_LOW_SPEED, defaults.gpsLowSpeed);
    defaults.gpsTopSpeed = preferences_.getFloat(PREF_GPS_TOP_SPEED, defaults.gpsTopSpeed);
    defaults.gpsLightshowSpeedEnabled = preferences_.getBool(PREF_GPS_LIGHTSHOW_SPEED_ENABLED, defaults.gpsLightshowSpeedEnabled);
    
    defaults.version = preferences_.getInt(PREF_VERSION, DEFAULTS_VERSION);
    
    // Load effect color
    defaults.effectColor.r = preferences_.getUChar(PREF_EFFECT_COLOR_R, defaults.effectColor.r);
    defaults.effectColor.g = preferences_.getUChar(PREF_EFFECT_COLOR_G, defaults.effectColor.g);
    defaults.effectColor.b = preferences_.getUChar(PREF_EFFECT_COLOR_B, defaults.effectColor.b);
    
    // Load LED strip configuration
    String pinsJson = readString(PREF_LED_PINS, "");
    String ordersJson = readString(PREF_COLOR_ORDERS, "");
    
    if (pinsJson.length() > 0 && ordersJson.length() > 0) {
        DynamicJsonDocument pinsDoc(1024);
        DynamicJsonDocument ordersDoc(1024);
        
        if (deserializeJson(pinsDoc, pinsJson) == DeserializationError::Ok &&
            deserializeJson(ordersDoc, ordersJson) == DeserializationError::Ok) {
            
            for (int i = 0; i < MAX_LED_STRIPS && i < defaults.activeLEDStrips; i++) {
                if (i < pinsDoc.size()) {
                    defaults.ledStrips[i].pin = pinsDoc[i]["pin"];
                    defaults.ledStrips[i].numLeds = pinsDoc[i]["leds"];
                    defaults.ledStrips[i].enabled = pinsDoc[i]["enabled"];
                }
                if (i < ordersDoc.size()) {
                    defaults.ledStrips[i].colorOrder = ordersDoc[i];
                }
            }
        }
    }
    
    // Validate and constrain loaded values
    constrainValues(defaults);
    
    return validateDefaults(defaults);
}

bool BMDeviceDefaults::saveDefaults(const DeviceDefaults& defaults) {
    if (!initialized_) {
        return false;
    }
    
    // Validate before saving
    DeviceDefaults validatedDefaults = defaults;
    constrainValues(validatedDefaults);
    
    if (!validateDefaults(validatedDefaults)) {
        Serial.println("[BMDeviceDefaults] Invalid defaults, cannot save");
        return false;
    }
    
    bool success = true;
    
    // Save core settings
    success &= preferences_.putInt(PREF_BRIGHTNESS, validatedDefaults.brightness);
    success &= preferences_.putInt(PREF_MAX_BRIGHTNESS, validatedDefaults.maxBrightness);
    success &= preferences_.putInt(PREF_SPEED, validatedDefaults.speed);
    success &= preferences_.putUChar(PREF_PALETTE, (uint8_t)validatedDefaults.palette);
    success &= preferences_.putUChar(PREF_EFFECT, (uint8_t)validatedDefaults.effect);
    success &= preferences_.putBool(PREF_DIRECTION, validatedDefaults.reverseDirection);
    success &= writeString(PREF_OWNER, validatedDefaults.owner);
    success &= writeString(PREF_DEVICE_NAME, validatedDefaults.deviceName);
    success &= writeString(PREF_DEVICE_TYPE, validatedDefaults.deviceType);
    success &= preferences_.putBool(PREF_AUTO_ON, validatedDefaults.autoOn);
    success &= preferences_.putInt(PREF_LED_COUNT, validatedDefaults.activeLEDStrips);
    success &= preferences_.putULong(PREF_STATUS_INTERVAL, validatedDefaults.statusUpdateInterval);
    success &= preferences_.putBool(PREF_GPS_ENABLED, validatedDefaults.gpsEnabled);
    
    // Save GPS speed settings
    success &= preferences_.putFloat(PREF_GPS_LOW_SPEED, validatedDefaults.gpsLowSpeed);
    success &= preferences_.putFloat(PREF_GPS_TOP_SPEED, validatedDefaults.gpsTopSpeed);
    success &= preferences_.putBool(PREF_GPS_LIGHTSHOW_SPEED_ENABLED, validatedDefaults.gpsLightshowSpeedEnabled);
    
    success &= preferences_.putInt(PREF_VERSION, validatedDefaults.version);
    
    // Save effect color
    success &= preferences_.putUChar(PREF_EFFECT_COLOR_R, validatedDefaults.effectColor.r);
    success &= preferences_.putUChar(PREF_EFFECT_COLOR_G, validatedDefaults.effectColor.g);
    success &= preferences_.putUChar(PREF_EFFECT_COLOR_B, validatedDefaults.effectColor.b);
    
    // Save LED configuration as JSON
    DynamicJsonDocument pinsDoc(1024);
    DynamicJsonDocument ordersDoc(1024);
    
    for (int i = 0; i < validatedDefaults.activeLEDStrips; i++) {
        JsonObject pinObj = pinsDoc.createNestedObject();
        pinObj["pin"] = validatedDefaults.ledStrips[i].pin;
        pinObj["leds"] = validatedDefaults.ledStrips[i].numLeds;
        pinObj["enabled"] = validatedDefaults.ledStrips[i].enabled;
        
        ordersDoc.add(validatedDefaults.ledStrips[i].colorOrder);
    }
    
    String pinsJson, ordersJson;
    serializeJson(pinsDoc, pinsJson);
    serializeJson(ordersDoc, ordersJson);
    
    success &= writeString(PREF_LED_PINS, pinsJson);
    success &= writeString(PREF_COLOR_ORDERS, ordersJson);
    
    if (success) {
        currentDefaults_ = validatedDefaults;
        Serial.println("[BMDeviceDefaults] Defaults saved successfully");
    } else {
        Serial.println("[BMDeviceDefaults] Failed to save some default values");
    }
    
    return success;
}

bool BMDeviceDefaults::resetToFactory() {
    if (!initialized_) {
        return false;
    }
    
    // Clear all preferences
    bool success = preferences_.clear();
    if (success) {
        currentDefaults_.setFactoryDefaults();
        success = saveDefaults(currentDefaults_);
        Serial.println("[BMDeviceDefaults] Reset to factory defaults");
    } else {
        Serial.println("[BMDeviceDefaults] Failed to clear preferences");
    }
    
    return success;
}

// Individual setting operations
bool BMDeviceDefaults::setBrightness(int brightness) {
    currentDefaults_.brightness = constrain(brightness, 1, currentDefaults_.maxBrightness);
    return preferences_.putInt(PREF_BRIGHTNESS, currentDefaults_.brightness);
}

bool BMDeviceDefaults::setMaxBrightness(int maxBrightness) {
    currentDefaults_.maxBrightness = constrain(maxBrightness, 1, 256);
    // If current brightness exceeds new max, adjust it
    if (currentDefaults_.brightness > currentDefaults_.maxBrightness) {
        currentDefaults_.brightness = currentDefaults_.maxBrightness;
        preferences_.putInt(PREF_BRIGHTNESS, currentDefaults_.brightness);
    }
    return preferences_.putInt(PREF_MAX_BRIGHTNESS, currentDefaults_.maxBrightness);
}

bool BMDeviceDefaults::setSpeed(int speed) {
    currentDefaults_.speed = constrain(speed, 5, 200);
    return preferences_.putInt(PREF_SPEED, currentDefaults_.speed);
}

bool BMDeviceDefaults::setPalette(AvailablePalettes palette) {
    currentDefaults_.palette = palette;
    return preferences_.putUChar(PREF_PALETTE, (uint8_t)palette);
}

bool BMDeviceDefaults::setEffect(LightSceneID effect) {
    currentDefaults_.effect = effect;
    return preferences_.putUChar(PREF_EFFECT, (uint8_t)effect);
}

bool BMDeviceDefaults::setDirection(bool reverse) {
    currentDefaults_.reverseDirection = reverse;
    return preferences_.putBool(PREF_DIRECTION, reverse);
}

bool BMDeviceDefaults::setOwner(const String& owner) {
    currentDefaults_.owner = owner;
    return writeString(PREF_OWNER, owner);
}

bool BMDeviceDefaults::setDeviceName(const String& name) {
    currentDefaults_.deviceName = name;
    return writeString(PREF_DEVICE_NAME, name);
}

bool BMDeviceDefaults::setAutoOn(bool autoOn) {
    currentDefaults_.autoOn = autoOn;
    return preferences_.putBool(PREF_AUTO_ON, autoOn);
}

bool BMDeviceDefaults::setStatusInterval(unsigned long interval) {
    currentDefaults_.statusUpdateInterval = constrain(interval, 1000UL, 60000UL);
    return preferences_.putULong(PREF_STATUS_INTERVAL, currentDefaults_.statusUpdateInterval);
}

bool BMDeviceDefaults::setEffectColor(CRGB color) {
    currentDefaults_.effectColor = color;
    bool success = true;
    success &= preferences_.putUChar(PREF_EFFECT_COLOR_R, color.r);
    success &= preferences_.putUChar(PREF_EFFECT_COLOR_G, color.g);
    success &= preferences_.putUChar(PREF_EFFECT_COLOR_B, color.b);
    return success;
}

bool BMDeviceDefaults::setGPSEnabled(bool enabled) {
    currentDefaults_.gpsEnabled = enabled;
    return preferences_.putBool(PREF_GPS_ENABLED, enabled);
}

bool BMDeviceDefaults::setGPSLowSpeed(float speed) {
    currentDefaults_.gpsLowSpeed = constrain(speed, 0.0f, 100.0f);
    return preferences_.putFloat(PREF_GPS_LOW_SPEED, currentDefaults_.gpsLowSpeed);
}

bool BMDeviceDefaults::setGPSTopSpeed(float speed) {
    currentDefaults_.gpsTopSpeed = constrain(speed, 0.0f, 200.0f);
    // Ensure top speed is always higher than low speed
    if (currentDefaults_.gpsTopSpeed <= currentDefaults_.gpsLowSpeed) {
        currentDefaults_.gpsTopSpeed = currentDefaults_.gpsLowSpeed + 1.0f;
    }
    return preferences_.putFloat(PREF_GPS_TOP_SPEED, currentDefaults_.gpsTopSpeed);
}

bool BMDeviceDefaults::setGPSLightshowSpeedEnabled(bool enabled) {
    currentDefaults_.gpsLightshowSpeedEnabled = enabled;
    return preferences_.putBool(PREF_GPS_LIGHTSHOW_SPEED_ENABLED, enabled);
}

float BMDeviceDefaults::getGPSLowSpeed() {
    return currentDefaults_.gpsLowSpeed;
}

float BMDeviceDefaults::getGPSTopSpeed() {
    return currentDefaults_.gpsTopSpeed;
}

bool BMDeviceDefaults::isGPSLightshowSpeedEnabled() {
    return currentDefaults_.gpsLightshowSpeedEnabled;
}

bool BMDeviceDefaults::setDeviceType(const String& deviceType) {
    currentDefaults_.deviceType = deviceType;
    return writeString(PREF_DEVICE_TYPE, deviceType);
}

bool BMDeviceDefaults::setLEDStripConfig(int stripIndex, int pin, int numLeds, int colorOrder, bool enabled) {
    if (stripIndex >= MAX_LED_STRIPS || stripIndex < 0) return false;
    
    currentDefaults_.ledStrips[stripIndex].pin = pin;
    currentDefaults_.ledStrips[stripIndex].numLeds = numLeds;
    currentDefaults_.ledStrips[stripIndex].colorOrder = colorOrder;
    currentDefaults_.ledStrips[stripIndex].enabled = enabled;
    
    // Save the updated defaults
    return saveDefaults(currentDefaults_);
}

bool BMDeviceDefaults::setActiveLEDStrips(int count) {
    if (count > MAX_LED_STRIPS || count < 0) return false;
    
    currentDefaults_.activeLEDStrips = count;
    return preferences_.putInt(PREF_LED_COUNT, count);
}

LEDStripConfig BMDeviceDefaults::getLEDStripConfig(int stripIndex) {
    if (stripIndex >= 0 && stripIndex < MAX_LED_STRIPS) {
        return currentDefaults_.ledStrips[stripIndex];
    }
    return LEDStripConfig(); // Return default config
}

int BMDeviceDefaults::getActiveLEDStrips() {
    return currentDefaults_.activeLEDStrips;
}

DeviceDefaults BMDeviceDefaults::getCurrentDefaults() {
    return currentDefaults_;
}

String BMDeviceDefaults::defaultsToJSON() const {
    StaticJsonDocument<1024> doc;
    
    doc["brightness"] = currentDefaults_.brightness;
    doc["maxBrightness"] = currentDefaults_.maxBrightness;
    doc["speed"] = currentDefaults_.speed;
    doc["palette"] = LightShow::paletteIdToName(currentDefaults_.palette);
    doc["paletteId"] = (uint8_t)currentDefaults_.palette;
    doc["effect"] = LightShow::effectIdToName(currentDefaults_.effect);
    doc["effectId"] = (uint8_t)currentDefaults_.effect;
    doc["reverseDirection"] = currentDefaults_.reverseDirection;
    doc["owner"] = currentDefaults_.owner;
    doc["deviceName"] = currentDefaults_.deviceName;
    doc["autoOn"] = currentDefaults_.autoOn;
    doc["statusInterval"] = currentDefaults_.statusUpdateInterval;
    doc["gpsEnabled"] = currentDefaults_.gpsEnabled;
    doc["version"] = currentDefaults_.version;
    
    JsonObject colorObj = doc.createNestedObject("effectColor");
    colorObj["r"] = currentDefaults_.effectColor.r;
    colorObj["g"] = currentDefaults_.effectColor.g;
    colorObj["b"] = currentDefaults_.effectColor.b;
    
    String output;
    serializeJson(doc, output);
    return output;
}

bool BMDeviceDefaults::defaultsFromJSON(const String& json) {
    StaticJsonDocument<1024> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.print("[BMDeviceDefaults] JSON parse error: ");
        Serial.println(error.c_str());
        return false;
    }
    
    DeviceDefaults newDefaults = currentDefaults_;
    
    // Update values if present in JSON
    if (doc.containsKey("brightness")) newDefaults.brightness = doc["brightness"];
    if (doc.containsKey("maxBrightness")) newDefaults.maxBrightness = doc["maxBrightness"];
    if (doc.containsKey("speed")) newDefaults.speed = doc["speed"];
    if (doc.containsKey("paletteId")) newDefaults.palette = (AvailablePalettes)(uint8_t)doc["paletteId"];
    if (doc.containsKey("effectId")) newDefaults.effect = (LightSceneID)(uint8_t)doc["effectId"];
    if (doc.containsKey("reverseDirection")) newDefaults.reverseDirection = doc["reverseDirection"];
    if (doc.containsKey("owner")) newDefaults.owner = doc["owner"].as<String>();
    if (doc.containsKey("deviceName")) newDefaults.deviceName = doc["deviceName"].as<String>();
    if (doc.containsKey("autoOn")) newDefaults.autoOn = doc["autoOn"];
    if (doc.containsKey("statusInterval")) newDefaults.statusUpdateInterval = doc["statusInterval"];
    if (doc.containsKey("gpsEnabled")) newDefaults.gpsEnabled = doc["gpsEnabled"];
    
    if (doc.containsKey("effectColor")) {
        JsonObject colorObj = doc["effectColor"];
        newDefaults.effectColor.r = colorObj["r"];
        newDefaults.effectColor.g = colorObj["g"];
        newDefaults.effectColor.b = colorObj["b"];
    }
    
    return saveDefaults(newDefaults);
}

bool BMDeviceDefaults::validateDefaults(const DeviceDefaults& defaults) {
    // Validate ranges and values
    if (defaults.brightness < 1 || defaults.brightness > 100) return false;
    if (defaults.maxBrightness < 1 || defaults.maxBrightness > 100) return false;
    if (defaults.speed < 5 || defaults.speed > 200) return false;
    if (defaults.statusUpdateInterval < 1000 || defaults.statusUpdateInterval > 60000) return false;
    if (defaults.version < 1) return false;
    
    // Check enum values are valid
    if ((uint8_t)defaults.palette > (uint8_t)AvailablePalettes::moltenmetal) return false;
    if ((uint8_t)defaults.effect > (uint8_t)LightSceneID::spiral_galaxy) return false;
    
    return true;
}

bool BMDeviceDefaults::migrateIfNeeded() {
    if (!initialized_) {
        return false;
    }
    
    int storedVersion = preferences_.getInt(PREF_VERSION, 0);
    
    if (storedVersion == 0) {
        // Fresh install or very old version
        Serial.println("[BMDeviceDefaults] Fresh installation, no migration needed");
        return true;
    }
    
    if (storedVersion < DEFAULTS_VERSION) {
        Serial.printf("[BMDeviceDefaults] Migrating from version %d to %d\n", storedVersion, DEFAULTS_VERSION);
        
        // Add migration logic here for future versions
        // For now, just update the version
        preferences_.putInt(PREF_VERSION, DEFAULTS_VERSION);
        
        Serial.println("[BMDeviceDefaults] Migration complete");
    }
    
    return true;
}

bool BMDeviceDefaults::hasValidDefaults() {
    return initialized_ && validateDefaults(currentDefaults_);
}

void BMDeviceDefaults::printCurrentDefaults() {
    if (!initialized_) {
        Serial.println("[BMDeviceDefaults] Not initialized");
        return;
    }
    
    Serial.println("═══ Current Device Defaults ═══");
    Serial.printf("Owner: %s\n", currentDefaults_.owner.c_str());
    Serial.printf("Device Name: %s\n", currentDefaults_.deviceName.c_str());
    Serial.printf("Brightness: %d (max: %d)\n", currentDefaults_.brightness, currentDefaults_.maxBrightness);
    Serial.printf("Speed: %d\n", currentDefaults_.speed);
    Serial.printf("Palette: %s (%d)\n", LightShow::paletteIdToName(currentDefaults_.palette), (uint8_t)currentDefaults_.palette);
    Serial.printf("Effect: %s (%d)\n", LightShow::effectIdToName(currentDefaults_.effect), (uint8_t)currentDefaults_.effect);
    Serial.printf("Direction: %s\n", currentDefaults_.reverseDirection ? "Reverse" : "Forward");
    Serial.printf("Auto On: %s\n", currentDefaults_.autoOn ? "Yes" : "No");
    Serial.printf("GPS Enabled: %s\n", currentDefaults_.gpsEnabled ? "Yes" : "No");
    Serial.printf("Status Interval: %lu ms\n", currentDefaults_.statusUpdateInterval);
    Serial.printf("Effect Color: RGB(%d,%d,%d)\n", 
                  currentDefaults_.effectColor.r, 
                  currentDefaults_.effectColor.g, 
                  currentDefaults_.effectColor.b);
    Serial.printf("Version: %d\n", currentDefaults_.version);
    Serial.println("══════════════════════════════");
}

void BMDeviceDefaults::constrainValues(DeviceDefaults& defaults) {
    defaults.brightness = constrain(defaults.brightness, 1, 100);
    defaults.maxBrightness = constrain(defaults.maxBrightness, 1, 100);
    defaults.speed = constrain(defaults.speed, 5, 200);
    defaults.statusUpdateInterval = constrain(defaults.statusUpdateInterval, 1000UL, 60000UL);
    
    // Ensure brightness doesn't exceed max brightness
    if (defaults.brightness > defaults.maxBrightness) {
        defaults.brightness = defaults.maxBrightness;
    }
    
    // Constrain GPS speed values
    defaults.gpsLowSpeed = constrain(defaults.gpsLowSpeed, 0.0f, 100.0f);
    defaults.gpsTopSpeed = constrain(defaults.gpsTopSpeed, 0.0f, 200.0f);
    
    // Ensure top speed is always higher than low speed
    if (defaults.gpsTopSpeed <= defaults.gpsLowSpeed) {
        defaults.gpsTopSpeed = defaults.gpsLowSpeed + 1.0f;
    }
    
    // Constrain string lengths
    if (defaults.owner.length() > 32) {
        defaults.owner = defaults.owner.substring(0, 32);
    }
    if (defaults.deviceName.length() > 32) {
        defaults.deviceName = defaults.deviceName.substring(0, 32);
    }
}

bool BMDeviceDefaults::writeString(const char* key, const String& value) {
    if (!initialized_) {
        return false;
    }
    
    return preferences_.putString(key, value) == value.length();
}

String BMDeviceDefaults::readString(const char* key, const String& defaultValue) {
    if (!initialized_) {
        return defaultValue;
    }
    
    return preferences_.getString(key, defaultValue);
} 