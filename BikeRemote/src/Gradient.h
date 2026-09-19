#pragma once
#include <stdint.h>

struct RGB8 { uint8_t r, g, b; };

/**
 * Gradient sampling for the palette cards, ported from ElkRemote (itself from
 * RNUmbrella/utils/gradient.ts): the color list is treated as evenly spaced
 * stops blended linearly in sRGB - what LinearGradient previews and FastLED
 * renders. Takes a raw color array rather than a PaletteDef so the same card
 * can draw a built-in palette or a device-reported custom slot.
 */
RGB8 gradientColorAt(const uint8_t (*colors)[3], uint8_t count, float t);
