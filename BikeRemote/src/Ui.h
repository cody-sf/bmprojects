#pragma once
#include "Theme.h"

/** Shared widgets, drawn in the RNUmbrella visual language. */

struct Rect {
  int16_t x, y, w, h;
  bool contains(int16_t px, int16_t py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
};

uint16_t rgbTo565(uint8_t r, uint8_t g, uint8_t b);
void hsvToRgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b);

void uiBegin();

// Display sleep: the backlight is the whole power story on a CYD, so sleep is
// simply switching it off. The panel keeps drawing underneath - state stays
// current, waking is instant.
void uiBacklight(bool on);

// Main screens keep the rail; modal screens (color, devices, cal) take the
// full width.
void uiClearContent(bool modal);
// Header: title on the left, the device name + link dot on the right.
void uiHeader(const char* title, bool withRail);

// The left rail: four tabs stacked, gear at the bottom.
static const char* const TAB_LABELS[4] = {"Home", "Palettes", "Modes", "Tweaks"};
void uiTabRail(int active);
int uiTabHit(int16_t x, int16_t y);  // -1 none, 0-3 tabs, 4 gear

void uiChip(const Rect& r, const char* label, bool selected, uint16_t bg = COL_CHIP,
            uint16_t fg = COL_TEXT);
void uiButton(const Rect& r, const char* label, uint16_t bg, uint16_t fg);
void uiToggle(const Rect& r, bool on, bool enabled);
void uiSwatch(const Rect& r, uint16_t color);
void uiCaption(int16_t x, int16_t y, const char* text);

// A labelled slider row (label + value above a track). Total height 34.
// uiSliderRow shows the value as a percentage; the Range variant shows the
// raw number and maps the fill across [minV, maxV] (the Tweaks params).
void uiSliderRow(int16_t x, int16_t y, int16_t w, const char* label, int value);
void uiSliderRowRange(int16_t x, int16_t y, int16_t w, const char* label, int value,
                      int minV, int maxV);
Rect uiSliderHit(int16_t x, int16_t y, int16_t w);
int uiSliderValue(int16_t x, int16_t w, int16_t touchX);  // 0..100
int uiSliderValueRange(int16_t x, int16_t w, int16_t touchX, int minV, int maxV);

// Gradient card with the palette name, orange ring when selected. Raw colors
// so a built-in palette and a device-reported custom slot draw the same way.
void uiPaletteCard(const Rect& r, const uint8_t (*colors)[3], uint8_t count,
                   const char* name, bool selected);
