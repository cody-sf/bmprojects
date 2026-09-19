#include "BMSync.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>

// A channel nothing at camp is likely to sit on matters less than every device
// agreeing on one; 6 is what the old sync used, so mixed old/new flashes still
// share a channel. Only used in OWN mode - riding an AP uses its channel.
#define BM_SYNC_CHANNEL 6
// Floor between two state broadcasts, so a slider drag streams a handful of
// packets rather than hundreds. Receivers coalesce naturally (single-slot
// mailbox), so more would only heat the air.
#define BM_SYNC_MIN_SEND_GAP_MS 150
// The no-retry repeat of a state packet, spaced past the coex sleep windows.
#define BM_SYNC_REPEAT_GAP_MS 90
// How often to retry claiming the radio while something else (OTA) holds it.
#define BM_SYNC_RADIO_RETRY_MS 500
// How often OWN mode verifies the channel is still where it was pinned.
#define BM_SYNC_CHANNEL_CHECK_MS 2000

static const uint8_t kBroadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

BMSync* BMSync::instance_ = nullptr;

// The receive callback signature changed with IDF 5 (Arduino core 3.x - the
// pioarduino platform the C6 builds on). The classic env is core 2.x / IDF 4.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static void bmSyncOnReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    (void)info;
    if (BMSync::instanceForReceive()) {
        BMSync::instanceForReceive()->receiveFromRadio(data, len);
    }
}
#else
static void bmSyncOnReceive(const uint8_t* mac, const uint8_t* data, int len) {
    (void)mac;
    if (BMSync::instanceForReceive()) {
        BMSync::instanceForReceive()->receiveFromRadio(data, len);
    }
}
#endif

BMSync::BMSync()
    : clock_(nullptr),
      radio_(RADIO_DOWN), lastRadioAttemptAt_(0), lastChannelCheckAt_(0),
      baselinePrimed_(false), primeNextService_(false),
      lastSendAt_(0), replyAt_(0), repeatAt_(0),
      txCount_(0), rxAccepted_(0), rxWrongGroup_(0), lastWrongGroupLogged_(0),
      lastTxErrorLogAt_(0),
      rxPending_(false), rxQueryPending_(false) {
    memset(group_, 0, sizeof(group_));
    memset(&lastSent_, 0, sizeof(lastSent_));
    memset(&rxPacket_, 0, sizeof(rxPacket_));
    memset(lastWrongGroup_, 0, sizeof(lastWrongGroup_));
    instance_ = this;
}

void BMSync::configure(Clock* clock) {
    clock_ = clock;
}

void BMSync::setGroup(const String& group) {
    memset(group_, 0, sizeof(group_));
    strncpy(group_, group.c_str(), sizeof(group_) - 1);
}

void BMSync::setApplyCallback(std::function<void(const BMSyncState&)> callback) {
    applyCallback_ = callback;
}

const char* BMSync::radioStateName() const {
    switch (radio_) {
        case RADIO_OWN: return "own";
        case RADIO_RIDE: return "ride";
        default: return "off";
    }
}

