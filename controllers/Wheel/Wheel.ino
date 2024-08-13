#include <FastLED.h>

#define LED_PIN     25
#define HALL_SENSOR_PIN 32
#define NUM_LEDS    84 // 14 LEDs * 6 segments
#define BRIGHTNESS  64
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

volatile unsigned long lastHallTrigger = 0;
volatile unsigned long rotationTime = 0; // Time for one rotation in milliseconds

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  Serial.println("Setup!");
  pinMode(HALL_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), hallSensorTriggered, FALLING);
}

void loop() {
  if (rotationTime > 0) {
    unsigned long currentTime = millis();
    float position = (float)(currentTime % rotationTime) / rotationTime; // Wheel position: 0.0 to 1.0

    updateLEDs(position);
    FastLED.show();
  }
}

void hallSensorTriggered() {
  Serial.println("Hall Sensor Triggered");
  unsigned long currentTime = millis();
  if (lastHallTrigger > 0) {
    rotationTime = currentTime - lastHallTrigger;
  }
  lastHallTrigger = currentTime;
}

void updateLEDs(float position) {
  for (int i = 0; i < NUM_LEDS; i++) {
    int segment = i / 14; // Determine the segment of the current LED
    int offset = i % 14; // Position within the segment

    // Calculate the LED's effective position in the pattern, considering the zigzag arrangement
    int effectivePosition = (segment % 2 == 0) ? offset : (13 - offset); // Reverse position for every other segment

    // Map the effective position to the wheel's current position
    if ((int)(position * 84) % 84 == effectivePosition) {
      leds[i] = CRGB::Red; // Set the LED to red when its position matches the wheel's position
    } else {
      leds[i] = CRGB::Black; // Otherwise, turn the LED off
    }
  }
}
