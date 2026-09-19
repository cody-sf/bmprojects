#include "Screens.h"
#include "Actions.h"
#include "AppState.h"
#include "ElkCodec.h"
#include "ElkLink.h"
#include "Gradient.h"
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
static const Rect HOME_SWATCH    = {268, 160, 32, 32};
static const Rect HOME_COLOR_ROW = {84, 156, 228, 42};

// Palettes / Modes grids
static const int16_t GRID_Y = 50;
static const int16_t PAL_CARD_W = 116, PAL_CARD_H = 48, PAL_STEP = 54;
static const int16_t PAL_PER_PAGE = 6;
static const int16_t PAL_PAGER_Y = 212;
static const int16_t MODE_CHIP_H = 26, MODE_ROW_STEP = 30, MODES_PER_PAGE = 10;
static const int16_t MODE_PAGER_Y = 202;
static const int16_t GRID_COL_X[2] = {72, 194};
static const Rect PAL_CYCLE = {252, 26, 58, 22};

// Bars
static const int16_t BARS_ROW_Y = 56, BARS_ROW_H = 22;
static const Rect BARS_RESCAN = {238, 28, 72, 24};

// Color picker (modal, full width)
static const Rect COLOR_FIELD   = {12, 36, 196, 160};
static const Rect COLOR_PREVIEW = {224, 36, 84, 44};
static const Rect COLOR_WHITE   = {224, 96, 84, 36};
static const Rect COLOR_DONE    = {224, 192, 84, 36};

// Settings
static const Rect SET_RECAL  = {72, 36, 236, 32};
static const Rect SET_RESCAN = {72, 76, 236, 32};
static const Rect SET_FORGET = {72, 116, 236, 32};

// ---------------------------------------------------------------- state

static int palettesPage = 0;
static int modesPage = 0;

static int8_t dragSlider = 0;  // 0 none, 1 brightness, 2 speed
static int dragValue = 0;
static uint32_t lastSliderSend = 0;

static int8_t colorTarget = -1;  // -1 = all bars, else bar index
static Screen colorReturn = SCR_HOME;
static uint8_t pickR = 255, pickG = 136, pickB = 0;
static uint32_t lastColorSend = 0;
static bool colorPending = false;

static int8_t renameTarget = -1;
static char renameBuf[LABEL_MAX + 1];
static bool renameCaps = true;

static bool forgetArmed = false;
static uint32_t forgetArmedAt = 0;

static Screen calReturn = SCR_HOME;
static int calStep = 0;
static int calPhase = 0;  // 0 waiting for touch, 1 waiting for release
static uint32_t calPhaseAt = 0;
static int32_t calRaw[3][2];
static float calM[6];
static const int16_t CAL_PTS[4][2] = {{25, 25}, {295, 25}, {295, 215}, {25, 215}};

static int barOrder[MAX_BARS];
static int barRows = 0;
static uint32_t lastScanChip = 0;
static bool lastScanState = false;

static void enter(Screen s);

// ---------------------------------------------------------------- shared

static void rebuildBarOrder() {
  barRows = 0;
  for (int i = 0; i < MAX_BARS; i++) {
    if (app.bars[i].used) barOrder[barRows++] = i;
  }
  for (int i = 1; i < barRows; i++) {
    int key = barOrder[i], j = i - 1;
    while (j >= 0 && strcasecmp(app.bars[barOrder[j]].label, app.bars[key].label) > 0) {
      barOrder[j + 1] = barOrder[j];
      j--;
    }
    barOrder[j + 1] = key;
  }
}

static const char* headerTitle() {
  switch (cur) {
    case SCR_HOME:     return "Light Bars";
    case SCR_PALETTES: return "Palettes";
    case SCR_MODES:    return "Modes";
    case SCR_BARS:     return "Bars";
    case SCR_SETTINGS: return "Setup";
    case SCR_COLOR:    return "Color";
    case SCR_RENAME:   return "Rename";
    default:           return "";
  }
}

static bool isMainScreen(Screen s) {
  return s == SCR_HOME || s == SCR_PALETTES || s == SCR_MODES || s == SCR_BARS ||
         s == SCR_SETTINGS;
}

static int activeTab() {
  switch (cur) {
    case SCR_HOME:     return 0;
    case SCR_PALETTES: return 1;
    case SCR_MODES:    return 2;
    case SCR_BARS:     return 3;
    case SCR_SETTINGS: return 4;
    default:           return -1;
  }
}

// ---------------------------------------------------------------- home

