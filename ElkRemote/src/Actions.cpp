#include "Actions.h"
#include "AppState.h"
#include "ElkCodec.h"
#include "ElkLink.h"
#include "Gradient.h"
#include "Palettes.h"
#include "Store.h"
#include <Arduino.h>
#include <string.h>

static void sendPower(int idx, bool on) {
  uint8_t frame[ELK_FRAME_LEN];
  elkPowerFrame(frame, on);
  elkSend(idx, frame);
  app.bars[idx].power = on;
}

static void ensureOn(int idx) {
  if (!app.bars[idx].power) sendPower(idx, true);
}

static void sendColor(int idx, uint8_t r, uint8_t g, uint8_t b) {
  uint8_t frame[ELK_FRAME_LEN];
  elkColorFrame(frame, r, g, b);
  elkSend(idx, frame);
  Bar& bar = app.bars[idx];
  bar.hasColor = true;
  bar.r = r;
  bar.g = g;
  bar.b = b;
}

void actPowerAll(bool on) {
  for (int i = 0; i < MAX_BARS; i++) {
    if (app.bars[i].used && app.bars[i].connected) sendPower(i, on);
  }
}

void actPowerBar(int idx, bool on) {
  if (app.bars[idx].connected) sendPower(idx, on);
}

void actBrightness(uint8_t pct) {
  app.brightness = pct;
  uint8_t frame[ELK_FRAME_LEN];
  elkBrightnessFrame(frame, pct);
  for (int i = 0; i < MAX_BARS; i++) {
    if (app.bars[i].used && app.bars[i].connected) elkSend(i, frame);
  }
}

void actSpeed(uint8_t pct) {
  app.speed = pct;
  uint8_t frame[ELK_FRAME_LEN];
  elkSpeedFrame(frame, pct);
  for (int i = 0; i < MAX_BARS; i++) {
    if (app.bars[i].used && app.bars[i].connected) elkSend(i, frame);
  }
}

void actColorAll(uint8_t r, uint8_t g, uint8_t b) {
  app.allR = r;
  app.allG = g;
  app.allB = b;
  app.selectedPalette = -1;
  app.selectedMode = -1;
  for (int i = 0; i < MAX_BARS; i++) {
    if (!app.bars[i].used || !app.bars[i].connected) continue;
    ensureOn(i);
    sendColor(i, r, g, b);
  }
}

void actColorBar(int idx, uint8_t r, uint8_t g, uint8_t b) {
  if (!app.bars[idx].connected) return;
  app.selectedPalette = -1;
  ensureOn(idx);
  sendColor(idx, r, g, b);
}

void actMode(int modeIdx) {
  if (modeIdx < 0 || modeIdx >= ELK_MODE_COUNT) return;
  app.selectedMode = modeIdx;
  app.selectedPalette = -1;
  uint8_t frame[ELK_FRAME_LEN];
  if (ELK_MODES[modeIdx].code == 0x00) {
    elkWarmWhiteFrame(frame);
  } else {
    elkModeFrame(frame, ELK_MODES[modeIdx].code);
  }
  for (int i = 0; i < MAX_BARS; i++) {
    if (!app.bars[i].used || !app.bars[i].connected) continue;
    ensureOn(i);
    elkSend(i, frame);
  }
}

/**
 * Connected bars in label order, so the same palette always lands the same
 * colour on the same bar - and reads left-to-right if the bars are laid out
 * in label order too (straight from Elk.tsx).
 */
