#include "Gradient.h"

RGB8 gradientColorAt(const uint8_t (*colors)[3], uint8_t count, float t) {
  if (count == 0) return {0, 0, 0};
  if (count == 1 || t <= 0.0f) return {colors[0][0], colors[0][1], colors[0][2]};
  if (t >= 1.0f) {
    const uint8_t* c = colors[count - 1];
    return {c[0], c[1], c[2]};
  }
  // Stops are evenly spaced (stopsFromColors), so the segment is arithmetic.
  float scaled = t * (count - 1);
  int idx = static_cast<int>(scaled);
  float local = scaled - idx;
  const uint8_t* a = colors[idx];
  const uint8_t* b = colors[idx + 1];
  return {
    static_cast<uint8_t>(a[0] + (b[0] - a[0]) * local + 0.5f),
    static_cast<uint8_t>(a[1] + (b[1] - a[1]) * local + 0.5f),
    static_cast<uint8_t>(a[2] + (b[2] - a[2]) * local + 0.5f),
  };
}
