#include "Actions.h"
#include "AppState.h"
#include "BmCodec.h"
#include "BmLink.h"
#include "Catalog.h"

static void sendBool(uint8_t code, bool v) {
  uint8_t cmd[BM_WRITE_MAX];
  bmQueueWrite(cmd, bmEncodeBool(cmd, code, v));
}

static void sendInt(uint8_t code, int32_t v) {
  uint8_t cmd[BM_WRITE_MAX];
  bmQueueWrite(cmd, bmEncodeInt(cmd, code, v));
}

static void sendString(uint8_t code, const char* s) {
  uint8_t cmd[BM_WRITE_MAX];
  bmQueueWrite(cmd, bmEncodeString(cmd, code, s));
}

void actPower(bool on) {
  app.power = on;
  sendBool(BM_CMD_POWER, on);
}

void actBrightness(uint8_t pct) {
  if (pct < 1) pct = 1;
  if (pct > app.maxBrightness) pct = app.maxBrightness;
  app.brightness = pct;
  sendInt(BM_CMD_BRIGHTNESS, pct);
}

void actSpeed(uint8_t pct) {
  if (pct < 1) pct = 1;
  if (pct > 100) pct = 100;
  app.speedPct = pct;
  sendInt(BM_CMD_SPEED, speedPctToRaw(pct));
}

void actDirection(bool reversed) {
  app.reversed = reversed;
  sendBool(BM_CMD_DIRECTION, reversed);
}

void actPalette(int16_t sel) {
  if (sel >= CUSTOM_SEL_BASE) {
    int slot = sel - CUSTOM_SEL_BASE;
    if (slot >= CUSTOM_PALETTE_COUNT || !app.customs[slot].used) return;
    char id[8] = {'c', 'u', 's', 't', 'o', 'm', (char)('1' + slot), 0};
    app.paletteSel = sel;
    sendString(BM_CMD_PALETTE, id);
    return;
  }
  if (sel < 0 || sel >= PALETTE_COUNT) return;
  app.paletteSel = sel;
  sendString(BM_CMD_PALETTE, PALETTES[sel].id);
}

void actEffect(int idx) {
  if (idx < 0 || idx >= EFFECT_COUNT) return;
  app.effectIdx = idx;
  sendString(BM_CMD_EFFECT, EFFECTS[idx].id);
}

void actEffectById(const char* id) {
  for (int i = 0; i < EFFECT_COUNT; i++) {
    if (strcmp(EFFECTS[i].id, id) == 0) {
      actEffect(i);
      return;
    }
  }
}

void actParam(uint8_t code, int value) {
  int slot = paramSlot(code);
  if (slot >= 0) app.params[slot] = value;
  sendInt(code, value);
}

void actColor(uint8_t r, uint8_t g, uint8_t b) {
  app.colR = r;
  app.colG = g;
  app.colB = b;
  uint8_t cmd[BM_WRITE_MAX];
  bmQueueWrite(cmd, bmEncodeColor(cmd, r, g, b));
}

void actIdentify() {
  uint8_t cmd[BM_WRITE_MAX];
  bmQueueWrite(cmd, bmEncodeBare(cmd, BM_CMD_IDENTIFY));
}

void actSync(bool on) {
  app.syncEnabled = on;
  sendBool(BM_CMD_SET_SYNC_ENABLED, on);
}

void actSaveAsDefaults() {
  sendBool(BM_CMD_SAVE_AS_DEFAULTS, true);
}

void actMarqueeText(const char* text) {
  strlcpy(app.marquee, text, sizeof(app.marquee));
  sendString(BM_CMD_SET_MARQUEE_TEXT, text);
}

void actTextStyle(bool bold) {
  app.boldText = bold;
  sendBool(BM_CMD_SET_TEXT_STYLE, bold);
}

void actTextFill(uint8_t fill) {
  if (fill > 3) fill = 0;
  app.textFill = fill;
  uint8_t cmd[BM_WRITE_MAX];
  cmd[0] = BM_CMD_SET_TEXT_FILL;
  cmd[1] = fill;
  bmQueueWrite(cmd, 2);
}

void actMatrixDisplay(uint8_t mode) {
  if (mode > 4) mode = 0;
  app.matrixDisplay = mode;
  uint8_t cmd[BM_WRITE_MAX];
  cmd[0] = BM_CMD_SET_MATRIX_DISPLAY;
  cmd[1] = mode;
  bmQueueWrite(cmd, 2);
}

void actMatrixTest() {
  uint8_t cmd[BM_WRITE_MAX];
  bmQueueWrite(cmd, bmEncodeBare(cmd, BM_CMD_MATRIX_TEST));
}
