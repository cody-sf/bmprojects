#ifndef BM_DEVICE_H
#define BM_DEVICE_H

#include <Arduino.h>
#include <FastLED.h>
#include <LightShow.h>
#ifndef TARGET_ESP32_C6
#include <LocationService.h>
#endif
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <vector>
#include <functional>

#include "BMDeviceState.h"
#include "BMBluetoothHandler.h"
#include "BMDeviceDefaults.h"
#include "BMSync.h"

#define DEFAULT_BT_REFRESH_INTERVAL 5000
#define DEFAULT_GPS_BAUD 9600

// Status updates are event-driven, not periodic. A change (BLE write, encoder
// turn, defaults reload) marks the status dirty; the burst goes out once the
// device has been quiet for STATUS_SETTLE_MS so that dragging a slider or the
// startup brightness fade produces one update instead of dozens.
#define STATUS_SETTLE_MS 350
// Floor on the gap between two bursts, so a stream of writes can never turn
// into a stream of 4-chunk notifications.
#define STATUS_MIN_INTERVAL_MS 1000
// How often the state fingerprint is recomputed to catch direct writes to
// getState() (encoder menu, custom device code).
#define STATUS_FINGERPRINT_POLL_MS 200
// How often a GPS-driven show (speedometer, position_status, GPS speed
// animation) re-reads the fix and adjusts itself.
#define GPS_SHOW_REFRESH_MS 1000

// Chunked status update system
enum StatusUpdateState {
    STATUS_IDLE,
    STATUS_SENDING_CHUNKS
};

struct StatusChunk {
    String type;
    std::function<void()> sendFunction;
    String description;
};

#define STATUS_UPDATE_DELAY 25  // 25ms delay between chunks

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
        recordRegisteredStrip(DATA_PIN, numLeds);
        Serial.print("[BMDevice] Added LED strip: ");
        Serial.print(numLeds);
        Serial.print(" LEDs on pin ");
        Serial.println(DATA_PIN);
    }
    
    // GPS Integration (optional)
#ifndef TARGET_ESP32_C6
    void enableGPS(int rxPin = 21, int txPin = 22, int baud = DEFAULT_GPS_BAUD);
    void setLocationService(LocationService* locationService);
#endif
    
    // Device lifecycle
    bool begin();
    void loop();
    
    // State access
    BMDeviceState& getState() { return deviceState_; }
    LightShow& getLightShow() { return lightShow_; }
    BMBluetoothHandler& getBluetoothHandler() { return bluetoothHandler_; }
#ifndef TARGET_ESP32_C6
    LocationService* getLocationService() { return locationService_; }
#endif
    BMDeviceDefaults& getDefaults() { return defaults_; }
    
    // Configuration
    // Legacy: status updates are event-driven now, so this only affects the
    // "interval" value reported to the app in the device config chunk.
    void setStatusUpdateInterval(unsigned long interval) { statusUpdateInterval_ = interval; }
    void setBrightness(int brightness);
    void setEffect(LightSceneID effect);
    void setPalette(AvailablePalettes palette);
    /// The matrix display overlay (MATRIX_DISPLAY_*): what the grid strip
    /// shows, independent of the running effect. Persisted; 0 hands the
    /// panel back to the effect.
    void setMatrixDisplay(uint8_t mode);
    /// The display's own pace in ms per animation frame (text scales off the
    /// same value). Persisted; the effect speed knob never touches it.
    void setMatrixSpeed(uint16_t ms);
    /// Blink every strip `blinks` times in `color`, then hand the LEDs back
    /// to the show (or the dark). On-device UI feedback with no screen — the
    /// encoder menu uses it to answer "which menu did that press land on".
    void flashFeedback(const CRGB& color, uint8_t blinks);
    
    // Defaults management
    bool loadDefaults();
    bool saveCurrentAsDefaults();
    bool resetToFactoryDefaults();
    void applyDefaults();
    void setMaxBrightness(int maxBrightness);
    void setDeviceOwner(const String& owner);
    void setFriendlyName(const String& name);
    
    // Callbacks for custom behavior
    void setCustomFeatureHandler(std::function<bool(uint8_t feature, const uint8_t* data, size_t length)> handler);
    void setCustomConnectionHandler(std::function<void(bool connected)> handler);

    // Feed a command through the same dispatch BLE writes land in (buffer[0]
    // is the feature code, payload follows, exactly as written on the wire).
    // For on-device bridges (the sign's MQTT/Home Assistant bridge) so every
    // transport shares one set of semantics - constraints, persistence and
    // the status fingerprint all behave as if the app had sent it.
    void injectFeature(uint8_t feature, const uint8_t* buffer, size_t length) {
        handleFeatureCommand(feature, buffer, length);
    }
    
    // Chunked status update system
    void registerStatusChunk(const String& type, std::function<void()> sendFunction, const String& description = "");
    void startChunkedStatusUpdate();
    void clearStatusChunks();

    // Event-driven status updates.
    //
    // Call markStatusDirty() after changing device settings outside of the
    // standard BLE feature handlers (encoder menus, custom feature handlers).
    // Changes made through setBrightness()/setEffect()/setPalette() or written
    // straight into getState() are picked up automatically by the fingerprint
    // check, so marking is an optimisation rather than a requirement.
    void markStatusDirty();
    // Returns true at most once per pending change, when a status burst is due:
    // the central is subscribed, the settings have settled, and the minimum gap
    // since the last burst has elapsed. Devices that drive their own loop (e.g.
    // BTUmbrellaV3) call this instead of BMDevice::loop().
    bool takeDueStatusUpdate();

    // Multi-device sync (ESP-NOW). loop() calls this; devices that drive
    // their own loop (BTUmbrellaV3) call it alongside takeDueStatusUpdate().
    void serviceSync();
    // Hard opt-out for devices that are not light shows (the battery charger
    // forces power on and must never adopt a group power-off). Unlike the
    // 0x24 preference this is not user-reachable.
    void setSyncAvailable(bool available) { syncAvailable_ = available; }