void BMSync::service(const BMSyncState& current, bool active) {
    unsigned long now = millis();

    // A packet applied last pass has since flowed into `current` (possibly
    // clamped by this device's max brightness or missing custom palette).
    // Adopt that as the baseline *before* diffing, or the clamped version
    // would read as a local change and be broadcast back - a lower cap on one
    // device would then dim the whole group.
    if (primeNextService_) {
        primeNextService_ = false;
        lastSent_ = current;
    }

    if (rxQueryPending_) {
        rxQueryPending_ = false;
        // Jittered so eight answers don't all land in the same slot.
        if (radio_ != RADIO_DOWN && active) {
            replyAt_ = now + random(100, 500);
        }
    }

    if (rxPending_) {
        Packet packet;
        memcpy(&packet, (const void*)&rxPacket_, sizeof(packet));
        rxPending_ = false;
        if (active && applyCallback_) {
            if (clock_) {
                TimeReference ref = {packet.clockNow};
                clock_->synchronize(ref);
            }
            Serial.printf("[BMSync] applying group state: fx=%d pal=%d bri=%d spd=%d pwr=%d\n",
                          packet.state.effect, packet.state.palette,
                          packet.state.brightnessPercent, packet.state.speed,
                          packet.state.power);
            applyCallback_(packet.state);
            primeNextService_ = true;
        }
    }

    // Surface wrong-group traffic: the single most likely reason "sync does
    // nothing" is two devices with different owner strings.
    if (rxWrongGroup_ != lastWrongGroupLogged_ && now - lastTxErrorLogAt_ > 5000) {
        lastWrongGroupLogged_ = rxWrongGroup_;
        lastTxErrorLogAt_ = now;
        Serial.printf("[BMSync] hearing another group '%s' (mine '%s', %lu ignored) - owners must match to sync\n",
                      lastWrongGroup_, group_, (unsigned long)rxWrongGroup_);
    }

    // Keep the radio one beat past power-off/disable so the final state (the
    // "I'm off" packet) reaches the group before the radio drops.
    bool unsentChange = radio_ != RADIO_DOWN && baselinePrimed_ && !stateEquals(current, lastSent_);
    ensureRadio(active || unsentChange);

    if (radio_ == RADIO_DOWN) {
        return;
    }

    if (!baselinePrimed_) {
        // Fresh on the air: adopt, don't impose. Prime the baseline silently
        // and ask the group where it left off; only *subsequent* local
        // changes broadcast.
        baselinePrimed_ = true;
        lastSent_ = current;
        sendPacket(PACKET_QUERY, current);
        return;
    }

    if (replyAt_ != 0 && now >= replyAt_) {
        replyAt_ = 0;
        sendPacket(PACKET_STATE, current);
        lastSent_ = current;
        return;
    }

    if (!stateEquals(current, lastSent_) && now - lastSendAt_ >= BM_SYNC_MIN_SEND_GAP_MS) {
        sendPacket(PACKET_STATE, current);
        lastSent_ = current;
        repeatAt_ = now + BM_SYNC_REPEAT_GAP_MS;
        return;
    }

    // Broadcast gets no link-layer retries and BLE coexistence eats frames,
    // so every change goes out twice. Receiving the copy is idempotent.
    if (repeatAt_ != 0 && now >= repeatAt_) {
        repeatAt_ = 0;
        sendPacket(PACKET_STATE, lastSent_);
    }
}

void BMSync::ensureRadio(bool want) {
    unsigned long now = millis();
    wifi_mode_t mode = WiFi.getMode();
    bool connected = (WiFi.status() == WL_CONNECTED);

    if (!want) {
        if (radio_ == RADIO_OWN) {
            stopEspNow(true /* ours to switch off */, "sync inactive");
        } else if (radio_ == RADIO_RIDE) {
            stopEspNow(false /* OTA's radio, leave it */, "sync inactive");
        }
        return;
    }

    switch (radio_) {
        case RADIO_OWN:
            if (mode != WIFI_STA) {
                // Something tore the STA down underneath us.
                stopEspNow(false, "lost STA");
                break;
            }
            if (connected) {
                // OTA associated: the AP owns the channel now. Ride it -
                // devices on the same AP stay in sync right through OTA.
                radio_ = RADIO_RIDE;
                WiFi.setSleep(false);
                Serial.println("[BMSync] OTA online - riding the AP's channel");
                break;
            }
            // Scans and coexistence can drag the primary channel around;
            // verify and re-pin so two OWN-mode devices can always meet.
            if (now - lastChannelCheckAt_ >= BM_SYNC_CHANNEL_CHECK_MS) {
                lastChannelCheckAt_ = now;
                uint8_t ch = 0;
                wifi_second_chan_t sc;
                if (esp_wifi_get_channel(&ch, &sc) == ESP_OK && ch != BM_SYNC_CHANNEL) {
                    Serial.printf("[BMSync] channel drifted to %d - re-pinning\n", ch);
                    pinChannel();
                }
            }
            break;

        case RADIO_RIDE:
            if (mode == WIFI_OFF) {
                // OTA shut the radio down; reclaim OWN mode next pass.
                stopEspNow(false, "OTA radio off");
            }
            // Disassociated but STA still up: OTA is between retries - stay
            // put, ESP-NOW keeps working on whatever channel we're on.
            break;

        case RADIO_DOWN:
        default:
            if (now - lastRadioAttemptAt_ < BM_SYNC_RADIO_RETRY_MS) {
                return;
            }
            lastRadioAttemptAt_ = now;
            if (mode == WIFI_OFF) {
                claimRadio();
            } else if (connected) {
                // OTA is online: join on its channel rather than waiting.
                WiFi.setSleep(false);
                startEspNow(RADIO_RIDE);
            }
            // else: someone else's STA is mid-connect - check again shortly.
            break;
    }
}

void BMSync::claimRadio() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    // ESP-NOW receive under BLE coexistence is hopeless with modem sleep on -
    // most frames land in a sleep window.
    WiFi.setSleep(false);
    pinChannel();
    startEspNow(RADIO_OWN);
}

