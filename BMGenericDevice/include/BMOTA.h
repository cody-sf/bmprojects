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

    /// Set WiFi credentials (from BLE). Stored in Preferences.
    void setWifiCredentials(const char* ssid, const char* password);

    /// Get WiFi status as JSON for BLE response: {"wifiConfigured":bool,"wifiSsid":"..."}
    String getWifiStatusJson() const;

    /// Check if WiFi credentials are configured (from Preferences or build config)
    bool hasWifiCredentials() const;

private:
    enum State {
        IDLE,
        CONNECTING,
        CONNECTED,
        CHECKING,
        UPDATING,
        UPDATE_FAILED
    };

    State state_ = IDLE;
    unsigned long lastCheckTime_ = 0;
    unsigned long bootCompleteTime_ = 0;
    bool bootDelayComplete_ = false;

    void handleConnecting();
    void handleConnected();
    bool performUpdate();
};

#endif  // OTA_ENABLED

#endif  // BMOTA_H
