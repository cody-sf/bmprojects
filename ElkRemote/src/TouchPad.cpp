#include "TouchPad.h"
#include "Store.h"
#include "Theme.h"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

// CYD touch wiring; not the TFT bus.
#define XPT_CLK  25
#define XPT_MISO 39
#define XPT_MOSI 32
#define XPT_CS   33
#define XPT_IRQ  36

static SPIClass touchSpi(HSPI);
static XPT2046_Touchscreen ts(XPT_CS, XPT_IRQ);

static float cal[6] = {0};
static bool hasCal = false;

void touchInit() {
  touchSpi.begin(XPT_CLK, XPT_MISO, XPT_MOSI, XPT_CS);
  ts.begin(touchSpi);
  hasCal = storeLoadCal(cal);
}

bool touchHasCal() {
  return hasCal;
}

void touchSetCal(const float m[6]) {
  memcpy(cal, m, sizeof(cal));
  hasCal = true;
  storeSaveCal(m);
}

bool touchRawSample(int16_t& rx, int16_t& ry) {
  if (!ts.touched()) return false;
  int32_t sx = 0, sy = 0;
  for (int i = 0; i < 4; i++) {
    TS_Point p = ts.getPoint();
    sx += p.x;
    sy += p.y;
  }
  rx = sx / 4;
  ry = sy / 4;
  return true;
}

static bool wasDown = false;
static int16_t lastX = 0, lastY = 0;
static uint32_t nextPoll = 0;

TouchEvent touchPoll() {
  TouchEvent ev = {T_NONE, 0, 0};
  if (!hasCal) return ev;
  uint32_t now = millis();
  if (now < nextPoll) return ev;
  nextPoll = now + 15;

  int16_t rx, ry;
  bool down = touchRawSample(rx, ry);
  if (down) {
    int16_t x = static_cast<int16_t>(cal[0] * rx + cal[1] * ry + cal[2]);
    int16_t y = static_cast<int16_t>(cal[3] * rx + cal[4] * ry + cal[5]);
    x = constrain(x, 0, SCREEN_W - 1);
    y = constrain(y, 0, SCREEN_H - 1);
    if (!wasDown) {
      ev = {T_DOWN, x, y};
    } else if (abs(x - lastX) > 2 || abs(y - lastY) > 2) {
      ev = {T_MOVE, x, y};
    }
    if (ev.type != T_NONE) {
      lastX = x;
      lastY = y;
    }
    wasDown = true;
  } else if (wasDown) {
    wasDown = false;
    ev = {T_UP, lastX, lastY};
  }
  return ev;
}
