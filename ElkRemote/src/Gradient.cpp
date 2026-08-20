#include "Gradient.h"
#include <Arduino.h>

RGB8 paletteColorAt(const PaletteDef& p, float t) {
  if (p.count == 0) return {0, 0, 0};
  if (p.count == 1 || t <= 0.0f) return {p.colors[0][0], p.colors[0][1], p.colors[0][2]};
  if (t >= 1.0f) {
    const uint8_t* c = p.colors[p.count - 1];
    return {c[0], c[1], c[2]};
  }
  // Stops are evenly spaced (stopsFromColors), so the segment is arithmetic.
  float scaled = t * (p.count - 1);
  int idx = static_cast<int>(scaled);
  float local = scaled - idx;
  const uint8_t* a = p.colors[idx];
  const uint8_t* b = p.colors[idx + 1];
  return {
    static_cast<uint8_t>(a[0] + (b[0] - a[0]) * local + 0.5f),
    static_cast<uint8_t>(a[1] + (b[1] - a[1]) * local + 0.5f),
    static_cast<uint8_t>(a[2] + (b[2] - a[2]) * local + 0.5f),
  };
}

void spreadPalette(const PaletteDef& p, int count, RGB8* out) {
  if (count <= 0) return;
  if (count > 3) {
    // Even spread: endpoints included, palette reads across the row.
    for (int i = 0; i < count; i++) {
      float t = count < 2 ? 0.0f : static_cast<float>(i) / (count - 1);
      out[i] = paletteColorAt(p, t);
    }
    return;
  }
  // Jittered: a pool of 16 even samples, one random pick per window.
  const int POOL = 16;
  for (int i = 0; i < count; i++) {
    int start = (i * POOL) / count;
    int end = ((i + 1) * POOL) / count;
    int span = end - start;
    int pick = start + (span > 0 ? random(span) : 0);
    if (pick > POOL - 1) pick = POOL - 1;
    out[i] = paletteColorAt(p, POOL < 2 ? 0.0f : static_cast<float>(pick) / (POOL - 1));
  }
}
