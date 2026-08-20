#include <Arduino.h>
#include "Actions.h"
#include "AppState.h"
#include "ElkLink.h"
#include "Screens.h"
#include "Theme.h"
#include "TouchPad.h"
#include "Ui.h"

/**
 * ElkRemote: a standalone controller for the ELK-BLEDOM light bars on the
 * ESP32 "Cheap Yellow Display" (1-USB / ILI9341 variant).
 *
 * No WiFi on purpose: the radio belongs entirely to the 8 BLE links, and the
 * heap it frees goes to the display. The board's RGB LED shows link health -
 * red none, amber some, green all.
 */

AppState app;

// CYD RGB LED, active low.
static const int LED_R = 4, LED_G = 16, LED_B = 17;
static uint32_t lastLed = 0;

static void ledTick() {
  uint32_t now = millis();
  if (now - lastLed < 500) return;
  lastLed = now;
  int total = barCount(), connected = connectedCount();
  bool green = total > 0 && connected == total;
  bool amber = connected > 0 && !green;
  digitalWrite(LED_R, (amber || (!green && !amber)) ? LOW : HIGH);
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
  elkInit();
  screensBegin();
}

void loop() {
  TouchEvent ev = touchPoll();
  if (ev.type != T_NONE) {
    screensTouch(ev);
  }
  screensTick();
  actTick();
  ledTick();
  delay(5);
}
