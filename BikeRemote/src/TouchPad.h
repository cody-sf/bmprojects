#pragma once
#include <stdint.h>

/**
 * XPT2046 resistive touch on its own SPI bus (the CYD wires it separately
 * from the TFT), turned into DOWN/MOVE/UP events in screen coordinates via a
 * stored affine calibration. No calibration yet = raw reads only, which is
 * what the calibration screen itself uses.
 */

enum TouchType : uint8_t { T_NONE, T_DOWN, T_MOVE, T_UP };

struct TouchEvent {
  TouchType type;
  int16_t x, y;
};

void touchInit();
bool touchHasCal();
void touchSetCal(const float m[6]);  // also persists

// Averaged raw sample (0..4095 axes); true while pressed.
bool touchRawSample(int16_t& rx, int16_t& ry);

// Calibrated event stream; call every loop.
TouchEvent touchPoll();
