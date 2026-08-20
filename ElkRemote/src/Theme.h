#pragma once
#include <stdint.h>
#include <TFT_eSPI.h>

/**
 * The RNUmbrella look, in RGB565: black ground, #1a1a1a cards, #2a2a2a chips,
 * the orange accents, grey captions. Values lifted from pages/Elk/Elk.tsx so
 * the remote reads as the same app.
 */
constexpr uint16_t rgb565(uint32_t rgb) {
  return static_cast<uint16_t>(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
}

constexpr uint16_t COL_BG       = rgb565(0x000000);
constexpr uint16_t COL_CARD     = rgb565(0x1a1a1a);
constexpr uint16_t COL_CHIP     = rgb565(0x2a2a2a);
constexpr uint16_t COL_BTN      = rgb565(0x333333);
constexpr uint16_t COL_ACCENT   = rgb565(0xF78702);
constexpr uint16_t COL_ACCENT2  = rgb565(0xFFB84D);
constexpr uint16_t COL_TEXT     = rgb565(0xFFFFFF);
constexpr uint16_t COL_CAPTION  = rgb565(0x888888);
constexpr uint16_t COL_GREEN    = rgb565(0x00CC66);
constexpr uint16_t COL_DOT_OFF  = rgb565(0x555555);
constexpr uint16_t COL_BORDER   = rgb565(0x444444);
constexpr uint16_t COL_RED      = rgb565(0xCC2222);

// Layout: portrait 240x320, phone-shaped like the app.
constexpr int16_t SCREEN_W   = 240;
constexpr int16_t SCREEN_H   = 320;
constexpr int16_t HEADER_H   = 30;
constexpr int16_t TABBAR_H   = 36;
constexpr int16_t CONTENT_Y  = HEADER_H;
constexpr int16_t CONTENT_H  = SCREEN_H - HEADER_H - TABBAR_H;
constexpr int16_t PAD        = 10;

extern TFT_eSPI tft;
