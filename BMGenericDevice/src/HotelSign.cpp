#include "HotelSign.h"

#ifdef TARGET_HOTEL_SIGN

#include <ArduinoJson.h>
#include <Preferences.h>

static const char* SIGN_PREFS_NAMESPACE = "hotelsign";

// ~60 fps. The fastest stutter half period is 18 ms, so every visual event
// still resolves, and clocking 350 LEDs (~10 ms of blocking output) leaves
// room for the BLE poll.
static const unsigned long FRAME_INTERVAL_MS = 16;

// Parameters are written to NVRAM this long after the last change, so dragging
// a slider is one flash write rather than one per pixel of travel.
static const unsigned long SAVE_DEBOUNCE_MS = 1500;

/** Named parameter sets, indexed by SignPreset. Index 0 (custom) is unused. */
static const HotelSignParams PRESETS[SIGN_PRESET_COUNT] = {
    // preset, burn, hum, rate, len, stutter, dropout, colour (kept, see applyPreset)
    {SIGN_PRESET_CUSTOM,      90,  25, 38, 34, 55, 28, CRGB::Red},
    {SIGN_PRESET_ALWAYS_ON,  100,   0,  0,  0, 50,  0, CRGB::Red},
    {SIGN_PRESET_WARM_HUM,    94,  22, 14, 18, 40,  8, CRGB::Red},
    {SIGN_PRESET_OLD_NEON,    90,  28, 38, 34, 55, 28, CRGB::Red},
    {SIGN_PRESET_BAD_BALLAST, 84,  38, 64, 52, 72, 46, CRGB::Red},
    {SIGN_PRESET_LAST_LEGS,   70,  50, 88, 76, 84, 72, CRGB::Red},
};

static const uint8_t DEFAULT_PRESET = SIGN_PRESET_BAD_BALLAST;

static uint8_t clampPercent(long value) {
    if (value < 0) return 0;
    if (value > 100) return 100;
    return (uint8_t)value;
}

/** The app sends parameters as [code][int32 little-endian]. */
static bool readInt32(const uint8_t* data, size_t length, long& out) {
    if (length < 5) return false;
    out = (long)((uint32_t)data[1] | ((uint32_t)data[2] << 8) |
                 ((uint32_t)data[3] << 16) | ((uint32_t)data[4] << 24));
    return true;
}

/**
 * Gap between glitches, in ms, for a 1-100 rate.
 *
 * Geometric rather than linear: a linear map spends most of its travel in the
 * "once every ten seconds" range, so the middle of the slider felt dead.
 */
static unsigned long maxGapForRate(uint8_t rate) {
    const float slowest = 12000.0f;
    const float fastest = 250.0f;
    float t = (float)(rate - 1) / 99.0f;
    return (unsigned long)(slowest * powf(fastest / slowest, t));
}

void HotelSign::begin(CRGB* leds, uint16_t numLeds) {
    leds_ = leds;
    numLeds_ = numLeds;
    loadParams();
    state_ = IDLE;
    nextEventMs_ = millis() + 1000;
    Serial.printf("[HotelSign] preset %u, burn %u, hum %u, rate %u, len %u, stutter %u, dropout %u\n",
                  params_.preset, params_.burnLevel, params_.hum, params_.glitchRate,
                  params_.glitchLength, params_.stutterSpeed, params_.dropout);
}

void HotelSign::loadParams() {
    params_ = PRESETS[DEFAULT_PRESET];

    Preferences prefs;
    if (!prefs.begin(SIGN_PREFS_NAMESPACE, true)) {
        Serial.println("[HotelSign] No saved settings - using defaults");
        return;
    }
    params_.preset = clampPercent(prefs.getUChar("preset", params_.preset));
    if (params_.preset >= SIGN_PRESET_COUNT) params_.preset = DEFAULT_PRESET;
    params_.burnLevel = clampPercent(prefs.getUChar("burn", params_.burnLevel));
    params_.hum = clampPercent(prefs.getUChar("hum", params_.hum));
    params_.glitchRate = clampPercent(prefs.getUChar("rate", params_.glitchRate));
    params_.glitchLength = clampPercent(prefs.getUChar("len", params_.glitchLength));
    params_.stutterSpeed = clampPercent(prefs.getUChar("stut", params_.stutterSpeed));
    params_.dropout = clampPercent(prefs.getUChar("drop", params_.dropout));
    params_.color = CRGB(prefs.getUChar("r", params_.color.r),
                         prefs.getUChar("g", params_.color.g),
                         prefs.getUChar("b", params_.color.b));
    prefs.end();
}

