#include "Screens.h"
#include "Actions.h"
#include "AppState.h"
#include "BmLink.h"
#include "Catalog.h"
#include "Store.h"
#include "Theme.h"
#include "Ui.h"
#include <Arduino.h>

static Screen cur = SCR_HOME;

// ---------------------------------------------------------------- layout
// Main-screen content sits right of the rail: x 64..320, y 28..240,
// with an 8px inset -> usable 72..312.

// Home
static const Rect HOME_CARD      = {72, 36, 240, 168};
static const Rect HOME_TOGGLE    = {256, 44, 44, 24};
static const int16_t HOME_SLIDER_X = 84, HOME_SLIDER_W = 216;
static const int16_t HOME_BRIGHT_Y = 76, HOME_SPEED_Y = 118;
static const Rect HOME_REVERSE   = {148, 162, 44, 24};
static const Rect HOME_FIND      = {204, 162, 52, 24};
static const Rect HOME_SWATCH    = {268, 158, 32, 32};

// Palettes / Modes grids
static const int16_t GRID_Y = 50;
static const int16_t PAL_CARD_W = 116, PAL_CARD_H = 48, PAL_STEP = 54;
static const int16_t PAL_PER_PAGE = 6;
static const int16_t PAL_PAGER_Y = 212;
static const int16_t MODE_CHIP_H = 26, MODE_ROW_STEP = 30, MODES_PER_PAGE = 10;
static const int16_t MODE_PAGER_Y = 202;
static const int16_t GRID_COL_X[2] = {72, 194};
static const Rect MODE_MATRIX = {248, 26, 62, 22};

// Matrix display (modal, full width)
static const Rect MTX_FIELD  = {12, 44, 296, 26};
static const Rect MTX_SHOW[4] = {  // Text, Words, Art, Anim
    {12, 80, 71, 30}, {87, 80, 71, 30}, {162, 80, 71, 30}, {237, 80, 71, 30}};
static const char* const MTX_SHOW_LABEL[4] = {"Text", "Words", "Art", "Anim"};
static const Rect MTX_FILL[4] = {  // glyph fill chips
    {52, 118, 60, 24}, {116, 118, 60, 24}, {180, 118, 60, 24}, {244, 118, 60, 24}};
static const char* const MTX_FILL_LABEL[4] = {"Grad", "Fire", "Rain", "Plasma"};
static const Rect MTX_BOLD = {264, 150, 44, 24};
static const Rect MTX_TEST = {12, 186, 150, 26};
static const Rect MTX_BACK = {228, 186, 80, 26};

// Tweaks
static const Rect TWEAKS_CARD = {72, 52, 240, 152};
static const int16_t TWEAK_SLIDER_X = 84, TWEAK_SLIDER_W = 216;
static const int16_t TWEAK_ROW_Y[3] = {62, 108, 154};

// Settings
static const Rect SET_RECAL   = {72, 36, 236, 28};
static const Rect SET_CHOOSE  = {72, 70, 236, 28};
static const Rect SET_SAVE    = {72, 104, 236, 28};
static const Rect SET_FORGET  = {72, 138, 236, 28};
static const Rect SET_SYNC    = {264, 172, 44, 24};

// Color picker (modal, full width)
static const Rect COLOR_FIELD   = {12, 36, 196, 160};
static const Rect COLOR_PREVIEW = {224, 36, 84, 44};
static const Rect COLOR_WHITE   = {224, 96, 84, 36};
static const Rect COLOR_DONE    = {224, 192, 84, 36};

// Choose Light (modal)
static const int16_t DEV_ROW_Y = 44, DEV_ROW_H = 28;
static const Rect DEV_CANCEL = {12, 206, 80, 26};

// ---------------------------------------------------------------- state

static int palettesPage = 0;
static int modesPage = 0;

// 0 none, 1 brightness, 2 speed, 3..5 tweak param slot 0..2
static int8_t dragSlider = 0;
static int dragValue = 0;
static uint32_t lastSliderSend = 0;

static uint8_t pickR = 255, pickG = 136, pickB = 0;
static uint32_t lastColorSend = 0;
static bool colorPending = false;

static bool forgetArmed = false;
static uint32_t forgetArmedAt = 0;

static char textBuf[MARQUEE_MAX + 1];
static bool textCaps = true;
static uint32_t matrixTestSentAt = 0;

static Screen calReturn = SCR_HOME;
static int calStep = 0;
static int calPhase = 0;  // 0 waiting for touch, 1 waiting for release
static uint32_t calPhaseAt = 0;
static int32_t calRaw[3][2];
static float calM[6];
static const int16_t CAL_PTS[4][2] = {{25, 25}, {295, 25}, {295, 215}, {25, 215}};

static uint32_t lastScanCheck = 0;
static bool lastScanState = false;
static bool lastConnState = false;

static void enter(Screen s);

// ---------------------------------------------------------------- shared

static const char* headerTitle() {
  switch (cur) {
    case SCR_HOME:     return "Controls";
    case SCR_PALETTES: return "Palettes";
    case SCR_MODES:    return "Modes";
    case SCR_TWEAKS:   return "Tweaks";
    case SCR_SETTINGS: return "Setup";
    case SCR_COLOR:    return "Color";
    case SCR_MATRIX:   return "Matrix";
    case SCR_TEXT:     return "Marquee Text";
    case SCR_DEVICES:  return "Choose Light";
    default:           return "";
  }
}

static bool isMainScreen(Screen s) {
  return s == SCR_HOME || s == SCR_PALETTES || s == SCR_MODES || s == SCR_TWEAKS ||
         s == SCR_SETTINGS;
}

