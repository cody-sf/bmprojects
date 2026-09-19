#include <Arduino.h>
#include "AppState.h"
#include "BmLink.h"
#include "Screens.h"
#include "TouchPad.h"
#include "Ui.h"

/**
 * BikeRemote: a standalone handlebar controller for one BMDevice (the bike)
 * on the ESP32 "Cheap Yellow Display" (1-USB / ILI9341 variant), mirroring
 * the RNUmbrella device page over the same BLE protocol the phone and watch
 * speak.
 *
 * No WiFi on purpose: the radio belongs to the BLE link. The board's RGB LED
 * shows link health - green connected, amber searching, red no light chosen.
 */

AppState app;

// CYD RGB LED, active low.
static const int LED_R = 4, LED_G = 16, LED_B = 17;
static uint32_t lastLed = 0;

// Display sleep: backlight off after this long without a touch. The remote
// stays connected and keeps tracking status underneath; the first tap wakes
// the screen and is swallowed, so waking can never also press a button.
static const uint32_t SLEEP_TIMEOUT_MS = 15000;
static uint32_t lastActivity = 0;
static bool displayAsleep = false;
static bool swallowTouch = false;

static void ledTick() {
  uint32_t now = millis();
  if (now - lastLed < 500) return;
  lastLed = now;
  bool green = app.connected;
  bool amber = !green && app.haveTarget;
  digitalWrite(LED_R, green ? HIGH : LOW);
  digitalWrite(LED_G, (green || amber) ? LOW : HIGH);
  digitalWrite(LED_B, HIGH);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  digitalWrite(LED_R, HIGH);
  digitalWrite(LED_G, HIGH);
  digitalWrite(LED_B, HIGH);

  uiBegin();
  touchInit();
  bmInit();
  screensBegin();
}

void loop() {
  uint32_t now = millis();

  TouchEvent ev = touchPoll();
  if (ev.type != T_NONE) {
    lastActivity = now;
    if (displayAsleep) {
      displayAsleep = false;
      uiBacklight(true);
      swallowTouch = true;
    }
    if (swallowTouch) {
      if (ev.type == T_UP) swallowTouch = false;
    } else {
      screensTouch(ev);
    }
  }

  // Calibration reads raw touch outside touchPoll, so its taps never land
  // here - keep the clock fresh instead of blacking out mid-target.
  if (!screensAllowSleep()) lastActivity = now;
  if (!displayAsleep && now - lastActivity > SLEEP_TIMEOUT_MS) {
    displayAsleep = true;
    uiBacklight(false);
  }

  screensTick();
  ledTick();
  delay(5);
}
