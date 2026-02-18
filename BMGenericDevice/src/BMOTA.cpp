/**
 * BMOTA - HTTPS OTA update implementation
 */

#if OTA_ENABLED

#include "BMOTA.h"
#include "OTAConfig.h"
#include <WiFi.h>
#include <HttpsOTAUpdate.h>

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

void BMOTA::begin() {
    Serial.println("[OTA] HTTPS OTA enabled - will check for updates");
    Serial.printf("[OTA] Firmware URL: %s\n", OTA_FIRMWARE_URL);
    WiFi.mode(WIFI_STA);
    WiFi.begin(OTA_WIFI_SSID, OTA_WIFI_PASSWORD);
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