static int activeTab() {
  switch (cur) {
    case SCR_HOME:     return 0;
    case SCR_PALETTES: return 1;
    case SCR_MODES:    return 2;
    case SCR_TWEAKS:   return 3;
    case SCR_SETTINGS: return 4;
    default:           return -1;
  }
}

static Rect pagerPrevRect(int16_t y) { return {72, y, 40, 24}; }
static Rect pagerNextRect(int16_t y) { return {270, y, 40, 24}; }

static void drawPager(int16_t y, int page, int pages) {
  uiChip(pagerPrevRect(y), "<", false, page > 0 ? COL_BTN : COL_CARD,
         page > 0 ? COL_ACCENT2 : COL_CAPTION);
  uiChip(pagerNextRect(y), ">", false, page < pages - 1 ? COL_BTN : COL_CARD,
         page < pages - 1 ? COL_ACCENT2 : COL_CAPTION);
  tft.fillRect(116, y, 150, 24, COL_BG);
  char label[16];
  snprintf(label, sizeof(label), "%d / %d", page + 1, pages);
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_CAPTION, COL_BG);
  tft.drawString(label, 192, y + 13);
  tft.setTextDatum(TL_DATUM);
}

// ---------------------------------------------------------------- home

static void drawHomeCaption() {
  tft.fillRect(RAIL_W, 208, SCREEN_W - RAIL_W, 20, COL_BG);
  tft.setTextDatum(TC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(COL_CAPTION, COL_BG);
  const char* text;
  char buf[48];
  if (!app.haveTarget) {
    text = "No light chosen - open Setup";
  } else if (app.connected) {
    text = "";
  } else {
    snprintf(buf, sizeof(buf), "Searching for %s...",
             app.deviceName[0] ? app.deviceName : "your light");
    text = buf;
  }
  if (text[0]) tft.drawString(text, RAIL_W + (SCREEN_W - RAIL_W) / 2, 210);
  tft.setTextDatum(TL_DATUM);
}

static void drawHome() {
  uiClearContent(false);
  tft.fillRoundRect(HOME_CARD.x, HOME_CARD.y, HOME_CARD.w, HOME_CARD.h, 12, COL_CARD);

  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setTextColor(COL_TEXT, COL_CARD);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("Power", 84, HOME_TOGGLE.y + 12);
  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);
  uiToggle(HOME_TOGGLE, app.power, app.connected);

  uiSliderRow(HOME_SLIDER_X, HOME_BRIGHT_Y, HOME_SLIDER_W, "Brightness", app.brightness);
  uiSliderRow(HOME_SLIDER_X, HOME_SPEED_Y, HOME_SLIDER_W, "Speed", app.speedPct);

  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT, COL_CARD);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("Reverse", 84, HOME_REVERSE.y + HOME_REVERSE.h / 2);
  tft.setTextDatum(TL_DATUM);
  uiToggle(HOME_REVERSE, app.reversed, app.connected);
  uiChip(HOME_FIND, "Find", false, COL_BTN, app.connected ? COL_ACCENT2 : COL_CAPTION);
  uiSwatch(HOME_SWATCH, rgbTo565(app.colR, app.colG, app.colB));

  drawHomeCaption();
}

static void homeTouch(const TouchEvent& ev) {
  Rect brightHit = uiSliderHit(HOME_SLIDER_X, HOME_BRIGHT_Y, HOME_SLIDER_W);
  Rect speedHit = uiSliderHit(HOME_SLIDER_X, HOME_SPEED_Y, HOME_SLIDER_W);

  if (ev.type == T_DOWN) {
    if (HOME_TOGGLE.contains(ev.x, ev.y)) {
      if (app.connected) {
        actPower(!app.power);
        uiToggle(HOME_TOGGLE, app.power, true);
      }
      return;
    }
    if (HOME_REVERSE.contains(ev.x, ev.y)) {
      if (app.connected) {
        actDirection(!app.reversed);
        uiToggle(HOME_REVERSE, app.reversed, true);
      }
      return;
    }
    if (HOME_FIND.contains(ev.x, ev.y)) {
      actIdentify();
      return;
    }
    if (HOME_SWATCH.contains(ev.x, ev.y)) {
      pickR = app.colR; pickG = app.colG; pickB = app.colB;
      enter(SCR_COLOR);
      return;
    }
    if (brightHit.contains(ev.x, ev.y)) dragSlider = 1;
    else if (speedHit.contains(ev.x, ev.y)) dragSlider = 2;
    else return;
  }

  if (dragSlider && (ev.type == T_DOWN || ev.type == T_MOVE || ev.type == T_UP)) {
    dragValue = uiSliderValue(HOME_SLIDER_X, HOME_SLIDER_W, ev.x);
    if (dragValue < 1) dragValue = 1;
    if (dragSlider == 1 && dragValue > app.maxBrightness) dragValue = app.maxBrightness;
    int16_t rowY = dragSlider == 1 ? HOME_BRIGHT_Y : HOME_SPEED_Y;
    uiSliderRow(HOME_SLIDER_X, rowY, HOME_SLIDER_W,
                dragSlider == 1 ? "Brightness" : "Speed", dragValue);
    uint32_t now = millis();
    if (ev.type == T_UP || now - lastSliderSend > 150) {
      lastSliderSend = now;
      if (dragSlider == 1) actBrightness(dragValue);
      else actSpeed(dragValue);
    }
    if (ev.type == T_UP) dragSlider = 0;
  }
}

// ---------------------------------------------------------------- palettes
// One combined list: the generated built-ins, then whatever custom slots the
// device has reported. Rebuilt each draw because cpal chunks arrive whenever
// the phone rewrites a slot.

