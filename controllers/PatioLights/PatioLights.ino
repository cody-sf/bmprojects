#include <FastLED.h>

#define LED_PIN     27
#define NUM_LEDS    50
#define BRIGHTNESS  64
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

// Define the coolest and warmest color temperatures
const CRGB CoolestTemp = CRGB(0, 0, 255); // Coolest (blueish)
const CRGB WarmestTemp = CRGB(255, 165, 0); // Warmest (reddish)

// Rotary encoder pins
#define ENCODER_PIN_A 16
#define ENCODER_PIN_B 17
#define ENCODER_BUTTON_PIN 18



volatile long encoderPosition = 0;
volatile int lastEncoded = 0;

void IRAM_ATTR handleEncoder() {
    int MSB = digitalRead(ENCODER_PIN_A); // Most significant bit
    int LSB = digitalRead(ENCODER_PIN_B); // Least significant bit

    int encoded = (MSB << 1) | LSB; // Convert the 2 pin values to a single number
    int sum = (lastEncoded << 2) | encoded; // Add the previous encoded value

    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) {
      if (encoderPosition < 256) {
        encoderPosition++;
      }
    } 
    if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) {
      if(encoderPosition > 0) {
        encoderPosition--;  
      }
    } 

    lastEncoded = encoded; // Store this value for next time
}

void setup() {
    Serial.begin(115200);
    delay(3000); // 3 second delay for recovery

    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(BRIGHTNESS);

    pinMode(ENCODER_PIN_A, INPUT_PULLUP);
    pinMode(ENCODER_PIN_B, INPUT_PULLUP);
    pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), handleEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), handleEncoder, CHANGE);

    Serial.println("Setup complete. Rotate the encoder and press the button to see the output.");
}

void loop() {
    static long lastEncoderPosition = 0;

    // Check if the encoder position has changed
    if (encoderPosition != lastEncoderPosition) {
        lastEncoderPosition = encoderPosition;
        Serial.print("Encoder Position: ");
        Serial.println(encoderPosition);

        // Clamp the colorTemperature between 0 and 255
        int colorTemperature = constrain(encoderPosition, 0, 255);
        CRGB colorTemp = interpolateColor(CoolestTemp, WarmestTemp, colorTemperature);

        for (int i = 0; i < NUM_LEDS; i++) {
            leds[i] = colorTemp;
        }
        FastLED.show();
    }

    // Check if the encoder button is pressed
    if (digitalRead(ENCODER_BUTTON_PIN) == LOW) {
        Serial.println("Button Pressed!");
    }

    delay(50); // Adjust the delay as needed
}

// Interpolate between two colors
CRGB interpolateColor(CRGB color1, CRGB color2, uint8_t amount) {
    return blend(color1, color2, amount);
}
