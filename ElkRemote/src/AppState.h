#pragma once
#include <stdint.h>

constexpr uint8_t MAX_BARS = 8;
constexpr uint8_t LABEL_MAX = 16;  // chars, excluding NUL

/**
 * One registered light bar. `addr`/`addrType`/`label` persist in NVS; the
 * rest is runtime. `power`/`color` are optimistic - the bars never report
 * state (see Elk.tsx), so this is simply the last thing we told them.
 */
struct Bar {
  bool used = false;
  char addr[18] = {0};       // "aa:bb:cc:dd:ee:ff"
  uint8_t addrType = 0;
  char label[LABEL_MAX + 1] = {0};
  volatile bool connected = false;
  bool power = false;
  bool hasColor = false;
  uint8_t r = 0, g = 0, b = 0;
};

/** Group state, mirroring the Elk page's local state. */
struct AppState {
  Bar bars[MAX_BARS];
  uint8_t brightness = 100;
  uint8_t speed = 50;
  uint8_t allR = 0xff, allG = 0x88, allB = 0x00;
  int8_t selectedPalette = -1;
  int8_t selectedMode = -1;
  // Set by the BLE task whenever a bar connects/drops; screens repaint and
  // clear it from the UI loop.
  volatile bool connectionsDirty = false;
};

extern AppState app;

inline int barCount() {
  int n = 0;
  for (int i = 0; i < MAX_BARS; i++) if (app.bars[i].used) n++;
  return n;
}

inline int connectedCount() {
  int n = 0;
  for (int i = 0; i < MAX_BARS; i++) if (app.bars[i].used && app.bars[i].connected) n++;
  return n;
}