struct PalRow {
  int16_t sel;  // value for actPalette / app.paletteSel
  const uint8_t (*colors)[3];
  uint8_t count;
  const char* name;
};
static PalRow palRows[PALETTE_COUNT + CUSTOM_PALETTE_COUNT];
static int palRowCount = 0;

static void rebuildPalRows() {
  palRowCount = 0;
  for (int i = 0; i < PALETTE_COUNT; i++) {
    palRows[palRowCount++] = {(int16_t)i, PALETTES[i].colors, PALETTES[i].count,
                             PALETTES[i].name};
  }
  for (int slot = 0; slot < CUSTOM_PALETTE_COUNT; slot++) {
    const CustomPalette& cp = app.customs[slot];
    if (!cp.used) continue;
    palRows[palRowCount++] = {(int16_t)(CUSTOM_SEL_BASE + slot), cp.colors,
                             CUSTOM_PALETTE_ENTRIES, cp.name};
  }
}

static int palettePages() {
  return (palRowCount + PAL_PER_PAGE - 1) / PAL_PER_PAGE;
}

static Rect paletteCardRect(int slot) {
  return {GRID_COL_X[slot % 2],
          static_cast<int16_t>(GRID_Y + (slot / 2) * PAL_STEP),
          PAL_CARD_W, PAL_CARD_H};
}

static void drawPaletteCardRow(int idx) {
  const PalRow& row = palRows[idx];
  uiPaletteCard(paletteCardRect(idx % PAL_PER_PAGE), row.colors, row.count, row.name,
                app.paletteSel == row.sel);
}

static void drawPalettes() {
  rebuildPalRows();
  if (palettesPage >= palettePages()) palettesPage = palettePages() - 1;
  uiClearContent(false);
  uiCaption(72, 30, "Tap to play on the light");
  for (int slot = 0; slot < PAL_PER_PAGE; slot++) {
    int idx = palettesPage * PAL_PER_PAGE + slot;
    if (idx >= palRowCount) break;
    drawPaletteCardRow(idx);
  }
  drawPager(PAL_PAGER_Y, palettesPage, palettePages());
}

static void palettesTouch(const TouchEvent& ev) {
  if (ev.type != T_DOWN) return;
  if (pagerPrevRect(PAL_PAGER_Y).contains(ev.x, ev.y) && palettesPage > 0) {
    palettesPage--;
    drawPalettes();
    return;
  }
  if (pagerNextRect(PAL_PAGER_Y).contains(ev.x, ev.y) && palettesPage < palettePages() - 1) {
    palettesPage++;
    drawPalettes();
    return;
  }
  for (int slot = 0; slot < PAL_PER_PAGE; slot++) {
    int idx = palettesPage * PAL_PER_PAGE + slot;
    if (idx >= palRowCount) break;
    Rect r = paletteCardRect(slot);
    if (r.contains(ev.x, ev.y)) {
      int16_t prev = app.paletteSel;
      actPalette(palRows[idx].sel);
      if (prev != app.paletteSel) {
        // Un-ring the old card if it is on this page, ring the new one.
        for (int s = 0; s < PAL_PER_PAGE; s++) {
          int i = palettesPage * PAL_PER_PAGE + s;
          if (i >= palRowCount) break;
          if (palRows[i].sel == prev) drawPaletteCardRow(i);
        }
        drawPaletteCardRow(idx);
      }
      return;
    }
  }
}

// ---------------------------------------------------------------- modes
// The catalog filtered to what this device can actually show: GPS effects
// need a fix source, the display effects need a matrix grid. Unmapped rigs
// still render substitutes for chevron/panel effects, so those always stay.

static int16_t visibleModes[EFFECT_COUNT];
static int visibleModeCount = 0;

static void rebuildVisibleModes() {
  visibleModeCount = 0;
  for (int i = 0; i < EFFECT_COUNT; i++) {
    if ((EFFECTS[i].flags & FX_NEEDS_GPS) && !app.gpsAvailable) continue;
    if ((EFFECTS[i].flags & FX_NEEDS_MATRIX) && app.mtxW <= 0) continue;
    visibleModes[visibleModeCount++] = i;
  }
}

static int modePages() {
  return (visibleModeCount + MODES_PER_PAGE - 1) / MODES_PER_PAGE;
}

static Rect modeChipRect(int slot) {
  return {GRID_COL_X[slot % 2],
          static_cast<int16_t>(GRID_Y + (slot / 2) * MODE_ROW_STEP),
          PAL_CARD_W, MODE_CHIP_H};
}

static void drawModes() {
  rebuildVisibleModes();
  if (modesPage >= modePages()) modesPage = modePages() - 1;
  uiClearContent(false);
  uiCaption(72, 30, "Tweaks apply per mode");
  if (app.mtxW > 0) uiChip(MODE_MATRIX, "Matrix", false, COL_BTN, COL_ACCENT2);
  for (int slot = 0; slot < MODES_PER_PAGE; slot++) {
    int idx = modesPage * MODES_PER_PAGE + slot;
    if (idx >= visibleModeCount) break;
    int fx = visibleModes[idx];
    uiChip(modeChipRect(slot), EFFECTS[fx].name, app.effectIdx == fx);
  }
  drawPager(MODE_PAGER_Y, modesPage, modePages());
}

