#include <ArduinoBLE.h>

// Configuration
#define SCAN_DURATION_MS 10000  // 10 seconds
#define CONNECT_TIMEOUT_MS 5000 // 5 seconds

// Variables to track connection state
BLEDevice connectedDevice;
BLEService connectedService;
BLECharacteristic buttonCharacteristic;
bool isConnected = false;
bool isScanning = false;
String targetDeviceName = ""; // Will be set when user chooses a device by name
String targetDeviceAddress = ""; // Will be set when user chooses a device by address

// Function prototypes
void scanForDevices();
void connectToDevice(BLEDevice device);
void monitorButtonPresses(BLEDevice device);
void printMenu();
void handleSerialInput();

void setup() {
    Serial.begin(115200);
    while (!Serial);
    
    Serial.println("BTBeacon Test Program");
    Serial.println("=====================");
    
    // Initialize BLE
    if (!BLE.begin()) {
        Serial.println("Failed to initialize BLE!");
        while (1);
    }
    
    Serial.println("BLE initialized successfully!");
    Serial.print("Local MAC address: ");
    Serial.println(BLE.address());
    
    printMenu();
}

void loop() {
    // Handle serial input for menu commands
    handleSerialInput();
    
    // Poll BLE events
    BLE.poll();
    
    // Check connection status
    if (isConnected && !connectedDevice.connected()) {
        Serial.println("Device disconnected!");
        isConnected = false;
        connectedDevice = BLEDevice();
    }
    
    delay(100);
}

void printMenu() {
    Serial.println("\n=== BTBeacon Test Menu ===");
    Serial.println("1. Scan for nearby BLE devices");
    Serial.println("2. Connect to device by name");
    Serial.println("3. Connect to device by address");
    Serial.println("4. Disconnect current device");
    Serial.println("5. Show connection status");
    Serial.println("6. Show this menu");
    Serial.println("\nEnter command number:");
}

void handleSerialInput() {
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        
        if (input == "1") {
            scanForDevices();
        } else if (input == "2") {
            if (isScanning) {
                Serial.println("Already scanning. Please wait...");
                return;
            }
            Serial.println("Enter device name to connect to:");
            while (!Serial.available()) {
                delay(100);
            }
            targetDeviceName = Serial.readStringUntil('\n');
            targetDeviceName.trim();
            Serial.println("Scanning for device: " + targetDeviceName);
            scanForDevices();
        } else if (input == "3") {
            if (isScanning) {
                Serial.println("Already scanning. Please wait...");
                return;
            }
            Serial.println("Enter device address to connect to:");
            while (!Serial.available()) {
                delay(100);
            }
            targetDeviceAddress = Serial.readStringUntil('\n');
            targetDeviceAddress.trim();
            Serial.println("Scanning for device with address: " + targetDeviceAddress);
            scanForDevices();
        } else if (input == "4") {
            if (isConnected) {
                connectedDevice.disconnect();
                Serial.println("Disconnected from device");
                isConnected = false;
            } else {
                Serial.println("No device connected");
            }
        } else if (input == "5") {
            if (isConnected) {
                Serial.println("Connected to: " + connectedDevice.localName());
                Serial.println("Device address: " + connectedDevice.address());
                Serial.println("RSSI: " + String(connectedDevice.rssi()));
            } else {
                Serial.println("No device connected");
            }
        } else if (input == "6") {
            printMenu();
        } else {
            Serial.println("Invalid command. Enter 1-6.");
        }
    }
}

void scanForDevices() {
    if (isScanning) {
        Serial.println("Already scanning...");
        return;
    }
    
    isScanning = true;
    Serial.println("Scanning for BLE devices...");
    
    // Start scanning
    BLE.scan();
    
    unsigned long startTime = millis();
    int deviceCount = 0;
    
    while (millis() - startTime < SCAN_DURATION_MS) {
        BLEDevice peripheral = BLE.available();
        
        if (peripheral) {
            deviceCount++;
            
            // Print device information with prominent address
            Serial.print("Device #");
            Serial.print(deviceCount);
            Serial.println(":");
            Serial.print("  📍 Address: ");
            Serial.println(peripheral.address());
            Serial.print("  📱 Name: ");
            if (peripheral.localName().length() > 0) {
                Serial.println(peripheral.localName());
            } else {
                Serial.println("(Unknown)");
            }
            Serial.print("  📶 RSSI: ");
            Serial.println(peripheral.rssi());
            
            // If we're looking for a specific device, try to connect
            bool foundTarget = false;
            if (targetDeviceName.length() > 0 && 
                peripheral.localName().indexOf(targetDeviceName) >= 0) {
                Serial.println("Found target device by name! Attempting to connect...");
                foundTarget = true;
            } else if (targetDeviceAddress.length() > 0 && 
                       peripheral.address().indexOf(targetDeviceAddress) >= 0) {
                Serial.println("Found target device by address! Attempting to connect...");
                foundTarget = true;
            }
            
            if (foundTarget) {
                BLE.stopScan();
                isScanning = false;
                connectToDevice(peripheral);
                targetDeviceName = "";
                targetDeviceAddress = "";
                return;
            }
            
            // Print available services
            Serial.print("  Advertised Service: ");
            Serial.println(peripheral.advertisedServiceUuid());
            
            Serial.println("  ---");
        }
    }
    
    BLE.stopScan();
    isScanning = false;
    
    Serial.println("Scan complete. Found " + String(deviceCount) + " devices.");
    
    if (targetDeviceName.length() > 0) {
        Serial.println("Target device '" + targetDeviceName + "' not found.");
        targetDeviceName = "";
    }
    
    if (targetDeviceAddress.length() > 0) {
        Serial.println("Target device with address '" + targetDeviceAddress + "' not found.");
        targetDeviceAddress = "";
    }
}

