#include "BMDevice.h"

// Sketches that ship OTA (BMGenericDevice) provide include/version.h; the rest
// just report "dev". Without the guard the library only builds for projects
// that happen to carry that header.
#if defined(__has_include)
#if __has_include("version.h")
#include "version.h"
#endif
#endif
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

BMDevice::BMDevice(const char* deviceName, const char* serviceUUID, const char* featuresUUID, const char* statusUUID)
    : bluetoothHandler_(deviceName, serviceUUID, featuresUUID, statusUUID), lightShow_(std::vector<CLEDController*>(), deviceClock_),
      gpsEnabled_(false),
#ifndef TARGET_ESP32_C6
      ownGPSSerial_(false), locationService_(nullptr),
#endif
      lastBluetoothSync_(0),
      statusUpdateInterval_(DEFAULT_BT_REFRESH_INTERVAL),
      statusDirty_(false), statusDirtyAt_(0), lastStatusSentAt_(0), lastFingerprintAt_(0),
      stateFingerprint_(0), wasSubscribed_(false), ledsBlanked_(false),
      statusUpdateState_(STATUS_IDLE), statusUpdateTimer_(0), currentChunkIndex_(0),
      dynamicNaming_(false) {

    // Initialize LED arrays (this constructor takes strips from the sketch, but
    // the destructor walks the array either way)
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

BMDevice::BMDevice(const char* serviceUUID, const char* featuresUUID, const char* statusUUID)
    : bluetoothHandler_("", serviceUUID, featuresUUID, statusUUID), lightShow_(std::vector<CLEDController*>(), deviceClock_),
      gpsEnabled_(false),
#ifndef TARGET_ESP32_C6
      ownGPSSerial_(false), locationService_(nullptr),
#endif
      lastBluetoothSync_(0),
      statusUpdateInterval_(DEFAULT_BT_REFRESH_INTERVAL),
      statusDirty_(false), statusDirtyAt_(0), lastStatusSentAt_(0), lastFingerprintAt_(0),
      stateFingerprint_(0), wasSubscribed_(false), ledsBlanked_(false),
      statusUpdateState_(STATUS_IDLE), statusUpdateTimer_(0), currentChunkIndex_(0),
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
#ifndef TARGET_ESP32_C6
    if (ownGPSSerial_ && locationService_) {
        delete locationService_;
    }
#endif
    
    // Clean up LED arrays
    for (int i = 0; i < MAX_LED_STRIPS; i++) {
        if (ledArrays_[i]) {
            delete[] ledArrays_[i];
            ledArrays_[i] = nullptr;
        }
    }
}

#ifndef TARGET_ESP32_C6
void BMDevice::enableGPS(int rxPin, int txPin, int baud) {
    Serial.printf("[BMDevice] enableGPS() called with pins RX:%d TX:%d @ %d baud\n", rxPin, txPin, baud);
    
    // Create and configure LocationService
    if (!locationService_) {
        Serial.println("[BMDevice] Creating new LocationService");
        locationService_ = new LocationService();
        ownGPSSerial_ = true; // We created the LocationService
    } else {
        Serial.println("[BMDevice] Using existing LocationService");
    }
    
    gpsEnabled_ = true;
    Serial.println("[BMDevice] Calling locationService_->start_tracking_position()");
    locationService_->start_tracking_position();
    
    // Also update the defaults to reflect GPS is enabled
    defaults_.setGPSEnabled(true);
    
    Serial.printf("[BMDevice] GPS enabled using LocationService (pins RX:%d TX:%d @ %d baud)\n", 
                 rxPin, txPin, baud);
    Serial.println("[BMDevice] GPS will auto-update position and speed");
}
#endif

#ifndef TARGET_ESP32_C6
void BMDevice::setLocationService(LocationService* locationService) {
    locationService_ = locationService;
    gpsEnabled_ = true;
    ownGPSSerial_ = false;
    
    // Ensure GPS tracking is started
    locationService_->start_tracking_position();
    
    // Also update the defaults to reflect GPS is enabled
    defaults_.setGPSEnabled(true);
    
    Serial.println("[BMDevice] Using external LocationService for GPS");
}
#endif

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
        String deviceName = buildAdvertisedName();

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
    
    // Set initial brightness (may be overridden by defaults). Internal scale is 1-255.
    lightShow_.brightness(deviceState_.brightness);
    
    // Update light show with initial state
    updateLightShow();
    
    // Initialize default status chunks for all devices
    initializeDefaultStatusChunks();
    
    Serial.println("[BMDevice] Setup complete");
    return true;
}

void BMDevice::loop() {
    // Update GPS first and more frequently to prevent data loss
    if (gpsEnabled_) {
        updateGPS();
    }

    bluetoothHandler_.poll();

    // Handle chunked status updates
    handleChunkedStatusUpdate();

    // Status is pushed on connect and whenever settings change - never on a timer
    if (takeDueStatusUpdate()) {
        startChunkedStatusUpdate();
    }

    // Handle power state
    if (!deviceState_.power) {
        // Blank once on the transition. Re-clocking every strip on every loop
        // burns the CPU and the LED data lines for no visible difference
        // (8 x 450 LEDs is >100 ms of blocking output per pass).
        if (!ledsBlanked_) {
            FastLED.clear();
            FastLED.show();
            ledsBlanked_ = true;
        }
        // Off but reachable: nothing to render, so poll the radio at a relaxed
        // pace instead of spinning. Well inside a BLE connection interval.
        delay(20);
        return;
    }
    ledsBlanked_ = false;

    // GPS-driven modes are re-evaluated here: updateLightShow() otherwise only
    // runs when a BLE command arrives, so the show never actually followed the
    // GPS. setSpeed() adjusts the frame period in place - no scene restart, so
    // the animation doesn't visibly jump when the pace changes.
    if (gpsEnabled_ && deviceState_.positionAvailable &&
        millis() - lastGpsShowRefresh_ >= GPS_SHOW_REFRESH_MS) {
        lastGpsShowRefresh_ = millis();
        if (deviceState_.currentEffect == LightSceneID::speedometer) {
            updateLightShow();  // recomputes the slow/fast colour blend
        } else if (deviceState_.currentEffect == LightSceneID::position_status) {
            lightShow_.setSpeed(positionStatusSpeed());
        } else if (deviceState_.gpsLightshowSpeedEnabled) {
            lightShow_.setSpeed(calculateEffectiveSpeed());
        }
    }

    // Render light show
    lightShow_.render();

    // Frames are gated on millis() inside render(), so this only adds <=1 ms of
    // jitter to a >=5 ms frame period - invisible - while handing the CPU to
    // the idle task instead of spinning this loop flat out.
    delay(1);
}