static void drawHomeScanCaption() {
  tft.fillRect(RAIL_W, 208, SCREEN_W - RAIL_W, 20, COL_BG);
  if (elkIsScanning()) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(COL_CAPTION, COL_BG);
    tft.drawString("Scanning for bars...", RAIL_W + (SCREEN_W - RAIL_W) / 2, 210);
    tft.setTextDatum(TL_DATUM);
  }
}

static void drawHome() {
  uiClearContent(false);
  tft.fillRoundRect(HOME_CARD.x, HOME_CARD.y, HOME_CARD.w, HOME_CARD.h, 12, COL_CARD);

  tft.setFreeFont(&FreeSansBold9pt7b);
  tft.setTextColor(COL_TEXT, COL_CARD);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("All Bars", 84, HOME_TOGGLE.y + 12);
  tft.setTextFont(2);
  tft.setTextDatum(TL_DATUM);

  bool anyPowered = false;
  for (int i = 0; i < MAX_BARS; i++) {
    if (app.bars[i].used && app.bars[i].connected && app.bars[i].power) anyPowered = true;
  }
  uiToggle(HOME_TOGGLE, anyPowered, connectedCount() > 0);

  uiSliderRow(HOME_SLIDER_X, HOME_BRIGHT_Y, HOME_SLIDER_W, "Brightness", app.brightness);
  uiSliderRow(HOME_SLIDER_X, HOME_SPEED_Y, HOME_SLIDER_W, "Speed", app.speed);

  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT, COL_CARD);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("Solid Color", 84, HOME_SWATCH.y + HOME_SWATCH.h / 2);
  tft.setTextDatum(TL_DATUM);
  uiSwatch(HOME_SWATCH, rgbTo565(app.allR, app.allG, app.allB));

  drawHomeScanCaption();
}

