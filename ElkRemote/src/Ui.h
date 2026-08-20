#pragma once
#include "Theme.h"
#include "Palettes.h"

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
void uiClearContent(bool fullHeight);

// Header: bold title, "n/m" connected count, optional gear (tap = Setup).
void uiHeader(const char* title, bool showGear);
Rect uiGearRect();

static const char* const TAB_LABELS[4] = {"Home", "Palettes", "Modes", "Bars"};
void uiTabBar(int active);
int uiTabHit(int16_t x, int16_t y);

void uiChip(const Rect& r, const char* label, bool selected, uint16_t bg = COL_CHIP,
            uint16_t fg = COL_TEXT);
void uiButton(const Rect& r, const char* label, uint16_t bg, uint16_t fg);
void uiToggle(const Rect& r, bool on, bool enabled);
void uiSwatch(const Rect& r, uint16_t color);
void uiCaption(int16_t x, int16_t y, const char* text);

// A labelled slider row (label + value% above a track). Total height 34.
void uiSliderRow(int16_t x, int16_t y, int16_t w, const char* label, int value);
Rect uiSliderHit(int16_t x, int16_t y, int16_t w);
int uiSliderValue(int16_t x, int16_t w, int16_t touchX);

// Gradient card with the palette name, orange ring when selected.
void uiPaletteCard(const Rect& r, const PaletteDef& p, bool selected);
