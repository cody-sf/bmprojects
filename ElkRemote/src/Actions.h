#pragma once
#include <stdint.h>

/**
 * The Elk page's behaviour, one level above raw frames: everything visual
 * switches an off bar on first (ensureOn), group sends walk every connected
 * bar, and a palette tap deals one sampled colour per bar in label order.
 * State updates are optimistic - the bars never talk back.
 */

void actPowerAll(bool on);
void actPowerBar(int idx, bool on);
void actBrightness(uint8_t pct);
void actSpeed(uint8_t pct);
void actColorAll(uint8_t r, uint8_t g, uint8_t b);
void actColorBar(int idx, uint8_t r, uint8_t g, uint8_t b);
void actMode(int modeIdx);
void actPalette(int paletteIdx);

// Toggle the palette cycle (persisted). The cycle runs from actTick
// whenever a palette is selected: each bar walks the vivid track at its
// own offset, so the group always spans the palette. The Speed slider
// sets the pace.
void actSetCycle(bool on);

// Flash one bar white for a moment so you can tell which is which; the
// restore happens in actTick.
void actIdentify(int idx);
void actTick();