void connectToDevice(BLEDevice device) {
    Serial.println("Attempting to connect to: " + device.localName());
    
    if (device.connect()) {
        Serial.println("Connected successfully!");
        connectedDevice = device;
        isConnected = true;
        
        // Discover all attributes
        Serial.println("Discovering attributes...");
        if (device.discoverAttributes()) {
            Serial.println("Attributes discovered:");
            
            // List all services
            for (int i = 0; i < device.serviceCount(); i++) {
                BLEService service = device.service(i);
                Serial.print("  Service ");
                Serial.print(i);
                Serial.print(": ");
                Serial.println(service.uuid());
                
                // List characteristics for each service
                for (int j = 0; j < service.characteristicCount(); j++) {
                    BLECharacteristic characteristic = service.characteristic(j);
                    Serial.print("    Characteristic ");
                    Serial.print(j);
                    Serial.print(": ");
                    Serial.print(characteristic.uuid());
                    
                    // Check properties
                    Serial.print(" (Properties: 0x");
                    Serial.print(characteristic.properties(), HEX);
                    Serial.print(")");
                    
                    // Check if readable
                    if (characteristic.canRead()) {
                        Serial.print(" [R]");
                    }
                    if (characteristic.canWrite()) {
                        Serial.print(" [W]");
                    }
                    if (characteristic.canSubscribe()) {
                        Serial.print(" [N]");
                    }
                    Serial.println();
                    
                    // If this characteristic can be subscribed to, it might be for button events
                    if (characteristic.canSubscribe()) {
                        Serial.println("      -> This characteristic supports notifications!");
                        Serial.println("      -> This could be where button presses are reported");
                    }
                }
            }
            
            // Start monitoring for button presses
            monitorButtonPresses(device);
            
        } else {
            Serial.println("Failed to discover attributes");
        }
        
    } else {
        Serial.println("Failed to connect to device");
    }
}

void monitorButtonPresses(BLEDevice device) {
    Serial.println("\n=== Button Press Monitor ===");
    Serial.println("Looking for characteristics that can send notifications...");
    
    // Try to find and subscribe to notification characteristics
    bool foundNotificationChar = false;
    
    for (int i = 0; i < device.serviceCount(); i++) {
        BLEService service = device.service(i);
        
        for (int j = 0; j < service.characteristicCount(); j++) {
            BLECharacteristic characteristic = service.characteristic(j);
            
            if (characteristic.canSubscribe()) {
                Serial.print("Subscribing to characteristic ");
                Serial.print(characteristic.uuid());
                Serial.print(" in service ");
                Serial.print(service.uuid());
                Serial.println("...");
                
                if (characteristic.subscribe()) {
                    Serial.println("  -> Successfully subscribed!");
                    foundNotificationChar = true;
                } else {
                    Serial.println("  -> Failed to subscribe");
                }
            }
        }
    }
    
    if (!foundNotificationChar) {
        Serial.println("No notification characteristics found. The device may not support button notifications.");
        Serial.println("Try pressing the button to see if any characteristics change.");
    } else {
        Serial.println("\nMonitoring for button presses...");
        Serial.println("Press the button on your beacon!");
    }
    
    // Monitor loop
    while (device.connected()) {
        // Check all subscribed characteristics for updates
        for (int i = 0; i < device.serviceCount(); i++) {
            BLEService service = device.service(i);
            
            for (int j = 0; j < service.characteristicCount(); j++) {
                BLECharacteristic characteristic = service.characteristic(j);
                
                if (characteristic.canSubscribe() && characteristic.valueUpdated()) {
                    Serial.println("\n🔔 BUTTON PRESS DETECTED! 🔔");
                    Serial.print("Service: ");
                    Serial.println(service.uuid());
                    Serial.print("Characteristic: ");
                    Serial.println(characteristic.uuid());
                    Serial.print("Value length: ");
                    Serial.println(characteristic.valueLength());
                    
                    // Print the raw value
                    if (characteristic.valueLength() > 0) {
                        Serial.print("Raw value: ");
                        for (int k = 0; k < characteristic.valueLength(); k++) {
                            Serial.print("0x");
                            if (characteristic.value()[k] < 16) Serial.print("0");
                            Serial.print(characteristic.value()[k], HEX);
                            Serial.print(" ");
                        }
                        Serial.println();
                        
                        // Try to interpret as different data types
                        if (characteristic.valueLength() == 1) {
                            byte byteValue = characteristic.value()[0];
                            Serial.print("As byte: ");
                            Serial.print(byteValue);
                            Serial.print(" (binary: ");
                            Serial.print(byteValue, BIN);
                            Serial.println(")");
                        }
                    }
                    
                    Serial.println("---");
                }
            }
        }
        
        delay(10); // Small delay to prevent overwhelming the serial output
    }
    
    Serial.println("Device disconnected!");
    isConnected = false;
} 