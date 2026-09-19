#pragma once
#include "TouchPad.h"

/**
 * Screen router. Four tabbed main screens (Home, Palettes, Modes, Tweaks)
 * plus Setup behind the rail gear, and the modal screens (color picker,
 * matrix display, marquee keyboard, choose-light, touch calibration) that
 * take the whole display.
 */

enum Screen : uint8_t {
  SCR_HOME,
  SCR_PALETTES,
  SCR_MODES,
  SCR_TWEAKS,
  SCR_SETTINGS,
  SCR_COLOR,
  SCR_MATRIX,
  SCR_TEXT,
  SCR_DEVICES,
  SCR_CAL,
};

void screensBegin();
void screensTouch(const TouchEvent& ev);
void screensTick();

// False while calibrating - its raw touch reads never reach the display-sleep
// activity tracking, so sleeping there would strand the flow mid-target.
bool screensAllowSleep();
