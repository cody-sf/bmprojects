#include "Ui.h"
#include "AppState.h"
#include "Gradient.h"
#include <Arduino.h>

TFT_eSPI tft;

uint16_t rgbTo565(uint8_t r, uint8_t g, uint8_t b) {
  return tft.color565(r, g, b);
}

void hsvToRgb(float h, float s, float v, uint8_t& r, uint8_t& g, uint8_t& b) {
  float c = v * s;
  float hp = h / 60.0f;
  float x = c * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));
  float r1 = 0, g1 = 0, b1 = 0;
  if (hp < 1)      { r1 = c; g1 = x; }
  else if (hp < 2) { r1 = x; g1 = c; }
  else if (hp < 3) { g1 = c; b1 = x; }
  else if (hp < 4) { g1 = x; b1 = c; }
  else if (hp < 5) { r1 = x; b1 = c; }
  else             { r1 = c; b1 = x; }
  float m = v - c;
  r = static_cast<uint8_t>((r1 + m) * 255.0f + 0.5f);
  g = static_cast<uint8_t>((g1 + m) * 255.0f + 0.5f);
  b = static_cast<uint8_t>((b1 + m) * 255.0f + 0.5f);
}

void uiBegin() {
  tft.init();
  tft.setRotation(1);  // landscape; use 3 if your case mounts the USB on the left
  tft.fillScreen(COL_BG);
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif
}

void uiClearContent(bool modal) {
  int16_t x = modal ? 0 : RAIL_W;
  tft.fillRect(x, CONTENT_Y, SCREEN_W - x, CONTENT_H, COL_BG);
}

void uiHeader(const char* title, bool withRail) {
  int16_t x0 = withRail ? RAIL_W : 0;
  tft.fillRect(x0, 0, SCREEN_W - x0, HEADER_H, COL_BG);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(title, x0 + 8, HEADER_H / 2 + 1);
  tft.setTextFont(2);

  char count[8];
  snprintf(count, sizeof(count), "%d/%d", connectedCount(), barCount());
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(connectedCount() ? COL_GREEN : COL_CAPTION, COL_BG);
  tft.drawString(count, SCREEN_W - 8, HEADER_H / 2 + 1);
  tft.setTextDatum(TL_DATUM);
}

static const int16_t RAIL_TAB_Y0 = 6, RAIL_TAB_H = 42, RAIL_TAB_STEP = 46;
static const int16_t RAIL_GEAR_Y = 190;

void uiTabRail(int active) {
  tft.fillRect(0, 0, RAIL_W, SCREEN_H, COL_CARD);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  for (int i = 0; i < 4; i++) {
    int16_t y = RAIL_TAB_Y0 + i * RAIL_TAB_STEP;
    if (i == active) {
      tft.fillRect(0, y, 3, RAIL_TAB_H, COL_ACCENT);
    }
    tft.setTextColor(i == active ? COL_ACCENT2 : COL_CAPTION, COL_CARD);
    tft.drawString(TAB_LABELS[i], RAIL_W / 2 + 1, y + RAIL_TAB_H / 2 + 1);
  }
  tft.setTextDatum(TL_DATUM);

  // Gear at the foot of the rail: circle, four teeth, hollow centre.
  int16_t cx = RAIL_W / 2, cy = 216;
  uint16_t col = active == 4 ? COL_ACCENT2 : COL_CAPTION;
  if (active == 4) tft.fillRect(0, RAIL_GEAR_Y, 3, SCREEN_H - RAIL_GEAR_Y, COL_ACCENT);
  tft.fillCircle(cx, cy, 8, col);
  tft.fillRect(cx - 2, cy - 12, 4, 5, col);
  tft.fillRect(cx - 2, cy + 7, 4, 5, col);
  tft.fillRect(cx - 12, cy - 2, 5, 4, col);
  tft.fillRect(cx + 7, cy - 2, 5, 4, col);
  tft.fillCircle(cx, cy, 4, COL_CARD);
}

int uiTabHit(int16_t x, int16_t y) {
  if (x >= RAIL_W) return -1;
  if (y >= RAIL_GEAR_Y) return 4;
  int idx = (y - RAIL_TAB_Y0) / RAIL_TAB_STEP;
  return idx < 0 ? 0 : (idx > 3 ? 3 : idx);
}

void uiChip(const Rect& r, const char* label, bool selected, uint16_t bg, uint16_t fg) {
  tft.fillRoundRect(r.x, r.y, r.w, r.h, r.h / 2 > 10 ? 10 : r.h / 2, bg);
  if (selected) {
    tft.drawRoundRect(r.x, r.y, r.w, r.h, r.h / 2 > 10 ? 10 : r.h / 2, COL_ACCENT);
    tft.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2,
                      (r.h - 2) / 2 > 9 ? 9 : (r.h - 2) / 2, COL_ACCENT);
  }
  tft.setTextFont(2);
  if (tft.textWidth(label) > r.w - 8) tft.setTextFont(1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fg, bg);
  tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2 + 1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
}

