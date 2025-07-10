// #include <BMDevice.h>
// #include <Preferences.h>

// // === HARDWARE CONFIGURATION ===
// // LED Configuration - 9 LED strips for sign (now possible with I2S)
// #define NUM_STRIPS 8
// #define LEDS_PER_STRIP 450
// #define COLOR_ORDER GRB

// // BLE Configuration - Same UUIDs as original SlutplatzSign
// // #define SERVICE_UUID "0a503709-997d-4bcf-8283-56f146b96b94"
// // #define FEATURES_UUID "705b29ea-1693-4c51-8544-54c4823eb2b1"
// // #define STATUS_UUID "ee3b9e84-689e-4523-92e3-427695018752"
// #define SERVICE_UUID "a6b43832-0e6e-46df-a80b-e7ab5b4d7c99"
// #define FEATURES_UUID "ef399903-6234-4ba8-894b-6b537bc6739b"
// #define STATUS_UUID "8d78c901-7dc0-4e1b-9ac9-34731a1ccf49"
// // === LED SETUP ===
// // Individual LED strip arrays for BMDevice
// CRGB leds0[LEDS_PER_STRIP], leds1[LEDS_PER_STRIP], leds2[LEDS_PER_STRIP], leds3[LEDS_PER_STRIP];
// CRGB leds4[LEDS_PER_STRIP], leds5[LEDS_PER_STRIP], leds6[LEDS_PER_STRIP], leds7[LEDS_PER_STRIP];
// //CRGB leds8[LEDS_PER_STRIP];

// // Create BMDevice - SlutplatzSign with no GPS support
// BMDevice device("CowboyHat-CL", SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

// void setup() {
//     Serial.begin(115200);
//     Serial.println("Starting SlutplatzSign device...");
    
//     // Add LED strips using same pins as original SlutplatzSign
//     device.addLEDStrip<WS2812B, 32, COLOR_ORDER>(leds0, LEDS_PER_STRIP);
//     device.addLEDStrip<WS2812B, 33, COLOR_ORDER>(leds1, LEDS_PER_STRIP);
//     device.addLEDStrip<WS2812B, 27, COLOR_ORDER>(leds2, LEDS_PER_STRIP);
//     device.addLEDStrip<WS2812B, 14, COLOR_ORDER>(leds3, LEDS_PER_STRIP);
//     device.addLEDStrip<WS2812B, 12, COLOR_ORDER>(leds4, LEDS_PER_STRIP);
//     device.addLEDStrip<WS2812B, 13, COLOR_ORDER>(leds5, LEDS_PER_STRIP);
//     device.addLEDStrip<WS2812B, 18, COLOR_ORDER>(leds6, LEDS_PER_STRIP);
//     device.addLEDStrip<WS2812B, 5, COLOR_ORDER>(leds7, LEDS_PER_STRIP);
//     //device.addLEDStrip<WS2812B, 22, COLOR_ORDER>(leds8, LEDS_PER_STRIP);
    
//     // Note: NOT enabling GPS since this is a stationary sign
//     // This will automatically exclude GPS-related light scenes
    
//     // Start the device (this will automatically load persistent defaults)
//     if (!device.begin()) {
//         Serial.println("Failed to initialize BMDevice!");
//         while (1);
//     }
    
//     // Optional: Set some sign-specific defaults on first boot
//     BMDeviceDefaults& defaults = device.getDefaults();
    
//     // Set device-specific defaults if needed
//     // These will be saved and persist across reboots
    
//     Serial.println("SlutplatzSign BMDevice setup complete with persistent defaults!");
    
//     // Print current defaults to Serial for debugging
//     defaults.printCurrentDefaults();
// }

// void loop() {
//     // Everything is handled automatically by BMDevice!
//     device.loop();
    
//     // Optional: Add custom sign-specific behavior here
//     // BMDeviceState& state = device.getState();
//     // LightShow& lightShow = device.getLightShow();
//     // BMDeviceDefaults& defaults = device.getDefaults();
    