static void modesTouch(const TouchEvent& ev) {
  if (ev.type != T_DOWN) return;
  if (app.mtxW > 0 && MODE_MATRIX.contains(ev.x, ev.y)) {
    enter(SCR_MATRIX);
    return;
  }
  if (pagerPrevRect(MODE_PAGER_Y).contains(ev.x, ev.y) && modesPage > 0) {
    modesPage--;
    drawModes();
    return;
  }
  if (pagerNextRect(MODE_PAGER_Y).contains(ev.x, ev.y) && modesPage < modePages() - 1) {
    modesPage++;
    drawModes();
    return;
  }
  for (int slot = 0; slot < MODES_PER_PAGE; slot++) {
    int idx = modesPage * MODES_PER_PAGE + slot;
    if (idx >= visibleModeCount) break;
    Rect r = modeChipRect(slot);
    if (r.contains(ev.x, ev.y)) {
      int fx = visibleModes[idx];
      int prev = app.effectIdx;
      actEffect(fx);
      for (int s = 0; s < MODES_PER_PAGE; s++) {
        int i = modesPage * MODES_PER_PAGE + s;
        if (i >= visibleModeCount) break;
        if (visibleModes[i] == prev) {
          uiChip(modeChipRect(s), EFFECTS[prev].name, false);
        }
      }
      uiChip(r, EFFECTS[fx].name, true);
      return;
    }
  }
}

// ---------------------------------------------------------------- tweaks
// The current mode's extra sliders (EFFECT_PARAMETERS in the app), fed by the
// device's effectParams status chunk so they open on the real values.

static const BmParamDef* tweakParam(int row) {
  if (app.effectIdx < 0 || app.effectIdx >= EFFECT_COUNT) return nullptr;
  int8_t idx = EFFECTS[app.effectIdx].params[row];
  return idx < 0 ? nullptr : &PARAM_DEFS[idx];
}

static int tweakValue(const BmParamDef* def) {
  int slot = paramSlot(def->code);
  int v = slot >= 0 ? app.params[slot] : def->defV;
  if (v < def->minV || v > def->maxV) v = def->defV;
  return v;
}

static void drawTweakRow(int row) {
  const BmParamDef* def = tweakParam(row);
  if (!def) return;
  uiSliderRowRange(TWEAK_SLIDER_X, TWEAK_ROW_Y[row], TWEAK_SLIDER_W, def->name,
                   tweakValue(def), def->minV, def->maxV);
}

static void drawTweaks() {
  uiClearContent(false);
  const char* fxName =
      (app.effectIdx >= 0 && app.effectIdx < EFFECT_COUNT) ? EFFECTS[app.effectIdx].name : "?";
  char caption[48];
  snprintf(caption, sizeof(caption), "%s", fxName);
  uiCaption(72, 30, caption);

  if (!tweakParam(0)) {
    tft.setTextFont(2);
    tft.setTextColor(COL_CAPTION, COL_BG);
    tft.drawString("This mode has no extra controls.", 72, 70);
    tft.drawString("Brightness and speed live on Home.", 72, 88);
    return;
  }
  tft.fillRoundRect(TWEAKS_CARD.x, TWEAKS_CARD.y, TWEAKS_CARD.w, TWEAKS_CARD.h, 12, COL_CARD);
  for (int row = 0; row < 3; row++) drawTweakRow(row);
}

static void tweaksTouch(const TouchEvent& ev) {
  if (ev.type == T_DOWN) {
    for (int row = 0; row < 3; row++) {
      if (!tweakParam(row)) break;
      if (uiSliderHit(TWEAK_SLIDER_X, TWEAK_ROW_Y[row], TWEAK_SLIDER_W).contains(ev.x, ev.y)) {
        dragSlider = 3 + row;
        break;
      }
    }
    if (dragSlider < 3) return;
  }

  if (dragSlider >= 3 && (ev.type == T_DOWN || ev.type == T_MOVE || ev.type == T_UP)) {
    int row = dragSlider - 3;
    const BmParamDef* def = tweakParam(row);
    if (!def) {
      dragSlider = 0;
      return;
    }
    dragValue = uiSliderValueRange(TWEAK_SLIDER_X, TWEAK_SLIDER_W, ev.x, def->minV, def->maxV);
    uiSliderRowRange(TWEAK_SLIDER_X, TWEAK_ROW_Y[row], TWEAK_SLIDER_W, def->name, dragValue,
                     def->minV, def->maxV);
    uint32_t now = millis();
    if (ev.type == T_UP || now - lastSliderSend > 150) {
      lastSliderSend = now;
      actParam(def->code, dragValue);
    }
    if (ev.type == T_UP) dragSlider = 0;
  }
}

// ---------------------------------------------------------------- matrix
// The app's Matrix Display card: the marquee text and its style live on the
// device and are edited here; pixel art and animation frames are uploaded by
// the phone, so their buttons only *show* what is already stored. The Show
// chips drive the display overlay (0x89) - independent of the mode list, so
// the rig keeps playing its effect while the matrix shows content. Tapping
// the lit chip again turns the display off and hands the panel back.

static void drawMarqueeField() {
  tft.fillRoundRect(MTX_FIELD.x, MTX_FIELD.y, MTX_FIELD.w, MTX_FIELD.h, 6, COL_CHIP);
  tft.setTextFont(2);
  // Long text keeps its tail visible - that is where editing happens.
  const char* shown = app.marquee[0] ? app.marquee : "(tap to set text)";
  while (*shown && tft.textWidth(shown) > MTX_FIELD.w - 16) shown++;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(app.marquee[0] ? COL_TEXT : COL_CAPTION, COL_CHIP);
  tft.drawString(shown, MTX_FIELD.x + 8, MTX_FIELD.y + MTX_FIELD.h / 2 + 1);
  tft.setTextDatum(TL_DATUM);
}

static bool matrixShowEnabled(int i) {
  if (i == 2) return app.hasBitmap;
  if (i == 3) return app.animFrames > 0;
  return true;
}

static void drawMatrixShowRow() {
  // Chip order text/words/bitmap/anim = display modes 1..4.
  for (int i = 0; i < 4; i++) {
    bool enabled = matrixShowEnabled(i);
    uiChip(MTX_SHOW[i], MTX_SHOW_LABEL[i], app.matrixDisplay == i + 1,
           enabled ? COL_BTN : COL_CARD, enabled ? COL_TEXT : COL_CAPTION);
  }
}

