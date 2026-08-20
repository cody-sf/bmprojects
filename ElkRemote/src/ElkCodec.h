#pragma once
#include <stdint.h>
#include <string.h>

/**
 * The ELK-BLEDOM 9-byte frames, ported from encodeElkCommand in
 * RNUmbrella/providers/Bluetooth/commandCodec.ts. Those frames are pinned by
 * the app's tests and were verified against Cody's bars, so this file mirrors
 * them byte for byte: `7E 00 <cmd> <up to 4 args> .. EF`, unused args zero.
 */

constexpr uint8_t ELK_FRAME_LEN = 9;

// Bars' stock 16-bit UUIDs (ELK_UUIDS in constants.ts).
#define ELK_SERVICE_UUID  "fff0"
#define ELK_WRITE_UUID    "fff3"

inline void elkFrame(uint8_t* out, uint8_t cmd, const uint8_t* args, uint8_t argCount) {
  const uint8_t base[ELK_FRAME_LEN] = {0x7e, 0x00, cmd, 0x00, 0x00, 0x00, 0x00, 0x00, 0xef};
  memcpy(out, base, ELK_FRAME_LEN);
  for (uint8_t i = 0; i < argCount && i < 4; i++) {
    out[3 + i] = args[i];
  }
}

inline uint8_t elkPercent(int value) {
  return value < 0 ? 0 : (value > 100 ? 100 : static_cast<uint8_t>(value));
}

inline void elkPowerFrame(uint8_t* out, bool on) {
  const uint8_t onArgs[4]  = {0xf0, 0x00, 0x01, 0xff};
  const uint8_t offArgs[4] = {0x00, 0x00, 0x00, 0xff};
  elkFrame(out, 0x04, on ? onArgs : offArgs, 4);
}

inline void elkBrightnessFrame(uint8_t* out, int pct) {
  const uint8_t args[1] = {elkPercent(pct)};
  elkFrame(out, 0x01, args, 1);
}

inline void elkSpeedFrame(uint8_t* out, int pct) {
  const uint8_t args[1] = {elkPercent(pct)};
  elkFrame(out, 0x02, args, 1);
}

inline void elkColorFrame(uint8_t* out, uint8_t r, uint8_t g, uint8_t b) {
  const uint8_t args[4] = {0x03, r, g, b};
  elkFrame(out, 0x05, args, 4);
}

// warm_white is the white-channel command masquerading as a mode; level rides
// the master brightness the page already controls, so send full (see codec).
inline void elkWarmWhiteFrame(uint8_t* out) {
  const uint8_t args[2] = {0x01, 100};
  elkFrame(out, 0x05, args, 2);
}

inline void elkModeFrame(uint8_t* out, uint8_t code) {
  const uint8_t args[2] = {code, 0x03};
  elkFrame(out, 0x03, args, 2);
}

/** ELK_MODES from constants.ts. code 0x00 = the warm-white pseudo-mode. */
struct ElkMode { uint8_t code; const char* name; };
static const ElkMode ELK_MODES[] = {
  {0x00, "Warm White"},
  {0x87, "Jump: R/G/B"},
  {0x88, "Jump: Rainbow"},
  {0x89, "Fade: R/G/B"},
  {0x8a, "Fade: Rainbow"},
  {0x8b, "Breathe: Red"},
  {0x8c, "Breathe: Green"},
  {0x8d, "Breathe: Blue"},
  {0x8e, "Breathe: Yellow"},
  {0x8f, "Breathe: Cyan"},
  {0x90, "Breathe: Magenta"},
  {0x91, "Breathe: White"},
  {0x92, "Fade: Red/Green"},
  {0x93, "Fade: Red/Blue"},
  {0x94, "Fade: Green/Blue"},
  {0x95, "Strobe: Rainbow"},
  {0x96, "Strobe: Red"},
  {0x97, "Strobe: Green"},
  {0x98, "Strobe: Blue"},
  {0x99, "Strobe: Yellow"},
  {0x9a, "Strobe: Cyan"},
  {0x9b, "Strobe: Magenta"},
  {0x9c, "Strobe: White"},
};
static const uint8_t ELK_MODE_COUNT = sizeof(ELK_MODES) / sizeof(ELK_MODES[0]);