static void homeTouch(const TouchEvent& ev) {
  Rect brightHit = uiSliderHit(HOME_SLIDER_X, HOME_BRIGHT_Y, HOME_SLIDER_W);
  Rect speedHit = uiSliderHit(HOME_SLIDER_X, HOME_SPEED_Y, HOME_SLIDER_W);

  if (ev.type == T_DOWN) {
    if (HOME_TOGGLE.contains(ev.x, ev.y)) {
      bool anyPowered = false;
      for (int i = 0; i < MAX_BARS; i++) {
        if (app.bars[i].used && app.bars[i].connected && app.bars[i].power) anyPowered = true;
      }
      actPowerAll(!anyPowered);
      uiToggle(HOME_TOGGLE, !anyPowered, connectedCount() > 0);
      return;
    }
    if (HOME_COLOR_ROW.contains(ev.x, ev.y)) {
      colorTarget = -1;
      colorReturn = SCR_HOME;
      pickR = app.allR; pickG = app.allG; pickB = app.allB;
      enter(SCR_COLOR);
      return;
    }
    if (brightHit.contains(ev.x, ev.y)) dragSlider = 1;
    else if (speedHit.contains(ev.x, ev.y)) dragSlider = 2;
    else return;
  }

  if (dragSlider && (ev.type == T_DOWN || ev.type == T_MOVE || ev.type == T_UP)) {
    dragValue = uiSliderValue(HOME_SLIDER_X, HOME_SLIDER_W, ev.x);
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

static int palettePages() {
  return (PALETTE_COUNT + PAL_PER_PAGE - 1) / PAL_PER_PAGE;
}

static Rect paletteCardRect(int slot) {
  return {GRID_COL_X[slot % 2],
          static_cast<int16_t>(GRID_Y + (slot / 2) * PAL_STEP),
          PAL_CARD_W, PAL_CARD_H};
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

static void drawPalettes() {
  uiClearContent(false);
  char caption[48];
  snprintf(caption, sizeof(caption), "Spread across %d bars", connectedCount());
  uiCaption(72, 30, caption);
  uiChip(PAL_CYCLE, "Cycle", app.cycleEnabled);
  for (int slot = 0; slot < PAL_PER_PAGE; slot++) {
    int idx = palettesPage * PAL_PER_PAGE + slot;
    if (idx >= PALETTE_COUNT) break;
    uiPaletteCard(paletteCardRect(slot), PALETTES[idx], app.selectedPalette == idx);
  }
  drawPager(PAL_PAGER_Y, palettesPage, palettePages());
}

static void palettesTouch(const TouchEvent& ev) {
  if (ev.type != T_DOWN) return;
  if (PAL_CYCLE.contains(ev.x, ev.y)) {
    actSetCycle(!app.cycleEnabled);
    uiChip(PAL_CYCLE, "Cycle", app.cycleEnabled);
    return;
  }
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
    if (idx >= PALETTE_COUNT) break;
    Rect r = paletteCardRect(slot);
    if (r.contains(ev.x, ev.y)) {
      int prev = app.selectedPalette;
      actPalette(idx);
      if (prev != app.selectedPalette) {
        if (prev >= 0 && prev / PAL_PER_PAGE == palettesPage) {
          uiPaletteCard(paletteCardRect(prev % PAL_PER_PAGE), PALETTES[prev], false);
        }
        uiPaletteCard(r, PALETTES[idx], true);
      }
      return;
    }
  }
}

// ---------------------------------------------------------------- modes

static int modePages() {
  return (ELK_MODE_COUNT + MODES_PER_PAGE - 1) / MODES_PER_PAGE;
}

static Rect modeChipRect(int slot) {
  return {GRID_COL_X[slot % 2],
          static_cast<int16_t>(GRID_Y + (slot / 2) * MODE_ROW_STEP),
          PAL_CARD_W, MODE_CHIP_H};
}

static void drawModes() {
  uiClearContent(false);
  uiCaption(72, 30, "Built into the bars - speed applies");
  for (int slot = 0; slot < MODES_PER_PAGE; slot++) {
    int idx = modesPage * MODES_PER_PAGE + slot;
    if (idx >= ELK_MODE_COUNT) break;
    uiChip(modeChipRect(slot), ELK_MODES[idx].name, app.selectedMode == idx);
  }
  drawPager(MODE_PAGER_Y, modesPage, modePages());
}

static void modesTouch(const TouchEvent& ev) {
  if (ev.type != T_DOWN) return;
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
    if (idx >= ELK_MODE_COUNT) break;
    Rect r = modeChipRect(slot);
    if (r.contains(ev.x, ev.y)) {
      int prev = app.selectedMode;
      actMode(idx);
      if (prev >= 0 && prev / MODES_PER_PAGE == modesPage) {
        uiChip(modeChipRect(prev % MODES_PER_PAGE), ELK_MODES[prev].name, false);
      }
      uiChip(r, ELK_MODES[idx].name, true);
      return;
    }
  }
}

// ---------------------------------------------------------------- bars

static Rect barToggleRect(int row) {
  return {264, static_cast<int16_t>(BARS_ROW_Y + row * BARS_ROW_H + 1), 44, 20};
}
static Rect barSwatchRect(int row) {
  return {230, static_cast<int16_t>(BARS_ROW_Y + row * BARS_ROW_H + 1), 26, 20};
}
static Rect barIdRect(int row) {
  return {192, static_cast<int16_t>(BARS_ROW_Y + row * BARS_ROW_H + 1), 32, 20};
}
static Rect barLabelRect(int row) {
  return {68, static_cast<int16_t>(BARS_ROW_Y + row * BARS_ROW_H), 120, BARS_ROW_H};
}

static void drawBarRow(int row) {
  int idx = barOrder[row];
  const Bar& bar = app.bars[idx];
  int16_t y = BARS_ROW_Y + row * BARS_ROW_H;
  tft.fillRect(RAIL_W, y, SCREEN_W - RAIL_W, BARS_ROW_H, COL_BG);
  tft.fillCircle(78, y + 11, 4, bar.connected ? COL_GREEN : COL_DOT_OFF);
  tft.setTextFont(2);
  tft.setTextColor(bar.connected ? COL_TEXT : COL_CAPTION, COL_BG);
  tft.setTextDatum(ML_DATUM);
  tft.setViewport(88, y, 100, BARS_ROW_H);
  tft.drawString(bar.label, 0, BARS_ROW_H / 2);
  tft.resetViewport();
  tft.setTextDatum(TL_DATUM);
  uiChip(barIdRect(row), "ID", false, bar.connected ? COL_BTN : COL_CARD,
         bar.connected ? COL_ACCENT2 : COL_CAPTION);
  uiSwatch(barSwatchRect(row),
           bar.hasColor ? rgbTo565(bar.r, bar.g, bar.b) : COL_CHIP);
  uiToggle(barToggleRect(row), bar.power, bar.connected);
}

static void drawBarsRows() {
  rebuildBarOrder();
  tft.fillRect(RAIL_W, BARS_ROW_Y, SCREEN_W - RAIL_W, SCREEN_H - BARS_ROW_Y, COL_BG);
  if (!barRows) {
    tft.setTextFont(2);
    tft.setTextColor(COL_CAPTION, COL_BG);
    tft.drawString("No light bars yet - power one", 72, 70);
    tft.drawString("on and tap Rescan.", 72, 88);
    return;
  }
  for (int row = 0; row < barRows; row++) drawBarRow(row);
}

static void drawRescanChip() {
  bool scanning = elkIsScanning();
  uiChip(BARS_RESCAN, scanning ? "Scanning" : "Rescan", false, COL_BTN,
         scanning ? COL_CAPTION : COL_ACCENT2);
  lastScanState = scanning;
}

static void drawBars() {
  uiClearContent(false);
  uiCaption(72, 32, "Tap a name to rename");
  drawRescanChip();
  drawBarsRows();
}

static void barsTouch(const TouchEvent& ev) {
  if (ev.type != T_DOWN) return;
  if (BARS_RESCAN.contains(ev.x, ev.y)) {
    elkRequestScan();
    drawRescanChip();
    return;
  }
  for (int row = 0; row < barRows; row++) {
    int idx = barOrder[row];
    Bar& bar = app.bars[idx];
    if (barToggleRect(row).contains(ev.x, ev.y)) {
      if (bar.connected) {
        actPowerBar(idx, !bar.power);
        drawBarRow(row);
      }
      return;
    }
    if (barSwatchRect(row).contains(ev.x, ev.y)) {
      if (bar.connected) {
        colorTarget = idx;
        colorReturn = SCR_BARS;
        if (bar.hasColor) { pickR = bar.r; pickG = bar.g; pickB = bar.b; }
        enter(SCR_COLOR);
      }
      return;
    }
    if (barIdRect(row).contains(ev.x, ev.y)) {
      actIdentify(idx);
      return;
    }
    if (barLabelRect(row).contains(ev.x, ev.y)) {
      renameTarget = idx;
      strlcpy(renameBuf, bar.label, sizeof(renameBuf));
      renameCaps = true;
      enter(SCR_RENAME);
      return;
    }
  }
}

// ---------------------------------------------------------------- settings

static void drawSettings() {
  uiClearContent(false);
  uiButton(SET_RECAL, "Recalibrate Touch", COL_BTN, COL_TEXT);
  uiButton(SET_RESCAN, "Rescan Bars", COL_BTN, COL_TEXT);
  uiButton(SET_FORGET, forgetArmed ? "Tap again to confirm" : "Forget All Bars",
           forgetArmed ? COL_RED : COL_BTN, forgetArmed ? COL_TEXT : COL_RED);
  char line[48];
  snprintf(line, sizeof(line), "%d bars registered, %d connected", barCount(), connectedCount());
  uiCaption(72, 160, line);
  snprintf(line, sizeof(line), "Free heap: %u KB", (unsigned)(ESP.getFreeHeap() / 1024));
  uiCaption(72, 178, line);
  uiCaption(72, 196, "ElkRemote for ESP32 CYD");
}

static void settingsTouch(const TouchEvent& ev) {
  if (ev.type != T_DOWN) return;
  if (SET_RECAL.contains(ev.x, ev.y)) {
    calReturn = SCR_SETTINGS;
    enter(SCR_CAL);
    return;
  }
  if (SET_RESCAN.contains(ev.x, ev.y)) {
    elkRequestScan();
    return;
  }
  if (SET_FORGET.contains(ev.x, ev.y)) {
    if (!forgetArmed) {
      forgetArmed = true;
      forgetArmedAt = millis();
    } else {
      forgetArmed = false;
      elkForgetAll();
    }
    drawSettings();
  }
}

// ---------------------------------------------------------------- color picker

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

static void sendPickedColor() {
  if (colorTarget < 0) actColorAll(pickR, pickG, pickB);
  else actColorBar(colorTarget, pickR, pickG, pickB);
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
      sendPickedColor();
      colorPending = false;
    }
    return;
  }
  if (ev.type == T_UP && colorPending) {
    sendPickedColor();
    colorPending = false;
    return;
  }
  if (ev.type != T_DOWN) return;
  if (COLOR_WHITE.contains(ev.x, ev.y)) {
    pickR = pickG = pickB = 255;
    drawColorPreview();
    sendPickedColor();
    return;
  }
  if (COLOR_DONE.contains(ev.x, ev.y)) {
    enter(colorReturn);
  }
}