static void drawMatrixFillRow() {
  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("Fill", 12, MTX_FILL[0].y + MTX_FILL[0].h / 2 + 1);
  tft.setTextDatum(TL_DATUM);
  for (int i = 0; i < 4; i++) {
    uiChip(MTX_FILL[i], MTX_FILL_LABEL[i], app.textFill == i);
  }
}

static void drawMatrixTestChip() {
  bool running = matrixTestSentAt && millis() - matrixTestSentAt < 30000;
  uiButton(MTX_TEST, running ? "Wiring test running" : "Wiring Test (30 s)", COL_BTN,
           running ? COL_CAPTION : COL_TEXT);
}

static void drawMatrix() {
  uiClearContent(true);
  uiCaption(12, 28, "Scrolling text - tap to edit");
  drawMarqueeField();
  drawMatrixShowRow();
  drawMatrixFillRow();
  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("Bold text", 12, MTX_BOLD.y + MTX_BOLD.h / 2 + 1);
  tft.setTextDatum(TL_DATUM);
  uiToggle(MTX_BOLD, app.boldText, app.connected);
  drawMatrixTestChip();
  uiButton(MTX_BACK, "Back", COL_ACCENT, COL_TEXT);
  uiCaption(12, 218, "Art & animations upload from the phone app");
}

static void matrixTouch(const TouchEvent& ev) {
  if (ev.type != T_DOWN) return;
  if (MTX_FIELD.contains(ev.x, ev.y)) {
    strlcpy(textBuf, app.marquee, sizeof(textBuf));
    textCaps = textBuf[0] == 0;
    enter(SCR_TEXT);
    return;
  }
  for (int i = 0; i < 4; i++) {
    if (MTX_SHOW[i].contains(ev.x, ev.y)) {
      if (matrixShowEnabled(i)) {
        // Tap to show; tap the lit chip again to hand the panel back.
        actMatrixDisplay(app.matrixDisplay == i + 1 ? 0 : i + 1);
        drawMatrixShowRow();
      }
      return;
    }
    if (MTX_FILL[i].contains(ev.x, ev.y)) {
      actTextFill(i);
      drawMatrixFillRow();
      return;
    }
  }
  if (MTX_BOLD.contains(ev.x, ev.y)) {
    if (app.connected) {
      actTextStyle(!app.boldText);
      uiToggle(MTX_BOLD, app.boldText, true);
    }
    return;
  }
  if (MTX_TEST.contains(ev.x, ev.y)) {
    actMatrixTest();
    matrixTestSentAt = millis();
    drawMatrixTestChip();
    return;
  }
  if (MTX_BACK.contains(ev.x, ev.y)) {
    enter(SCR_MODES);
  }
}

// ---------------------------------------------------------------- marquee keyboard
// Ported from ElkRemote's rename screen, holding MARQUEE_MAX (63) chars. Done
// sends the text (0x80); the device stores it and panel_text picks it up
// mid-scroll.

struct Key {
  Rect r;
  char ch;      // 0 for specials
  int8_t op;    // 0 char, 1 caps, 2 del, 3 space, 4 done, 5 cancel
};
static Key keys[46];
static int keyCount = 0;

static void addKeyRow(const char* row, int16_t y, int16_t x0) {
  for (const char* c = row; *c; c++) {
    keys[keyCount++] = {{x0, y, 29, 26}, *c, 0};
    x0 += 31;
  }
}

static void buildKeys() {
  keyCount = 0;
  addKeyRow("1234567890", 64, 6);
  addKeyRow("qwertyuiop", 94, 6);
  addKeyRow("asdfghjkl", 124, 21);
  keys[keyCount++] = {{6, 154, 40, 26}, 0, 1};     // caps
  addKeyRow("zxcvbnm", 154, 52);
  keys[keyCount++] = {{274, 154, 40, 26}, 0, 2};   // del
  keys[keyCount++] = {{6, 186, 48, 28}, 0, 5};     // cancel
  keys[keyCount++] = {{62, 186, 150, 28}, ' ', 3};
  keys[keyCount++] = {{220, 186, 94, 28}, 0, 4};   // done
}

static void drawTextField() {
  Rect f = {12, 32, 296, 26};
  tft.fillRoundRect(f.x, f.y, f.w, f.h, 6, COL_CHIP);
  char shown[MARQUEE_MAX + 2];
  snprintf(shown, sizeof(shown), "%s_", textBuf);
  tft.setTextFont(2);
  const char* p = shown;
  while (*p && tft.textWidth(p) > f.w - 16) p++;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COL_TEXT, COL_CHIP);
  tft.drawString(p, f.x + 8, f.y + f.h / 2 + 1);
  tft.setTextDatum(TL_DATUM);
}

static void drawKeys() {
  for (int i = 0; i < keyCount; i++) {
    const Key& k = keys[i];
    char label[8] = {0};
    uint16_t bg = COL_CHIP, fg = COL_TEXT;
    switch (k.op) {
      case 0:
        label[0] = textCaps ? toupper(k.ch) : k.ch;
        break;
      case 1: strcpy(label, textCaps ? "AB" : "ab"); bg = COL_BTN; break;
      case 2: strcpy(label, "del"); bg = COL_BTN; break;
      case 3: strcpy(label, "space"); break;
      case 4: strcpy(label, "Done"); bg = COL_ACCENT; break;
      case 5: strcpy(label, "Esc"); bg = COL_BTN; fg = COL_CAPTION; break;
    }
    uiChip(k.r, label, false, bg, fg);
  }
}

