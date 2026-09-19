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

// Below this peak channel a sample reads as "off" on a bar and is dropped.
static const uint8_t DARK_CUTOFF = 28;
// Survivors are scaled so their peak channel reaches at least this - hue and
// saturation keep their ratios, only the level comes up.
static const uint8_t VIVID_FLOOR = 110;

static uint8_t maxChannel(const RGB8& c) {
  uint8_t m = c.r > c.g ? c.r : c.g;
  return c.b > m ? c.b : m;
}

static RGB8 vividize(const RGB8& c) {
  uint8_t m = maxChannel(c);
  if (m == 0 || m >= VIVID_FLOOR) return c;
  float scale = static_cast<float>(VIVID_FLOOR) / m;
  auto up = [scale](uint8_t v) {
    float s = v * scale;
    return static_cast<uint8_t>(s > 255.0f ? 255 : s + 0.5f);
  };
  return {up(c.r), up(c.g), up(c.b)};
}

int buildVividTrack(const PaletteDef& p, RGB8* track, int maxLen) {
  int len = 0;
  for (int i = 0; i < maxLen; i++) {
    float t = maxLen < 2 ? 0.0f : static_cast<float>(i) / (maxLen - 1);
    RGB8 c = paletteColorAt(p, t);
    if (maxChannel(c) >= DARK_CUTOFF) {
      track[len++] = vividize(c);
    }
  }
  if (len == 0) {
    // Dark everywhere (nothing clears the cutoff): keep the shape, boosted.
    for (int i = 0; i < maxLen; i++) {
      float t = maxLen < 2 ? 0.0f : static_cast<float>(i) / (maxLen - 1);
      track[len++] = vividize(paletteColorAt(p, t));
    }
  }
  return len;
}

RGB8 trackColorAt(const RGB8* track, int len, float phase) {
  if (len <= 0) return {0, 0, 0};
  if (len == 1) return track[0];
  phase -= floorf(phase);
  float f = phase * len;
  int i = static_cast<int>(f);
  if (i >= len) i = len - 1;
  float local = f - i;
  // Circular: the seam between last and first blends directly, never
  // through the dark band that may have been cut between them.
  const RGB8& a = track[i];
  const RGB8& b = track[(i + 1) % len];
  return {
    static_cast<uint8_t>(a.r + (b.r - a.r) * local + 0.5f),
    static_cast<uint8_t>(a.g + (b.g - a.g) * local + 0.5f),
    static_cast<uint8_t>(a.b + (b.b - a.b) * local + 0.5f),
  };
}
