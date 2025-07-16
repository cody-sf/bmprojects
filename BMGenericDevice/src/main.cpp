#include <BMDevice.h>

// Default UUIDs for generic device
#define SERVICE_UUID "4746abe4-2135-4a84-8f2f-f47f3a73e73b"
#define FEATURES_UUID "3927e9db-012b-4db9-8890-984fe28faf83"
#define STATUS_UUID "c054c450-cf93-4e6f-848e-2c521e739f4b"

// BMDevice instance with dynamic naming
BMDevice device(SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("=== BMGeneric Device Starting ===");
    
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
}

void loop() {
    device.loop();
} 