void BMDevice::markStatusDirty() {
    statusDirty_ = true;
    statusDirtyAt_ = millis();
}

// Cheap rolling hash over everything the app displays. Deliberately excludes
// GPS position/speed: those change continuously and would defeat the point of
// event-driven updates (the app polls while a GPS screen is open instead).
uint32_t BMDevice::computeStateFingerprint() {
    uint32_t h = 2166136261u;  // FNV-1a
    auto mix = [&h](uint32_t v) {
        h ^= v;
        h *= 16777619u;
    };

    mix(deviceState_.power ? 1u : 2u);
    mix((uint32_t)deviceState_.brightness);
    mix((uint32_t)deviceState_.speed);
    mix(deviceState_.reverseStrip ? 1u : 2u);
    mix((uint32_t)deviceState_.currentPalette);
    mix((uint32_t)deviceState_.currentEffect);

    mix((uint32_t)deviceState_.waveWidth);
    mix((uint32_t)deviceState_.meteorCount);
    mix((uint32_t)deviceState_.trailLength);
    mix((uint32_t)deviceState_.heatVariance);
    mix((uint32_t)deviceState_.mirrorCount);
    mix((uint32_t)deviceState_.cometCount);
    mix((uint32_t)deviceState_.dropRate);
    mix((uint32_t)deviceState_.cloudScale);
    mix((uint32_t)deviceState_.blobCount);
    mix((uint32_t)deviceState_.waveCount);
    mix((uint32_t)deviceState_.flashIntensity);
    mix((uint32_t)deviceState_.flashFrequency);
    mix((uint32_t)deviceState_.explosionSize);
    mix((uint32_t)deviceState_.spiralArms);
    mix(((uint32_t)deviceState_.effectColor.r << 16) |
        ((uint32_t)deviceState_.effectColor.g << 8) |
        (uint32_t)deviceState_.effectColor.b);
    mix(deviceState_.gpsLightshowSpeedEnabled ? 1u : 2u);

    return h;
}

bool BMDevice::takeDueStatusUpdate() {
    unsigned long now = millis();

    // Nothing can be delivered until the central subscribes to notifications.
    // The rising edge is also the trigger for the on-connect burst: sending
    // from the BLEConnected callback races the app, which only subscribes after
    // it has discovered services.
    bool subscribed = bluetoothHandler_.isConnected() && bluetoothHandler_.isSubscribed();
    if (!subscribed) {
        wasSubscribed_ = false;
        // Keep the fingerprint current so reconnecting doesn't replay old
        // churn - but on the same cadence as the subscribed path, not every
        // single pass through loop().
        if (now - lastFingerprintAt_ >= STATUS_FINGERPRINT_POLL_MS) {
            lastFingerprintAt_ = now;
            stateFingerprint_ = computeStateFingerprint();
        }
        return false;
    }
    if (!wasSubscribed_) {
        wasSubscribed_ = true;
        statusDirty_ = true;
        statusDirtyAt_ = 0;      // send the connect burst immediately
        lastStatusSentAt_ = 0;
    }

    // Catch changes made straight through getState() (encoder menu, sketches)
    if (now - lastFingerprintAt_ >= STATUS_FINGERPRINT_POLL_MS) {
        lastFingerprintAt_ = now;
        uint32_t fingerprint = computeStateFingerprint();
        if (fingerprint != stateFingerprint_) {
            stateFingerprint_ = fingerprint;
            markStatusDirty();
        }
    }

    if (!statusDirty_) {
        return false;
    }
    // Coalesce bursts of changes (slider drags, startup fades) into one update
    if (statusDirtyAt_ != 0 && (now - statusDirtyAt_) < STATUS_SETTLE_MS) {
        return false;
    }
    if (lastStatusSentAt_ != 0 && (now - lastStatusSentAt_) < STATUS_MIN_INTERVAL_MS) {
        return false;
    }

    statusDirty_ = false;
    lastStatusSentAt_ = now;
    lastBluetoothSync_ = now;
    stateFingerprint_ = computeStateFingerprint();
    return true;
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
        case BLE_FEATURE_REQUEST_STATUS:
            // App asked for a refresh (page opened, poll tick). Answer now
            // rather than waiting on the settle window.
            statusDirty_ = true;
            statusDirtyAt_ = 0;
            lastStatusSentAt_ = 0;
            return;
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
        case BLE_FEATURE_SPEEDOMETER:
            handleSpeedometerFeature(buffer, length);
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
        
        // GPS Speed configuration commands
        case BLE_FEATURE_SET_GPS_LOW_SPEED:
            handleSetGPSLowSpeedFeature(buffer, length);
            break;
        case BLE_FEATURE_SET_GPS_TOP_SPEED:
            handleSetGPSTopSpeedFeature(buffer, length);
            break;
        case BLE_FEATURE_SET_GPS_LIGHTSHOW_SPEED_ENABLED:
            handleSetGPSLightshowSpeedEnabledFeature(buffer, length);
            break;
        
        // Generic device configuration commands
        case BLE_FEATURE_SET_OWNER:
            handleSetDeviceOwnerFeature(buffer, length);
            break;
        case BLE_FEATURE_SET_DEVICE_NAME:
            handleSetDeviceNameFeature(buffer, length);
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
            
        // Custom palettes
        case BLE_FEATURE_SET_CUSTOM_PALETTE:
            handleSetCustomPaletteFeature(buffer, length);
            break;
        case BLE_FEATURE_DELETE_CUSTOM_PALETTE:
            handleDeleteCustomPaletteFeature(buffer, length);
            break;
            
        default:
            Serial.print("[BMDevice] Unknown feature: 0x");
            Serial.println(feature, HEX);
            break;
    }
}

void BMDevice::handleConnectionChange(bool connected) {
    // The connect burst is not sent from here: at this point the central has
    // not enabled notifications yet, so anything written to the status
    // characteristic is dropped. takeDueStatusUpdate() fires it on the
    // subscribe edge instead.
    if (!connected) {
        wasSubscribed_ = false;
        statusDirty_ = false;
        statusUpdateState_ = STATUS_IDLE;
    }

    if (customConnectionHandler_) {
        customConnectionHandler_(connected);
    }
}

