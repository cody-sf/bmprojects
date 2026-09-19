/**
 * BMMQTT - Home Assistant bridge for mains-powered signs.
 *
 * Publishes the device as one MQTT-discovery light entity (on/off +
 * brightness only - effects, palettes and everything deeper stay with the
 * app/watch/encoder) and mirrors every state change back over MQTT, so Hue
 * switches and HA automations stay in agreement with the other controls.
 *
 * Compiled in only when this build keeps WiFi alive (OTA_KEEP_WIFI_ALIVE,
 * i.e. the mains-powered signs) AND WiFiCreds.h names a broker via
 * MQTT_BROKER_HOST. Battery wearables and CI release builds (no WiFiCreds.h)
 * get the empty stub below at zero cost.
 */

#ifndef BMMQTT_H
#define BMMQTT_H

#include <Arduino.h>
#include "OTAConfig.h"

#if OTA_ENABLED && OTA_KEEP_WIFI_ALIVE && defined(MQTT_BROKER_HOST)
#define BM_MQTT_ENABLED 1
#else
#define BM_MQTT_ENABLED 0
#endif

class BMDevice;

#if BM_MQTT_ENABLED

#include <mqtt_client.h>

#ifndef MQTT_BROKER_PORT
#define MQTT_BROKER_PORT 1883
#endif

class BMMQTT {
public:
    /// Call once from setup(), after device.begin() (needs the loaded
    /// defaults for the Home Assistant device name).
    void begin(BMDevice* device);

    /// Call every loop(). Starts the client once WiFi is up, applies queued
    /// commands on the main loop (never the MQTT task), and publishes state
    /// changes once they settle.
    void loop();

private:
    struct CmdMsg {
        char payload[160];
    };

    void maybeStart();
    void announce();
    void applyCommand(const char* json);
    void pollStatePublish();
    void publishDiscovery();
    void publishState();
    String friendlyName() const;

    static void mqttEvent(void* arg, esp_event_base_t base, int32_t eventId,
                          void* eventData);

    BMDevice* device_ = nullptr;
    esp_mqtt_client_handle_t client_ = nullptr;
    QueueHandle_t cmdQueue_ = nullptr;
    bool started_ = false;
    unsigned long lastStartAttempt_ = 0;

    // Set from the MQTT task, consumed on the main loop.
    volatile bool mqttUp_ = false;
    volatile bool needAnnounce_ = false;

    String hostId_;       // "bm-xxxx", same derivation as the espota hostname
    String stateTopic_;   // bmlights/<host>/state (retained JSON)
    String cmdTopic_;     // bmlights/<host>/set
    String availTopic_;   // bmlights/<host>/availability (online/offline LWT)
    String discTopic_;    // homeassistant/light/<host>/light/config

    // Settle-then-publish, so the encoder or a slider drag produces one
    // retained state instead of a stream (mirrors the BLE status settle).
    uint32_t lastFingerprint_ = 0;
    unsigned long lastPollAt_ = 0;
    unsigned long dirtyAt_ = 0;
    bool dirty_ = false;
};

#else  // !BM_MQTT_ENABLED

class BMMQTT {
public:
    void begin(BMDevice*) {}
    void loop() {}
};

#endif  // BM_MQTT_ENABLED

#endif  // BMMQTT_H
