#pragma once
#include <stdint.h>
#include "Palettes.h"

struct RGB8 { uint8_t r, g, b; };

/**
 * Palette sampling, ported from RNUmbrella/utils/gradient.ts.
 *
 * The app treats a built-in palette's colour list as evenly spaced gradient
 * stops (`stopsFromColors`) and blends linearly in sRGB between them
 * (`sampleGradient`); banded built-ins get their hard edges from duplicated
 * adjacent colours in the list itself, so one code path covers everything.
 */

// Colour at position t (0..1) along the palette's gradient.
RGB8 paletteColorAt(const PaletteDef& p, float t);

// The palette spread from Elk.tsx: one colour per bar. Big groups get the
// even spread (samplePalette); 3 or fewer draw one random position inside
// each of `count` equal windows (samplePaletteJittered) so a re-tap
// reshuffles instead of always landing the endpoints.
void spreadPalette(const PaletteDef& p, int count, RGB8* out);
