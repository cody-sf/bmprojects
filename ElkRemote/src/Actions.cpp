#include "Actions.h"
#include "AppState.h"
#include "ElkCodec.h"
#include "ElkLink.h"
#include "Gradient.h"
#include "Palettes.h"
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

void actPalette(int paletteIdx) {
  if (paletteIdx < 0 || paletteIdx >= PALETTE_COUNT) return;
  int order[MAX_BARS];
  int n = sortedConnected(order);
  if (!n) return;
  app.selectedPalette = paletteIdx;
  app.selectedMode = -1;
  RGB8 spread[MAX_BARS];
  spreadPalette(PALETTES[paletteIdx], n, spread);
  for (int i = 0; i < n; i++) {
    ensureOn(order[i]);
    sendColor(order[i], spread[i].r, spread[i].g, spread[i].b);
  }
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

void actTick() {
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
