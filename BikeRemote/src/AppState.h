#pragma once
#include <stdint.h>
#include <string.h>

constexpr uint8_t DEV_NAME_MAX = 24;    // chars, excluding NUL
constexpr uint8_t FOUND_MAX = 8;        // picker rows during a discovery scan
constexpr uint8_t MARQUEE_MAX = 63;     // MARQUEE_TEXT_MAX in LightShow.h
constexpr uint8_t CUSTOM_PALETTE_COUNT = 4;
constexpr uint8_t CUSTOM_PALETTE_ENTRIES = 16;

// Effect parameters ride BLE codes 0x0B..0x18; values live in a flat array
// indexed by code - PARAM_CODE_BASE, mirroring the firmware's effectParams
// status chunk (ww/mc/tl/...).
constexpr uint8_t PARAM_CODE_BASE = 0x0B;
constexpr uint8_t PARAM_SLOTS = 14;

/** One BMDevice seen during a discovery scan (the Choose Light screen). */
struct FoundDevice {
  bool used = false;
  char addr[18] = {0};
  uint8_t addrType = 0;
  char name[DEV_NAME_MAX + 1] = {0};
  int8_t rssi = 0;
};

/** One custom palette slot mirrored from the device's cpal status chunks. */
struct CustomPalette {
  bool used = false;
  char name[17] = {0};
  uint8_t colors[CUSTOM_PALETTE_ENTRIES][3] = {{0}};
};

/**
 * The remote's whole world: one saved target device, its last reported state,
 * and the discovery list. Unlike the ELK bars, a BMDevice *does* talk back -
 * everything here is optimistic only until the next status chunk lands, and
 * the firmware pushes one on every change (app, watch or encoder included),
 * so this stays honest without polling.
 */
struct AppState {
  // Registry: the one light this remote is bound to. Persisted in NVS.
  bool haveTarget = false;
  char targetAddr[18] = {0};
  uint8_t targetType = 0;
  char deviceName[DEV_NAME_MAX + 1] = {0};

  volatile bool connected = false;
  volatile bool hasStatus = false;

  // Reported device state (see BMDevice's basicStatus/devConfig chunks).
  bool power = false;
  uint8_t brightness = 50;    // percent 1-100, capped by maxBrightness
  uint8_t speedPct = 50;      // percent 1-100, higher = faster
  bool reversed = false;
  uint8_t maxBrightness = 100;
  bool gpsAvailable = false;
  bool syncEnabled = false;
  int16_t mtxW = 0, mtxH = 0;
  // Matrix display content (matrix chunk): the stored scrolling text, the
  // stored-bitmap flag, and the text style. The content itself lives on the
  // device; pixel art is authored in the phone app only.
  char marquee[MARQUEE_MAX + 1] = {0};
  bool hasBitmap = false;
  bool boldText = false;
  uint8_t textFill = 0;      // 0 gradient, 1 fire, 2 rain, 3 plasma
  int16_t animFrames = 0;    // stored animation frames (phone-uploaded)
  // The display overlay's mode (`disp`): 0 off - the running effect owns the
  // panel - 1 marquee, 2 word zoom, 3 bitmap, 4 animation. Independent of
  // effectIdx: the rig keeps playing its mode while the display shows.
  uint8_t matrixDisplay = 0;

  int8_t effectIdx = 0;       // index into EFFECTS, -1 unknown
  // Palette selection: 0..PALETTE_COUNT-1 built-in, 100+slot custom, -1 unknown.
  int16_t paletteSel = -1;
  CustomPalette customs[CUSTOM_PALETTE_COUNT];

  int16_t params[PARAM_SLOTS] = {0};   // by code - PARAM_CODE_BASE
  uint8_t colR = 255, colG = 136, colB = 0;  // effect color (0x19)

  // Discovery (Choose Light screen).
  FoundDevice found[FOUND_MAX];

  // Set by the BLE task, consumed by the UI loop.
  volatile bool connectionsDirty = false;
  volatile bool statusDirty = false;
  volatile bool foundDirty = false;
};

extern AppState app;

constexpr int16_t CUSTOM_SEL_BASE = 100;

inline int paramSlot(uint8_t code) {
  int slot = code - PARAM_CODE_BASE;
  return (slot >= 0 && slot < PARAM_SLOTS) ? slot : -1;
}

// The app shows speed as 1-100 (higher = faster); the firmware wants a frame
// duration in 5..200 (lower = faster). Same mapping as BMDevice.tsx /
// BMSpeed in the watch app.
inline int speedPctToRaw(int pct) {
  if (pct < 1) pct = 1;
  if (pct > 100) pct = 100;
  int raw = (100 - pct) * 195 / 100 + 5;
  return raw < 5 ? 5 : (raw > 200 ? 200 : raw);
}

inline int speedRawToPct(int raw) {
  if (raw < 5) raw = 5;
  if (raw > 200) raw = 200;
  int pct = 100 - (raw - 5) * 100 / 195;
  return pct < 1 ? 1 : (pct > 100 ? 100 : pct);
}