static void drawText() {
  uiClearContent(true);
  drawTextField();
  buildKeys();
  drawKeys();
}

static void textTouch(const TouchEvent& ev) {
  if (ev.type != T_DOWN) return;
  for (int i = 0; i < keyCount; i++) {
    const Key& k = keys[i];
    if (!k.r.contains(ev.x, ev.y)) continue;
    size_t len = strlen(textBuf);
    switch (k.op) {
      case 0:
      case 3:
        if (len < MARQUEE_MAX) {
          textBuf[len] = k.op == 3 ? ' ' : (textCaps ? toupper(k.ch) : k.ch);
          textBuf[len + 1] = 0;
          // Phone-style shift: capitalise one letter, then drop back.
          if (k.op == 0 && textCaps) { textCaps = false; drawKeys(); }
          drawTextField();
        }
        return;
      case 1:
        textCaps = !textCaps;
        drawKeys();
        return;
      case 2:
        if (len) {
          textBuf[len - 1] = 0;
          drawTextField();
        }
        return;
      case 4:
        if (strlen(textBuf)) actMarqueeText(textBuf);
        enter(SCR_MATRIX);
        return;
      case 5:
        enter(SCR_MATRIX);
        return;
    }
  }
}

// ---------------------------------------------------------------- settings

static void drawSettings() {
  uiClearContent(false);
  uiButton(SET_RECAL, "Recalibrate Touch", COL_BTN, COL_TEXT);
  uiButton(SET_CHOOSE, "Choose Light", COL_BTN, COL_TEXT);
  uiButton(SET_SAVE, "Save Look As Power-On", COL_BTN,
           app.connected ? COL_TEXT : COL_CAPTION);
  uiButton(SET_FORGET, forgetArmed ? "Tap again to confirm" : "Forget Light",
           forgetArmed ? COL_RED : COL_BTN, forgetArmed ? COL_TEXT : COL_RED);

  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString("Group Sync (ESP-NOW)", 72, SET_SYNC.y + 5);
  uiToggle(SET_SYNC, app.syncEnabled, app.connected);

  char line[48];
  snprintf(line, sizeof(line), "Free heap: %u KB", (unsigned)(ESP.getFreeHeap() / 1024));
  uiCaption(72, 204, line);
  uiCaption(72, 222, "BikeRemote for ESP32 CYD");
}

static void settingsTouch(const TouchEvent& ev) {
  if (ev.type != T_DOWN) return;
  if (SET_RECAL.contains(ev.x, ev.y)) {
    calReturn = SCR_SETTINGS;
    enter(SCR_CAL);
    return;
  }
  if (SET_CHOOSE.contains(ev.x, ev.y)) {
    enter(SCR_DEVICES);
    return;
  }
  if (SET_SAVE.contains(ev.x, ev.y)) {
    if (app.connected) {
      actSaveAsDefaults();
      uiButton(SET_SAVE, "Saved!", COL_BTN, COL_GREEN);
    }
    return;
  }
  if (SET_SYNC.contains(ev.x, ev.y)) {
    if (app.connected) {
      actSync(!app.syncEnabled);
      uiToggle(SET_SYNC, app.syncEnabled, true);
    }
    return;
  }
  if (SET_FORGET.contains(ev.x, ev.y)) {
    if (!forgetArmed) {
      forgetArmed = true;
      forgetArmedAt = millis();
    } else {
      forgetArmed = false;
      bmForgetTarget();
    }
    drawSettings();
  }
}

// ---------------------------------------------------------------- color picker
// Sets the device's effect color (0x19) - what Color Wheel / Color Radial and
// the color-tinted effects render.

static void drawColorPreview() {
  uiSwatch(COLOR_PREVIEW, rgbTo565(pickR, pickG, pickB));
}

static void drawColor() {
  uiClearContent(true);
  // Hue across, fading toward pale at the bottom; 4px blocks keep it quick.
  for (int16_t dy = 0; dy < COLOR_FIELD.h; dy += 4) {
    float s = 1.0f - 0.85f * (static_cast<float>(dy) / COLOR_FIELD.h);
    for (int16_t dx = 0; dx < COLOR_FIELD.w; dx += 4) {
      float h = 360.0f * dx / COLOR_FIELD.w;
      uint8_t r, g, b;
      hsvToRgb(h, s, 1.0f, r, g, b);
      tft.fillRect(COLOR_FIELD.x + dx, COLOR_FIELD.y + dy, 4, 4, rgbTo565(r, g, b));
    }
  }
  tft.drawRoundRect(COLOR_FIELD.x - 1, COLOR_FIELD.y - 1, COLOR_FIELD.w + 2,
                    COLOR_FIELD.h + 2, 4, COL_BORDER);
  drawColorPreview();
  uiButton(COLOR_WHITE, "White", COL_CHIP, COL_TEXT);
  uiButton(COLOR_DONE, "Done", COL_ACCENT, COL_TEXT);
}

static void colorTouch(const TouchEvent& ev) {
  if (COLOR_FIELD.contains(ev.x, ev.y) &&
      (ev.type == T_DOWN || ev.type == T_MOVE)) {
    float h = 360.0f * (ev.x - COLOR_FIELD.x) / COLOR_FIELD.w;
    float s = 1.0f - 0.85f * (static_cast<float>(ev.y - COLOR_FIELD.y) / COLOR_FIELD.h);
    hsvToRgb(h, s, 1.0f, pickR, pickG, pickB);
    drawColorPreview();
    colorPending = true;
    uint32_t now = millis();
    if (now - lastColorSend > 150) {
      lastColorSend = now;
      actColor(pickR, pickG, pickB);
      colorPending = false;
    }
    return;
  }
  if (ev.type == T_UP && colorPending) {
    actColor(pickR, pickG, pickB);
    colorPending = false;
    return;
  }
  if (ev.type != T_DOWN) return;
  if (COLOR_WHITE.contains(ev.x, ev.y)) {
    pickR = pickG = pickB = 255;
    drawColorPreview();
    actColor(pickR, pickG, pickB);
    return;
  }
  if (COLOR_DONE.contains(ev.x, ev.y)) {
    enter(SCR_HOME);
  }
}

