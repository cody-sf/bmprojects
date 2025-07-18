#include <BMDevice.h>

// Default UUIDs for generic device
#define SERVICE_UUID "4746abe4-2135-4a84-8f2f-f47f3a73e73b"
#define FEATURES_UUID "3927e9db-012b-4db9-8890-984fe28faf83"
#define STATUS_UUID "c054c450-cf93-4e6f-848e-2c521e739f4b"

// Chip-specific LED Configuration
#ifdef TARGET_ESP32_C6
    // ESP32-C6 configuration (single strip for now)
    #define NUM_STRIPS 1
    #define LEDS_PER_STRIP 350
    #define COLOR_ORDER GRB
    
    // Individual LED strip arrays for C6
    CRGB leds0[LEDS_PER_STRIP];
    
#else
    // ESP32 classic configuration (8 strips)
    #define NUM_STRIPS 8
    #define LEDS_PER_STRIP 450
    #define COLOR_ORDER GRB
    
    // Individual LED strip arrays for all 8 strips
    CRGB leds0[LEDS_PER_STRIP], leds1[LEDS_PER_STRIP], leds2[LEDS_PER_STRIP], leds3[LEDS_PER_STRIP];
    CRGB leds4[LEDS_PER_STRIP], leds5[LEDS_PER_STRIP], leds6[LEDS_PER_STRIP], leds7[LEDS_PER_STRIP];
#endif

// BMDevice instance with dynamic naming
BMDevice device(SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=== BMGeneric Device Starting ===");
    
    // Add LED strips based on chip type
    Serial.println("Adding LED strips...");
    
#ifdef TARGET_ESP32_C6
    // ESP32-C6: Single strip on GPIO 17
    device.addLEDStrip<WS2812B, 17, COLOR_ORDER>(leds0, LEDS_PER_STRIP);
    Serial.println("ESP32-C6 configuration:");
    Serial.println("  Strip 0: GPIO 17, 350 LEDs, GRB color order");
    
#else
    // ESP32 classic: 8 strips on the specified pins
    device.addLEDStrip<WS2812B, 32, COLOR_ORDER>(leds0, LEDS_PER_STRIP);  // Strip 0: GPIO 32
    device.addLEDStrip<WS2812B, 33, COLOR_ORDER>(leds1, LEDS_PER_STRIP);  // Strip 1: GPIO 33
    device.addLEDStrip<WS2812B, 27, COLOR_ORDER>(leds2, LEDS_PER_STRIP);  // Strip 2: GPIO 27
    device.addLEDStrip<WS2812B, 14, COLOR_ORDER>(leds3, LEDS_PER_STRIP);  // Strip 3: GPIO 14
    device.addLEDStrip<WS2812B, 12, COLOR_ORDER>(leds4, LEDS_PER_STRIP);  // Strip 4: GPIO 12
    device.addLEDStrip<WS2812B, 13, COLOR_ORDER>(leds5, LEDS_PER_STRIP);  // Strip 5: GPIO 13
    device.addLEDStrip<WS2812B, 18, COLOR_ORDER>(leds6, LEDS_PER_STRIP);  // Strip 6: GPIO 18
    device.addLEDStrip<WS2812B, 5, COLOR_ORDER>(leds7, LEDS_PER_STRIP);   // Strip 7: GPIO 5
    
    Serial.println("ESP32 classic configuration:");
    Serial.println("  Strip 0: GPIO 32, 450 LEDs, GRB color order");
    Serial.println("  Strip 1: GPIO 33, 450 LEDs, GRB color order");
    Serial.println("  Strip 2: GPIO 27, 450 LEDs, GRB color order");
    Serial.println("  Strip 3: GPIO 14, 450 LEDs, GRB color order");
    Serial.println("  Strip 4: GPIO 12, 450 LEDs, GRB color order");
    Serial.println("  Strip 5: GPIO 13, 450 LEDs, GRB color order");
    Serial.println("  Strip 6: GPIO 18, 450 LEDs, GRB color order");
    Serial.println("  Strip 7: GPIO 5, 450 LEDs, GRB color order");
#endif
    
    // Start the device (this will handle everything automatically)
    if (!device.begin()) {
        Serial.println("Failed to initialize BMDevice!");
        while (1);
    }
    
    Serial.println("=== BMGeneric Device Ready ===");
    Serial.println("All configuration is now handled by BMDevice library!");
    Serial.println("Use BLE commands to configure the device:");
    Serial.println("  BLE_FEATURE_SET_OWNER (0x30) - Set owner");
    Serial.println("  BLE_FEATURE_SET_DEVICE_TYPE (0x31) - Set device type");
    Serial.println("  BLE_FEATURE_CONFIGURE_LED_STRIP (0x32) - Configure LED strip");
    Serial.println("  BLE_FEATURE_GET_CONFIGURATION (0x33) - Get configuration");
    Serial.println("  BLE_FEATURE_RESET_TO_DEFAULTS (0x34) - Reset to defaults");
    Serial.flush();
}

void loop() {
    device.loop();
} 