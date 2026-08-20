#pragma once
#include "TouchPad.h"

/**
 * Screen router. Four tabbed main screens (Home, Palettes, Modes, Bars) plus
 * Setup behind the header gear, and three modal screens (color picker,
 * rename keyboard, touch calibration) that take the whole display.
 */

enum Screen : uint8_t {
  SCR_HOME,
  SCR_PALETTES,
  SCR_MODES,
  SCR_BARS,
  SCR_SETTINGS,
  SCR_COLOR,
  SCR_RENAME,
  SCR_CAL,
};

void screensBegin();
void screensTouch(const TouchEvent& ev);
void screensTick();
