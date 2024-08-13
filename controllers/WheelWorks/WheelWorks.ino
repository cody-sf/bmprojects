#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

#define LED_PIN 25
#define HALL_SENSOR_PIN 32
#define NUM_LEDS 84 // Total number of LEDs
#define BRIGHTNESS 64
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

Adafruit_MPU6050 mpu;
volatile unsigned long lastHallTrigger = 0;
volatile unsigned long rotationTime = 0; // Time for one rotation in milliseconds
float initialYaw = 0;

void setup() {
  Serial.begin(115200);
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  Serial.println("Setup!");
  
  pinMode(HALL_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), hallSensorTriggered, FALLING);
  
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }

  Serial.println("MPU6050 Found!");
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  delay(100);
  setInitialYaw();
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

void setInitialYaw() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  initialYaw = atan2(a.acceleration.y, a.acceleration.x) * 180 / PI;
}


void displayStaticImage()
{
	float currentYaw = getCurrentYaw();
	float rotationAngle = currentYaw - initialYaw;

	// Map rotation angle to LED index
	int ledIndex = mapAngleToLEDIndex(rotationAngle);

	for (int i = 0; i < NUM_LEDS; i++)
	{
		if (i == ledIndex)
		{
			leds[i] = CRGB::Red; // Example: light up the LED at the current position in red
		}
		else
		{
			leds[i] = CRGB::Black;
		}
	}
	FastLED.show();
}

float getCurrentYaw() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  float yaw = atan2(a.acceleration.y, a.acceleration.x) * 180 / PI;
  return yaw;
}

int mapAngleToLEDIndex(float angle)
{
	// Assuming a full rotation maps to all LEDs
	int index = (int)(angle / 360.0 * NUM_LEDS) % NUM_LEDS;
	return index >= 0 ? index : NUM_LEDS + index; // Ensure index is positive
}

void updateLEDs(float position) {
    // Clear the LEDs to start with a blank slate
    fill_solid(leds, NUM_LEDS, CRGB::Black);

    // Define the static image as a sequence of LEDs to be lit
    const int imageLength = 10; // Example length of the image
    const int imageStart = (int)(position * NUM_LEDS) % NUM_LEDS; // Calculate starting position for the image

    for(int i = 0; i < imageLength; i++) {
        int ledIndex = (imageStart + i) % NUM_LEDS; // Wrap around if the index exceeds the total number of LEDs
        leds[ledIndex] = CRGB::Red; // Example: Set part of the image to red
    }

    FastLED.show();
}