static int sortedConnected(int* out) {
  int n = 0;
  for (int i = 0; i < MAX_BARS; i++) {
    if (app.bars[i].used && app.bars[i].connected) out[n++] = i;
  }
  for (int i = 1; i < n; i++) {
    int key = out[i];
    int j = i - 1;
    while (j >= 0 && strcasecmp(app.bars[out[j]].label, app.bars[key].label) > 0) {
      out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = key;
  }
  return n;
}

// --- palette deal + cycle ---
// One shared vivid track: rebuilt on every palette pick, walked by the
// cycle. Everything a bar is told to show comes off this track, so the deal
// and the cycle can never disagree about what a palette looks like.

static RGB8 cycleTrack[VIVID_TRACK_MAX];
static int cycleTrackLen = 0;
static float cyclePhase = 0.0f;
static uint32_t lastCycleMs = 0;

void actPalette(int paletteIdx) {
  if (paletteIdx < 0 || paletteIdx >= PALETTE_COUNT) return;
  int order[MAX_BARS];
  int n = sortedConnected(order);
  if (!n) return;
  app.selectedPalette = paletteIdx;
  app.selectedMode = -1;
  cycleTrackLen = buildVividTrack(PALETTES[paletteIdx], cycleTrack);
  cyclePhase = 0.0f;
  lastCycleMs = millis();
  for (int i = 0; i < n; i++) {
    // Cycling sweeps every colour past every bar anyway, so exact offsets
    // are right; a static deal to a small group keeps the app's jitter so a
    // re-tap reshuffles instead of always landing the same colours.
    float pos = static_cast<float>(i) / n;
    if (!app.cycleEnabled && n <= 3) {
      pos += static_cast<float>(random(1000)) / (1000.0f * n);
    }
    RGB8 c = trackColorAt(cycleTrack, cycleTrackLen, pos);
    ensureOn(order[i]);
    sendColor(order[i], c.r, c.g, c.b);
  }
}

void actSetCycle(bool on) {
  app.cycleEnabled = on;
  storeSaveCycle(on);
  lastCycleMs = millis();
}

// --- identify blink ---

static int8_t blinkIdx = -1;
static uint32_t blinkRestoreAt = 0;
static bool blinkPrevPower = false;
static bool blinkPrevHasColor = false;
static uint8_t blinkPrevR, blinkPrevG, blinkPrevB;

void actIdentify(int idx) {
  if (!app.bars[idx].connected || blinkIdx >= 0) return;
  Bar& bar = app.bars[idx];
  blinkIdx = idx;
  blinkPrevPower = bar.power;
  blinkPrevHasColor = bar.hasColor;
  blinkPrevR = bar.r;
  blinkPrevG = bar.g;
  blinkPrevB = bar.b;
  ensureOn(idx);
  sendColor(idx, 255, 255, 255);
  blinkRestoreAt = millis() + 900;
}

static void blinkTick() {
  if (blinkIdx < 0 || millis() < blinkRestoreAt) return;
  int idx = blinkIdx;
  blinkIdx = -1;
  if (!app.bars[idx].connected) return;
  if (blinkPrevHasColor) {
    sendColor(idx, blinkPrevR, blinkPrevG, blinkPrevB);
  }
  if (!blinkPrevPower) {
    uint8_t frame[ELK_FRAME_LEN];
    elkPowerFrame(frame, false);
    elkSend(idx, frame);
    app.bars[idx].power = false;
  }
  Bar& bar = app.bars[idx];
  bar.hasColor = blinkPrevHasColor;
  bar.r = blinkPrevR;
  bar.g = blinkPrevG;
  bar.b = blinkPrevB;
}

/**
 * The gentle cycle. Every 200ms each bar gets the track colour at
 * (phase + its offset); a step at these speeds moves a channel a couple of
 * counts, which the eye reads as a crossfade. Colours that haven't changed
 * since the last send are skipped, so slow settings stay quiet on the radio.
 */
static void cycleTick() {
  uint32_t now = millis();
  if (!app.cycleEnabled || app.selectedPalette < 0 || cycleTrackLen < 2) {
    lastCycleMs = now;  // no dt cliff when the cycle switches back on
    return;
  }
  if (now - lastCycleMs < 200) return;
  float dt = (now - lastCycleMs) / 1000.0f;
  lastCycleMs = now;

  // Speed 0 -> a full lap in ~3 minutes, 100 -> ~8 seconds.
  float period = 180.0f - app.speed * 1.72f;
  cyclePhase += dt / period;
  cyclePhase -= floorf(cyclePhase);

  int order[MAX_BARS];
  int n = sortedConnected(order);
  for (int i = 0; i < n; i++) {
    Bar& bar = app.bars[order[i]];
    if (!bar.power || order[i] == blinkIdx) continue;
    RGB8 c = trackColorAt(cycleTrack, cycleTrackLen, cyclePhase + static_cast<float>(i) / n);
    if (c.r != bar.r || c.g != bar.g || c.b != bar.b) {
      sendColor(order[i], c.r, c.g, c.b);
    }
  }
}

void actTick() {
  blinkTick();
  cycleTick();
}