void BMDevice::updateGPS() {
#ifndef TARGET_ESP32_C6
    static unsigned long lastGPSDebug = 0;
    static bool lastPositionState = false;
    
    if (locationService_) {
        // Use LocationService - it handles all GPS complexity
        locationService_->update_position();
        
        // Update device state from LocationService
        if (locationService_->is_current_position_available()) {
            deviceState_.currentPosition = locationService_->current_position();
            deviceState_.positionAvailable = true;
            deviceState_.currentSpeed = locationService_->current_speed();
            
            // Log position changes
            if (!lastPositionState) {
                Position pos = deviceState_.currentPosition;
                Serial.printf("[BMDevice] GPS fix acquired: %.6f, %.6f (speed: %.2f km/h)\n", 
                            pos.latitude(), pos.longitude(), deviceState_.currentSpeed);
                lastPositionState = true;
            }
        } else {
            deviceState_.positionAvailable = false;
            if (lastPositionState) {
                Serial.println("[BMDevice] GPS fix lost");
                lastPositionState = false;
            }
        }
        
        // Debug output every 60 seconds
        if (millis() - lastGPSDebug > 60000) {
            Serial.printf("[BMDevice] GPS Status - Fix: %s, Speed: %.2f km/h\n",
                         deviceState_.positionAvailable ? "YES" : "NO",
                         deviceState_.currentSpeed);
            
            // Check LocationService directly
            bool locAvail = locationService_->is_current_position_available();
            bool initialAvail = locationService_->is_initial_position_available();
            Serial.printf("[BMDevice] LocationService - Current: %s, Initial: %s\n",
                         locAvail ? "YES" : "NO", initialAvail ? "YES" : "NO");
            
            if (locAvail) {
                Position pos = locationService_->current_position();
                float speed = locationService_->current_speed();
                Serial.printf("[BMDevice] LocationService pos: %.6f, %.6f, speed: %.2f\n",
                             pos.latitude(), pos.longitude(), speed);
            }
            
            if (!deviceState_.positionAvailable) {
                Serial.println("[BMDevice] No GPS fix yet - move device outdoors with clear sky view");
            }
            
            lastGPSDebug = millis();
        }
    }
#else
    // GPS not supported on C6 - just disable GPS features
    deviceState_.positionAvailable = false;
    deviceState_.currentSpeed = 0.0f;
#endif
}

uint16_t BMDevice::calculateEffectiveSpeed() {
    // If GPS lightshow speed is disabled or GPS not available, use normal speed
    if (!deviceState_.gpsLightshowSpeedEnabled || !gpsEnabled_ || !deviceState_.positionAvailable) {
        return deviceState_.speed;
    }
    
    // Get current GPS speed
    float currentGPSSpeed = deviceState_.currentSpeed;
    
    // Constrain GPS speed to our defined range
    currentGPSSpeed = constrain(currentGPSSpeed, deviceState_.gpsLowSpeed, deviceState_.gpsTopSpeed);
    
    // Map GPS speed to lightshow speed (inverse relationship)
    // Low GPS speed = high lightshow delay (slow lightshow)
    // High GPS speed = low lightshow delay (fast lightshow)
    
    // Define lightshow speed range (delays in ms)
    const uint16_t MIN_LIGHTSHOW_SPEED = 20;   // Fastest lightshow (20ms delay)
    const uint16_t MAX_LIGHTSHOW_SPEED = 200;  // Slowest lightshow (200ms delay)
    
    // Calculate the normalized GPS speed (0.0 to 1.0)
    float gpsSpeedRange = deviceState_.gpsTopSpeed - deviceState_.gpsLowSpeed;
    float normalizedGPSSpeed = (currentGPSSpeed - deviceState_.gpsLowSpeed) / gpsSpeedRange;
    
    // Invert for lightshow speed (higher GPS speed = lower delay)
    float invertedSpeed = 1.0f - normalizedGPSSpeed;
    
    // Map to lightshow speed range
    uint16_t effectiveSpeed = MIN_LIGHTSHOW_SPEED + (uint16_t)(invertedSpeed * (MAX_LIGHTSHOW_SPEED - MIN_LIGHTSHOW_SPEED));
    
    // Debug output
    static unsigned long lastDebugTime = 0;
    if (millis() - lastDebugTime > 5000) { // Debug every 5 seconds
        Serial.printf("[BMDevice] GPS Speed Mapping: GPS=%.1f km/h, Lightshow Speed=%d ms\n", 
                     currentGPSSpeed, effectiveSpeed);
        lastDebugTime = millis();
    }
    
    return effectiveSpeed;
}

// Frame period for position_status: distance from origin (0-1000 m) mapped to
// speed (200 slow .. 20 fast) - closer to the origin cycles faster.
uint16_t BMDevice::positionStatusSpeed() {
    float distance = deviceState_.currentPosition.distance_from(deviceState_.origin);
    return constrain(map((long)distance, 0, 1000, 200, 20), 20, 200);
}

void BMDevice::updateLightShow() {
    // Calculate effective speed (may be GPS-adjusted)
    uint16_t effectiveSpeed = calculateEffectiveSpeed();
    
    // Map LightSceneID to LightShow effect
    switch (deviceState_.currentEffect) {
        case LightSceneID::palette_stream:
            lightShow_.palette_stream(effectiveSpeed, deviceState_.currentPalette, deviceState_.reverseStrip);
            break;
        case LightSceneID::pulse_wave:
            lightShow_.pulse_wave(effectiveSpeed, deviceState_.waveWidth, deviceState_.currentPalette);
            break;
        case LightSceneID::meteor_shower:
            lightShow_.meteor_shower(effectiveSpeed, deviceState_.meteorCount, deviceState_.trailLength, deviceState_.currentPalette);
            break;
        case LightSceneID::fire_plasma:
            lightShow_.fire_plasma(effectiveSpeed, deviceState_.heatVariance, deviceState_.currentPalette);
            break;
        case LightSceneID::kaleidoscope:
            lightShow_.kaleidoscope(effectiveSpeed, deviceState_.mirrorCount, deviceState_.currentPalette);
            break;
        case LightSceneID::rainbow_comet:
            lightShow_.rainbow_comet(effectiveSpeed, deviceState_.cometCount, deviceState_.trailLength);
            break;
        case LightSceneID::matrix_rain:
            lightShow_.matrix_rain(effectiveSpeed, deviceState_.dropRate, deviceState_.effectColor);
            break;
        case LightSceneID::plasma_clouds:
            lightShow_.plasma_clouds(effectiveSpeed, deviceState_.cloudScale, deviceState_.currentPalette);
            break;
        case LightSceneID::lava_lamp:
            lightShow_.lava_lamp(effectiveSpeed, deviceState_.blobCount, deviceState_.currentPalette);
            break;
        case LightSceneID::aurora_borealis:
            lightShow_.aurora_borealis(effectiveSpeed, deviceState_.waveCount, deviceState_.currentPalette);
            break;
        case LightSceneID::lightning_storm:
            lightShow_.lightning_storm(effectiveSpeed, deviceState_.flashIntensity, deviceState_.flashFrequency);
            break;
        case LightSceneID::color_explosion:
            lightShow_.color_explosion(effectiveSpeed, deviceState_.explosionSize, deviceState_.currentPalette);
            break;
        case LightSceneID::spiral_galaxy:
            lightShow_.spiral_galaxy(effectiveSpeed, deviceState_.spiralArms, deviceState_.currentPalette);
            break;
        case LightSceneID::speedometer:
            // GPS speedometer effect - blend colors based on current speed
            if (gpsEnabled_ && deviceState_.positionAvailable) {
                // Normalize speed to 0.0-1.0 range
                float normalizedSpeed = constrain((deviceState_.currentSpeed - deviceState_.gpsLowSpeed) / 
                                                (deviceState_.gpsTopSpeed - deviceState_.gpsLowSpeed), 0.0f, 1.0f);
                
                // Use FastLED blend function to interpolate between slow and fast colors
                CRGB speedColor = blend(deviceState_.gpsSlowColor, deviceState_.gpsFastColor, 
                                      static_cast<uint8_t>(normalizedSpeed * 255));
                
                lightShow_.solid(speedColor);
            } else {
                // Fallback to static slow color if no GPS
                lightShow_.solid(deviceState_.gpsSlowColor);
            }
            break;
        case LightSceneID::position_status:
            // GPS position status effect - use palette cycling with position-based speed
            if (gpsEnabled_ && deviceState_.positionAvailable) {
                lightShow_.palette_stream(positionStatusSpeed(), deviceState_.currentPalette, deviceState_.reverseStrip);
            } else {
                // Fallback to normal palette stream if no GPS
                lightShow_.palette_stream(effectiveSpeed, deviceState_.currentPalette, deviceState_.reverseStrip);
            }
            break;
        default:
            lightShow_.palette_stream(effectiveSpeed, deviceState_.currentPalette, deviceState_.reverseStrip);
            break;
    }
}