// ---------------------------------------------------------------- choose light

static Rect deviceRowRect(int row) {
  return {12, static_cast<int16_t>(DEV_ROW_Y + row * DEV_ROW_H), 296, DEV_ROW_H};
}

static void drawDeviceRows() {
  tft.fillRect(0, DEV_ROW_Y, SCREEN_W, DEV_CANCEL.y - DEV_ROW_Y - 4, COL_BG);
  tft.setTextFont(2);
  int shown = 0;
  for (int i = 0; i < FOUND_MAX && shown < 5; i++) {
    const FoundDevice& f = app.found[i];
    if (!f.used) continue;
    Rect r = deviceRowRect(shown);
    bool isTarget = app.haveTarget && strcasecmp(f.addr, app.targetAddr) == 0;
    tft.fillCircle(r.x + 10, r.y + r.h / 2, 4, isTarget ? COL_GREEN : COL_DOT_OFF);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.drawString(f.name, r.x + 24, r.y + r.h / 2 + 1);
    char meta[24];
    snprintf(meta, sizeof(meta), "%d dB", f.rssi);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(COL_CAPTION, COL_BG);
    tft.drawString(meta, r.x + r.w - 8, r.y + r.h / 2 + 1);
    tft.setTextDatum(TL_DATUM);
    shown++;
  }
  if (!shown) {
    tft.setTextColor(COL_CAPTION, COL_BG);
    tft.drawString("Power the light on - it will", 24, 70);
    tft.drawString("appear here in a few seconds.", 24, 88);
  }
}

static void drawDevices() {
  uiClearContent(true);
  uiCaption(12, 30, "Tap a light to make it this remote's");
  drawDeviceRows();
  uiButton(DEV_CANCEL, "Back", COL_BTN, COL_TEXT);
  tft.setTextFont(2);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(COL_CAPTION, COL_BG);
  tft.drawString("Scanning...", SCREEN_W - 12, DEV_CANCEL.y + DEV_CANCEL.h / 2);
  tft.setTextDatum(TL_DATUM);
}

static void devicesTouch(const TouchEvent& ev) {
  if (ev.type != T_DOWN) return;
  if (DEV_CANCEL.contains(ev.x, ev.y)) {
    enter(SCR_SETTINGS);
    return;
  }
  int shown = 0;
  for (int i = 0; i < FOUND_MAX && shown < 5; i++) {
    if (!app.found[i].used) continue;
    if (deviceRowRect(shown).contains(ev.x, ev.y)) {
      bmChooseTarget(i);
      enter(SCR_HOME);
      return;
    }
    shown++;
  }
}

// ---------------------------------------------------------------- calibration

static void drawCalTarget(int step) {
  tft.fillScreen(COL_BG);
  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString("Touch Calibration", SCREEN_W / 2, 100);
  tft.setTextFont(2);
  tft.setTextColor(COL_CAPTION, COL_BG);
  tft.drawString(step < 3 ? "Tap the center of each target" : "One more to verify",
                 SCREEN_W / 2, 126);
  tft.setTextDatum(TL_DATUM);
  int16_t cx = CAL_PTS[step][0], cy = CAL_PTS[step][1];
  tft.drawCircle(cx, cy, 10, COL_ACCENT);
  tft.drawFastHLine(cx - 14, cy, 28, COL_ACCENT);
  tft.drawFastVLine(cx, cy - 14, 28, COL_ACCENT);
}

static void enterCal() {
  calStep = 0;
  calPhase = 1;  // require a clean release first
  calPhaseAt = millis();
  drawCalTarget(0);
}

static bool solveAffine(const int32_t raw[3][2], const int16_t scr[3][2], float m[6]) {
  float rx0 = raw[0][0], ry0 = raw[0][1];
  float rx1 = raw[1][0], ry1 = raw[1][1];
  float rx2 = raw[2][0], ry2 = raw[2][1];
  float det = rx0 * (ry1 - ry2) - ry0 * (rx1 - rx2) + (rx1 * ry2 - rx2 * ry1);
  if (fabsf(det) < 1.0f) return false;
  for (int axis = 0; axis < 2; axis++) {
    float s0 = scr[0][axis], s1 = scr[1][axis], s2 = scr[2][axis];
    m[axis * 3 + 0] = (s0 * (ry1 - ry2) - ry0 * (s1 - s2) + (s1 * ry2 - s2 * ry1)) / det;
    m[axis * 3 + 1] = (rx0 * (s1 - s2) - s0 * (rx1 - rx2) + (rx1 * s2 - rx2 * s1)) / det;
    m[axis * 3 + 2] =
        (rx0 * (ry1 * s2 - ry2 * s1) - ry0 * (rx1 * s2 - rx2 * s1) + s0 * (rx1 * ry2 - rx2 * ry1)) /
        det;
  }
  return true;
}

