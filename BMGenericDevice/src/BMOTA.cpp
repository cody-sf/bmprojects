/**
 * BMOTA - HTTPS OTA update implementation
 */

#include "OTAConfig.h"

#if OTA_ENABLED

#include "BMOTA.h"
#include "version.h"
#include <WiFi.h>
#if LOCAL_OTA_ENABLED
#include <ArduinoOTA.h>
#endif
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_https_ota.h>
#include <esp_http_client.h>
#include <esp_idf_version.h>

static const char* OTA_PREFS_NAMESPACE = "ota";
static const char* OTA_PREF_SSID = "ssid";
static const char* OTA_PREF_PASS = "pass";

enum OtaResult { OTA_RESULT_PENDING, OTA_RESULT_SUCCESS, OTA_RESULT_FAIL };
static volatile OtaResult otaResult = OTA_RESULT_PENDING;

enum VersionCheckResult { VERSION_CHECK_PENDING, VERSION_CHECK_NEW, VERSION_CHECK_SAME, VERSION_CHECK_FAIL };
static volatile VersionCheckResult versionCheckResult = VERSION_CHECK_PENDING;

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

static char versionResponseBuf[64];
static int versionResponseLen = 0;

static esp_err_t versionHttpEvent(esp_http_client_event_t* event) {
    if (event->event_id == HTTP_EVENT_ON_DATA) {
        int copyLen = event->data_len;
        if (versionResponseLen + copyLen >= (int)sizeof(versionResponseBuf) - 1)
            copyLen = sizeof(versionResponseBuf) - 1 - versionResponseLen;
        if (copyLen > 0) {
            memcpy(versionResponseBuf + versionResponseLen, event->data, copyLen);
            versionResponseLen += copyLen;
            versionResponseBuf[versionResponseLen] = '\0';
        }
    }
    return ESP_OK;
}

static void otaCheckAndUpdateTask(void* param) {
    // --- Phase 1: Version check ---
    Serial.printf("[OTA] Checking version at: %s\n", OTA_VERSION_URL);
    Serial.printf("[OTA] Current firmware version: %s\n", FIRMWARE_VERSION);

    versionResponseLen = 0;
    versionResponseBuf[0] = '\0';

    {
        esp_http_client_config_t config = {};
        config.url = OTA_VERSION_URL;
        config.cert_pem = OTA_SERVER_CERT;
        config.event_handler = versionHttpEvent;
        config.buffer_size = 4096;
        config.buffer_size_tx = 2048;
        config.skip_cert_common_name_check = true;
        config.max_redirection_count = 5;

        esp_http_client_handle_t client = esp_http_client_init(&config);
        esp_err_t err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err != ESP_OK || status != 200) {
            Serial.printf("[OTA] Version check failed (err=%s, status=%d)\n", esp_err_to_name(err), status);
            versionCheckResult = VERSION_CHECK_FAIL;
            vTaskDelete(NULL);
            return;
        }
    }

    String remoteVersion = String(versionResponseBuf);
    remoteVersion.trim();
    Serial.printf("[OTA] Remote version: %s\n", remoteVersion.c_str());

    if (remoteVersion.length() == 0 || remoteVersion == FIRMWARE_VERSION) {
        Serial.println("[OTA] Already running latest version, no update needed");
        versionCheckResult = VERSION_CHECK_SAME;
        vTaskDelete(NULL);
        return;
    }

    Serial.printf("[OTA] New version available: %s -> %s\n", FIRMWARE_VERSION, remoteVersion.c_str());
    versionCheckResult = VERSION_CHECK_NEW;

    // Brief pause to let network stack fully clean up before new connections
    Serial.println("[OTA] Preparing to download update...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // --- Phase 2: OTA download ---
    Serial.printf("[OTA] Fetching firmware from: %s\n", OTA_FIRMWARE_URL);
    Serial.printf("[OTA] Free heap: %u bytes\n", ESP.getFreeHeap());

    esp_http_client_config_t httpConfig = {};
    httpConfig.url = OTA_FIRMWARE_URL;
    httpConfig.cert_pem = OTA_SERVER_CERT;
    httpConfig.event_handler = httpEvent;
    httpConfig.buffer_size = 4096;
    httpConfig.buffer_size_tx = 2048;
    httpConfig.skip_cert_common_name_check = true;

    Serial.println("[OTA] Starting OTA download...");
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    esp_https_ota_config_t otaConfig = {};
    otaConfig.http_config = &httpConfig;
    esp_err_t ret = esp_https_ota(&otaConfig);
#else
    esp_err_t ret = esp_https_ota(&httpConfig);
#endif
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

    // Apply immediately so a just-provisioned device can update without a
    // reboot. If it is mid-check/mid-update, leave it be - the new creds are
    // saved and take effect on the next connect. Provisioning counts as a
    // manual check: skip the boot delay and (on battery builds) linger online
    // afterwards so the person setting the device up sees it connect.
    if (state_ == IDLE || state_ == CONNECTING) {
        bootDelayComplete_ = true;
        forceCheckPending_ = true;
        WiFi.disconnect();
        connectWifi();
    }
}

