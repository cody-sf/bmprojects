#pragma once
#include <stdint.h>

/**
 * The BMDevice page's behaviour, one level above raw commands. State updates
 * are optimistic for instant UI - unlike the ELK bars the device talks back,
 * and its next status chunk overwrites whatever we guessed (the firmware
 * pushes one on every change, so a twist of the bike's encoder shows up here
 * too).
 */

void actPower(bool on);
void actBrightness(uint8_t pct);   // 1-100, capped by the device's maxBri
void actSpeed(uint8_t pct);        // 1-100, higher = faster
void actDirection(bool reversed);
void actPalette(int16_t sel);      // built-in index, or CUSTOM_SEL_BASE + slot
void actEffect(int idx);           // index into EFFECTS
void actEffectById(const char* id);  // the Matrix screen's Show buttons
void actParam(uint8_t code, int value);
void actColor(uint8_t r, uint8_t g, uint8_t b);
void actIdentify();                // find-me strobe on the light, 5 s
void actSync(bool on);
void actSaveAsDefaults();          // persist the current look as power-on state

// Matrix display content (devices reporting mtxW > 0). The text and its
// style live on the device; pixel art and animation frames are uploaded by
// the phone - the remote only switches to them.
void actMarqueeText(const char* text);
void actTextStyle(bool bold);
void actTextFill(uint8_t fill);    // 0 gradient, 1 fire, 2 rain, 3 plasma
void actMatrixDisplay(uint8_t mode); // display overlay: 0 off, 1 text, 2 words, 3 bitmap, 4 anim
void actMatrixTest();              // 30 s wiring-diagnostic rainbow
