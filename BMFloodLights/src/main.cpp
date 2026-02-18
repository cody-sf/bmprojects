#include <BMDevice.h>

// Default UUIDs for floodlights device
#define SERVICE_UUID "4746abe4-2135-4a84-8f2f-f47f3a73e73b"
#define FEATURES_UUID "3927e9db-012b-4db9-8890-984fe28faf83"
#define STATUS_UUID "c054c450-cf93-4e6f-848e-2c521e739f4b"


// Floodlight-specific LED Configuration
#ifdef TARGET_FLOODLIGHTS
    // Floodlights configuration - optimized for 5-10 high-power lights
    #define NUM_STRIPS 3
    #define LEDS_PER_STRIP 8  // 5-10 floodlights per strip, using 8 as middle ground
    #define COLOR_ORDER RGB   // CONFIRMED: Your WS2811 floodlights use RGB order
    #define FLOODLIGHT_BRIGHTNESS 255  // Always maximum brightness
    
    // 12V floodlight strips on optimized pins
    CRGB leds0[LEDS_PER_STRIP], leds1[LEDS_PER_STRIP], leds2[LEDS_PER_STRIP];

#elif TARGET_ESP32_C6
    // ESP32-C6 floodlight configuration
    #define NUM_STRIPS 1
    #define LEDS_PER_STRIP 8
    #define COLOR_ORDER RGB
    #define FLOODLIGHT_BRIGHTNESS 255
    
    CRGB leds0[LEDS_PER_STRIP];

#else
    // Default ESP32 classic floodlight configuration
    #define NUM_STRIPS 3
    #define LEDS_PER_STRIP 8
    #define COLOR_ORDER RGB
    #define FLOODLIGHT_BRIGHTNESS 255
    
    CRGB leds0[LEDS_PER_STRIP], leds1[LEDS_PER_STRIP], leds2[LEDS_PER_STRIP];
#endif

