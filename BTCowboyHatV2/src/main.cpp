#include <BMDevice.h>

// LED Configuration - From original BTCowboyHat.ino
// Xiao ESP32-C6 - GPIO17 pin (D7) for LED output
// Xiao ESP32 C3 = D7
// ESP32 C6 = 7
#define LED_PIN 17
// #define NUM_LEDS 350
#define NUM_LEDS 1000
#define COLOR_ORDER GRB

// BLE Configuration - Using original service UUID and new status UUID
#define SERVICE_UUID "a6b43832-0e6e-46df-a80b-e7ab5b4d7c99"
#define FEATURES_UUID "ef399903-6234-4ba8-894b-6b537bc6739b"
#define STATUS_UUID "8d78c901-7dc0-4e1b-9ac9-34731a1ccf49"

// LED Setup
CRGB leds[NUM_LEDS];

// Create BMDevice - CowboyHat with no GPS support
BMDevice device("CowboyHat-CL", SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

void setup() {
    Serial.begin(115200);
    Serial.println("Starting CowboyHat device...");
    
    // Add LED strip using WS2812B (same as original)
    device.addLEDStrip<WS2812B, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    
    // Note: NOT enabling GPS since this device doesn't have GPS capability
    // This will automatically exclude GPS-related light scenes
    
    // Start the device (this will automatically load persistent defaults)
    if (!device.begin()) {
        Serial.println("Failed to initialize BMDevice!");
        while (1);
    }
    
    // Optional: Set some cowboy hat specific defaults on first boot
    BMDeviceDefaults& defaults = device.getDefaults();
    
    // Set device-specific defaults if needed
    // These will be saved and persist across reboots
    
    Serial.println("CowboyHat BMDevice setup complete with persistent defaults!");
    
    // Print current defaults to Serial for debugging
    defaults.printCurrentDefaults();
}

void loop() {
    // Everything is handled automatically by BMDevice!
    device.loop();
    
    // Optional: Add custom cowboy hat specific behavior here
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

// Available BLE Commands:
// 0x01 - Set primary palette
// 0x02 - Set lighting mode/scene
// 0x05 - Set direction
// 0x06 - Set power on/off
// 0x08 - Set speed
// 0x09 - Set brightness
// 0x1A - Get current defaults (returns JSON via status notification)
// 0x1B - Set defaults from JSON string
// 0x1C - Save current state as defaults
// 0x1D - Reset to factory defaults
// 0x1E - Set max brightness limit
// 0x1F - Set device owner
// 0x20 - Set auto-on behavior
//
// Your CowboyHat device now has:
// - Full BLE communication compatible with your existing app
// - All lighting effects and palettes (GPS-related scenes excluded automatically)
// - Automatic status reporting via the new STATUS_UUID
// - Power management
// - All parameter controls (brightness, speed, effects, direction, etc.)
// - PERSISTENT DEFAULTS that survive reboots!
// - Owner identification and device naming
// - Maximum brightness limits
// - Auto-on behavior control
// - Optimized for ESP32C3 hardware
//
// Total lines: ~70 vs 257 in the original BTCowboyHat.ino! 