void HotelSign::saveParams() {
    Preferences prefs;
    if (!prefs.begin(SIGN_PREFS_NAMESPACE, false)) {
        Serial.println("[HotelSign] Failed to open preferences for write");
        return;
    }
    prefs.putUChar("preset", params_.preset);
    prefs.putUChar("burn", params_.burnLevel);
    prefs.putUChar("hum", params_.hum);
    prefs.putUChar("rate", params_.glitchRate);
    prefs.putUChar("len", params_.glitchLength);
    prefs.putUChar("stut", params_.stutterSpeed);
    prefs.putUChar("drop", params_.dropout);
    prefs.putUChar("r", params_.color.r);
    prefs.putUChar("g", params_.color.g);
    prefs.putUChar("b", params_.color.b);
    prefs.end();
}

void HotelSign::touched() {
    params_.preset = SIGN_PRESET_CUSTOM;
    saveDueAt_ = millis() + SAVE_DEBOUNCE_MS;
}

void HotelSign::applyPreset(uint8_t preset) {
    if (preset == SIGN_PRESET_CUSTOM || preset >= SIGN_PRESET_COUNT) {
        return;
    }
    CRGB keepColor = params_.color;  // the colour is the sign's, not the preset's
    params_ = PRESETS[preset];
    params_.color = keepColor;
    saveDueAt_ = millis() + SAVE_DEBOUNCE_MS;

    // Re-arm rather than wait out a gap scheduled under the old rate: switching
    // to "last legs" should look different straight away.
    state_ = IDLE;
    enterIdle(millis());
}

void HotelSign::enterIdle(unsigned long now) {
    state_ = IDLE;
    if (params_.glitchRate == 0) {
        nextEventMs_ = (unsigned long)-1;  // never; a steady sign has no fits
        return;
    }
    unsigned long maxGap = maxGapForRate(params_.glitchRate);
    unsigned long minGap = max(100UL, maxGap / 5);
    nextEventMs_ = now + random(minGap, maxGap + 1);
}

void HotelSign::rollEvent(unsigned long now) {
    unsigned long minDur = 40 + (unsigned long)params_.glitchLength * 2;
    unsigned long maxDur = 120 + (unsigned long)params_.glitchLength * 24;
    stateEndMs_ = now + random(minDur, maxDur + 1);

    uint8_t roll = random8(100);
    uint8_t dimCutoff = params_.dropout + (uint8_t)(((100 - params_.dropout) * 2) / 5);

    if (roll < params_.dropout) {
        state_ = DEAD;
    } else if (roll < dimCutoff) {
        state_ = DIM;
    } else {
        state_ = STUTTER;
        lastStutterMs_ = now;
        stutterHigh_ = true;
        stutterHalfPeriodMs_ = max(18, 100 - (params_.stutterSpeed * 4) / 5);
    }
}

