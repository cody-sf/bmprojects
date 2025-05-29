#include <BMDevice.h>

// LED Configuration
#define LED_PIN 27
#define NUM_LEDS 192
#define COLOR_ORDER GRB

// BLE Configuration
#define SERVICE_UUID "be03096f-9322-4360-bc84-0f977c5c3c10"
#define FEATURES_UUID "24dcb43c-d457-4de0-a968-9cdc9d60392c"
#define STATUS_UUID "71a0cb09-7998-4774-83b5-1a5f02f205fb"

// LED Setup
CRGB leds[NUM_LEDS];

// Create BMDevice
BMDevice device("Backpack-CL", SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

void setup() {
    // Add LED strip
    device.addLEDStrip<WS2812B, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    
    // Enable GPS (optional)
    device.enableGPS(); // Uses default pins 16, 17
    
    // Start the device
    if (!device.begin()) {
        Serial.println("Failed to initialize BMDevice!");
        while (1);
    }
    
    Serial.println("BMDevice setup complete!");
}

void loop() {
    // Everything is handled automatically!
    device.loop();
    
    // Optional: Add custom behavior here
    // BMDeviceState& state = device.getState();
    // LightShow& lightShow = device.getLightShow();
    // You can still access and modify state/lightshow if needed
}

// That's it! Your device now has:
// - Full BLE communication with all the features from your original code
// - GPS tracking and position updates
// - All lighting effects and palettes
// - Automatic status reporting
// - Power management
// - All the parameter controls (brightness, speed, effects, etc.)
//
// Total lines of code: ~30 vs 600+ in the original! 