static void calTick() {
  int16_t rx, ry;
  bool down = touchRawSample(rx, ry);
  uint32_t now = millis();

  if (calPhase == 1) {  // waiting for release
    if (down) {
      calPhaseAt = now;
    } else if (now - calPhaseAt > 250) {
      calPhase = 0;
    }
    return;
  }

  if (!down) return;

  // Average a short burst for a stable point.
  int32_t sx = 0, sy = 0;
  int samples = 0;
  uint32_t until = now + 120;
  while (millis() < until) {
    if (touchRawSample(rx, ry)) {
      sx += rx;
      sy += ry;
      samples++;
    }
    delay(5);
  }
  if (samples < 4) return;
  int32_t px = sx / samples, py = sy / samples;

  if (calStep < 3) {
    calRaw[calStep][0] = px;
    calRaw[calStep][1] = py;
    calStep++;
    calPhase = 1;
    calPhaseAt = millis();
    if (calStep == 3 && !solveAffine(calRaw, CAL_PTS, calM)) {
      enterCal();  // degenerate points; start over
      return;
    }
    drawCalTarget(calStep);
    return;
  }

  // Verification tap against the 4th target.
  float vx = calM[0] * px + calM[1] * py + calM[2];
  float vy = calM[3] * px + calM[4] * py + calM[5];
  float dx = vx - CAL_PTS[3][0], dy = vy - CAL_PTS[3][1];
  if (dx * dx + dy * dy <= 25.0f * 25.0f) {
    touchSetCal(calM);
    enter(calReturn);
  } else {
    enterCal();
    tft.setTextFont(2);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_RED, COL_BG);
    tft.drawString("Off target - try again", SCREEN_W / 2, 150);
    tft.setTextDatum(TL_DATUM);
  }
}

// ---------------------------------------------------------------- router

static void enter(Screen s) {
  if (cur == SCR_DEVICES && s != SCR_DEVICES) bmSetDiscover(false);
  if (s == SCR_DEVICES && cur != SCR_DEVICES) bmSetDiscover(true);
  cur = s;
  dragSlider = 0;
  if (s == SCR_CAL) {
    enterCal();
    return;
  }
  if (isMainScreen(s)) {
    uiTabRail(activeTab());
  } else {
    tft.fillRect(0, 0, RAIL_W, SCREEN_H, COL_BG);
  }
  uiHeader(headerTitle(), isMainScreen(s));
  switch (s) {
    case SCR_HOME:     drawHome(); break;
    case SCR_PALETTES: drawPalettes(); break;
    case SCR_MODES:    drawModes(); break;
    case SCR_TWEAKS:   drawTweaks(); break;
    case SCR_SETTINGS: drawSettings(); break;
    case SCR_COLOR:    drawColor(); break;
    case SCR_MATRIX:   drawMatrix(); break;
    case SCR_TEXT:     drawText(); break;
    case SCR_DEVICES:  drawDevices(); break;
    default: break;
  }
}

void screensBegin() {
  if (!touchHasCal()) {
    calReturn = SCR_HOME;
    cur = SCR_CAL;
    enterCal();
  } else {
    enter(SCR_HOME);
  }
}

void screensTouch(const TouchEvent& ev) {
  if (cur == SCR_CAL) return;  // calibration reads raw in its tick

  if (isMainScreen(cur) && ev.type == T_DOWN) {
    int tab = uiTabHit(ev.x, ev.y);
    if (tab >= 0) {
      static const Screen TAB_SCREENS[5] = {SCR_HOME, SCR_PALETTES, SCR_MODES, SCR_TWEAKS,
                                            SCR_SETTINGS};
      if (TAB_SCREENS[tab] != cur) enter(TAB_SCREENS[tab]);
      return;
    }
  }

  switch (cur) {
    case SCR_HOME:     homeTouch(ev); break;
    case SCR_PALETTES: palettesTouch(ev); break;
    case SCR_MODES:    modesTouch(ev); break;
    case SCR_TWEAKS:   tweaksTouch(ev); break;
    case SCR_SETTINGS: settingsTouch(ev); break;
    case SCR_COLOR:    colorTouch(ev); break;
    case SCR_MATRIX:   matrixTouch(ev); break;
    case SCR_TEXT:     textTouch(ev); break;
    case SCR_DEVICES:  devicesTouch(ev); break;
    default: break;
  }
}

void screensTick() {
  if (cur == SCR_CAL) {
    calTick();
    return;
  }

  if (app.connectionsDirty) {
    app.connectionsDirty = false;
    uiHeader(headerTitle(), isMainScreen(cur));
    if (cur == SCR_HOME) drawHome();
    else if (cur == SCR_SETTINGS) drawSettings();
    else if (cur == SCR_DEVICES) drawDeviceRows();
  }

  // A status chunk landed: the device is the source of truth, so repaint -
  // but not mid-drag, where the echo of our own throttled writes would yank
  // the knob out from under the finger. The keyboard is deliberately not
  // repainted: a chunk arriving mid-typing must not clobber the buffer's
  // on-screen state.
  if (app.statusDirty && dragSlider == 0) {
    app.statusDirty = false;
    switch (cur) {
      case SCR_HOME:     drawHome(); break;
      case SCR_PALETTES: drawPalettes(); break;
      case SCR_MODES:    drawModes(); break;
      case SCR_TWEAKS:   drawTweaks(); break;
      case SCR_SETTINGS: drawSettings(); break;
      case SCR_MATRIX:   drawMatrix(); break;
      default: break;
    }
  }

  if (cur == SCR_MATRIX && matrixTestSentAt && millis() - matrixTestSentAt > 30000) {
    matrixTestSentAt = 0;
    drawMatrixTestChip();
  }

  if (app.foundDirty && cur == SCR_DEVICES) {
    app.foundDirty = false;
    drawDeviceRows();
  }

  if (forgetArmed && millis() - forgetArmedAt > 3000) {
    forgetArmed = false;
    if (cur == SCR_SETTINGS) drawSettings();
  }
}

bool screensAllowSleep() {
  return cur != SCR_CAL;
}
