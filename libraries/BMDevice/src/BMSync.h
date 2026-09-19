#ifndef BM_SYNC_H
#define BM_SYNC_H

#include <Arduino.h>
#include <functional>
#include <Clock.h>

// ESP-NOW group sync: every device broadcasts its look (power, brightness,
// speed, direction, effect, palette) whenever the look changes locally, and
// adopts whatever it hears from devices with the same owner. Pure broadcast to
// FF:FF:FF:FF:FF:FF - no peer roster, no MAC tables - so a new board (any
// chip, including the C6) joins the group just by sharing an owner string
// (compared case-insensitively).
//
// Packets also carry the sender's Clock::now(), and receivers feed it to
// Clock::synchronize(), so effects on every device run off one shared
// timebase - streams and meteors sweep in phase, not just in the same colours.
//
// The radio runs in one of two modes:
//  - OWN:  nobody else wanted WiFi, so sync owns the radio - STA up,
//          modem sleep off, pinned to BM_SYNC_CHANNEL (verified and re-pinned
//          if anything drags it off).
//  - RIDE: BMOTA has the STA associated with an AP (a bench at home, or a
//          mains-powered sign that keeps WiFi up), so sync rides the AP's
//          channel instead of going deaf. Devices on the same AP share a
//          channel and keep syncing right through OTA windows.
// The radio is only up at all while the device is powered on and sync is
// enabled - always-on ESP-NOW receive costs tens of mA. A device that powers
// on late announces itself with a QUERY, and the group answers with its look.

/// The synchronized subset of device state, in wire units. Brightness crosses
/// as a fraction (0-100) of the *sender's* max brightness, and each receiver
/// re-scales it against its own max - so the group dims proportionally and a
/// device with a low cap is never pushed past it (or slammed onto it).
struct BMSyncState {
    uint8_t power;
    uint8_t brightnessPercent;
    uint16_t speed;
    uint8_t reverse;
    uint8_t effect;
    uint8_t palette;
};

class BMSync {
public:
    BMSync();

    /// One-time setup. The clock outlives BMSync (it is BMDevice's member).
    /// Radio bring-up does not happen here - service() owns the radio.
    void configure(Clock* clock);

    /// The sync group. Devices only apply packets whose group matches
    /// (case-insensitive); called again whenever the owner changes over BLE.
    void setGroup(const String& group);

    /// Called from the sync apply path and anywhere local state changes: what
    /// the next service() compares against to decide whether to broadcast.
    void setApplyCallback(std::function<void(const BMSyncState&)> callback);

    /// Drive everything: radio lifecycle, receive mailbox, change broadcast.
    /// `current` is the device's live state; `active` is power && syncEnabled.
    /// Call at a relaxed pace (~100 ms) - it rate-limits its own sends.
    void service(const BMSyncState& current, bool active);

    // Diagnostics, reported in the devConfig status chunk so a bench test can
    // see the radio from the app: "own" / "ride" / "off", and packet counts.
    const char* radioStateName() const;
    uint32_t txCount() const { return txCount_; }
    uint32_t rxCount() const { return rxAccepted_; }

    // For the ESP-NOW receive trampoline only (it runs in the WiFi task, so
    // this must do nothing but fill the mailbox). The trampoline is a free
    // function in the .cpp because its signature differs between IDF 4 (the
    // classic env's core 2.x) and IDF 5 (the C6's pioarduino core 3.x).
    static BMSync* instanceForReceive() { return instance_; }
    void receiveFromRadio(const uint8_t* data, int len) { handleReceived(data, len); }

private:
    enum PacketType : uint8_t {
        PACKET_STATE = 1,
        // A just-powered-on device asking the group where it left off. Every
        // peer answers with its state after a random delay; the newcomer
        // applies whichever it hears (they agree if the group was in sync).
        PACKET_QUERY = 2,
    };

    enum RadioState : uint8_t {
        RADIO_DOWN = 0,
        RADIO_OWN,   // our STA, pinned to BM_SYNC_CHANNEL
        RADIO_RIDE,  // OTA's STA, the AP's channel
    };

    struct __attribute__((packed)) Packet {
        char magic[4];      // "BMSY"
        uint8_t version;
        uint8_t type;       // PacketType
        char group[16];     // owner string, NUL-padded
        uint32_t clockNow;  // sender's Clock::now() at send time
        BMSyncState state;  // meaningful for PACKET_STATE only
    };

    void ensureRadio(bool want);
    void claimRadio();
    bool startEspNow(RadioState mode);
    void stopEspNow(bool turnWifiOff, const char* why);
    void pinChannel();
    void sendPacket(PacketType type, const BMSyncState& state);
    void handleReceived(const uint8_t* data, int len);
    bool stateEquals(const BMSyncState& a, const BMSyncState& b) const;

    static BMSync* instance_;

    Clock* clock_;
    char group_[16];
    std::function<void(const BMSyncState&)> applyCallback_;

    RadioState radio_;
    unsigned long lastRadioAttemptAt_;
    unsigned long lastChannelCheckAt_;

    BMSyncState lastSent_;
    bool baselinePrimed_;   // lastSent_ holds a real snapshot
    bool primeNextService_; // absorb an applied packet before diffing again
    unsigned long lastSendAt_;
    unsigned long replyAt_;  // deadline for answering a QUERY (0 = none)
    // Broadcast frames get no retries, and BLE coexistence eats frames, so
    // every state change is sent twice a beat apart.
    unsigned long repeatAt_;

    // Diagnostics
    uint32_t txCount_;
    volatile uint32_t rxAccepted_;
    volatile uint32_t rxWrongGroup_;
    uint32_t lastWrongGroupLogged_;
    char lastWrongGroup_[17];
    unsigned long lastTxErrorLogAt_;

    // Single-slot mailbox from the WiFi task's receive callback to service().
    // Applying state from the callback would race the render loop.
    volatile bool rxPending_;
    volatile bool rxQueryPending_;
    Packet rxPacket_;
};

#endif // BM_SYNC_H