void uiButton(const Rect& r, const char* label, uint16_t bg, uint16_t fg) {
  tft.fillRoundRect(r.x, r.y, r.w, r.h, 8, bg);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(fg, bg);
  tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2 + 1);
  tft.setTextDatum(TL_DATUM);
}

void uiToggle(const Rect& r, bool on, bool enabled) {
  uint16_t track = !enabled ? COL_CHIP : (on ? COL_ACCENT : COL_DOT_OFF);
  tft.fillRoundRect(r.x, r.y, r.w, r.h, r.h / 2, track);
  int16_t knobR = r.h / 2 - 3;
  int16_t cx = on ? r.x + r.w - r.h / 2 : r.x + r.h / 2;
  tft.fillCircle(cx, r.y + r.h / 2, knobR, enabled ? COL_TEXT : COL_CAPTION);
}

void uiSwatch(const Rect& r, uint16_t color) {
  tft.fillRoundRect(r.x, r.y, r.w, r.h, 6, color);
  tft.drawRoundRect(r.x, r.y, r.w, r.h, 6, COL_BORDER);
}

void uiCaption(int16_t x, int16_t y, const char* text) {
  tft.setTextFont(2);
  tft.setTextColor(COL_CAPTION, COL_BG);
  tft.drawString(text, x, y);
}

void uiSliderRow(int16_t x, int16_t y, int16_t w, const char* label, int value) {
  tft.fillRect(x - 8, y, w + 16, 34, COL_CARD);
  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT, COL_CARD);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(label, x, y);
  char pct[8];
  snprintf(pct, sizeof(pct), "%d%%", value);
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_ACCENT2, COL_CARD);
  tft.drawString(pct, x + w, y);
  tft.setTextDatum(TL_DATUM);

  int16_t ty = y + 22;
  tft.fillRoundRect(x, ty, w, 6, 3, COL_CHIP);
  int16_t fill = (w * value) / 100;
  if (fill > 0) tft.fillRoundRect(x, ty, fill, 6, 3, COL_ACCENT);
  tft.fillCircle(x + fill, ty + 3, 8, COL_TEXT);
}

Rect uiSliderHit(int16_t x, int16_t y, int16_t w) {
  return {static_cast<int16_t>(x - 10), static_cast<int16_t>(y + 12),
          static_cast<int16_t>(w + 20), 26};
}

int uiSliderValue(int16_t x, int16_t w, int16_t touchX) {
  int v = ((touchX - x) * 100) / w;
  return v < 0 ? 0 : (v > 100 ? 100 : v);
}

/** Round the corners of a just-filled rect back to the background colour. */
static void maskRoundCorners(const Rect& r, int16_t rad) {
  for (int16_t dy = 0; dy < rad; dy++) {
    for (int16_t dx = 0; dx < rad; dx++) {
      int32_t cx = rad - 1 - dx, cy = rad - 1 - dy;
      if (cx * cx + cy * cy > (int32_t)rad * rad) {
        tft.drawPixel(r.x + dx, r.y + dy, COL_BG);
        tft.drawPixel(r.x + r.w - 1 - dx, r.y + dy, COL_BG);
        tft.drawPixel(r.x + dx, r.y + r.h - 1 - dy, COL_BG);
        tft.drawPixel(r.x + r.w - 1 - dx, r.y + r.h - 1 - dy, COL_BG);
      }
    }
  }
}

void uiPaletteCard(const Rect& r, const PaletteDef& p, bool selected) {
  for (int16_t col = 0; col < r.w; col++) {
    float t = r.w < 2 ? 0.0f : static_cast<float>(col) / (r.w - 1);
    RGB8 c = paletteColorAt(p, t);
    tft.drawFastVLine(r.x + col, r.y, r.h, rgbTo565(c.r, c.g, c.b));
  }
  maskRoundCorners(r, 8);

  // Name, shadowed so it survives any gradient (the app's textShadow).
  tft.setTextFont(2);
  if (tft.textWidth(p.name) > r.w - 8) tft.setTextFont(1);
  tft.setTextDatum(BC_DATUM);
  int16_t cx = r.x + r.w / 2, by = r.y + r.h - 4;
  tft.setTextColor(rgb565(0x000000));
  tft.drawString(p.name, cx + 1, by + 1);
  tft.setTextColor(COL_TEXT);
  tft.drawString(p.name, cx, by);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);

  if (selected) {
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 8, COL_ACCENT);
    tft.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 7, COL_ACCENT);
  }
}
