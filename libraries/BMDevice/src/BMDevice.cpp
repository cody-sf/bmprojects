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
      lastSyncServiceAt_(0),
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
      lastSyncServiceAt_(0),
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
    // Just long enough for a freshly opened monitor to catch the banner. The
    // old two-second pause was two seconds of dark, full-power boot on every
    // battery in the field.
    delay(250);

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

    // Multi-device sync: same-owner devices trade their look over ESP-NOW
    // broadcast. serviceSync() re-reads the owner every pass, so a BLE owner
    // change moves the device between groups without a reboot.
    sync_.configure(&deviceClock_);
    sync_.setApplyCallback([this](const BMSyncState& state) {
        this->applySyncState(state);
    });
    
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
    
    // Apply each registered strip's saved grouping and brightness ceiling
    // (sketch-added strips are in before begin() runs; the NVRAM-configured
    // ones were just added).
    for (size_t i = 0; i < registeredStripCount_; i++) {
        lightShow_.setStripGroupSize(i, (uint8_t)defaults_.getStripGroupSize((int)i));
        lightShow_.setStripMaxBrightness(i, (uint8_t)defaults_.getStripMaxBrightness((int)i));
    }

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

    // Sync runs before the powered-off early return: an off device still has
    // to broadcast its farewell packet and wind the radio down.
    serviceSync();

    // Handle chunked status updates
    handleChunkedStatusUpdate();

    // Status is pushed on connect and whenever settings change - never on a timer
    if (takeDueStatusUpdate()) {
        startChunkedStatusUpdate();
    }

    // Find-me strobe: overrides everything, including the powered-off blank,
    // because a lost light is usually a switched-off light.
    if (identifyUntil_ != 0) {
        if (millis() < identifyUntil_) {
            bool flashOn = (millis() / 150) % 2 == 0;
            FastLED.showColor(flashOn ? CRGB::White : CRGB::Black, flashOn ? 96 : 0);
            delay(10);
            return;
        }
        identifyUntil_ = 0;
        // Put the show - or the dark - back the way it was.
        lightShow_.requestRepaint();
        ledsBlanked_ = false;
    }

    // Feedback blink (flashFeedback): like the find-me strobe it overrides
    // the powered-off blank - a button press on a dark device should still
    // show where it landed.
    if (feedbackBlinks_ != 0) {
        unsigned long elapsed = millis() - feedbackStartAt_;
        if (elapsed < feedbackBlinks_ * 240UL) {
            bool flashOn = (elapsed / 120) % 2 == 0;
            FastLED.showColor(flashOn ? feedbackColor_ : CRGB::Black, flashOn ? 96 : 0);
            delay(10);
            return;
        }
        feedbackBlinks_ = 0;
        lightShow_.requestRepaint();
        ledsBlanked_ = false;
    }

    // Wiring test, bypassing maps, render orders and grids. Two alternating
    // phases: a rainbow along the raw LED chain (hue = chain index) shows the
    // macro layout, and a corner pattern - the first three 8-LED runs in red /
    // green / blue, each run's first LED white - shows where the chain starts
    // and whether the runs zigzag, which the rainbow's 11-degree in-column
    // drift cannot. Overrides the powered-off blank like the strobe - it's a
    // bench diagnostic. Reading guide: generate_matrix_map.py.
    if (matrixTestUntil_ != 0) {
        if (millis() < matrixTestUntil_) {
            // Three rotating phases: chain rainbow (macro layout), corner
            // runs (start corner + zigzag), and the grid test card (the whole
            // display pipeline, no app content involved).
            uint8_t phase = (millis() / 4000) % 3;
            if (phase == 2) {
                lightShow_.renderMatrixTestCard(48);
                delay(50);
                return;
            }
            bool cornerPhase = phase == 1;
            for (int i = 0; i < FastLED.count(); i++) {
                CLEDController &ctrl = FastLED[i];
                CRGB *leds = ctrl.leds();
                for (int n = 0; n < ctrl.size(); n++) {
                    if (!cornerPhase) {
                        leds[n] = CHSV((uint8_t)n, 255, 255);
                    } else if (n < 24) {
                        leds[n] = (n % 8 == 0) ? CRGB(255, 255, 255)
                                               : (n < 8 ? CRGB(255, 0, 0)
                                                        : (n < 16 ? CRGB(0, 255, 0)
                                                                  : CRGB(0, 0, 255)));
                    } else {
                        leds[n] = CRGB(4, 4, 8);
                    }
                }
                // Modest brightness: a full-panel rainbow at play brightness
                // is a power spike, and the camera reads hues better dim.
                ctrl.showLeds(48);
            }
            delay(50);
            return;
        }
        matrixTestUntil_ = 0;
        lightShow_.requestRepaint();
        ledsBlanked_ = false;
    }

    // Handle power state
    if (!deviceState_.power) {
        // Blank on the transition, then re-clock the dark frame once a second
        // - not every loop, which burns the CPU and the LED data lines for no
        // visible difference (8 x 450 LEDs is >100 ms of blocking output).
        //
        // The blank used to be one parallel FastLED.show(), the only place
        // strips ever clocked out concurrently - the render path shows them
        // one at a time. With WiFi (and now MQTT) busy, RMT interrupt latency
        // during that parallel burst could cost a channel its frame, and a
        // strip that misses the one-shot blank holds its last frame until the
        // next power-on: the hotel sign's VACANCY strip stayed lit at full
        // every time. Sequential output plus the periodic re-clock means a
        // missed frame - or a noise-latched pixel during a long off stretch -
        // goes dark within a second.
        if (!ledsBlanked_ || millis() - lastBlankAt_ >= 1000) {
            FastLED.clear();
            for (int i = 0; i < FastLED.count(); i++) {
                FastLED[i].showLeds(0);
            }
            ledsBlanked_ = true;
            lastBlankAt_ = millis();
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

    // Sleep until the show wants its next frame (a running transition reports
    // its own faster tick), capped at 10 ms so BLE writes and encoder input
    // are still picked up promptly. Frames are gated on millis() inside
    // render(), so this only trims idle spinning - it cannot slow a show down.
    unsigned long idleMs = lightShow_.msUntilNextFrame(millis());
    if (idleMs > 10) idleMs = 10;
    delay(idleMs > 0 ? idleMs : 1);
}

void BMDevice::markStatusDirty() {
    statusDirty_ = true;
    statusDirtyAt_ = millis();
}

void BMDevice::serviceSync() {
    unsigned long now = millis();
    if (now - lastSyncServiceAt_ < 100) {
        return;
    }
    lastSyncServiceAt_ = now;

    sync_.setGroup(defaults_.getCurrentDefaults().owner);

    BMSyncState current;
    current.power = deviceState_.power ? 1 : 0;
    // Brightness syncs as a fraction of each device's *own* max, not an
    // absolute level: a pack capped at 100% and a hat capped at 40% both at
    // "half" read 50 on the wire, so the group dims together proportionally
    // instead of every device slamming into the weakest cap.
    int maxPercent = defaults_.getCurrentDefaults().maxBrightness;
    if (maxPercent <= 0) maxPercent = 100;
    int absPercent = brightnessLevelToPercent(deviceState_.brightness);
    current.brightnessPercent = (uint8_t)constrain((absPercent * 100 + maxPercent / 2) / maxPercent, 0, 100);
    current.speed = deviceState_.speed;
    current.reverse = deviceState_.reverseStrip ? 1 : 0;
    current.effect = (uint8_t)deviceState_.currentEffect;
    current.palette = (uint8_t)deviceState_.currentPalette;

    // The radio only listens while the lights are on: always-on ESP-NOW
    // receive costs tens of mA, which is real money on a battery but noise
    // next to a running show. A switched-off device rejoins on power-up.
    sync_.service(current, syncAvailable_ && defaults_.isSyncEnabled() && deviceState_.power);
}

void BMDevice::applySyncState(const BMSyncState& state) {
    deviceState_.power = state.power != 0;

    // The wire carries brightness as a fraction of the sender's max; scale it
    // by this device's own max so every device sits at the same *relative*
    // level and none exceeds its cap.
    DeviceDefaults defaults = defaults_.getCurrentDefaults();
    int maxPercent = defaults.maxBrightness;
    if (maxPercent <= 0) maxPercent = 100;
    int absPercent = ((int)state.brightnessPercent * maxPercent + 50) / 100;
    setBrightness(brightnessPercentToLevel(absPercent));

    deviceState_.speed = constrain((int)state.speed, 5, 200);
    deviceState_.reverseStrip = state.reverse != 0;

    if (state.effect <= (uint8_t)LIGHT_SCENE_ID_MAX) {
        LightSceneID fx = (LightSceneID)state.effect;
        // An old-firmware sender may still broadcast a display id; the
        // matrix display is local content now, not a look to adopt.
        if (fx != LightSceneID::panel_text && fx != LightSceneID::panel_words &&
            fx != LightSceneID::panel_bitmap && fx != LightSceneID::panel_anim) {
            deviceState_.currentEffect = fx;
        }
    }
    // isPaletteAvailable also turns down a custom slot this device has not
    // been dealt - the rest of the packet still applies.
    if (state.palette <= (uint8_t)AvailablePalettes::custom4 &&
        lightShow_.isPaletteAvailable((AvailablePalettes)state.palette)) {
        deviceState_.currentPalette = (AvailablePalettes)state.palette;
    }

    updateLightShow();
    markStatusDirty();  // the connected phone should see the group's change
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
    // The display "effects" became the matrix overlay (0x89). Every path that
    // sets an effect funnels through here - BLE by id, BLE by name, boot
    // defaults - so an old client (or an old NVRAM default) that selects one
    // switches the display on instead, and the running effect stays put.
    switch (effect) {
        case LightSceneID::panel_text:
            setMatrixDisplay(MATRIX_DISPLAY_TEXT);
            return;
        case LightSceneID::panel_words:
            setMatrixDisplay(MATRIX_DISPLAY_WORDS);
            return;
        case LightSceneID::panel_bitmap:
            setMatrixDisplay(MATRIX_DISPLAY_BITMAP);
            return;
        case LightSceneID::panel_anim:
            setMatrixDisplay(MATRIX_DISPLAY_ANIM);
            return;
        default:
            break;
    }
    deviceState_.currentEffect = effect;
    updateLightShow();
}

void BMDevice::setPalette(AvailablePalettes palette) {
    deviceState_.currentPalette = palette;
    updateLightShow();
}

void BMDevice::setMatrixDisplay(uint8_t mode) {
    if (mode > MATRIX_DISPLAY_MAX) {
        mode = MATRIX_DISPLAY_OFF;
    }
    defaults_.setMatrixDisplay(mode);
    lightShow_.setMatrixDisplay(mode);
    Serial.printf("[BMDevice] Matrix display mode set to %u\n", mode);
    markStatusDirty();
}

void BMDevice::flashFeedback(const CRGB& color, uint8_t blinks) {
    feedbackColor_ = color;
    feedbackBlinks_ = blinks;
    feedbackStartAt_ = millis();
}

void BMDevice::setMatrixSpeed(uint16_t ms) {
    defaults_.setMatrixSpeed(ms);
    // The defaults setter clamps; feed the show the same value it stored.
    lightShow_.setMatrixSpeed(defaults_.getMatrixSpeed());
    Serial.printf("[BMDevice] Matrix display speed set to %u ms\n",
                  defaults_.getMatrixSpeed());
    markStatusDirty();
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
            // [0x02] alone asks for the full chunk burst. [0x02][0x01] asks
            // for just the first registered chunk - the live one (basicStatus
            // here, "batt" on the charger) - which is what the app's interval
            // poll uses: one notification instead of the whole burst.
            if (length >= 2 && buffer[1] == 0x01) {
                if (!statusChunks_.empty()) {
                    statusChunks_[0].sendFunction();
                } else {
                    sendStatusUpdate();
                }
                return;
            }
            // Full refresh (page opened). Answer now rather than waiting on
            // the settle window.
            statusDirty_ = true;
            statusDirtyAt_ = 0;
            lastStatusSentAt_ = 0;
            return;
        case BLE_FEATURE_IDENTIFY:
            // Strobe for 5 s; the app re-sends while its Find screen is open,
            // so the strobe dies on its own if the phone walks away.
            identifyUntil_ = millis() + 5000;
            Serial.println("[BMDevice] Identify: find-me strobe for 5s");
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
        case BLE_FEATURE_SET_SYNC_ENABLED:
            handleSetSyncEnabledFeature(buffer, length);
            break;
        case BLE_FEATURE_SET_STRIP_GROUP:
            handleSetStripGroupFeature(buffer, length);
            break;
        case BLE_FEATURE_SET_STRIP_BRIGHTNESS:
            handleSetStripBrightnessFeature(buffer, length);
            break;
        case BLE_FEATURE_SET_MARQUEE_TEXT:
            handleSetMarqueeTextFeature(buffer, length);
            break;
        case BLE_FEATURE_SET_MATRIX_BITMAP:
            handleSetMatrixBitmapFeature(buffer, length);
            break;
        case BLE_FEATURE_CLEAR_MATRIX_BITMAP:
            handleClearMatrixBitmapFeature(buffer, length);
            break;
        case BLE_FEATURE_MATRIX_TEST:
            matrixTestUntil_ = millis() + 30000;
            Serial.println("[BMDevice] Matrix wiring test: chain rainbow for 30s");
            return;
        case BLE_FEATURE_SET_TEXT_STYLE:
            if (length >= 2) {
                uint8_t style = buffer[1] ? 1 : 0;
                defaults_.setTextStyle(style);
                // Live, like the text itself - the next frame renders bold.
                lightShow_.setTextStyle(style);
                Serial.printf("[BMDevice] Text style set to %s\n", style ? "bold" : "normal");
                markStatusDirty();
            }
            return;
        case BLE_FEATURE_SET_TEXT_FILL:
            if (length >= 2) {
                uint8_t fill = buffer[1] < 4 ? buffer[1] : 0;
                defaults_.setTextFill(fill);
                // Live, like the style - the next frame renders the new fill.
                lightShow_.setTextFill(fill);
                Serial.printf("[BMDevice] Text fill set to %u\n", fill);
                markStatusDirty();
            }
            return;
        case BLE_FEATURE_SET_ANIM_FRAME:
            handleSetAnimFrameFeature(buffer, length);
            break;
        case BLE_FEATURE_CLEAR_ANIM_FRAMES:
            handleClearAnimFramesFeature(buffer, length);
            break;
        case BLE_FEATURE_SET_MATRIX_DISPLAY:
            if (length >= 2) {
                setMatrixDisplay(buffer[1]);
            }
            return;
        case BLE_FEATURE_SET_MATRIX_SPEED:
            if (length >= 2) {
                // int32 LE from the apps; a bare byte still parses (LSB
                // first), which keeps hand-rolled clients honest.
                int value = 0;
                memcpy(&value, buffer + 1, min(length - 1, sizeof(int)));
                if (value > 0) {
                    setMatrixSpeed((uint16_t)min(value, 0xFFFF));
                }
            }
            return;
        
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

    // Keep the matrix display overlay's styling in step: the marquee's glyph
    // fills draw from the current palette and the scroll follows the
    // direction toggle, even though the display itself is not an effect.
    lightShow_.setMatrixStyle(deviceState_.currentPalette, !deviceState_.reverseStrip);

    // The chevron and panel effects need panel geometry; the straight-strip
    // fallback they'd get on unmapped rigs reads as noise. Render the nearest
    // 1D look instead while *reporting* the panel id unchanged, so a synced
    // group can agree on one effect even when only the bike can draw it
    // properly.
    if (!lightShow_.hasPanelMap()) {
        switch (deviceState_.currentEffect) {
            case LightSceneID::chevron_wave:
            case LightSceneID::chevron_chase:
            case LightSceneID::chevron_burst:
            case LightSceneID::chevron_glow:
            case LightSceneID::chevron_eq:
                lightShow_.color_explosion(effectiveSpeed, deviceState_.explosionSize, deviceState_.currentPalette);
                return;
            case LightSceneID::panel_waves:
                lightShow_.plasma_clouds(effectiveSpeed, deviceState_.cloudScale, deviceState_.currentPalette);
                return;
            case LightSceneID::panel_fire:
                lightShow_.fire_plasma(effectiveSpeed, deviceState_.heatVariance, deviceState_.currentPalette);
                return;
            case LightSceneID::panel_rain:
                lightShow_.matrix_rain(effectiveSpeed, deviceState_.dropRate, deviceState_.effectColor);
                return;
            case LightSceneID::panel_spin:
                lightShow_.spiral_galaxy(effectiveSpeed, deviceState_.spiralArms, deviceState_.currentPalette);
                return;
            case LightSceneID::starfield:
                lightShow_.meteor_shower(effectiveSpeed, deviceState_.meteorCount, deviceState_.trailLength, deviceState_.currentPalette);
                return;
            case LightSceneID::panel_puddle:
                lightShow_.matrix_rain(effectiveSpeed, deviceState_.dropRate, deviceState_.effectColor);
                return;
            default:
                break;
        }
    }

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
        // The new effects reuse existing effect-parameter fields (and so their
        // existing BLE codes): scale rides cloudScale, density rides dropRate,
        // ripple width rides waveWidth, the cylon trail rides trailLength.
        case LightSceneID::noise_flow:
            lightShow_.noise_flow(effectiveSpeed, deviceState_.cloudScale, deviceState_.currentPalette);
            break;
        case LightSceneID::twinkle:
            lightShow_.twinkle(effectiveSpeed, deviceState_.dropRate, deviceState_.currentPalette);
            break;
        case LightSceneID::ripple:
            lightShow_.ripple(effectiveSpeed, deviceState_.waveWidth, deviceState_.currentPalette);
            break;
        case LightSceneID::cylon:
            lightShow_.cylon(effectiveSpeed, deviceState_.trailLength, deviceState_.currentPalette);
            break;
        case LightSceneID::fireworks:
            lightShow_.fireworks(effectiveSpeed, deviceState_.explosionSize, deviceState_.currentPalette);
            break;
        // Chevron (panel) effects reuse existing parameter fields too. Their
        // direction is the strip-direction toggle, with false = the natural
        // way: waves toward the front, bursts out of the points.
        case LightSceneID::chevron_wave:
            lightShow_.chevron_wave(effectiveSpeed, deviceState_.waveWidth, deviceState_.currentPalette, !deviceState_.reverseStrip);
            break;
        case LightSceneID::chevron_chase:
            lightShow_.chevron_chase(effectiveSpeed, deviceState_.trailLength, deviceState_.currentPalette, !deviceState_.reverseStrip);
            break;
        case LightSceneID::chevron_burst:
            lightShow_.chevron_burst(effectiveSpeed, deviceState_.waveWidth, deviceState_.currentPalette, !deviceState_.reverseStrip);
            break;
        case LightSceneID::chevron_glow:
            lightShow_.chevron_glow(effectiveSpeed, deviceState_.currentPalette);
            break;
        case LightSceneID::chevron_eq:
            lightShow_.chevron_eq(effectiveSpeed, deviceState_.currentPalette);
            break;
        // The FastLED classics reuse existing parameter fields (and BLE codes)
        // like the batch above: confetti density rides dropRate, juggle's ball
        // count rides cometCount, the sinelon trail rides trailLength. bpm and
        // pacifica have no knob beyond the speed slider.
        case LightSceneID::confetti:
            lightShow_.confetti(effectiveSpeed, deviceState_.dropRate, deviceState_.currentPalette);
            break;
        case LightSceneID::juggle:
            lightShow_.juggle(effectiveSpeed, deviceState_.cometCount, deviceState_.currentPalette);
            break;
        case LightSceneID::sinelon:
            lightShow_.sinelon(effectiveSpeed, deviceState_.trailLength, deviceState_.currentPalette);
            break;
        case LightSceneID::bpm:
            lightShow_.bpm(effectiveSpeed, deviceState_.currentPalette);
            break;
        case LightSceneID::pacifica:
            lightShow_.pacifica(effectiveSpeed);
            break;
        // True-2D panel effects, same parameter-reuse scheme: the wave scale
        // rides cloudScale, flame height rides heatVariance, rain density
        // rides dropRate, and the spoke count rides spiralArms.
        case LightSceneID::panel_waves:
            lightShow_.panel_waves(effectiveSpeed, deviceState_.cloudScale, deviceState_.currentPalette);
            break;
        case LightSceneID::panel_fire:
            lightShow_.panel_fire(effectiveSpeed, deviceState_.heatVariance, deviceState_.currentPalette);
            break;
        case LightSceneID::panel_rain:
            lightShow_.panel_rain(effectiveSpeed, deviceState_.dropRate, deviceState_.currentPalette);
            break;
        case LightSceneID::panel_spin:
            lightShow_.panel_spin(effectiveSpeed, deviceState_.spiralArms, deviceState_.currentPalette);
            break;
        // Same parameter-reuse scheme as the rest: star density rides
        // dropRate, the comet tail rides trailLength, rain density dropRate.
        case LightSceneID::starfield:
            lightShow_.starfield(effectiveSpeed, deviceState_.dropRate, deviceState_.currentPalette, !deviceState_.reverseStrip);
            break;
        case LightSceneID::orbit_comet:
            lightShow_.orbit_comet(effectiveSpeed, deviceState_.trailLength, deviceState_.currentPalette, !deviceState_.reverseStrip);
            break;
        case LightSceneID::panel_puddle:
            lightShow_.panel_puddle(effectiveSpeed, deviceState_.dropRate, deviceState_.currentPalette);
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
            if (effectId <= (uint8_t)LIGHT_SCENE_ID_MAX) {
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
    // Start from the stored defaults so everything this button does not mean
    // to capture - identity, strip rows, GPS tuning, sync, device type -
    // survives the save. Building from a fresh DeviceDefaults (factory
    // settings) and copying fields across silently reset whatever was not on
    // the copy list.
    DeviceDefaults newDefaults = defaults_.getCurrentDefaults();

    // The look. Internal brightness is 1-255; store as 1-100 for app
    newDefaults.brightness = constrain(brightnessLevelToPercent(deviceState_.brightness), 1, newDefaults.maxBrightness);
    newDefaults.speed = deviceState_.speed;
    newDefaults.palette = deviceState_.currentPalette;
    newDefaults.effect = deviceState_.currentEffect;
    newDefaults.reverseDirection = deviceState_.reverseStrip;
    newDefaults.effectColor = deviceState_.effectColor;

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
    // And the marquee text / bitmap, in case the default effect displays them.
    applyMatrixArt();
    
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

// [0x80][ASCII text]
void BMDevice::handleSetMarqueeTextFeature(const uint8_t* buffer, size_t length) {
    char text[MARQUEE_TEXT_MAX + 1] = {0};
    size_t textLength = length > 1 ? min(length - 1, (size_t)MARQUEE_TEXT_MAX) : 0;
    memcpy(text, buffer + 1, textLength);

    defaults_.setMarqueeText(String(text));
    // Live: panel_text reads the stored text every frame, mid-scroll included.
    lightShow_.setMarqueeText(text);

    Serial.printf("[BMDevice] Marquee text set to \"%s\"\n", text);
    markStatusDirty();
}

// [0x81][w][h][MATRIX_BITMAP_COLORS * RGB][ceil(w*h/2) packed 4bpp pixels]
void BMDevice::handleSetMatrixBitmapFeature(const uint8_t* buffer, size_t length) {
    const size_t paletteBytes = MATRIX_BITMAP_COLORS * 3;
    if (length < 3 + paletteBytes) {
        Serial.println("[BMDevice] Matrix bitmap write too short");
        return;
    }

    uint8_t w = buffer[1];
    uint8_t h = buffer[2];
    if (w == 0 || h == 0 || (size_t)w * h > MATRIX_BITMAP_MAX_PIXELS) {
        Serial.println("[BMDevice] Matrix bitmap dimensions out of range");
        return;
    }
    size_t pixelBytes = ((size_t)w * h + 1) / 2;
    if (length != 3 + paletteBytes + pixelBytes) {
        Serial.printf("[BMDevice] Matrix bitmap payload is %u bytes, expected %u\n",
                      (unsigned)length, (unsigned)(3 + paletteBytes + pixelBytes));
        return;
    }

    // The stored blob is the payload minus the feature byte, so load and save
    // stay the same bytes.
    if (!defaults_.setMatrixBitmap(buffer + 1, length - 1)) {
        Serial.println("[BMDevice] Failed to store matrix bitmap");
        return;
    }
    lightShow_.setMatrixBitmap(w, h, buffer + 3, buffer + 3 + paletteBytes);

    Serial.printf("[BMDevice] Matrix bitmap set: %ux%u\n", w, h);
    markStatusDirty();
}

// [0x82]
void BMDevice::handleClearMatrixBitmapFeature(const uint8_t* buffer, size_t length) {
    (void)buffer;
    (void)length;
    defaults_.clearMatrixBitmap();
    lightShow_.clearMatrixBitmap();
    Serial.println("[BMDevice] Matrix bitmap cleared");
    markStatusDirty();
}

// [0x86][slot][w][h][MATRIX_BITMAP_COLORS * RGB][ceil(w*h/2) packed 4bpp pixels]
void BMDevice::handleSetAnimFrameFeature(const uint8_t* buffer, size_t length) {
    const size_t paletteBytes = MATRIX_BITMAP_COLORS * 3;
    if (length < 4 + paletteBytes) {
        Serial.println("[BMDevice] Anim frame write too short");
        return;
    }

    uint8_t slot = buffer[1];
    uint8_t w = buffer[2];
    uint8_t h = buffer[3];
    if (slot >= MATRIX_ANIM_MAX_FRAMES || w == 0 || h == 0 ||
        (size_t)w * h > MATRIX_BITMAP_MAX_PIXELS) {
        Serial.println("[BMDevice] Anim frame slot/dimensions out of range");
        return;
    }
    size_t pixelBytes = ((size_t)w * h + 1) / 2;
    if (length != 4 + paletteBytes + pixelBytes) {
        Serial.printf("[BMDevice] Anim frame payload is %u bytes, expected %u\n",
                      (unsigned)length, (unsigned)(4 + paletteBytes + pixelBytes));
        return;
    }

    // The stored blob is [w][h][palette][pixels] - the bitmap's format - so
    // load and save share the parsing.
    if (!defaults_.setAnimFrame(slot, buffer + 2, length - 2)) {
        Serial.println("[BMDevice] Failed to store anim frame");
        return;
    }
    lightShow_.setAnimFrame(slot, w, h, buffer + 4, buffer + 4 + paletteBytes);

    Serial.printf("[BMDevice] Anim frame %u set: %ux%u\n", slot, w, h);
    markStatusDirty();
}

// [0x87]
void BMDevice::handleClearAnimFramesFeature(const uint8_t* buffer, size_t length) {
    (void)buffer;
    (void)length;
    defaults_.clearAnimFrames();
    lightShow_.clearAnimFrames();
    Serial.println("[BMDevice] Anim frames cleared");
    markStatusDirty();
}

void BMDevice::applyMatrixArt() {
    lightShow_.setMarqueeText(defaults_.getMarqueeText().c_str());
    lightShow_.setTextStyle(defaults_.getTextStyle());
    lightShow_.setTextFill(defaults_.getTextFill());
    // The display overlay's mode and pace persist like the content does, so
    // the bike boots straight back into whatever it was showing.
    lightShow_.setMatrixSpeed(defaults_.getMatrixSpeed());
    lightShow_.setMatrixDisplay(defaults_.getMatrixDisplay());

    uint8_t blob[2 + MATRIX_BITMAP_COLORS * 3 + MATRIX_BITMAP_MAX_PIXELS / 2];
    // A frame blob shares the bitmap's [w][h][palette][pixels] layout, and a
    // gap ends playback, so loading stops at the first empty slot.
    for (uint8_t slot = 0; slot < MATRIX_ANIM_MAX_FRAMES; slot++) {
        size_t stored = defaults_.getAnimFrame(slot, blob, sizeof(blob));
        if (stored < 2 + (size_t)MATRIX_BITMAP_COLORS * 3) {
            break;
        }
        uint8_t w = blob[0];
        uint8_t h = blob[1];
        if (w == 0 || h == 0 || (size_t)w * h > MATRIX_BITMAP_MAX_PIXELS ||
            stored != 2 + (size_t)MATRIX_BITMAP_COLORS * 3 + ((size_t)w * h + 1) / 2) {
            break;
        }
        lightShow_.setAnimFrame(slot, w, h, blob + 2, blob + 2 + MATRIX_BITMAP_COLORS * 3);
    }

    size_t stored = defaults_.getMatrixBitmap(blob, sizeof(blob));
    if (stored < 2 + (size_t)MATRIX_BITMAP_COLORS * 3) {
        return;
    }
    uint8_t w = blob[0];
    uint8_t h = blob[1];
    if (w == 0 || h == 0 || (size_t)w * h > MATRIX_BITMAP_MAX_PIXELS ||
        stored != 2 + (size_t)MATRIX_BITMAP_COLORS * 3 + ((size_t)w * h + 1) / 2) {
        return;
    }
    lightShow_.setMatrixBitmap(w, h, blob + 2, blob + 2 + MATRIX_BITMAP_COLORS * 3);
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

void BMDevice::handleSetSyncEnabledFeature(const uint8_t* buffer, size_t length) {
    if (length >= 2) {
        bool enabled = buffer[1] != 0;
        defaults_.setSyncEnabled(enabled);
        // serviceSync() picks the change up on its next pass: enabling joins
        // the group (radio up, QUERY out), disabling drops the radio.
        markStatusDirty();
        Serial.print("[BMDevice] Device sync ");
        Serial.println(enabled ? "enabled" : "disabled");
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

// [0x7F][stripIndex][ledsPerPixel]: mark a strand as grouped (a 12 V glow
// strip drives 6 LEDs from every pixel) or normal. Indexed by registration
// order - the same order the "strips" chunk reports - so it reaches strips
// the sketch hardcodes (the bike's chevron panel) as well as NVRAM rows.
void BMDevice::handleSetStripGroupFeature(const uint8_t* buffer, size_t length) {
    if (length >= 3) {
        int stripIndex = buffer[1];
        int groupSize = buffer[2];
        if (stripIndex < 0 || (size_t)stripIndex >= registeredStripCount_) {
            Serial.printf("[BMDevice] Strip group: no strip %d (have %d)\n",
                         stripIndex, (int)registeredStripCount_);
            return;
        }
        defaults_.setStripGroupSize(stripIndex, groupSize);
        lightShow_.setStripGroupSize((size_t)stripIndex, (uint8_t)constrain(groupSize, 1, 12));
        markStatusDirty();
        Serial.printf("[BMDevice] Strip %d (pin %d): %dx grouping\n",
                     stripIndex, registeredStrips_[stripIndex].pin, groupSize);
    }
}

// [0x88][stripIndex][max 1-255]: cap one strand's brightness. Same
// registration-order index as 0x7F; the master brightness scales within the
// cap, so the rig still dims together on one slider.
void BMDevice::handleSetStripBrightnessFeature(const uint8_t* buffer, size_t length) {
    if (length >= 3) {
        int stripIndex = buffer[1];
        int maxBrightness = buffer[2];
        if (stripIndex < 0 || (size_t)stripIndex >= registeredStripCount_) {
            Serial.printf("[BMDevice] Strip brightness: no strip %d (have %d)\n",
                         stripIndex, (int)registeredStripCount_);
            return;
        }
        defaults_.setStripMaxBrightness(stripIndex, maxBrightness);
        lightShow_.setStripMaxBrightness((size_t)stripIndex, (uint8_t)constrain(maxBrightness, 1, 255));
        lightShow_.requestRepaint();
        markStatusDirty();
        Serial.printf("[BMDevice] Strip %d (pin %d): max brightness %d/255\n",
                     stripIndex, registeredStrips_[stripIndex].pin, maxBrightness);
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
    doc["sync"] = defaults.syncEnabled;
    doc["gps"] = gpsEnabled_;  // Use runtime GPS state, not saved defaults
    doc["interval"] = defaults.statusUpdateInterval;
    doc["owner"] = defaults.owner;
    doc["deviceName"] = defaults.deviceName;
    doc["fwVer"] = FIRMWARE_VERSION;

    // Strips actually registered with the show. The per-strip rows moved to
    // the compact "strips" chunk: as an array here they overflowed both this
    // doc and the notify payload on an 8-strip rig.
    doc["strips"] = (int)registeredStripCount_;

    String status;
    serializeJson(doc, status);
    BM_LOGV("[BMDevice] Device config chunk: %s\n", status.c_str());
    bluetoothHandler_.sendStatusUpdate(status);
}

void BMDevice::sendRadioChunk() {
    // Sync radio diagnostics, so a bench test can see the radio state and
    // packet counts from the app instead of needing a serial cable. In their
    // own chunk because the counters grow: two lifetime packet counts at ten
    // digits each were part of what pushed devConfig past the ~239-byte
    // notify payload (see sendMatrixChunk for the failure mode).
    StaticJsonDocument<128> doc;
    doc["type"] = "radio";
    doc["syncSt"] = sync_.radioStateName();
    doc["syncTx"] = sync_.txCount();
    doc["syncRx"] = sync_.rxCount();

    String status;
    serializeJson(doc, status);
    BM_LOGV("[BMDevice] Radio chunk: %s\n", status.c_str());
    bluetoothHandler_.sendStatusUpdate(status);
}

void BMDevice::sendMatrixChunk() {
    // Matrix display state, in its own chunk. These keys used to ride
    // devConfig, which the txtFill/anim/sync-counter additions pushed past
    // the ~239-byte notify payload - a truncated chunk parses as nothing, so
    // every central silently lost mtxW (and with it the whole matrix UI).
    // Same story as the old per-strip rows; same fix. Every client merges
    // status keys regardless of chunk type, so the move is invisible to them.
    // 384: ten-ish slots plus a full-length marquee copied into the pool -
    // the SERIALIZED chunk stays well under the ~239-byte notify ceiling.
    StaticJsonDocument<384> doc;

    // The grid dimensions (0/absent = no display, which is how the apps know
    // whether to offer the text/pixel-art modes at all), the marquee text (so
    // an app's input can show what the device will scroll), the text style
    // and fill, and whether a bitmap / how many animation frames are stored.
    // The pixel content itself never rides status - the phone keeps its own.
    doc["type"] = "matrix";
    doc["mtxW"] = lightShow_.matrixGridWidth();
    doc["mtxH"] = lightShow_.matrixGridHeight();
    doc["marquee"] = defaults_.getMarqueeText();
    doc["bmp"] = lightShow_.hasMatrixBitmap();
    doc["txtSty"] = defaults_.getTextStyle();
    doc["txtFill"] = defaults_.getTextFill();
    doc["anim"] = lightShow_.animFrameCount();
    // The display overlay: what the panel is showing (0 = the effect owns
    // it) and the display's own pace in ms. Both independent of the effect
    // keys in the basic chunk.
    doc["disp"] = defaults_.getMatrixDisplay();
    doc["mtxMs"] = defaults_.getMatrixSpeed();

    String status;
    serializeJson(doc, status);
    BM_LOGV("[BMDevice] Matrix chunk: %s\n", status.c_str());
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
/// The strips actually playing the show, one compact row per strip:
/// "index,pin,ledCount,groupSize,maxBrightness;..." - a packed string rather
/// than a JSON array because eight object rows overflow a single notify, and
/// this must stay one chunk (two chunks would overwrite each other in the
/// app). New fields append to the row: the app takes what it knows and
/// defaults the rest, so either side can update first.
void BMDevice::sendStripsChunk() {
    StaticJsonDocument<384> doc;
    doc["type"] = "strips";

    String rows;
    for (size_t i = 0; i < registeredStripCount_; i++) {
        if (rows.length() > 0) rows += ';';
        rows += String((int)i) + ',' + String(registeredStrips_[i].pin) + ',' +
                String(registeredStrips_[i].numLeds) + ',' +
                String(defaults_.getStripGroupSize((int)i)) + ',' +
                String(defaults_.getStripMaxBrightness((int)i));
    }
    doc["rows"] = rows;

    String status;
    serializeJson(doc, status);
    BM_LOGV("[BMDevice] Strips chunk: %s\n", status.c_str());
    bluetoothHandler_.sendStatusUpdate(status);
}

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
    registerStatusChunk("matrix", [this]() { sendMatrixChunk(); }, "Matrix display grid, marquee text and stored content");
    registerStatusChunk("radio", [this]() { sendRadioChunk(); }, "Sync radio state and packet counters");
    registerStatusChunk("strips", [this]() { sendStripsChunk(); }, "Registered strips and their grouping");
    registerStatusChunk("effectParams", [this]() { sendEffectParametersChunk(); }, "Effect parameters controlled via BLE commands 0x0B-0x19");
    registerStatusChunk("defaults", [this]() { sendDefaultsChunk(); }, "Persistent default settings");
    for (int slot = 0; slot < CUSTOM_PALETTE_COUNT; slot++) {
        registerStatusChunk("cpal" + String(slot),
                            [this, slot]() { sendCustomPaletteChunk(slot); },
                            "Custom palette slot " + String(slot));
    }
    
    Serial.printf("[BMDevice] Initialized %d default status chunks\n", statusChunks_.size());
} 