void BMOTA::checkNow() {
    Serial.println("[OTA] Manual update check requested");
    forceCheckPending_ = true;
    bootDelayComplete_ = true;  // a user-initiated check should not wait out the boot delay

    if (state_ == IDLE) {
        if (!connectWifi()) {
            Serial.println("[OTA] Cannot check - no WiFi credentials");
            forceCheckPending_ = false;
        }
    }
    // If CONNECTED, loop() picks up forceCheckPending_ on the next pass; if
    // CONNECTING it checks as soon as the link is up; if already CHECKING/
    // UPDATING there is nothing to do.
}

String BMOTA::getWifiStatusJson() const {
    Preferences prefs;
    StaticJsonDocument<256> doc;

    String ssid = "";
    if (prefs.begin(OTA_PREFS_NAMESPACE, true)) {
        ssid = prefs.getString(OTA_PREF_SSID, "");
        prefs.end();
    }
    if (ssid.length() == 0 && strlen(OTA_WIFI_SSID) > 0) {
        ssid = OTA_WIFI_SSID;
    }

    bool connected = (WiFi.status() == WL_CONNECTED);
    doc["wifiConfigured"] = (ssid.length() > 0);
    doc["wifiSsid"] = ssid;
    doc["wifiConnected"] = connected;
    doc["wifiIp"] = connected ? WiFi.localIP().toString() : String("");
    doc["fwVer"] = FIRMWARE_VERSION;

    const char* stateName = "idle";
    switch (state_) {
        case CONNECTING:        stateName = "connecting"; break;
        case CONNECTED:         stateName = "online"; break;
        case CHECKING:
        case CHECKING_VERSION:  stateName = "checking"; break;
        case UPDATING:          stateName = "updating"; break;
        case UPDATE_FAILED:     stateName = "failed"; break;
        default:                stateName = "idle"; break;
    }
    doc["otaState"] = stateName;

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

bool BMOTA::connectWifi() {
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
        Serial.println("[OTA] No WiFi credentials configured - OTA idle. Set via app.");
        state_ = IDLE;
        return false;
    }
    Serial.println("[OTA] ---- OTA Configuration ----");
    Serial.printf("[OTA]   Firmware URL: %s\n", OTA_FIRMWARE_URL);
    Serial.printf("[OTA]   WiFi SSID:    %s\n", ssid.c_str());
    Serial.printf("[OTA]   Check interval: %lu ms\n", (unsigned long)OTA_CHECK_INTERVAL_MS);
    Serial.println("[OTA] ------------------------------");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.println("[OTA] WiFi connecting...");
    state_ = CONNECTING;
    wifiConnectStart_ = millis();
    return true;
}

void BMOTA::begin() {
    Serial.println("[OTA] Initializing OTA subsystem...");
    Serial.printf("[OTA]   Boot delay: %lu ms\n", (unsigned long)OTA_BOOT_DELAY_MS);
    // Start the boot-delay clock regardless of whether WiFi comes up now, so a
    // later provisioning + auto-check is not gated on it.
    bootCompleteTime_ = millis();
    connectWifi();
}