void BMSync::pinChannel() {
    // The promiscuous wrap is the battle-tested recipe: on some cores a bare
    // esp_wifi_set_channel() on an unassociated STA reports OK but does not
    // take. Verify with a read-back and say what actually happened - a silent
    // channel mismatch is indistinguishable from "sync doesn't work at all".
    esp_wifi_set_promiscuous(true);
    esp_err_t err = esp_wifi_set_channel(BM_SYNC_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    uint8_t ch = 0;
    wifi_second_chan_t sc;
    esp_wifi_get_channel(&ch, &sc);
    if (err != ESP_OK || ch != BM_SYNC_CHANNEL) {
        Serial.printf("[BMSync] channel pin FAILED: err=%d, radio on %d (want %d)\n",
                      (int)err, ch, BM_SYNC_CHANNEL);
    } else {
        Serial.printf("[BMSync] channel pinned to %d\n", ch);
    }
}

bool BMSync::startEspNow(RadioState mode) {
    if (esp_now_init() != ESP_OK) {
        Serial.println("[BMSync] esp_now_init failed; retrying");
        if (mode == RADIO_OWN) {
            WiFi.mode(WIFI_OFF);
        }
        return false;
    }
    esp_now_register_recv_cb(bmSyncOnReceive);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, kBroadcastMac, 6);
    peer.channel = 0;  // whatever channel the radio is on
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    radio_ = mode;
    baselinePrimed_ = false;

    uint8_t ch = 0;
    wifi_second_chan_t sc;
    esp_wifi_get_channel(&ch, &sc);
    Serial.printf("[BMSync] radio up (%s) on channel %d, group '%s'\n",
                  radioStateName(), ch, group_);
    return true;
}

void BMSync::stopEspNow(bool turnWifiOff, const char* why) {
    esp_now_deinit();
    if (turnWifiOff) {
        WiFi.mode(WIFI_OFF);
    }
    radio_ = RADIO_DOWN;
    baselinePrimed_ = false;
    replyAt_ = 0;
    repeatAt_ = 0;
    rxPending_ = false;
    rxQueryPending_ = false;
    Serial.printf("[BMSync] radio down (%s)\n", why);
}

void BMSync::sendPacket(PacketType type, const BMSyncState& state) {
    Packet packet;
    memset(&packet, 0, sizeof(packet));
    memcpy(packet.magic, "BMSY", 4);
    packet.version = 1;
    packet.type = type;
    memcpy(packet.group, group_, sizeof(packet.group));
    packet.clockNow = clock_ ? (uint32_t)clock_->now() : (uint32_t)millis();
    packet.state = state;

    esp_err_t err = esp_now_send(kBroadcastMac, (const uint8_t*)&packet, sizeof(packet));
    unsigned long now = millis();
    if (err == ESP_OK) {
        txCount_++;
    } else if (now - lastTxErrorLogAt_ > 1000) {
        lastTxErrorLogAt_ = now;
        Serial.printf("[BMSync] esp_now_send failed: %d\n", (int)err);
    }
    lastSendAt_ = now;
}

void BMSync::handleReceived(const uint8_t* data, int len) {
    if (len != (int)sizeof(Packet)) {
        return;
    }
    Packet packet;
    memcpy(&packet, data, sizeof(packet));
    if (memcmp(packet.magic, "BMSY", 4) != 0 || packet.version != 1) {
        return;
    }
    // Case-insensitive: "Cody" and "cody" are one group. Both sides are
    // NUL-padded to a fixed 16, so the tails always agree.
    if (strncasecmp(packet.group, group_, sizeof(group_)) != 0) {
        rxWrongGroup_ = rxWrongGroup_ + 1;
        memcpy(lastWrongGroup_, packet.group, sizeof(packet.group));
        lastWrongGroup_[16] = 0;
        return;
    }

    if (packet.type == PACKET_QUERY) {
        rxAccepted_ = rxAccepted_ + 1;
        rxQueryPending_ = true;
        return;
    }
    if (packet.type != PACKET_STATE) {
        return;
    }
    rxAccepted_ = rxAccepted_ + 1;
    // Single-slot mailbox: only fill when empty. A dropped packet in a burst
    // costs nothing - the sender's next change (or repeat) re-delivers.
    if (!rxPending_) {
        memcpy((void*)&rxPacket_, &packet, sizeof(packet));
        rxPending_ = true;
    }
}

bool BMSync::stateEquals(const BMSyncState& a, const BMSyncState& b) const {
    // Field-wise, not memcmp: BMSyncState is unpacked and the padding byte is
    // indeterminate.
    return a.power == b.power &&
           a.brightnessPercent == b.brightnessPercent &&
           a.speed == b.speed &&
           a.reverse == b.reverse &&
           a.effect == b.effect &&
           a.palette == b.palette;
}