// ---------------------------------------------------------------- rename

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

static void drawRenameField() {
  Rect f = {12, 32, 296, 26};
  tft.fillRoundRect(f.x, f.y, f.w, f.h, 6, COL_CHIP);
  char shown[LABEL_MAX + 2];
  snprintf(shown, sizeof(shown), "%s_", renameBuf);
  tft.setTextFont(2);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(COL_TEXT, COL_CHIP);
  tft.drawString(shown, f.x + 8, f.y + f.h / 2 + 1);
  tft.setTextDatum(TL_DATUM);
}

static void drawKeys() {
  for (int i = 0; i < keyCount; i++) {
    const Key& k = keys[i];
    char label[8] = {0};
    uint16_t bg = COL_CHIP, fg = COL_TEXT;
    switch (k.op) {
      case 0:
        label[0] = renameCaps ? toupper(k.ch) : k.ch;
        break;
      case 1: strcpy(label, renameCaps ? "AB" : "ab"); bg = COL_BTN; break;
      case 2: strcpy(label, "del"); bg = COL_BTN; break;
      case 3: strcpy(label, "space"); break;
      case 4: strcpy(label, "Done"); bg = COL_ACCENT; break;
      case 5: strcpy(label, "Esc"); bg = COL_BTN; fg = COL_CAPTION; break;
    }
    uiChip(k.r, label, false, bg, fg);
  }
}

