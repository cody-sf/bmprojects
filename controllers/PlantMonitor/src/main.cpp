#include <Arduino.h>
#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include <DeviceRoles.h>

static Device plantmonitor;

// Pin definitions
#define SOIL_MOISTURE_PIN 34 // Analog pin for soil moisture sensor

// BLE Service and Characteristic UUIDs
#define SERVICE_UUID "f8e3a1b2-c4d5-6e7f-8a9b-0c1d2e3f4a5b"
#define FEATURES_UUID "a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d"

// BLE Service and Characteristic
BLEService plantMonitorService(SERVICE_UUID);
BLECharacteristic featuresCharacteristic(FEATURES_UUID, BLERead | BLEWrite | BLENotify, 512);

// Variables
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastBluetoothSync = 0;
int soilMoistureValue = 0;
int soilMoisturePercentage = 0;
int averageValue = 0;      // Added global variable
int readingsToAverage = 0; // Added global variable

// Rolling average variables
const int NUM_READINGS = 20;
const int READING_INTERVAL = 3000; // 3 seconds
int readings[NUM_READINGS] = {0};
int readingIndex = 0;
unsigned long lastReadingTime = 0;
bool hasFullMinuteData = false; // Track if we have a full minute of readings

// Calibration values
const int DRY_VALUE = 2700; // Value when sensor is dry in dirt
const int WET_VALUE = 1450; // Value when sensor is in water

void sendStatusUpdate()
{
    StaticJsonDocument<512> doc;
    doc["raw"] = soilMoistureValue;
    doc["average_raw"] = averageValue;
    doc["readings_used"] = readingsToAverage;
    doc["moisture"] = soilMoisturePercentage;
    String output;
    serializeJson(doc, output);
    output += "\n"; // Add newline

    Serial.print("Status: ");
    Serial.print(output.c_str());

    featuresCharacteristic.setValue(output.c_str());
}

void sendRawUpdate()
{
    StaticJsonDocument<512> doc;
    doc["raw"] = soilMoistureValue;
    String output;
    serializeJson(doc, output);

    Serial.print("Sending raw status: ");
    Serial.println(output.c_str());

    featuresCharacteristic.setValue(output.c_str());
}

void featureCallback(BLEDevice central, BLECharacteristic characteristic)
{
    Serial.println("Feature callback triggered");
    const uint8_t *buffer = featuresCharacteristic.value();
    unsigned int dataLength = featuresCharacteristic.valueLength();
    std::string value((char *)buffer, dataLength);

    if (!value.empty())
    {
        uint8_t feature = value[0];
        Serial.print("Feature requested: 0x");
        Serial.println(feature, HEX);

        // Handle features
        switch (feature)
        {
        case 0x01: // Moisture reading
            Serial.println("Moisture reading requested");
            sendStatusUpdate();
            break;
        case 0x02: // Raw reading
            Serial.println("Raw reading requested");
            sendRawUpdate();
            break;
        default:
            Serial.print("Unknown feature: 0x");
            Serial.println(feature, HEX);
            break;
        }
    }
}

void blePeripheralConnectHandler(BLEDevice central)
{
    Serial.print("Connected event, central: ");
    Serial.println(central.address());
    deviceConnected = true;
}

void blePeripheralDisconnectHandler(BLEDevice central)
{
    Serial.print("Disconnected event, central: ");
    Serial.println(central.address());
    deviceConnected = false;
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("\nPlant Monitor starting up...");

    // Initialize BLE
    if (!BLE.begin())
    {
        Serial.println("Starting Bluetooth® Low Energy module failed!");
        while (1)
            ;
    }

    Serial.println("BLE initialized successfully");

    // Set up BLE
    Serial.println("Setting up BLE...");
    Serial.print("Setting local name to: ");
    Serial.println("PlantMonitor-CL");
    BLE.setLocalName("PlantMonitor-CL");

    Serial.print("Setting advertised service UUID: ");
    Serial.println(SERVICE_UUID);
    BLE.setAdvertisedService(plantMonitorService);

    Serial.print("Adding characteristic UUID: ");
    Serial.println(FEATURES_UUID);
    plantMonitorService.addCharacteristic(featuresCharacteristic);

    Serial.println("Adding service to BLE...");
    BLE.addService(plantMonitorService);
    Serial.println("Service added successfully");

    // Assign event handlers
    Serial.println("Setting up event handlers...");
    BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
    BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
    featuresCharacteristic.setEventHandler(BLEWritten, featureCallback);
    Serial.println("Event handlers set up");

    // Start advertising
    Serial.println("Starting advertising...");
    BLE.advertise();
    Serial.println("BLE Plant Monitor Ready!");
    Serial.print("Service UUID: ");
    Serial.println(SERVICE_UUID);
    Serial.print("Feature UUID: ");
    Serial.println(FEATURES_UUID);
    Serial.println("Advertising started");
}

void loop()
{
    BLE.poll();

    // Take a reading every 3 seconds
    if (millis() - lastReadingTime >= READING_INTERVAL)
    {
        // Read soil moisture sensor
        soilMoistureValue = analogRead(SOIL_MOISTURE_PIN);

        // Store the reading in our array
        readings[readingIndex] = soilMoistureValue;

        // Calculate how many readings we have
        readingsToAverage = hasFullMinuteData ? NUM_READINGS : (readingIndex + 1);

        // Calculate average of all readings
        long sum = 0L;
        for (int i = 0; i < readingsToAverage; i++)
        {
            sum += static_cast<long>(readings[i]);
        }
        averageValue = readingsToAverage > 0 ? static_cast<int>(sum / readingsToAverage) : soilMoistureValue;

        // Move to next position in array
        readingIndex = (readingIndex + 1) % NUM_READINGS;

        // Check if we've filled the array once
        if (readingIndex == 0)
        {
            hasFullMinuteData = true;
        }

        // Convert average to percentage
        soilMoisturePercentage = map(averageValue, WET_VALUE, DRY_VALUE, 100, 0);
        soilMoisturePercentage = constrain(soilMoisturePercentage, 0, 100);

        // Print values for debugging
        Serial.print("Raw Value: ");
        Serial.print(soilMoistureValue);
        Serial.print(" | Average Raw: ");
        Serial.print(averageValue);
        Serial.print(" | Readings used: ");
        Serial.print(readingsToAverage);
        Serial.print(" | Moisture: ");
        Serial.print(soilMoisturePercentage);
        Serial.println("%");

        // Send updates if connected
        if (deviceConnected && (millis() - lastBluetoothSync) > 1000)
        {
            sendStatusUpdate();
            lastBluetoothSync = millis();
        }

        lastReadingTime = millis();
    }

    delay(100); // Small delay to prevent CPU overload
}