void BMDevice::sendStatusUpdate() {
    // Get current defaults for additional status info
    DeviceDefaults defaults = defaults_.getCurrentDefaults();
    
    // Start with the basic device state JSON
    StaticJsonDocument<896> doc;
    
    // Basic device state. Report brightness as 1-100 (percent) for app
    doc["pwr"] = deviceState_.power;
    doc["bri"] = brightnessLevelToPercent(deviceState_.brightness);
    doc["spd"] = deviceState_.speed;
    doc["dir"] = deviceState_.reverseStrip;
    
    const char* effectName = LightShow::effectIdToName(deviceState_.currentEffect);
    BM_LOGV("[BMDevice] sendStatusUpdate: Current effect ID: %u (%s)\n",
            (unsigned)deviceState_.currentEffect, effectName);

    doc["fx"] = effectName;
    doc["pal"] = LightShow::paletteIdToName(deviceState_.currentPalette);
    
    // GPS/Position data
    doc["gps"] = gpsEnabled_;
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
    doc["fwVer"] = FIRMWARE_VERSION;
    
    String status;
    serializeJson(doc, status);
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
        // App sends 1-100 (percent); scale to internal 1-255 and cap by max brightness
        DeviceDefaults defaults = defaults_.getCurrentDefaults();
        int scaledB = brightnessPercentToLevel(b);
        int maxScaled = brightnessPercentToLevel(defaults.maxBrightness);
        setBrightness(min(scaledB, maxScaled));
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
        // A 2-byte write is a numeric id only when the byte is in id range.
        // Valid ids stop well below printable ASCII, so a byte past custom4 is
        // a one-character name ("r") that would otherwise be unselectable.
        if (length == 2 && buffer[1] <= (uint8_t)AvailablePalettes::custom4) {
            uint8_t paletteId = buffer[1];
            // isPaletteAvailable also turns down an empty custom slot, which
            // would otherwise select a palette that renders as nothing.
            if (lightShow_.isPaletteAvailable((AvailablePalettes)paletteId)) {
                setPalette((AvailablePalettes)paletteId);
                Serial.print("[BMDevice] Palette set to ID: ");
                Serial.println(paletteId);
            }
        } else { // String
            char paletteStr[32] = {0};
            memcpy(paletteStr, buffer + 1, min(length - 1, sizeof(paletteStr) - 1));
            AvailablePalettes palette = LightShow::paletteNameToId(paletteStr);
            if (lightShow_.isPaletteAvailable(palette)) {
                setPalette(palette);
                Serial.print("[BMDevice] Palette set to: ");
                Serial.println(paletteStr);
            } else {
                Serial.print("[BMDevice] Ignoring empty custom palette: ");
                Serial.println(paletteStr);
            }
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
            case BLE_FEATURE_HEAT_VARIANCE:
                deviceState_.heatVariance = constrain(value, 1, 100);
                Serial.print("[BMDevice] Heat variance set to: ");
                Serial.println(deviceState_.heatVariance);
                break;
            case BLE_FEATURE_MIRROR_COUNT:
                deviceState_.mirrorCount = constrain(value, 1, 10);
                Serial.print("[BMDevice] Mirror count set to: ");
                Serial.println(deviceState_.mirrorCount);
                break;
            case BLE_FEATURE_COMET_COUNT:
                deviceState_.cometCount = constrain(value, 1, 10);
                Serial.print("[BMDevice] Comet count set to: ");
                Serial.println(deviceState_.cometCount);
                break;
            case BLE_FEATURE_DROP_RATE:
                deviceState_.dropRate = constrain(value, 1, 100);
                Serial.print("[BMDevice] Drop rate set to: ");
                Serial.println(deviceState_.dropRate);
                break;
            case BLE_FEATURE_CLOUD_SCALE:
                deviceState_.cloudScale = constrain(value, 1, 50);
                Serial.print("[BMDevice] Cloud scale set to: ");
                Serial.println(deviceState_.cloudScale);
                break;
            case BLE_FEATURE_BLOB_COUNT:
                deviceState_.blobCount = constrain(value, 1, 20);
                Serial.print("[BMDevice] Blob count set to: ");
                Serial.println(deviceState_.blobCount);
                break;
            case BLE_FEATURE_WAVE_COUNT:
                deviceState_.waveCount = constrain(value, 1, 15);
                Serial.print("[BMDevice] Wave count set to: ");
                Serial.println(deviceState_.waveCount);
                break;
            case BLE_FEATURE_FLASH_INTENSITY:
                deviceState_.flashIntensity = constrain(value, 1, 100);
                Serial.print("[BMDevice] Flash intensity set to: ");
                Serial.println(deviceState_.flashIntensity);
                break;
            case BLE_FEATURE_FLASH_FREQUENCY:
                deviceState_.flashFrequency = constrain(value, 100, 5000);
                Serial.print("[BMDevice] Flash frequency set to: ");
                Serial.println(deviceState_.flashFrequency);
                break;
            case BLE_FEATURE_EXPLOSION_SIZE:
                deviceState_.explosionSize = constrain(value, 1, 50);
                Serial.print("[BMDevice] Explosion size set to: ");
                Serial.println(deviceState_.explosionSize);
                break;
            case BLE_FEATURE_SPIRAL_ARMS:
                deviceState_.spiralArms = constrain(value, 1, 10);
                Serial.print("[BMDevice] Spiral arms set to: ");
                Serial.println(deviceState_.spiralArms);
                break;
            default:
                Serial.printf("[BMDevice] Unknown effect parameter: 0x%02X\n", feature);
                return;
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

void BMDevice::handleSpeedometerFeature(const uint8_t* buffer, size_t length) {
    if (length >= 7) { // 1 feature byte + 3 slow RGB + 3 fast RGB
        uint8_t slowR = buffer[1], slowG = buffer[2], slowB = buffer[3];
        uint8_t fastR = buffer[4], fastG = buffer[5], fastB = buffer[6];
        
        deviceState_.gpsSlowColor = CRGB(slowR, slowG, slowB);
        deviceState_.gpsFastColor = CRGB(fastR, fastG, fastB);
        
        Serial.print("[BMDevice] Speedometer colors set - Slow: RGB(");
        Serial.print(slowR); Serial.print(","); Serial.print(slowG); Serial.print(","); Serial.print(slowB);
        Serial.print("), Fast: RGB(");
        Serial.print(fastR); Serial.print(","); Serial.print(fastG); Serial.print(","); Serial.print(fastB);
        Serial.println(")");
        
        updateLightShow();
    } else {
        Serial.println("[BMDevice] Invalid speedometer data length");
    }
}

// Defaults Management Methods
bool BMDevice::loadDefaults() {
    DeviceDefaults defaults = defaults_.getCurrentDefaults();
    applyDefaults();
    return true;
}

bool BMDevice::saveCurrentAsDefaults() {
    DeviceDefaults currentDefaults = defaults_.getCurrentDefaults();
    DeviceDefaults newDefaults;
    
    // Copy current state to defaults. Internal brightness is 1-255; store as 1-100 for app
    newDefaults.brightness = constrain(brightnessLevelToPercent(deviceState_.brightness), 1, currentDefaults.maxBrightness);
    newDefaults.speed = deviceState_.speed;
    newDefaults.palette = deviceState_.currentPalette;
    newDefaults.effect = deviceState_.currentEffect;
    newDefaults.reverseDirection = deviceState_.reverseStrip;
    newDefaults.effectColor = deviceState_.effectColor;
    
    // Keep existing identity and behavior settings
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
        markStatusDirty();
    } else {
        Serial.println("[BMDevice] Failed to save current state as defaults");
    }
    
    return success;
}

bool BMDevice::resetToFactoryDefaults() {
    bool success = defaults_.resetToFactory();
    if (success) {
        applyDefaults();
        markStatusDirty();
        Serial.println("[BMDevice] Reset to factory defaults and applied");
    } else {
        Serial.println("[BMDevice] Failed to reset to factory defaults");
    }
    return success;
}

void BMDevice::applyDefaults() {
    DeviceDefaults defaults = defaults_.getCurrentDefaults();
    
    // Load the stored palettes first: the default palette may well be one of
    // them, and selecting an empty slot renders black.
    applyCustomPalettes();
    
    // Apply defaults to current state. Stored brightness/max are 1-100; scale to 1-255 for LED
    int scaledB = brightnessPercentToLevel(defaults.brightness);
    int maxScaled = brightnessPercentToLevel(defaults.maxBrightness);
    setBrightness(min(scaledB, maxScaled));
    setEffect(defaults.effect);
    setPalette(defaults.palette);
    deviceState_.speed = defaults.speed;
    deviceState_.reverseStrip = defaults.reverseDirection;
    deviceState_.effectColor = defaults.effectColor;
    deviceState_.power = defaults.autoOn;
    
    // Apply GPS speed settings
    deviceState_.gpsLowSpeed = defaults.gpsLowSpeed;
    deviceState_.gpsTopSpeed = defaults.gpsTopSpeed;
    deviceState_.gpsLightshowSpeedEnabled = defaults.gpsLightshowSpeedEnabled;
    
    // Apply status update interval
    statusUpdateInterval_ = defaults.statusUpdateInterval;
    
    // Update light show
    updateLightShow();
    
    Serial.println("[BMDevice] Applied defaults to current state");
}

void BMDevice::setMaxBrightness(int maxBrightness) {
    bool success = defaults_.setMaxBrightness(maxBrightness);
    if (success) {
        // App sends 1-100; cap internal brightness (1-255) to new max scaled to 1-255
        DeviceDefaults currentDefaults = defaults_.getCurrentDefaults();
        int maxScaled = brightnessPercentToLevel(currentDefaults.maxBrightness);
        if (deviceState_.brightness > maxScaled) {
            setBrightness(maxScaled);
        }
        Serial.print("[BMDevice] Max brightness set to: ");
        Serial.println(currentDefaults.maxBrightness);
        markStatusDirty();
    }
}

void BMDevice::setDeviceOwner(const String& owner) {
    bool success = defaults_.setOwner(owner);
    if (success) {
        Serial.print("[BMDevice] Device owner set to: ");
        Serial.println(owner);
        // The owner is part of the advertised name when no friendly name is set.
        if (dynamicNaming_) {
            bluetoothHandler_.setDeviceName(buildAdvertisedName().c_str());
        }
        markStatusDirty();
    }
}

void BMDevice::setFriendlyName(const String& name) {
    String trimmed = name;
    trimmed.trim();
    if (trimmed.length() == 0) {
        Serial.println("[BMDevice] Ignoring empty device name");
        return;
    }

    bool success = defaults_.setDeviceName(trimmed);
    if (success) {
        Serial.print("[BMDevice] Device name set to: ");
        Serial.println(trimmed);
        // Re-advertise so a scanning app sees the new name without a reboot.
        if (dynamicNaming_) {
            bluetoothHandler_.setDeviceName(buildAdvertisedName().c_str());
        }
        markStatusDirty();
    }
}

String BMDevice::buildAdvertisedName() const {
    DeviceDefaults current = const_cast<BMDeviceDefaults&>(defaults_).getCurrentDefaults();

    // The friendly name wins; the owner is the fallback for devices set up
    // before names existed. Either way the "BMDevice" identifier leads, because
    // that prefix is what the apps match on while scanning.
    String label = current.deviceName;
    label.trim();
    if (label.length() == 0 || label == "BMDevice") {
        label = current.owner;
        label.trim();
    }
    if (label.length() == 0) {
        label = "New";
    }

    // The local name rides in the scan response, which is 31 bytes with 2 of
    // overhead. ArduinoBLE's setLocalName refuses anything longer outright and
    // returns false, and BLE.advertise() then goes out with no name at all -
    // leaving the device invisible to any scan that matches on the name. Trim
    // instead: the full name still reaches the apps in the status payload.
    const unsigned int maxAdvertisedName = 29;
    String advertised = "BMDevice - " + label;
    if (advertised.length() > maxAdvertisedName) {
        advertised = advertised.substring(0, maxAdvertisedName);
        Serial.print("[BMDevice] Advertised name trimmed to fit the scan response: ");
        Serial.println(advertised);
    }
    return advertised;
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

// [0x7C][slot][nameLen][name ASCII][CUSTOM_PALETTE_ENTRIES * RGB]
void BMDevice::handleSetCustomPaletteFeature(const uint8_t* buffer, size_t length) {
    const size_t colorBytes = CUSTOM_PALETTE_ENTRIES * 3;
    if (length < 3) {
        Serial.println("[BMDevice] Custom palette write too short");
        return;
    }
    
    uint8_t slot = buffer[1];
    uint8_t nameLength = buffer[2];
    if (slot >= CUSTOM_PALETTE_COUNT || nameLength > CUSTOM_PALETTE_NAME_MAX) {
        Serial.println("[BMDevice] Custom palette slot or name out of range");
        return;
    }
    if (length != 3 + (size_t)nameLength + colorBytes) {
        Serial.printf("[BMDevice] Custom palette payload is %u bytes, expected %u\n",
                      (unsigned)length, (unsigned)(3 + nameLength + colorBytes));
        return;
    }
    
    char name[CUSTOM_PALETTE_NAME_MAX + 1] = {0};
    memcpy(name, buffer + 3, nameLength);
    
    const uint8_t* rgb = buffer + 3 + nameLength;
    if (!defaults_.setCustomPalette(slot, String(name), rgb)) {
        Serial.println("[BMDevice] Failed to store custom palette");
        return;
    }
    
    CRGB entries[CUSTOM_PALETTE_ENTRIES];
    for (int i = 0; i < CUSTOM_PALETTE_ENTRIES; i++) {
        entries[i] = CRGB(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
    }
    lightShow_.setCustomPalette(slot, entries);
    
    // Re-render if the slot being written is the one already playing, so
    // editing a palette shows up without reselecting it.
    if (deviceState_.currentPalette == LightShow::customPaletteId(slot)) {
        updateLightShow();
    }
    
    Serial.printf("[BMDevice] Custom palette %d set to \"%s\"\n", slot, name);
    markStatusDirty();
}

// [0x7D][slot]
void BMDevice::handleDeleteCustomPaletteFeature(const uint8_t* buffer, size_t length) {
    if (length < 2) {
        return;
    }
    
    uint8_t slot = buffer[1];
    if (slot >= CUSTOM_PALETTE_COUNT) {
        return;
    }
    
    defaults_.clearCustomPalette(slot);
    lightShow_.clearCustomPalette(slot);
    
    // Deleting the palette that is playing would leave the device reporting a
    // palette that no longer exists, so fall back to a built-in one.
    if (deviceState_.currentPalette == LightShow::customPaletteId(slot)) {
        setPalette(AvailablePalettes::cool);
    }
    
    Serial.printf("[BMDevice] Custom palette %d cleared\n", slot);
    markStatusDirty();
}

void BMDevice::applyCustomPalettes() {
    for (int slot = 0; slot < CUSTOM_PALETTE_COUNT; slot++) {
        const CustomPalette* stored = defaults_.getCustomPalette(slot);
        if (stored == nullptr || !stored->used) {
            lightShow_.clearCustomPalette(slot);
            continue;
        }
        
        CRGB entries[CUSTOM_PALETTE_ENTRIES];
        for (int i = 0; i < CUSTOM_PALETTE_ENTRIES; i++) {
            entries[i] = CRGB(stored->rgb[i * 3], stored->rgb[i * 3 + 1], stored->rgb[i * 3 + 2]);
        }
        lightShow_.setCustomPalette(slot, entries);
    }
}

void BMDevice::handleSetDeviceNameFeature(const uint8_t* buffer, size_t length) {
    if (length > 1) {
        char nameStr[33] = {0};
        size_t nameLength = min(length - 1, sizeof(nameStr) - 1);
        memcpy(nameStr, buffer + 1, nameLength);
        setFriendlyName(String(nameStr));
    }
}

void BMDevice::handleSetAutoOnFeature(const uint8_t* buffer, size_t length) {
    if (length >= 2) {
        bool autoOn = buffer[1] != 0;
        bool success = defaults_.setAutoOn(autoOn);
        if (success) {
            Serial.print("[BMDevice] Auto-on set to: ");
            Serial.println(autoOn ? "true" : "false");
            markStatusDirty();
        }
    }
}

void BMDevice::handleSetGPSLowSpeedFeature(const uint8_t* buffer, size_t length) {
    if (length >= 5) {
        float speed;
        memcpy(&speed, buffer + 1, sizeof(float));
        bool success = defaults_.setGPSLowSpeed(speed);
        if (success) {
            // Update device state
            deviceState_.gpsLowSpeed = defaults_.getGPSLowSpeed();
            Serial.print("[BMDevice] GPS low speed set to: ");
            Serial.print(speed);
            Serial.println(" km/h");
            markStatusDirty();
        }
    }
}

void BMDevice::handleSetGPSTopSpeedFeature(const uint8_t* buffer, size_t length) {
    if (length >= 5) {
        float speed;
        memcpy(&speed, buffer + 1, sizeof(float));
        bool success = defaults_.setGPSTopSpeed(speed);
        if (success) {
            // Update device state
            deviceState_.gpsTopSpeed = defaults_.getGPSTopSpeed();
            Serial.print("[BMDevice] GPS top speed set to: ");
            Serial.print(speed);
            Serial.println(" km/h");
            markStatusDirty();
        }
    }
}

void BMDevice::handleSetGPSLightshowSpeedEnabledFeature(const uint8_t* buffer, size_t length) {
    if (length >= 2) {
        bool enabled = buffer[1] != 0;
        bool success = defaults_.setGPSLightshowSpeedEnabled(enabled);
        if (success) {
            // Update device state
            deviceState_.gpsLightshowSpeedEnabled = enabled;
            Serial.print("[BMDevice] GPS lightshow speed control ");
            Serial.println(enabled ? "enabled" : "disabled");
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
            markStatusDirty();
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
            markStatusDirty();
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
    // ONLY supporting the 8 specific pins for ESP32 WROOM
    // Using GRB color order by default (colorOrder parameter ignored for now)
    switch (pin) {
        case 5:
            addLEDStrip<WS2812B, 5, GRB>(ledArray, numLeds);
            break;
        case 12:
            addLEDStrip<WS2812B, 12, GRB>(ledArray, numLeds);
            break;
        case 13:
            addLEDStrip<WS2812B, 13, GRB>(ledArray, numLeds);
            break;
        case 14:
            addLEDStrip<WS2812B, 14, GRB>(ledArray, numLeds);
            break;
        case 16:
            addLEDStrip<WS2812B, 16, GRB>(ledArray, numLeds);
            break;
        case 17:
            addLEDStrip<WS2812B, 17, GRB>(ledArray, numLeds);
            break;
        case 18:
            addLEDStrip<WS2812B, 18, GRB>(ledArray, numLeds);
            break;
        case 27:
            addLEDStrip<WS2812B, 27, GRB>(ledArray, numLeds);
            break;
#ifndef TARGET_ESP32_C6
        // These pins don't exist on ESP32-C6, only compile for ESP32 classic
        case 32:
            addLEDStrip<WS2812B, 32, GRB>(ledArray, numLeds);
            break;
        case 33:
            addLEDStrip<WS2812B, 33, GRB>(ledArray, numLeds);
            break;
#endif
        default:
            Serial.printf("[BMDevice] Error: Pin %d not supported. Only pins 5,12,13,14,16,17,18,27", pin);
#ifndef TARGET_ESP32_C6
            Serial.print(",32,33");
#endif
            Serial.println(" are supported for LEDs.");
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

// Chunked Status Update Implementation
void BMDevice::registerStatusChunk(const String& type, std::function<void()> sendFunction, const String& description) {
    StatusChunk chunk;
    chunk.type = type;
    chunk.sendFunction = sendFunction;
    chunk.description = description;
    statusChunks_.push_back(chunk);
    
    Serial.print("[BMDevice] Registered status chunk: ");
    Serial.print(type);
    if (description.length() > 0) {
        Serial.print(" - ");
        Serial.print(description);
    }
    Serial.println();
}

void BMDevice::startChunkedStatusUpdate() {
    if (statusChunks_.size() == 0) {
        // Fallback to legacy status update if no chunks registered
        sendStatusUpdate();
        return;
    }
    
    statusUpdateState_ = STATUS_SENDING_CHUNKS;
    currentChunkIndex_ = 0;
    statusUpdateTimer_ = millis();
    
    BM_LOGV("[BMDevice] Starting chunked status update (%d chunks)\n", statusChunks_.size());
}

void BMDevice::clearStatusChunks() {
    statusChunks_.clear();
    statusUpdateState_ = STATUS_IDLE;
    Serial.println("[BMDevice] Cleared all status chunks");
}

void BMDevice::handleChunkedStatusUpdate() {
    if (statusUpdateState_ != STATUS_SENDING_CHUNKS) {
        return;
    }
    
    unsigned long currentTime = millis();
    
    // Check if it's time to send the next chunk
    if (currentTime - statusUpdateTimer_ >= STATUS_UPDATE_DELAY) {
        if (currentChunkIndex_ < statusChunks_.size()) {
            // Send current chunk
            StatusChunk& chunk = statusChunks_[currentChunkIndex_];
            BM_LOGV("[BMDevice] Sending chunk %d/%d: %s\n",
                    currentChunkIndex_ + 1, statusChunks_.size(), chunk.type.c_str());
            
            chunk.sendFunction();
            
            // Move to next chunk
            currentChunkIndex_++;
            statusUpdateTimer_ = currentTime;
        } else {
            // All chunks sent
            statusUpdateState_ = STATUS_IDLE;
            BM_LOGV("[BMDevice] Chunked status update complete\n");
        }
    }
}

void BMDevice::sendBasicStatusChunk() {
    // Get current defaults for additional status info
    DeviceDefaults defaults = defaults_.getCurrentDefaults();
    
    // Start with the basic device state JSON
    StaticJsonDocument<512> doc;
    
    // Mark this as basic status chunk
    doc["type"] = "basicStatus";
    
    // Basic device state (same as original sendStatusUpdate). Report brightness as 1-100 for app
    doc["pwr"] = deviceState_.power;
    doc["bri"] = brightnessLevelToPercent(deviceState_.brightness);
    doc["spd"] = deviceState_.speed;
    doc["dir"] = deviceState_.reverseStrip;
    
    const char* effectName = LightShow::effectIdToName(deviceState_.currentEffect);
    doc["fx"] = effectName;
    doc["pal"] = LightShow::paletteIdToName(deviceState_.currentPalette);
    
    // GPS/Position data (abbreviated for size)
    doc["gps"] = gpsEnabled_;
    doc["posAvail"] = deviceState_.positionAvailable;
    doc["spdCur"] = deviceState_.currentSpeed;
    // The GPS speed-control settings ride along so the app's toggle and range
    // reflect the device instead of only ever echoing the app's own writes.
    doc["gpsLightSpdEn"] = deviceState_.gpsLightshowSpeedEnabled;
    doc["gpsLowSpd"] = deviceState_.gpsLowSpeed;
    doc["gpsTopSpd"] = deviceState_.gpsTopSpeed;

    if (deviceState_.positionAvailable) {
        Position& currentPos = const_cast<Position&>(deviceState_.currentPosition);
        JsonObject posObj = doc.createNestedObject("pos");
        posObj["lat"] = currentPos.latitude();
        posObj["lon"] = currentPos.longitude();
    }

    // Essential info only (move others to device config chunk)
    doc["maxBri"] = defaults.maxBrightness;
    
    String status;
    serializeJson(doc, status);
    BM_LOGV("[BMDevice] Basic status chunk: %s\n", status.c_str());
    bluetoothHandler_.sendStatusUpdate(status);
}

void BMDevice::sendDeviceConfigChunk() {
    StaticJsonDocument<512> doc;
    
    // Mark this as device configuration chunk
    doc["type"] = "devConfig";
    
    DeviceDefaults defaults = defaults_.getCurrentDefaults();
    
    // Device configuration - abbreviated keys
    doc["devType"] = defaults.deviceType;
    doc["auto"] = defaults.autoOn;
    doc["gps"] = gpsEnabled_;  // Use runtime GPS state, not saved defaults
    doc["interval"] = defaults.statusUpdateInterval;
    doc["owner"] = defaults.owner;
    doc["deviceName"] = defaults.deviceName;
    doc["fwVer"] = FIRMWARE_VERSION;
    
    // LED strip configuration - abbreviated
    doc["strips"] = defaults.activeLEDStrips;
    JsonArray stripsArray = doc.createNestedArray("leds");
    
    for (int i = 0; i < defaults.activeLEDStrips && i < MAX_LED_STRIPS; i++) {
        if (!defaults.ledStrips[i].enabled) continue;
        
        JsonObject stripObj = stripsArray.createNestedObject();
        stripObj["i"] = i;                                   // index
        stripObj["p"] = defaults.ledStrips[i].pin;           // pin
        stripObj["n"] = defaults.ledStrips[i].numLeds;       // numLeds
        stripObj["o"] = defaults.ledStrips[i].colorOrder;    // colorOrder
        stripObj["e"] = defaults.ledStrips[i].enabled;       // enabled
    }
    
    String status;
    serializeJson(doc, status);
    BM_LOGV("[BMDevice] Device config chunk: %s\n", status.c_str());
    bluetoothHandler_.sendStatusUpdate(status);
}

void BMDevice::sendDefaultsChunk() {
    StaticJsonDocument<512> doc;
    
    // Mark this as defaults chunk
    doc["type"] = "defaults";
    
    DeviceDefaults defaults = defaults_.getCurrentDefaults();
    
    // All default settings - abbreviated keys
    doc["dBri"] = defaults.brightness;
    doc["dSpd"] = defaults.speed;
    doc["dPal"] = LightShow::paletteIdToName(defaults.palette);
    doc["dFx"] = LightShow::effectIdToName(defaults.effect);
    doc["dDir"] = defaults.reverseDirection;
    
    // Effect color - abbreviated
    JsonObject colorObj = doc.createNestedObject("dCol");
    colorObj["r"] = defaults.effectColor.r;
    colorObj["g"] = defaults.effectColor.g;
    colorObj["b"] = defaults.effectColor.b;
    
    // Version info
    doc["ver"] = defaults.version;
    
    String status;
    serializeJson(doc, status);
    BM_LOGV("[BMDevice] Defaults chunk: %s\n", status.c_str());
    bluetoothHandler_.sendStatusUpdate(status);
}

/// One chunk per custom palette slot, so a full palette (16 colours) still fits
/// inside a single notification. An empty slot reports itself as empty rather
/// than staying silent - that is how the apps learn a palette was deleted.
void BMDevice::sendCustomPaletteChunk(int slot) {
    StaticJsonDocument<256> doc;
    doc["type"] = "cpal";
    doc["i"] = slot;
    
    const CustomPalette* palette = defaults_.getCustomPalette(slot);
    if (palette == nullptr || !palette->used) {
        doc["n"] = "";
    } else {
        doc["n"] = palette->name;
        
        // Packed rrggbb per entry, no separators: the whole palette is 96
        // characters that way, which leaves room for the name in one chunk.
        char colors[CUSTOM_PALETTE_ENTRIES * 6 + 1];
        for (int i = 0; i < CUSTOM_PALETTE_ENTRIES; i++) {
            snprintf(colors + i * 6, 7, "%02x%02x%02x",
                     palette->rgb[i * 3], palette->rgb[i * 3 + 1], palette->rgb[i * 3 + 2]);
        }
        doc["c"] = colors;
    }
    
    String status;
    serializeJson(doc, status);
    BM_LOGV("[BMDevice] Custom palette chunk: %s\n", status.c_str());
    bluetoothHandler_.sendStatusUpdate(status);
}

void BMDevice::sendEffectParametersChunk() {
    StaticJsonDocument<512> doc;
    
    // Mark this as effect parameters chunk
    doc["type"] = "effectParams";
    
    // All current effect parameters with abbreviated keys (BLE commands 0x0B-0x19)
    doc["ww"] = deviceState_.waveWidth;           // 0x0B waveWidth
    doc["mc"] = deviceState_.meteorCount;         // 0x0C meteorCount
    doc["tl"] = deviceState_.trailLength;         // 0x0D trailLength
    doc["hv"] = deviceState_.heatVariance;        // 0x0E heatVariance
    doc["mir"] = deviceState_.mirrorCount;        // 0x0F mirrorCount
    doc["cc"] = deviceState_.cometCount;          // 0x10 cometCount
    doc["dr"] = deviceState_.dropRate;            // 0x11 dropRate
    doc["cs"] = deviceState_.cloudScale;          // 0x12 cloudScale
    doc["bc"] = deviceState_.blobCount;           // 0x13 blobCount
    doc["wc"] = deviceState_.waveCount;           // 0x14 waveCount
    doc["fi"] = deviceState_.flashIntensity;      // 0x15 flashIntensity
    doc["ff"] = deviceState_.flashFrequency;      // 0x16 flashFrequency
    doc["es"] = deviceState_.explosionSize;       // 0x17 explosionSize
    doc["sa"] = deviceState_.spiralArms;          // 0x18 spiralArms
    
    // Effect color (0x19) - abbreviated
    JsonObject effectColorObj = doc.createNestedObject("col");
    effectColorObj["r"] = deviceState_.effectColor.r;
    effectColorObj["g"] = deviceState_.effectColor.g;
    effectColorObj["b"] = deviceState_.effectColor.b;
    
    String status;
    serializeJson(doc, status);
    BM_LOGV("[BMDevice] Effect parameters chunk: %s\n", status.c_str());
    bluetoothHandler_.sendStatusUpdate(status);
}

void BMDevice::initializeDefaultStatusChunks() {
    // Clear any existing chunks
    clearStatusChunks();
    
    // Register default chunks that all BMDevice instances will send (using abbreviated types)
    registerStatusChunk("basicStatus", [this]() { sendBasicStatusChunk(); }, "Core device state and settings");
    registerStatusChunk("devConfig", [this]() { sendDeviceConfigChunk(); }, "Device configuration and LED setup");
    registerStatusChunk("effectParams", [this]() { sendEffectParametersChunk(); }, "Effect parameters controlled via BLE commands 0x0B-0x19");
    registerStatusChunk("defaults", [this]() { sendDefaultsChunk(); }, "Persistent default settings");
    for (int slot = 0; slot < CUSTOM_PALETTE_COUNT; slot++) {
        registerStatusChunk("cpal" + String(slot),
                            [this, slot]() { sendCustomPaletteChunk(slot); },
                            "Custom palette slot " + String(slot));
    }
    
    Serial.printf("[BMDevice] Initialized %d default status chunks\n", statusChunks_.size());
} 