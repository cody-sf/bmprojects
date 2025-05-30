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

// Create BMDevice with defaults support
BMDevice device("Backpack-CL", SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

void setup() {
    // Add LED strip
    device.addLEDStrip<WS2812B, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    
    // Enable GPS (optional)
    device.enableGPS(); // Uses default pins 16, 17
    
    // Start the device (this will automatically load persistent defaults)
    if (!device.begin()) {
        Serial.println("Failed to initialize BMDevice!");
        while (1);
    }
    
    // Optional: Set some defaults programmatically on first boot
    // These will be saved and persist across reboots
    BMDeviceDefaults& defaults = device.getDefaults();

    
    Serial.println("BMDevice setup complete with persistent defaults!");
    
    // Print current defaults to Serial for debugging
    defaults.printCurrentDefaults();
}

void loop() {
    // Everything is handled automatically!
    device.loop();
    
    // Optional: Add custom behavior here
    // BMDeviceState& state = device.getState();
    // LightShow& lightShow = device.getLightShow();
    // BMDeviceDefaults& defaults = device.getDefaults();
    
    // You can access and modify state/lightshow/defaults if needed
    // For example, save current state as defaults with a button press:
    // if (buttonPressed) {
    //     device.saveCurrentAsDefaults();
    //     Serial.println("Current settings saved as defaults!");
    // }
}

// New BLE Commands Available:
// 0x1A - Get current defaults (returns JSON via status notification)
// 0x1B - Set defaults from JSON string
// 0x1C - Save current state as defaults
// 0x1D - Reset to factory defaults
// 0x1E - Set max brightness limit
// 0x1F - Set device owner
// 0x20 - Set auto-on behavior
//
// Your device now has:
// - Full BLE communication with all the features from your original code
// - GPS tracking and position updates  
// - All lighting effects and palettes
// - Automatic status reporting
// - Power management
// - All the parameter controls (brightness, speed, effects, etc.)
// - PERSISTENT DEFAULTS that survive reboots!
// - Owner identification and device naming
// - Maximum brightness limits
// - Auto-on behavior control
//
// Total lines of code: ~50 vs 600+ in the original! 