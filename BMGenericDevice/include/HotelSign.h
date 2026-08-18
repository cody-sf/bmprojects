/**
 * HotelSign - the neon flicker driver for the Capsule Hotel sign.
 *
 * The "NO VACANCY" tube hangs off its own pin and is deliberately not part of
 * the BMDevice light show: it is one colour doing one thing, and that thing is
 * failing convincingly. What used to be a fixed state machine is now six
 * parameters plus a colour, so the flicker can be dialled from "dead steady"
 * to "about to give up" without a reflash.
 *
 * Presets are just named parameter sets. Touching any parameter drops the sign
 * into SIGN_PRESET_CUSTOM, so the app never shows a preset that is no longer
 * what is actually running.
 */
#ifndef HOTEL_SIGN_H
#define HOTEL_SIGN_H

#include <Arduino.h>
#include <FastLED.h>

// BLE feature codes. These sit at 0x70+, clear of the BMDevice framework range
// (ends at 0x38) and of every other device's command table (the umbrella app
// reaches 0x6A). The mobile app treats them as "common" and checks that table
// first for every device, so the codes must not collide with any other type.
#define BLE_FEATURE_SIGN_PRESET         0x70
#define BLE_FEATURE_SIGN_BURN_LEVEL     0x71
#define BLE_FEATURE_SIGN_HUM            0x72
#define BLE_FEATURE_SIGN_GLITCH_RATE    0x73
#define BLE_FEATURE_SIGN_GLITCH_LENGTH  0x74
#define BLE_FEATURE_SIGN_STUTTER_SPEED  0x75
#define BLE_FEATURE_SIGN_DROPOUT        0x76
#define BLE_FEATURE_SIGN_COLOR          0x77

enum SignPreset : uint8_t {
    SIGN_PRESET_CUSTOM = 0,   // whatever the sliders say
    SIGN_PRESET_ALWAYS_ON,    // no flicker at all
    SIGN_PRESET_WARM_HUM,     // alive, restless, rarely drops
    SIGN_PRESET_OLD_NEON,     // the classic: mostly lit, occasional fit
    SIGN_PRESET_BAD_BALLAST,  // frequent stutters and dropouts
    SIGN_PRESET_LAST_LEGS,    // dark more often than not
    SIGN_PRESET_COUNT,
};

/**
 * Every knob, on a 0-100 scale so the app can render plain sliders and the
 * meaning survives a change of internal timing.
 */
struct HotelSignParams {
    uint8_t preset;        // SignPreset
    uint8_t burnLevel;     // lit brightness, as a % of the device brightness
    uint8_t hum;           // restlessness while lit
    uint8_t glitchRate;    // how often it acts up (0 = never)
    uint8_t glitchLength;  // how long each fit lasts
    uint8_t stutterSpeed;  // strobe rate inside a stuttering fit
    uint8_t dropout;       // how readily a fit goes fully dark
    CRGB color;            // tube colour; scaled, never replaced
};

class HotelSign {
public:
    /** Adopt the strip and load saved parameters from NVRAM. */
    void begin(CRGB* leds, uint16_t numLeds);

    /**
     * Advance one frame. `ceiling` is the device brightness (1-255) the lit
     * state is measured against. Returns true when the pixels changed and the
     * caller needs to clock the strip out - showing 350 LEDs costs ~10 ms of
     * blocking output, so this is deliberately not every loop pass.
     */
    bool update(bool power, uint8_t ceiling);

    /** Returns true if the code belonged to the sign and was consumed. */
    bool handleFeature(uint8_t feature, const uint8_t* data, size_t length);

    /** The "sign" status chunk, for the app's controls to track the hardware. */
    String statusJson() const;

    void applyPreset(uint8_t preset);
    const HotelSignParams& params() const { return params_; }

private:
    void loadParams();
    void saveParams();
    void enterIdle(unsigned long now);
    void rollEvent(unsigned long now);
    /** Mark parameters changed by hand: leaves any preset, schedules a save. */
    void touched();

    enum State : uint8_t { IDLE, STUTTER, DIM, DEAD };

    CRGB* leds_ = nullptr;
    uint16_t numLeds_ = 0;

    HotelSignParams params_;

    State state_ = IDLE;
    unsigned long stateEndMs_ = 0;
    unsigned long nextEventMs_ = 0;
    unsigned long lastFrameMs_ = 0;
    unsigned long lastStutterMs_ = 0;
    uint16_t stutterHalfPeriodMs_ = 40;
    bool stutterHigh_ = true;
    bool blanked_ = false;

    unsigned long saveDueAt_ = 0;  // 0 when there is nothing to write
};

#endif  // HOTEL_SIGN_H