void BMOTA::startLocalOta() {
#if LOCAL_OTA_ENABLED
    if (localOtaStarted_) return;

    // A short, per-device mDNS name so several signs on one network do not
    // clash: bm-<last 4 hex of the MAC>.local, also reachable by IP.
    uint64_t mac = ESP.getEfuseMac();
    char host[24];
    snprintf(host, sizeof(host), "bm-%04x", (uint16_t)(mac & 0xFFFF));
    ArduinoOTA.setHostname(host);
    if (strlen(LOCAL_OTA_PASSWORD) > 0) {
        ArduinoOTA.setPassword(LOCAL_OTA_PASSWORD);
    }
    ArduinoOTA.onStart([]() { Serial.println("[OTA] Local network flash starting"); });
    ArduinoOTA.onEnd([]() { Serial.println("[OTA] Local flash complete, rebooting"); });
    ArduinoOTA.onError([](ota_error_t err) { Serial.printf("[OTA] Local flash error: %u\n", err); });
    ArduinoOTA.begin();
    localOtaStarted_ = true;
    Serial.printf("[OTA] Local network flashing ready at %s.local (espota, port 3232)\n", host);
#endif
}

void BMOTA::loop() {
#if LOCAL_OTA_ENABLED
    // Cheap when idle; blocks and reboots only during an actual local push.
    // Skipped while the radio is down - battery builds turn WiFi fully off
    // between checks.
    if (localOtaStarted_ && WiFi.status() == WL_CONNECTED) ArduinoOTA.handle();
#endif

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
            if (firstCheck || intervalElapsed || forceCheckPending_) {
                lastCheckWasManual_ = forceCheckPending_;
                forceCheckPending_ = false;
                lastCheckTime_ = now;
                state_ = CHECKING;
                break;
            }
#if !OTA_KEEP_WIFI_ALIVE
            // Check done. On battery, drop the radio rather than sitting
            // associated until the next hourly interval; a manual check keeps
            // the link up for a few minutes so espota can follow it.
            if (lingerUntil_ == 0 || (long)(now - lingerUntil_) >= 0) {
                shutdownWifi();
            }
#endif
            break;
        }
        case CHECKING:
            Serial.println("[OTA] Starting update check...");
            versionCheckResult = VERSION_CHECK_PENDING;
            otaResult = OTA_RESULT_PENDING;
            updateStartTime_ = millis();
            xTaskCreate(otaCheckAndUpdateTask, "ota_task", 16384, NULL, 5, NULL);
            state_ = CHECKING_VERSION;
            break;
        case CHECKING_VERSION:
            if (versionCheckResult == VERSION_CHECK_SAME || versionCheckResult == VERSION_CHECK_FAIL) {
                lingerUntil_ = lastCheckWasManual_ ? millis() + OTA_MANUAL_LINGER_MS : 0;
                state_ = CONNECTED;
            } else if (versionCheckResult == VERSION_CHECK_NEW) {
                state_ = UPDATING;
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
                lingerUntil_ = lastCheckWasManual_ ? millis() + OTA_MANUAL_LINGER_MS : 0;
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

void BMOTA::shutdownWifi() {
    Serial.println("[OTA] WiFi radio off - reconnects on the next request (app: Check for updates)");
    WiFi.disconnect(true /* turn the radio off */);
    WiFi.mode(WIFI_OFF);
    lingerUntil_ = 0;
    state_ = IDLE;
}

void BMOTA::handleConnecting() {
    wl_status_t status = WiFi.status();
    if (status == WL_CONNECTED) {
        unsigned long elapsed = millis() - wifiConnectStart_;
        Serial.printf("[OTA] WiFi connected in %lu ms\n", elapsed);
        Serial.printf("[OTA] IP: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("[OTA] RSSI: %d dBm\n", WiFi.RSSI());
        state_ = CONNECTED;
        startLocalOta();  // now that WiFi is up, accept local network pushes too
    } else if (status == WL_CONNECT_FAILED || status == WL_NO_SSID_AVAIL) {
        Serial.printf("[OTA] WiFi connection failed (status: %d)\n", status);
        // Fully off, not just IDLE: the Arduino stack keeps auto-reconnecting
        // in STA mode otherwise, burning power with no state machine watching.
        shutdownWifi();
    } else if (millis() - wifiConnectStart_ > 30000) {
        Serial.println("[OTA] WiFi connection timed out after 30s");
        shutdownWifi();
    }
}


#endif  // OTA_ENABLED