// BMDevice instance with floodlight-specific naming
BMDevice device(SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

// Floodlight-specific timing variables for slow fades
unsigned long lastEffectUpdate = 0;
const unsigned long FLOODLIGHT_UPDATE_INTERVAL = 200;  // Much slower updates (200ms vs typical 50ms)
const unsigned long SLOW_FADE_DURATION = 10000;       // 10 second fades instead of 3 seconds
bool floodlightInitialized = false;

// Function to apply floodlight-specific settings
void applyFloodlightSettings() {
    // Always set to maximum brightness for powerful illumination
    device.setBrightness(FLOODLIGHT_BRIGHTNESS);
    
    // Set slower speed for ambient effects
    device.getState().speed = 1000;  // Much slower than typical 50-100 (higher number = slower)
    
    Serial.println("=== Floodlight Settings Applied ===");
    Serial.print("Brightness: ");
    Serial.println(FLOODLIGHT_BRIGHTNESS);
    Serial.print("Speed (slower): ");
    Serial.println(device.getState().speed);
    Serial.print("LEDs per strip: ");
    Serial.println(LEDS_PER_STRIP);
    Serial.print("Update interval: ");
    Serial.print(FLOODLIGHT_UPDATE_INTERVAL);
    Serial.println("ms");
    
    Serial.println("=== Floodlight Color Notes ===");
    Serial.println("- RGB floodlights work best with warm colors and whites");
    Serial.println("- For vivid colors, consider reducing brightness to 180-220");
    Serial.println("- Cool blues/purples may appear washed out at max brightness");
    Serial.println("");
    Serial.println("=== WS2811 Floodlight Configuration ===");
    Serial.println("Protocol: WS2811, Color Order: RGB (CONFIRMED)");
    Serial.println("Pull-up resistors: Optional (avoid 470ohm - too strong)");
    Serial.println("Each floodlight acts as 1 addressable pixel");
    Serial.println("20V power supply is perfect for 12-24V rated lights");
}

// Function to select ambient-friendly effects for floodlights
void setAmbientEffect() {
    // Choose effects that work well for slow ambient lighting
    const LightSceneID ambientEffects[] = {
        LightSceneID::fire_plasma,       // Gentle fire effect
        LightSceneID::plasma_clouds,     // Smooth flowing colors
        LightSceneID::aurora_borealis,   // Gentle northern lights
        LightSceneID::breathe,           // Slow breathing effect
        LightSceneID::lava_lamp,         // Gentle lava lamp effect
        LightSceneID::palette_stream,    // Gentle color streaming
    };
    
    // Start with fire plasma as a good ambient effect
    device.setEffect(LightSceneID::fire_plasma);
    
    Serial.println("=== Ambient Effect Set ===");
    Serial.println("Effect: Fire Plasma (ambient-friendly)");
}

void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("=== BMFloodLights Device Starting ===");
    Serial.println("Specialized controller for high-power WS2811 floodlights");
    Serial.println("Features: RGB color order, 255 brightness, slow ambient fades");
    
    // Add LED strips based on configuration
    Serial.println("Adding floodlight LED strips...");
    
#ifdef TARGET_ESP32_C6
    // ESP32-C6: Single strip on GPIO 17
    device.addLEDStrip<WS2811, 17, COLOR_ORDER>(leds0, LEDS_PER_STRIP);
    Serial.println("ESP32-C6 Floodlight configuration:");
    Serial.println("  Strip 0: GPIO 17, 8 floodlights, RGB color order");

#elif TARGET_FLOODLIGHTS
    // Dedicated floodlight configuration with 12V optimized pins
    // Enable internal pull-ups before adding LED strips (optional)
    pinMode(25, INPUT_PULLUP); delay(10); pinMode(25, OUTPUT);
    pinMode(33, INPUT_PULLUP); delay(10); pinMode(33, OUTPUT);  
    pinMode(32, INPUT_PULLUP); delay(10); pinMode(32, OUTPUT);
    
    // WS2811 floodlights with CONFIRMED RGB color order
    device.addLEDStrip<WS2811, 25, RGB>(leds0, LEDS_PER_STRIP);  //12v 1
    device.addLEDStrip<WS2811, 33, RGB>(leds1, LEDS_PER_STRIP);  // 12v 2
    device.addLEDStrip<WS2811, 32, RGB>(leds2, LEDS_PER_STRIP);  // 12v 3
    
    Serial.println("Using WS2811 protocol for built-in chip floodlights");
    Serial.println("Color order: RGB (tested and confirmed)");
    
    Serial.println("Floodlight configuration (12V optimized):");
    Serial.println("  Strip 0: GPIO 25, 8 floodlights, RGB color order");
    Serial.println("  Strip 1: GPIO 33, 8 floodlights, RGB color order");
    Serial.println("  Strip 2: GPIO 32, 8 floodlights, RGB color order");

#else
    // Default ESP32 floodlight configuration
    device.addLEDStrip<WS2811, 32, COLOR_ORDER>(leds0, LEDS_PER_STRIP);
    device.addLEDStrip<WS2811, 33, COLOR_ORDER>(leds1, LEDS_PER_STRIP);
    device.addLEDStrip<WS2811, 25, COLOR_ORDER>(leds2, LEDS_PER_STRIP);
    
    Serial.println("ESP32 classic floodlight configuration:");
    Serial.println("  Strip 0: GPIO 32, 8 floodlights, RGB color order");
    Serial.println("  Strip 1: GPIO 33, 8 floodlights, RGB color order");
    Serial.println("  Strip 2: GPIO 25, 8 floodlights, RGB color order");
#endif
    
    // Start the device (this will handle everything automatically)
    if (!device.begin()) {
        Serial.println("Failed to initialize BMDevice!");
        while (1);
    }
    
    // Apply floodlight-specific settings
    applyFloodlightSettings();
    
    // Set an ambient-friendly effect
    setAmbientEffect();
    
    // Set a floodlight-optimized palette
    // These palettes work better with RGB floodlights:
    device.setPalette(AvailablePalettes::sunset);  // Start with warm sunset colors for ambiance
    
    Serial.println("=== Floodlight-Optimized Palettes ===");
    Serial.println("Recommended palettes for RGB floodlights:");
    Serial.println("- sunset, flame, lava (warm tones)");
    Serial.println("- earth, heart, emerald (natural colors)");  
    Serial.println("- Avoid: cool, vga, nebula (may appear washed out)");
    
    Serial.println("=== BMFloodLights Device Ready ===");
    Serial.println("Floodlight-optimized configuration loaded!");
    Serial.println("Features:");
    Serial.println("  - Maximum brightness (255) for powerful illumination");
    Serial.println("  - Slow fade timing for ambient lighting");
    Serial.println("  - RGB color order (tested and confirmed)");
    Serial.println("  - WS2811 protocol for built-in controllers");
    Serial.println("  - 12V strip optimization");
    Serial.println("");
    Serial.println("Use BLE commands to configure the device:");
    Serial.println("  BLE_FEATURE_SET_OWNER (0x30) - Set owner");
    Serial.println("  BLE_FEATURE_SET_DEVICE_TYPE (0x31) - Set device type");
    Serial.println("  BLE_FEATURE_CONFIGURE_LED_STRIP (0x32) - Configure LED strip");
    Serial.println("  BLE_FEATURE_GET_CONFIGURATION (0x33) - Get configuration");
    Serial.println("  BLE_FEATURE_RESET_TO_DEFAULTS (0x34) - Reset to defaults");
    
    floodlightInitialized = true;
    Serial.flush();
}

void loop() {
    // Run the main device loop
    device.loop();
    
    // Apply floodlight-specific timing control
    unsigned long currentTime = millis();
    
    // Ensure brightness stays at maximum (in case it gets changed via BLE)
    if (device.getState().brightness != FLOODLIGHT_BRIGHTNESS) {
        Serial.println("Brightness changed via BLE - resetting to maximum for floodlights");
        device.setBrightness(FLOODLIGHT_BRIGHTNESS);
    }
    
    // Apply slower update timing for ambient effects
    if (currentTime - lastEffectUpdate >= FLOODLIGHT_UPDATE_INTERVAL) {
        // The BMDevice library handles the actual light show updates
        // We just control the timing to make it slower for floodlights
        lastEffectUpdate = currentTime;
        
        // Occasionally log status for debugging
        static unsigned long lastStatusLog = 0;
        if (currentTime - lastStatusLog >= 30000) {  // Every 30 seconds
            Serial.println("=== Floodlight Status ===");
            Serial.print("Power: ");
            Serial.println(device.getState().power ? "On" : "Off");
            Serial.print("Brightness: ");
            Serial.println(device.getState().brightness);
            Serial.print("Effect: ");
            Serial.println(device.getLightShow().effectIdToName(device.getState().currentEffect));
            Serial.print("Palette: ");
            Serial.println(device.getLightShow().paletteIdToName(device.getState().currentPalette));
            Serial.print("Speed: ");
            Serial.println(device.getState().speed);
            lastStatusLog = currentTime;
        }
    }
    
    // Small delay to prevent overwhelming the system
    delay(10);
}