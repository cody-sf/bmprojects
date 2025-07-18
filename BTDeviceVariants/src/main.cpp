#include <BMDevice.h>

// Device Type Definitions
#define COWBOY_HAT 1
#define FANNYPACK 2

// Helper macro to concatenate strings at compile time
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

// Device name construction: DEVICENAME-USERINITIALS-UUID4DIGITS
#define DEVICE_NAME_BASE_COWBOY_HAT "CowboyHat"
#define DEVICE_NAME_BASE_FANNYPACK "Fannypack"

// Device Configuration based on build flag
#if DEVICE_TYPE == COWBOY_HAT
    // CowboyHat Configuration
    #define DEVICE_NAME_BASE DEVICE_NAME_BASE_COWBOY_HAT
    #define SERVICE_UUID "a6b43832-0e6e-46df-a80b-e7ab5b4d7c99"
    #define FEATURES_UUID "ef399903-6234-4ba8-894b-6b537bc6739b"
    #define STATUS_UUID "8d78c901-7dc0-4e1b-9ac9-34731a1ccf49"
    #define LED_PIN 17
    #define NUM_LEDS 600
    #define COLOR_ORDER GRB
    
#elif DEVICE_TYPE == FANNYPACK
    // Fannypack Configuration
    #define DEVICE_NAME_BASE DEVICE_NAME_BASE_FANNYPACK
    #define SERVICE_UUID "ac34717f-b5d0-45b5-8bbb-7020974bea2f"
    #define FEATURES_UUID "dc23128c-6bc0-406e-9725-075e44e73a2d"
    #define STATUS_UUID "ca6b1a3e-7746-4e80-88fe-9b9df934a0bd"
    #define LED_PIN 27
    #define NUM_LEDS 30
    #define COLOR_ORDER GRB
    
#else
    #error "Device type not defined or unsupported. Use DEVICE_TYPE=COWBOY_HAT or DEVICE_TYPE=FANNYPACK"
#endif

// Construct the full device name: DEVICENAME-USERINITIALS-UUID4DIGITS
#define DEVICE_NAME_FULL DEVICE_NAME_BASE "-" TOSTRING(USER_INITIALS) "-" TOSTRING(DEVICE_UUID)

// LED Setup
CRGB leds[NUM_LEDS];

// Create BMDevice with configuration based on device type
BMDevice device(DEVICE_NAME_FULL, SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

void setup() {
    Serial.begin(115200);
    
    Serial.print("Starting device: ");
    Serial.println(DEVICE_NAME_FULL);
    
    #if DEVICE_TYPE == COWBOY_HAT
        Serial.println("Device type: CowboyHat");
    #elif DEVICE_TYPE == FANNYPACK
        Serial.println("Device type: Fannypack");
    #endif
    
    Serial.print("User initials: ");
    Serial.println(TOSTRING(USER_INITIALS));
    Serial.print("Device UUID: ");
    Serial.println(TOSTRING(DEVICE_UUID));
    
    // Add LED strip with device-specific configuration
    device.addLEDStrip<WS2812B, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    
    // Note: NOT enabling GPS since these devices don't have GPS capability
    // This will automatically exclude GPS-related light scenes
    
    // Start the device (this will automatically load persistent defaults)
    if (!device.begin()) {
        Serial.println("Failed to initialize BMDevice!");
        while (1);
    }
    
    // Register device-specific status chunk for variant information
    device.registerStatusChunk("deviceVariant", []() {
        // Create device variant-specific status JSON with abbreviated keys
        StaticJsonDocument<512> doc;
        doc["type"] = "devVariant";
        
        // Device variant configuration - abbreviated keys
        #if DEVICE_TYPE == COWBOY_HAT
            doc["devType"] = "CowboyHat";
            doc["pin"] = LED_PIN;
            doc["leds"] = NUM_LEDS;
            doc["order"] = "GRB";
            doc["gps"] = false;
            doc["base"] = DEVICE_NAME_BASE_COWBOY_HAT;
        #elif DEVICE_TYPE == FANNYPACK
            doc["devType"] = "Fannypack";
            doc["pin"] = LED_PIN;
            doc["leds"] = NUM_LEDS;
            doc["order"] = "GRB";
            doc["gps"] = false;
            doc["base"] = DEVICE_NAME_BASE_FANNYPACK;
        #endif
        
        // Device identification - abbreviated
        doc["init"] = TOSTRING(USER_INITIALS);
        doc["uuid"] = TOSTRING(DEVICE_UUID);
        doc["name"] = DEVICE_NAME_FULL;
        doc["svc"] = SERVICE_UUID;
        doc["feat"] = FEATURES_UUID;
        doc["stat"] = STATUS_UUID;
        
        String status;
        serializeJson(doc, status);
        Serial.printf("[DeviceVariant] Sending variant info: %s\n", status.c_str());
        
        // Get bluetooth handler and send the status
        BMBluetoothHandler& bluetoothHandler = device.getBluetoothHandler();
        bluetoothHandler.sendStatusUpdate(status);
    }, "Device variant specific configuration and identification");
    
    // Set device-specific defaults on first boot
    BMDeviceDefaults& defaults = device.getDefaults();
    
    // Device-specific default configurations
    #if DEVICE_TYPE == COWBOY_HAT
        // CowboyHat specific defaults if needed
        Serial.print("CowboyHat device ");
        Serial.print(DEVICE_NAME_FULL);
        Serial.println(" setup complete with persistent defaults!");
    #elif DEVICE_TYPE == FANNYPACK
        // Fannypack specific defaults if needed  
        Serial.print("Fannypack device ");
        Serial.print(DEVICE_NAME_FULL);
        Serial.println(" setup complete with persistent defaults!");
    #endif
    
    // Print current defaults to Serial for debugging
    defaults.printCurrentDefaults();
}

void loop() {
    // Everything is handled automatically by BMDevice!
    device.loop();
    
    // Optional: Add device-specific behavior here
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
// Device Features:
// - Full BLE communication compatible with your existing app
// - All lighting effects and palettes (GPS-related scenes excluded automatically)
// - Automatic status reporting via STATUS_UUID
// - Power management
// - All parameter controls (brightness, speed, effects, direction, etc.)
// - PERSISTENT DEFAULTS that survive reboots!
// - Owner identification and device naming
// - Maximum brightness limits
// - Auto-on behavior control
// - Optimized for ESP32C6 hardware
//
// Build Instructions:
// - For CowboyHat-AY-1234: pio run -e cowboyhat -t upload
// - For Fannypack-AY-5678: pio run -e fannypack -t upload
// - For CowboyHat-JD-9999: pio run -e cowboyhat-jd -t upload
// - For Fannypack-MK-0001: pio run -e fannypack-mk -t upload
//
// Device Naming Format: DEVICENAME-USERINITIALS-UUID4DIGITS
// - Customize by adding new environments in platformio.ini
// - Set USER_INITIALS and DEVICE_UUID in build_flags
// - Example: -DUSER_INITIALS="XY" -DDEVICE_UUID="1111" 