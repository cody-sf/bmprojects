#include "BMMQTT.h"

#if BM_MQTT_ENABLED

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <BMDevice.h>
#include "version.h"

#if ESP_IDF_VERSION_MAJOR < 5
#error "BMMQTT is written against the IDF 5 esp_mqtt config layout (arduino-esp32 3.x)"
#endif

void BMMQTT::begin(BMDevice* device) {
    device_ = device;

    // Same derivation as the espota hostname (BMOTA::startLocalOta), so the
    // device is "bm-xxxx" everywhere: mDNS, MQTT topics, HA unique ids.
    uint64_t mac = ESP.getEfuseMac();
    char host[16];
    snprintf(host, sizeof(host), "bm-%04x", (uint16_t)(mac & 0xFFFF));
    hostId_ = host;

    String base = String("bmlights/") + hostId_;
    stateTopic_ = base + "/state";
    cmdTopic_ = base + "/set";
    availTopic_ = base + "/availability";
    discTopic_ = String("homeassistant/light/") + hostId_ + "/light/config";

    cmdQueue_ = xQueueCreate(4, sizeof(CmdMsg));

    Serial.printf("[MQTT] Home Assistant bridge enabled: broker %s, device %s\n",
                  MQTT_BROKER_HOST, hostId_.c_str());
}

void BMMQTT::maybeStart() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }
    // Pace the attempts: a failed .local resolution blocks for its timeout,
    // so don't retry it every pass through loop().
    if (lastStartAttempt_ != 0 && millis() - lastStartAttempt_ < 15000) {
        return;
    }
    lastStartAttempt_ = millis();

    // esp_mqtt resolves through lwip DNS, which never consults mDNS - a
    // ".local" broker has to be resolved here first. Plain hostnames and
    // raw IPs go straight through.
    String host = MQTT_BROKER_HOST;
    if (host.endsWith(".local")) {
        String bare = host.substring(0, host.length() - 6);
        IPAddress ip = MDNS.queryHost(bare.c_str(), 2000);
        if (ip == IPAddress()) {
            Serial.printf("[MQTT] Could not resolve %s yet, will retry\n", host.c_str());
            return;
        }
        host = ip.toString();
    }

    String uri = String("mqtt://") + host + ":" + String(MQTT_BROKER_PORT);

    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = uri.c_str();
    cfg.credentials.client_id = hostId_.c_str();
#if defined(MQTT_USER) && defined(MQTT_PASS)
    if (strlen(MQTT_USER) > 0) {
        cfg.credentials.username = MQTT_USER;
        cfg.credentials.authentication.password = MQTT_PASS;
    }
#endif
    // The LWT flips the HA entity to "unavailable" when the sign loses power
    // or WiFi; announce() publishes the matching retained "online".
    cfg.session.last_will.topic = availTopic_.c_str();
    cfg.session.last_will.msg = "offline";
    cfg.session.last_will.msg_len = 0;  // 0 = use strlen
    cfg.session.last_will.qos = 1;
    cfg.session.last_will.retain = true;
    cfg.session.keepalive = 30;
    // Discovery config is the biggest payload (~700 B); default 1024 is
    // uncomfortably close once topics are counted in.
    cfg.buffer.size = 2048;

    client_ = esp_mqtt_client_init(&cfg);  // copies the config strings
    if (!client_) {
        Serial.println("[MQTT] Client init failed");
        return;
    }
    esp_mqtt_client_register_event(client_, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                   &BMMQTT::mqttEvent, this);
    esp_mqtt_client_start(client_);
    started_ = true;
    Serial.printf("[MQTT] Connecting to %s\n", uri.c_str());
}

// Runs on the esp-mqtt task: only set flags and queue payloads here, all
// device state is touched from loop() on the main task.
void BMMQTT::mqttEvent(void* arg, esp_event_base_t, int32_t eventId, void* eventData) {
    BMMQTT* self = static_cast<BMMQTT*>(arg);
    esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)eventData;

    switch ((esp_mqtt_event_id_t)eventId) {
        case MQTT_EVENT_CONNECTED:
            self->mqttUp_ = true;
            self->needAnnounce_ = true;
            break;
        case MQTT_EVENT_DISCONNECTED:
            self->mqttUp_ = false;
            break;
        case MQTT_EVENT_DATA: {
            // topic_len 0 marks a continuation fragment of an oversized
            // payload; ours are tiny, so just drop those.
            if (e->topic_len == 0) break;
            bool isCmd = e->topic_len == self->cmdTopic_.length() &&
                         memcmp(e->topic, self->cmdTopic_.c_str(), e->topic_len) == 0;
            if (isCmd) {
                CmdMsg m = {};
                size_t n = min((size_t)e->data_len, sizeof(m.payload) - 1);
                memcpy(m.payload, e->data, n);
                xQueueSend(self->cmdQueue_, &m, 0);
                break;
            }
            // Home Assistant's birth message: it just (re)started, so re-send
            // discovery and state even though our connection never dropped.
            static const char kHaStatus[] = "homeassistant/status";
            if (e->topic_len == sizeof(kHaStatus) - 1 &&
                memcmp(e->topic, kHaStatus, e->topic_len) == 0 &&
                e->data_len >= 6 && memcmp(e->data, "online", 6) == 0) {
                self->needAnnounce_ = true;
            }
            break;
        }
        default:
            break;
    }
}