private:
    // Core components
    BMDeviceState deviceState_;
    BMBluetoothHandler bluetoothHandler_;
    LightShow lightShow_;
    Clock deviceClock_;
    BMDeviceDefaults defaults_;
    BMSync sync_;
    unsigned long lastSyncServiceAt_;
    bool syncAvailable_ = true;
    
    // GPS components (optional)
    bool gpsEnabled_;
#ifndef TARGET_ESP32_C6
    bool ownGPSSerial_; // True if we created the LocationService
    LocationService* locationService_;
#endif
    
    // Timing
    unsigned long lastBluetoothSync_;
    unsigned long statusUpdateInterval_;

    // Event-driven status update tracking
    bool statusDirty_;
    unsigned long statusDirtyAt_;      // when the most recent change landed
    unsigned long lastStatusSentAt_;
    unsigned long lastFingerprintAt_;
    uint32_t stateFingerprint_;
    bool wasSubscribed_;
    // Last time a GPS-driven show was re-evaluated against the current fix
    unsigned long lastGpsShowRefresh_ = 0;

    // Find-me strobe deadline (0 = not identifying)
    unsigned long identifyUntil_ = 0;
    unsigned long matrixTestUntil_ = 0;

    // Feedback blink state (flashFeedback); 0 blinks = idle
    CRGB feedbackColor_ = CRGB::White;
    uint8_t feedbackBlinks_ = 0;
    unsigned long feedbackStartAt_ = 0;

    // Power-off LED state, so the strips are cleared once instead of every loop
    bool ledsBlanked_;
    // When the dark frame last went out; it is re-clocked once a second while
    // off, so a strip that misses the transition blank still goes dark.
    unsigned long lastBlankAt_ = 0;

    // Custom handlers
    std::function<bool(uint8_t, const uint8_t*, size_t)> customFeatureHandler_;
    std::function<void(bool)> customConnectionHandler_;
    
    // Chunked status update system
    std::vector<StatusChunk> statusChunks_;
    StatusUpdateState statusUpdateState_;
    unsigned long statusUpdateTimer_;
    size_t currentChunkIndex_;
    
    // LED strip management
    CRGB* ledArrays_[MAX_LED_STRIPS];
    bool dynamicNaming_;
    
    // Internal methods
    void handleFeatureCommand(uint8_t feature, const uint8_t* buffer, size_t length);
    void handleConnectionChange(bool connected);
    void updateGPS();
    void updateLightShow();
    void sendStatusUpdate();
    
    // GPS speed mapping helpers
    uint16_t calculateEffectiveSpeed();
    uint16_t positionStatusSpeed();
    
    // Every strip actually playing the show, in the order it was added -
    // whether the sketch hardcoded it (bike, SLUT, signs) or it came from the
    // NVRAM strip config. This is what the app's Strands screen lists and
    // what per-strip group sizes are keyed by; the config *rows* can't serve,
    // because static targets never populate them.
    struct RegisteredStrip {
        uint8_t pin;
        uint16_t numLeds;
    };
    RegisteredStrip registeredStrips_[MAX_LED_STRIPS];
    size_t registeredStripCount_ = 0;
    void recordRegisteredStrip(int pin, int numLeds) {
        if (registeredStripCount_ < MAX_LED_STRIPS) {
            registeredStrips_[registeredStripCount_].pin = (uint8_t)pin;
            registeredStrips_[registeredStripCount_].numLeds = (uint16_t)numLeds;
            registeredStripCount_++;
        }
    }
    void sendStripsChunk();
    void handleSetStripGroupFeature(const uint8_t* buffer, size_t length);
    void handleSetStripBrightnessFeature(const uint8_t* buffer, size_t length);

    // Chunked status update methods
    void handleChunkedStatusUpdate();
    uint32_t computeStateFingerprint();
    void sendBasicStatusChunk();
    void sendDeviceConfigChunk();
    void sendMatrixChunk();
    void sendRadioChunk();
    void sendDefaultsChunk();
    void sendEffectParametersChunk();
    void sendCustomPaletteChunk(int slot);
    void initializeDefaultStatusChunks();
    
    // Feature handlers
    void handlePowerFeature(const uint8_t* buffer, size_t length);
    void handleBrightnessFeature(const uint8_t* buffer, size_t length);
    void handleSpeedFeature(const uint8_t* buffer, size_t length);
    void handleDirectionFeature(const uint8_t* buffer, size_t length);
    void handleOriginFeature(const uint8_t* buffer, size_t length);
    void handlePaletteFeature(const uint8_t* buffer, size_t length);
    void handleSpeedometerFeature(const uint8_t* buffer, size_t length);
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
    void handleSetDeviceNameFeature(const uint8_t* buffer, size_t length);
    
    // Custom palette handlers
    void handleSetCustomPaletteFeature(const uint8_t* buffer, size_t length);
    void handleDeleteCustomPaletteFeature(const uint8_t* buffer, size_t length);
    /// Push every stored custom palette into the light show. Called at start-up
    /// and whenever the defaults are reloaded.
    void applyCustomPalettes();

    // Matrix display content (marquee text + uploaded bitmap)
    void handleSetMarqueeTextFeature(const uint8_t* buffer, size_t length);
    void handleSetMatrixBitmapFeature(const uint8_t* buffer, size_t length);
    void handleClearMatrixBitmapFeature(const uint8_t* buffer, size_t length);
    void handleSetAnimFrameFeature(const uint8_t* buffer, size_t length);
    void handleClearAnimFramesFeature(const uint8_t* buffer, size_t length);
    /// Push the stored marquee text and bitmap into the light show at start-up.
    void applyMatrixArt();
    void handleSetAutoOnFeature(const uint8_t* buffer, size_t length);
    
    // GPS Speed feature handlers
    void handleSetGPSLowSpeedFeature(const uint8_t* buffer, size_t length);
    void handleSetGPSTopSpeedFeature(const uint8_t* buffer, size_t length);
    void handleSetGPSLightshowSpeedEnabledFeature(const uint8_t* buffer, size_t length);
    void handleSetSyncEnabledFeature(const uint8_t* buffer, size_t length);

    // Sync apply path: a received group packet lands here (from serviceSync,
    // main-loop context - never the radio callback).
    void applySyncState(const BMSyncState& state);
    
    // Generic device configuration handlers
    void handleSetDeviceTypeFeature(const uint8_t* buffer, size_t length);
    /// The name to advertise: "BMDevice - <friendly name>", falling back to the
    /// owner and then "New". The "BMDevice" identifier always leads, because
    /// that prefix is how the apps recognise our gear while scanning.
    String buildAdvertisedName() const;
    void handleConfigureLEDStripFeature(const uint8_t* buffer, size_t length);
    void handleGetConfigurationFeature(const uint8_t* buffer, size_t length);
    void handleResetToDefaultsFeature(const uint8_t* buffer, size_t length);
    
    // LED strip management
    void addLEDStripByPin(int pin, CRGB* ledArray, int numLeds, int colorOrder);
    void initializeLEDStrips();
};

#endif // BM_DEVICE_H 