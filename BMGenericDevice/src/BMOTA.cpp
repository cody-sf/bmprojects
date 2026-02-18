/**
 * BMOTA - HTTPS OTA update implementation
 */

#if OTA_ENABLED

#include "BMOTA.h"
#include "OTAConfig.h"
#include <WiFi.h>
#include <HttpsOTAUpdate.h>
#include <Preferences.h>
#include <ArduinoJson.h>

static const char* OTA_PREFS_NAMESPACE = "ota";
static const char* OTA_PREF_SSID = "ssid";
static const char* OTA_PREF_PASS = "pass";

static HttpsOTAStatus_t otaStatus = HTTPS_OTA_IDLE;

static void httpEvent(HttpEvent_t* event) {
    switch (event->event_id) {
        case HTTP_EVENT_ERROR:
            Serial.println("[OTA] HTTP Event Error");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            Serial.println("[OTA] Connected to update server");
            break;
        case HTTP_EVENT_HEADER_SENT:
            Serial.println("[OTA] Request sent");
            break;
        case HTTP_EVENT_ON_HEADER:
            // Progress would go here
            break;
        case HTTP_EVENT_ON_DATA:
            break;
        case HTTP_EVENT_ON_FINISH:
            Serial.println("[OTA] Download complete");
            break;
        case HTTP_EVENT_DISCONNECTED:
            Serial.println("[OTA] Disconnected");
            break;
        default:
            break;
    }
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
#if defined(OTA_WIFI_SSID) && (strlen(OTA_WIFI_SSID) > 0)
    return true;
#else
    return false;
#endif
}

void BMOTA::begin() {
    Preferences prefs;
    String ssid, password;
    if (prefs.begin(OTA_PREFS_NAMESPACE, true)) {
        ssid = prefs.getString(OTA_PREF_SSID, "");
        password = prefs.getString(OTA_PREF_PASS, "");
        prefs.end();
    }
    if (ssid.length() == 0) {
        ssid = OTA_WIFI_SSID;
        password = OTA_WIFI_PASSWORD;
    }
    if (ssid.length() == 0) {
        Serial.println("[OTA] No WiFi credentials - OTA disabled. Set via app.");
        state_ = IDLE;
        return;
    }
    Serial.println("[OTA] HTTPS OTA enabled - will check for updates");
    Serial.printf("[OTA] Firmware URL: %s\n", OTA_FIRMWARE_URL);
    Serial.printf("[OTA] WiFi: %s\n", ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    state_ = CONNECTING;
    bootCompleteTime_ = millis();
}

void BMOTA::loop() {
    if (state_ == IDLE) return;

    // Wait for boot delay before first check
    if (!bootDelayComplete_) {
        if (millis() - bootCompleteTime_ >= OTA_BOOT_DELAY_MS) {
            bootDelayComplete_ = true;
        }
        return;
    }

    switch (state_) {
        case CONNECTING:
            handleConnecting();
            break;
        case CONNECTED: {
            unsigned long now = millis();
            bool shouldCheck = (OTA_CHECK_INTERVAL_MS == 0 && lastCheckTime_ == 0) ||
                              (OTA_CHECK_INTERVAL_MS > 0 && now - lastCheckTime_ >= OTA_CHECK_INTERVAL_MS);
            if (shouldCheck) {
                lastCheckTime_ = now;
                state_ = CHECKING;
            }
            break;
        }
        case CHECKING:
            if (performUpdate()) {
                state_ = UPDATING;
            } else {
                state_ = UPDATE_FAILED;
            }
            break;
        case UPDATING: {
            otaStatus = HttpsOTA.status();
            if (otaStatus == HTTPS_OTA_SUCCESS) {
                Serial.println("[OTA] Firmware update success! Rebooting...");
                Serial.flush();
                delay(500);
                ESP.restart();
            } else if (otaStatus == HTTPS_OTA_FAIL) {
                Serial.println("[OTA] Firmware update failed");
                state_ = CONNECTED;  // Retry on next interval
            }
            break;
        }
        case UPDATE_FAILED:
            state_ = CONNECTED;  // Retry on next interval
            break;
        default:
            break;
    }
}

void BMOTA::handleConnecting() {
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("[OTA] WiFi connected");
        Serial.printf("[OTA] IP: %s\n", WiFi.localIP().toString().c_str());
        state_ = CONNECTED;
    }
}

bool BMOTA::performUpdate() {
    Serial.println("[OTA] Checking for update...");
    HttpsOTA.onHttpEvent(httpEvent);
    HttpsOTA.begin(OTA_FIRMWARE_URL, OTA_SERVER_CERT, true);
    return true;  // HttpsOTA runs async, we'll poll status in UPDATING
}

#endif  // OTA_ENABLED
