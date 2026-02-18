/**
 * BMOTA - HTTPS OTA update implementation
 */

#include "OTAConfig.h"

#if OTA_ENABLED

#include "BMOTA.h"
#include <WiFi.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_https_ota.h>
#include <esp_http_client.h>

static const char* OTA_PREFS_NAMESPACE = "ota";
static const char* OTA_PREF_SSID = "ssid";
static const char* OTA_PREF_PASS = "pass";

enum OtaResult { OTA_RESULT_PENDING, OTA_RESULT_SUCCESS, OTA_RESULT_FAIL };
static volatile OtaResult otaResult = OTA_RESULT_PENDING;
static size_t otaBytesReceived = 0;

static esp_err_t httpEvent(esp_http_client_event_t* event) {
    switch (event->event_id) {
        case HTTP_EVENT_ERROR:
            Serial.println("[OTA] HTTP error during transfer");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            Serial.println("[OTA] Connected to server");
            otaBytesReceived = 0;
            break;
        case HTTP_EVENT_HEADER_SENT:
            Serial.println("[OTA] Request sent, waiting for response...");
            break;
        case HTTP_EVENT_ON_HEADER:
            break;
        case HTTP_EVENT_ON_DATA:
            otaBytesReceived += event->data_len;
            break;
        case HTTP_EVENT_ON_FINISH:
            Serial.printf("[OTA] Download complete (%u bytes)\n", otaBytesReceived);
            break;
        case HTTP_EVENT_DISCONNECTED:
            Serial.println("[OTA] Disconnected from server");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void otaTask(void* param) {
    esp_http_client_config_t httpConfig = {};
    httpConfig.url = OTA_FIRMWARE_URL;
    httpConfig.cert_pem = OTA_SERVER_CERT;
    httpConfig.event_handler = httpEvent;
    httpConfig.buffer_size = 4096;
    httpConfig.buffer_size_tx = 2048;
    httpConfig.skip_cert_common_name_check = true;

    esp_https_ota_config_t otaConfig = {};
    otaConfig.http_config = &httpConfig;

    Serial.println("[OTA] Starting OTA download...");
    esp_err_t ret = esp_https_ota(&otaConfig);
    if (ret == ESP_OK) {
        Serial.println("[OTA] OTA succeeded, will reboot");
        otaResult = OTA_RESULT_SUCCESS;
    } else {
        Serial.printf("[OTA] OTA failed: %s\n", esp_err_to_name(ret));
        otaResult = OTA_RESULT_FAIL;
    }
    vTaskDelete(NULL);
}

BMOTA::BMOTA() {}

void BMOTA::setWifiCredentials(const char* ssid, const char* password) {
    Preferences prefs;
    if (!prefs.begin(OTA_PREFS_NAMESPACE, false)) {
        Serial.println("[OTA] Failed to open prefs for write");
        return;
    }
    prefs.putString(OTA_PREF_SSID, ssid);
    prefs.putString(OTA_PREF_PASS, password);
    prefs.end();
    Serial.printf("[OTA] WiFi credentials saved (SSID: %s)\n", ssid);
}

String BMOTA::getWifiStatusJson() const {
    Preferences prefs;
    StaticJsonDocument<256> doc;
    if (!prefs.begin(OTA_PREFS_NAMESPACE, true)) {
        doc["wifiConfigured"] = false;
        doc["wifiSsid"] = "";
    } else {
        String ssid = prefs.getString(OTA_PREF_SSID, "");
        prefs.end();
        bool configured = (ssid.length() > 0);
        doc["wifiConfigured"] = configured;
        doc["wifiSsid"] = ssid;
    }
    String json;
    serializeJson(doc, json);
    return json;
}

bool BMOTA::hasWifiCredentials() const {
    Preferences prefs;
    if (!prefs.begin(OTA_PREFS_NAMESPACE, true)) return false;
    String ssid = prefs.getString(OTA_PREF_SSID, "");
    prefs.end();
    if (ssid.length() > 0) return true;
    if (strlen(OTA_WIFI_SSID) > 0) return true;
    return false;
}

void BMOTA::begin() {
    Serial.println("[OTA] Initializing OTA subsystem...");
    Preferences prefs;
    String ssid, password;
    if (prefs.begin(OTA_PREFS_NAMESPACE, true)) {
        ssid = prefs.getString(OTA_PREF_SSID, "");
        password = prefs.getString(OTA_PREF_PASS, "");
        prefs.end();
        if (ssid.length() > 0) {
            Serial.printf("[OTA] Using saved WiFi credentials (SSID: %s)\n", ssid.c_str());
        }
    }
    if (ssid.length() == 0) {
        ssid = OTA_WIFI_SSID;
        password = OTA_WIFI_PASSWORD;
        if (ssid.length() > 0) {
            Serial.printf("[OTA] Using build-time WiFi credentials (SSID: %s)\n", ssid.c_str());
        }
    }
    if (ssid.length() == 0) {
        Serial.println("[OTA] No WiFi credentials configured - OTA disabled. Set via app.");
        state_ = IDLE;
        return;
    }
    Serial.println("[OTA] ---- OTA Configuration ----");
    Serial.printf("[OTA]   Firmware URL: %s\n", OTA_FIRMWARE_URL);
    Serial.printf("[OTA]   WiFi SSID:    %s\n", ssid.c_str());
    Serial.printf("[OTA]   Check interval: %lu ms\n", (unsigned long)OTA_CHECK_INTERVAL_MS);
    Serial.printf("[OTA]   Boot delay:     %lu ms\n", (unsigned long)OTA_BOOT_DELAY_MS);
    Serial.println("[OTA] ------------------------------");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.println("[OTA] WiFi connecting...");
    state_ = CONNECTING;
    wifiConnectStart_ = millis();
    bootCompleteTime_ = millis();
}

void BMOTA::loop() {
    if (state_ == IDLE) return;

    // Wait for boot delay before first check
    if (!bootDelayComplete_) {
        if (millis() - bootCompleteTime_ >= OTA_BOOT_DELAY_MS) {
            bootDelayComplete_ = true;
            Serial.println("[OTA] Boot delay complete, OTA checks active");
        }
        return;
    }

    switch (state_) {
        case CONNECTING:
            handleConnecting();
            break;
        case CONNECTED: {
            unsigned long now = millis();
            bool firstCheck = (lastCheckTime_ == 0);
            bool intervalElapsed = (OTA_CHECK_INTERVAL_MS > 0 && now - lastCheckTime_ >= OTA_CHECK_INTERVAL_MS);
            if (firstCheck || intervalElapsed) {
                lastCheckTime_ = now;
                state_ = CHECKING;
            }
            break;
        }
        case CHECKING:
            Serial.println("[OTA] Starting update check...");
            if (performUpdate()) {
                state_ = UPDATING;
                updateStartTime_ = millis();
            } else {
                Serial.println("[OTA] Update check failed to start");
                state_ = UPDATE_FAILED;
            }
            break;
        case UPDATING: {
            if (otaResult == OTA_RESULT_SUCCESS) {
                unsigned long elapsed = millis() - updateStartTime_;
                Serial.printf("[OTA] Firmware update success! (took %lu ms) Rebooting...\n", elapsed);
                Serial.flush();
                delay(500);
                ESP.restart();
            } else if (otaResult == OTA_RESULT_FAIL) {
                unsigned long elapsed = millis() - updateStartTime_;
                Serial.printf("[OTA] Firmware update failed after %lu ms\n", elapsed);
                state_ = CONNECTED;
            }
            break;
        }
        case UPDATE_FAILED:
            Serial.println("[OTA] Will retry on next check interval");
            state_ = CONNECTED;
            break;
        default:
            break;
    }
}

void BMOTA::handleConnecting() {
    wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
        unsigned long elapsed = millis() - wifiConnectStart_;
        Serial.printf("[OTA] WiFi connected in %lu ms\n", elapsed);
        Serial.printf("[OTA] IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[OTA] RSSI: %d dBm\n", WiFi.RSSI());
        state_ = CONNECTED;
    } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
        Serial.printf("[OTA] WiFi connection failed (status: %d)\n", status);
        state_ = IDLE;
    } else if (millis() - wifiConnectStart_ > 30000) {
        Serial.println("[OTA] WiFi connection timed out after 30s");
        WiFi.disconnect();
        state_ = IDLE;
    }
}

bool BMOTA::performUpdate() {
    Serial.printf("[OTA] Fetching firmware from: %s\n", OTA_FIRMWARE_URL);
    Serial.printf("[OTA] Free heap: %u bytes\n", ESP.getFreeHeap());
    otaResult = OTA_RESULT_PENDING;
    xTaskCreate(otaTask, "ota_task", 8192, NULL, 5, NULL);
    return true;
}

#endif  // OTA_ENABLED
