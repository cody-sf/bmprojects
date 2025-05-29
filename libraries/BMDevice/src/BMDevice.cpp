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

BMDevice::~BMDevice() {
    if (ownGPSSerial_ && gpsSerial_) {
        delete gpsSerial_;
    }
    if (gps_) {
        delete gps_;
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
    
    // Initialize Bluetooth
    if (!bluetoothHandler_.begin()) {
        return false;
    }
    
    // Set initial brightness
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
    deviceState_.brightness = constrain(brightness, 1, 100);
    lightShow_.brightness(deviceState_.brightness * 2.55);
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
    String status = deviceState_.toJSON();
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
                setEffect((LightSceneID)effectId);
                Serial.print("[BMDevice] Effect set to ID: ");
                Serial.println(effectId);
            }
        } else { // String
            char effectStr[32] = {0};
            memcpy(effectStr, buffer + 1, min(length - 1, sizeof(effectStr) - 1));
            LightSceneID effect = LightShow::effectNameToId(effectStr);
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