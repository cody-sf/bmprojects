#pragma once
#include <stdint.h>
#include "Palettes.h"

struct RGB8 { uint8_t r, g, b; };

/**
 * Palette sampling, ported from RNUmbrella/utils/gradient.ts, then adapted
 * for single-colour fixtures.
 *
 * The app treats a built-in palette's colour list as evenly spaced gradient
 * stops (`stopsFromColors`) and blends linearly in sRGB between them
 * (`sampleGradient`). That is still how the cards are drawn - but a lot of
 * palettes are authored with black or near-black stretches (texture on a
 * 450-LED strip, a dead fixture on a light bar). So what actually goes to
 * the bars is the "vivid track": the gradient sampled densely, dark
 * stretches dropped, dim survivors boosted to a visible floor.
 */

// Colour at position t (0..1) along the palette's true gradient (card art).
RGB8 paletteColorAt(const PaletteDef& p, float t);

constexpr int VIVID_TRACK_MAX = 64;

// Build the vivid track for a palette. Always returns at least 1 entry; a
// palette that is dark everywhere keeps all its samples (boosted) rather
// than vanishing.
int buildVividTrack(const PaletteDef& p, RGB8* track, int maxLen = VIVID_TRACK_MAX);

// Colour at phase (0..1, wrapping) along a track, blending between samples -
// this is what makes the cycle read as a crossfade rather than steps.
RGB8 trackColorAt(const RGB8* track, int len, float phase);