static void drawRename() {
  uiClearContent(true);
  drawRenameField();
  buildKeys();
  drawKeys();
}

static void renameTouch(const TouchEvent& ev) {
  if (ev.type != T_DOWN) return;
  for (int i = 0; i < keyCount; i++) {
    const Key& k = keys[i];
    if (!k.r.contains(ev.x, ev.y)) continue;
    size_t len = strlen(renameBuf);
    switch (k.op) {
      case 0:
      case 3:
        if (len < LABEL_MAX) {
          renameBuf[len] = k.op == 3 ? ' ' : (renameCaps ? toupper(k.ch) : k.ch);
          renameBuf[len + 1] = 0;
          // Phone-style shift: capitalise one letter, then drop back.
          if (k.op == 0 && renameCaps) { renameCaps = false; drawKeys(); }
          drawRenameField();
        }
        return;
      case 1:
        renameCaps = !renameCaps;
        drawKeys();
        return;
      case 2:
        if (len) {
          renameBuf[len - 1] = 0;
          drawRenameField();
        }
        return;
      case 4:
        if (renameTarget >= 0 && strlen(renameBuf)) {
          strlcpy(app.bars[renameTarget].label, renameBuf,
                  sizeof(app.bars[renameTarget].label));
          storeSaveBars();
        }
        enter(SCR_BARS);
        return;
      case 5:
        enter(SCR_BARS);
        return;
    }
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
    case SCR_BARS:     drawBars(); break;
    case SCR_SETTINGS: drawSettings(); break;
    case SCR_COLOR:    drawColor(); break;
    case SCR_RENAME:   drawRename(); break;
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
      static const Screen TAB_SCREENS[5] = {SCR_HOME, SCR_PALETTES, SCR_MODES, SCR_BARS,
                                            SCR_SETTINGS};
      if (TAB_SCREENS[tab] != cur) enter(TAB_SCREENS[tab]);
      return;
    }
  }

  switch (cur) {
    case SCR_HOME:     homeTouch(ev); break;
    case SCR_PALETTES: palettesTouch(ev); break;
    case SCR_MODES:    modesTouch(ev); break;
    case SCR_BARS:     barsTouch(ev); break;
    case SCR_SETTINGS: settingsTouch(ev); break;
    case SCR_COLOR:    colorTouch(ev); break;
    case SCR_RENAME:   renameTouch(ev); break;
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
    if (cur == SCR_BARS) {
      drawBarsRows();
    } else if (cur == SCR_HOME) {
      drawHome();
    } else if (cur == SCR_SETTINGS) {
      drawSettings();
    }
  }

  uint32_t now = millis();
  if ((cur == SCR_BARS || cur == SCR_HOME) && now - lastScanChip > 800) {
    lastScanChip = now;
    if (elkIsScanning() != lastScanState) {
      if (cur == SCR_BARS) drawRescanChip();
      else drawHomeScanCaption();
      lastScanState = elkIsScanning();
    }
  }

  if (forgetArmed && now - forgetArmedAt > 3000) {
    forgetArmed = false;
    if (cur == SCR_SETTINGS) drawSettings();
  }
}
