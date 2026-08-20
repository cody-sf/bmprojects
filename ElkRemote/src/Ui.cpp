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
  tft.setRotation(0);  // portrait, phone-shaped; use 2 if your box mounts it flipped
  tft.fillScreen(COL_BG);
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif
}

void uiClearContent(bool fullHeight) {
  tft.fillRect(0, CONTENT_Y, SCREEN_W, fullHeight ? SCREEN_H - CONTENT_Y : CONTENT_H, COL_BG);
}

void uiHeader(const char* title, bool showGear) {
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, COL_BG);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(title, PAD, HEADER_H / 2 + 1);
  tft.setTextFont(2);

  char count[8];
  snprintf(count, sizeof(count), "%d/%d", connectedCount(), barCount());
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(connectedCount() ? COL_GREEN : COL_CAPTION, COL_BG);
  tft.drawString(count, showGear ? SCREEN_W - 34 : SCREEN_W - PAD, HEADER_H / 2 + 1);

  if (showGear) {
    // A small gear: circle, four teeth, hollow centre.
    int16_t cx = SCREEN_W - 18, cy = HEADER_H / 2;
    tft.fillCircle(cx, cy, 7, COL_CAPTION);
    tft.fillRect(cx - 2, cy - 10, 4, 4, COL_CAPTION);
    tft.fillRect(cx - 2, cy + 6, 4, 4, COL_CAPTION);
    tft.fillRect(cx - 10, cy - 2, 4, 4, COL_CAPTION);
    tft.fillRect(cx + 6, cy - 2, 4, 4, COL_CAPTION);
    tft.fillCircle(cx, cy, 3, COL_BG);
  }
  tft.setTextDatum(TL_DATUM);
}

Rect uiGearRect() {
  return {SCREEN_W - 32, 0, 32, HEADER_H};
}

void uiTabBar(int active) {
  int16_t y = SCREEN_H - TABBAR_H;
  tft.fillRect(0, y, SCREEN_W, TABBAR_H, COL_CARD);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  for (int i = 0; i < 4; i++) {
    int16_t x = i * 60;
    if (i == active) {
      tft.fillRect(x + 8, y, 44, 3, COL_ACCENT);
    }
    tft.setTextColor(i == active ? COL_ACCENT2 : COL_CAPTION, COL_CARD);
    tft.drawString(TAB_LABELS[i], x + 30, y + TABBAR_H / 2 + 1);
  }
  tft.setTextDatum(TL_DATUM);
}

int uiTabHit(int16_t x, int16_t y) {
  if (y < SCREEN_H - TABBAR_H) return -1;
  return x / 60;
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
  tft.setTextDatum(BC_DATUM);
  int16_t cx = r.x + r.w / 2, by = r.y + r.h - 4;
  tft.setTextColor(rgb565(0x000000));
  tft.drawString(p.name, cx + 1, by + 1);
  tft.setTextColor(COL_TEXT);
  tft.drawString(p.name, cx, by);
  tft.setTextDatum(TL_DATUM);

  if (selected) {
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 8, COL_ACCENT);
    tft.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 7, COL_ACCENT);
  }
}
