/**
 * BMOTA - HTTPS OTA update support for BMGenericDevice
 *
 * Connects to WiFi and periodically fetches firmware from a URL.
 * When OTA_ENABLED is 0 in OTAConfig.h, all OTA code is compiled out.
 */

#ifndef BMOTA_H
#define BMOTA_H

#include <Arduino.h>

#if OTA_ENABLED

class BMOTA {
public:
    BMOTA();
    ~BMOTA() = default;

    /// Call once from setup() - starts background WiFi connection
    void begin();

    /// Call every loop() - handles WiFi, update check, and OTA
    void loop();

    /// Set WiFi credentials (from BLE). Stored in Preferences. Brings WiFi up
    /// straight away if the device was offline, so a freshly provisioned device
    /// does not need a reboot before it can update.
    void setWifiCredentials(const char* ssid, const char* password);

    /// Force an immediate version check (from BLE / the app's "Update now").
    /// Connects WiFi first if needed and bypasses the boot delay.
    void checkNow();

    /// Get WiFi + OTA status as JSON for BLE response. Includes wifiConfigured,
    /// wifiSsid, wifiConnected, wifiIp, fwVer and otaState.
    String getWifiStatusJson() const;

    /// Check if WiFi credentials are configured (from Preferences or build config)
    bool hasWifiCredentials() const;

    /// Returns true while firmware is being downloaded/installed
    bool isUpdating() const { return state_ == UPDATING; }

private:
    enum State {
        IDLE,
        CONNECTING,
        CONNECTED,
        CHECKING,
        CHECKING_VERSION,
        UPDATING,
        UPDATE_FAILED
    };

    State state_ = IDLE;
    unsigned long lastCheckTime_ = 0;
    unsigned long bootCompleteTime_ = 0;
    unsigned long wifiConnectStart_ = 0;
    unsigned long updateStartTime_ = 0;
    bool bootDelayComplete_ = false;
    // Set by checkNow(); makes the next CONNECTED pass check immediately even if
    // the interval has not elapsed.
    bool forceCheckPending_ = false;

    /// Bring WiFi up from stored/build-time credentials. Returns false when
    /// there are none. Shared by begin(), setWifiCredentials() and checkNow().
    bool connectWifi();

    /// Start the ArduinoOTA (espota) listener once, when WiFi first comes up,
    /// so a local build can be pushed over the network. No-op if already up or
    /// LOCAL_OTA_ENABLED is 0.
    void startLocalOta();
    bool localOtaStarted_ = false;

    void handleConnecting();
    void handleConnected();
};

#endif  // OTA_ENABLED

#endif  // BMOTA_H