void BMMQTT::loop() {
    if (!started_) {
        maybeStart();
        return;
    }

    if (needAnnounce_) {
        needAnnounce_ = false;
        announce();
    }

    CmdMsg m;
    while (cmdQueue_ && xQueueReceive(cmdQueue_, &m, 0) == pdTRUE) {
        applyCommand(m.payload);
    }

    pollStatePublish();
}

void BMMQTT::announce() {
    esp_mqtt_client_subscribe(client_, cmdTopic_.c_str(), 0);
    esp_mqtt_client_subscribe(client_, "homeassistant/status", 0);
    publishDiscovery();
    esp_mqtt_client_publish(client_, availTopic_.c_str(), "online", 0, 1, true);
    publishState();
}

String BMMQTT::friendlyName() const {
    // The factory "BMDevice" is a placeholder, exactly as the apps treat it.
    String n = device_->getDefaults().getCurrentDefaults().deviceName;
    if (n.length() == 0 || n == "BMDevice") {
        n = "BM Light " + hostId_;
    }
    return n;
}

void BMMQTT::publishDiscovery() {
    StaticJsonDocument<1024> doc;
    doc["name"] = (const char*)nullptr;  // entity takes the device's name
    doc["unique_id"] = hostId_ + "_light";
    doc["state_topic"] = stateTopic_;
    doc["command_topic"] = cmdTopic_;
    doc["availability_topic"] = availTopic_;
    doc["schema"] = "json";
    doc["brightness"] = true;

    JsonObject dev = doc.createNestedObject("device");
    dev.createNestedArray("identifiers").add(hostId_);
    dev["name"] = friendlyName();
    dev["manufacturer"] = "Cody LaRocque";
    dev["model"] = "BMGenericDevice";
    dev["sw_version"] = FIRMWARE_VERSION;

    String out;
    serializeJson(doc, out);
    esp_mqtt_client_publish(client_, discTopic_.c_str(), out.c_str(), 0, 1, true);
}

void BMMQTT::publishState() {
    // Internal brightness is already the 1-255 FastLED level HA speaks.
    // Retained, so HA has a state the moment it subscribes.
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"state\":\"%s\",\"brightness\":%d}",
             device_->getState().power ? "ON" : "OFF", device_->getState().brightness);
    esp_mqtt_client_publish(client_, stateTopic_.c_str(), buf, 0, 0, true);
}

void BMMQTT::applyCommand(const char* json) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) {
        Serial.printf("[MQTT] Ignoring unparseable command: %s\n", json);
        return;
    }

    const char* state = doc["state"];
    bool on = !(state && strcasecmp(state, "OFF") == 0);
    if (state && device_->getState().power != on) {
        uint8_t buf[2] = {BLE_FEATURE_POWER, (uint8_t)(on ? 1 : 0)};
        device_->injectFeature(BLE_FEATURE_POWER, buf, sizeof(buf));
    }

    if (on && doc.containsKey("brightness")) {
        // HA sends 0-255; the wire (and handleBrightnessFeature) speak the
        // app's percent, which the device caps by its own max brightness.
        int level = constrain((int)doc["brightness"], 0, 255);
        int32_t percent = brightnessLevelToPercent(level);
        uint8_t buf[5] = {BLE_FEATURE_BRIGHTNESS};
        memcpy(buf + 1, &percent, sizeof(percent));
        device_->injectFeature(BLE_FEATURE_BRIGHTNESS, buf, sizeof(buf));
    }
}

void BMMQTT::pollStatePublish() {
    unsigned long now = millis();
    if (now - lastPollAt_ < 250) {
        return;
    }
    lastPollAt_ = now;

    uint32_t fp = (device_->getState().power ? 1u : 0u) |
                  ((uint32_t)device_->getState().brightness << 8);
    if (fp != lastFingerprint_) {
        lastFingerprint_ = fp;
        dirty_ = true;
        dirtyAt_ = now;
    }
    // Publish once the state has been quiet for a beat, so the encoder, the
    // boot fade-up, or a dragged slider produce one retained state, not a
    // stream. First publish happens via announce(), so dropping one here
    // while the broker is down loses nothing that reconnect won't resend.
    if (dirty_ && mqttUp_ && now - dirtyAt_ >= 400) {
        dirty_ = false;
        publishState();
    }
}

#endif  // BM_MQTT_ENABLED