bool HotelSign::update(bool power, uint8_t ceiling) {
    unsigned long now = millis();

    if (saveDueAt_ != 0 && now >= saveDueAt_) {
        saveDueAt_ = 0;
        saveParams();
    }

    if (!power) {
        if (blanked_) {
            return false;
        }
        fill_solid(leds_, numLeds_, CRGB::Black);
        blanked_ = true;
        return true;
    }
    blanked_ = false;

    if (now - lastFrameMs_ < FRAME_INTERVAL_MS) {
        return false;
    }
    lastFrameMs_ = now;

    // The lit level everything else is measured against.
    uint8_t base = (uint8_t)(((uint16_t)ceiling * params_.burnLevel) / 100);
    uint8_t nearDark = max(2, ceiling / 25);
    uint8_t bright = 0;

    switch (state_) {
        case IDLE: {
            // Neon dips rather than overshoots, so the noise is one-sided.
            uint8_t swing = (uint8_t)(((uint16_t)base * params_.hum) / 400);
            bright = swing > 0 ? qsub8(base, random8(swing + 1)) : base;
            if (now >= nextEventMs_) {
                rollEvent(now);
            }
            break;
        }
        case STUTTER: {
            if (now - lastStutterMs_ >= stutterHalfPeriodMs_) {
                stutterHigh_ = !stutterHigh_;
                lastStutterMs_ = now;
                // Jitter each half period: an even strobe reads as a disco
                // light, an uneven one reads as a tube fighting its ballast.
                uint16_t nominal = max(18, 100 - (params_.stutterSpeed * 4) / 5);
                stutterHalfPeriodMs_ = random(max(18U, (unsigned)(nominal * 3) / 5),
                                              (nominal * 3) / 2 + 1);
            }
            bright = stutterHigh_ ? base : nearDark;
            if (now >= stateEndMs_) enterIdle(now);
            break;
        }
        case DIM: {
            uint8_t dimBase = (uint8_t)(((uint16_t)base * 22) / 100);
            bright = qsub8(dimBase, random8(4));
            if (bright < 3) bright = 3;
            if (now >= stateEndMs_) enterIdle(now);
            break;
        }
        case DEAD: {
            bright = nearDark;
            if (now >= stateEndMs_) enterIdle(now);
            break;
        }
    }

    CRGB lit = params_.color;
    lit.nscale8_video(bright);
    fill_solid(leds_, numLeds_, lit);
    return true;
}

bool HotelSign::handleFeature(uint8_t feature, const uint8_t* data, size_t length) {
    long value = 0;

    switch (feature) {
        case BLE_FEATURE_SIGN_PRESET:
            if (!readInt32(data, length, value)) return true;
            applyPreset((uint8_t)value);
            Serial.printf("[HotelSign] Preset -> %ld\n", value);
            return true;

        case BLE_FEATURE_SIGN_BURN_LEVEL:
            if (!readInt32(data, length, value)) return true;
            params_.burnLevel = clampPercent(value);
            touched();
            return true;

        case BLE_FEATURE_SIGN_HUM:
            if (!readInt32(data, length, value)) return true;
            params_.hum = clampPercent(value);
            touched();
            return true;

        case BLE_FEATURE_SIGN_GLITCH_RATE:
            if (!readInt32(data, length, value)) return true;
            params_.glitchRate = clampPercent(value);
            touched();
            // The pending gap was scheduled under the old rate; re-arm now so
            // the change is audible immediately rather than up to 12 s later.
            if (state_ == IDLE) enterIdle(millis());
            return true;

        case BLE_FEATURE_SIGN_GLITCH_LENGTH:
            if (!readInt32(data, length, value)) return true;
            params_.glitchLength = clampPercent(value);
            touched();
            return true;

        case BLE_FEATURE_SIGN_STUTTER_SPEED:
            if (!readInt32(data, length, value)) return true;
            params_.stutterSpeed = clampPercent(value);
            touched();
            return true;

        case BLE_FEATURE_SIGN_DROPOUT:
            if (!readInt32(data, length, value)) return true;
            params_.dropout = clampPercent(value);
            touched();
            return true;

        case BLE_FEATURE_SIGN_COLOR:
            if (length < 4) return true;
            params_.color = CRGB(data[1], data[2], data[3]);
            // Colour is not part of a preset, so it does not force custom.
            saveDueAt_ = millis() + SAVE_DEBOUNCE_MS;
            return true;

        default:
            return false;
    }
}

String HotelSign::statusJson() const {
    StaticJsonDocument<256> doc;
    doc["type"] = "sign";
    doc["sgPre"] = params_.preset;
    doc["sgBurn"] = params_.burnLevel;
    doc["sgHum"] = params_.hum;
    doc["sgRate"] = params_.glitchRate;
    doc["sgLen"] = params_.glitchLength;
    doc["sgStut"] = params_.stutterSpeed;
    doc["sgDrop"] = params_.dropout;
    JsonObject col = doc.createNestedObject("sgCol");
    col["r"] = params_.color.r;
    col["g"] = params_.color.g;
    col["b"] = params_.color.b;

    String out;
    serializeJson(doc, out);
    return out;
}

#endif  // TARGET_HOTEL_SIGN