//     // You can access and modify state/lightshow/defaults if needed
//     // For example, save current state as defaults with a button press:
//     // if (buttonPressed) {
//     //     device.saveCurrentAsDefaults();
//     //     Serial.println("Current settings saved as defaults!");
//     // }
// }

// // Available BLE Commands:
// // 0x01 - Set primary palette
// // 0x02 - Set lighting mode/scene
// // 0x05 - Set direction
// // 0x06 - Set power on/off
// // 0x08 - Set speed
// // 0x09 - Set brightness
// // 0x1A - Get current defaults (returns JSON via status notification)
// // 0x1B - Set defaults from JSON string
// // 0x1C - Save current state as defaults
// // 0x1D - Reset to factory defaults
// // 0x1E - Set max brightness limit
// // 0x1F - Set device owner
// // 0x20 - Set auto-on behavior
// //
// // Your SlutplatzSign device now has:
// // - Full BLE communication compatible with your existing app
// // - All lighting effects and palettes (GPS-related scenes excluded automatically)
// // - Automatic status reporting via the STATUS_UUID
// // - Power management
// // - All parameter controls (brightness, speed, effects, direction, etc.)
// // - PERSISTENT DEFAULTS that survive reboots!
// // - Owner identification and device naming
// // - Maximum brightness limits
// // - Auto-on behavior control
// // - 9 synchronized LED strips (2700 total LEDs!)
// // - All effects work across all 9 strips simultaneously
// //
// // Total lines: ~80 vs 1803 in the original complex version!

// void restoreFactoryDefaults() {
//     Serial.println("🏭 [FACTORY RESET] Restoring factory defaults...");
    
//     // Reset BMDevice to factory defaults
//     bool success = device.resetToFactoryDefaults();
    
//     if (success) {
//         Serial.println("✅ [FACTORY RESET] Factory defaults restored successfully!");
//     } else {
//         Serial.println("❌ [FACTORY RESET] Failed to restore factory defaults");
//     }
// } 

#include <BMDevice.h>

// LED Configuration - From original BTCowboyHat.ino
// Xiao ESP32-C6 - GPIO17 pin (D7) for LED output
// Xiao ESP32 C3 = D7
// ESP32 C6 = 7
//#define LED_PIN 17
// #define NUM_LEDS 350
#define LEDS_PER_STRIP 500  // Reduced to allow 9th strip
#define COLOR_ORDER GRB

// BLE Configuration - Using original service UUID and new status UUID
#define SERVICE_UUID "a6b43832-0e6e-46df-a80b-e7ab5b4d7c99"
#define FEATURES_UUID "ef399903-6234-4ba8-894b-6b537bc6739b"
#define STATUS_UUID "8d78c901-7dc0-4e1b-9ac9-34731a1ccf49"

CRGB leds0[LEDS_PER_STRIP], leds1[LEDS_PER_STRIP], leds2[LEDS_PER_STRIP], leds3[LEDS_PER_STRIP];
CRGB leds4[LEDS_PER_STRIP], leds5[LEDS_PER_STRIP], leds6[LEDS_PER_STRIP], leds7[LEDS_PER_STRIP];


// // Create BMDevice - SlutplatzSign with no GPS support
BMDevice device("CowboyHat-CL", SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

void setup() {
    Serial.begin(115200);
    Serial.println("Starting SlutplatzSign device...");
    
    // Monitor memory usage to ensure we have enough for 9 strips
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("Total heap: %d bytes\n", ESP.getHeapSize());
    Serial.printf("Memory needed for 9 strips: ~%d bytes\n", 9 * 300 * 3);
    
//     // Add LED strips using same pins as original SlutplatzSign
    device.addLEDStrip<WS2812B, 32, COLOR_ORDER>(leds0, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 33, COLOR_ORDER>(leds1, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 27, COLOR_ORDER>(leds2, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 14, COLOR_ORDER>(leds3, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 12, COLOR_ORDER>(leds4, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 13, COLOR_ORDER>(leds5, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 18, COLOR_ORDER>(leds6, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 5, COLOR_ORDER>(leds7, LEDS_PER_STRIP);
    
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