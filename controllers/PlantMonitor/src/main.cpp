#include <Arduino.h>
#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include <DeviceRoles.h>
#include <Preferences.h>

static Device plantmonitor;

// Pin definitions
#define NUM_SENSORS 5
const int DEFAULT_SOIL_MOISTURE_PINS[NUM_SENSORS] = {34, 35, 32, 33, 25}; // Example default pins
int soilMoisturePins[NUM_SENSORS];

struct PlantSensor
{
    int rawValue = 0;
    int averageValue = 0;
    int readings[20] = {0};
    int readingIndex = 0;
    bool hasFullMinuteData = false;
    int readingsToAverage = 0;
    int moisturePercent = 0;
    char name[33] = ""; // 32 chars + null terminator
};

PlantSensor sensors[NUM_SENSORS];
Preferences preferences;

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

// Rolling average variables
const int NUM_READINGS = 20;
const int READING_INTERVAL = 3000; // 3 seconds
unsigned long lastReadingTime = 0;

// Calibration values
const int DRY_VALUE = 2700; // Value when sensor is dry in dirt
const int WET_VALUE = 1450; // Value when sensor is in water

void loadSensorNames()
{
    preferences.begin("plantnames", true);
    for (int i = 0; i < NUM_SENSORS; i++)
    {
        String key = String("name") + i;
        String n = preferences.getString(key.c_str(), "Plant " + String(i + 1));
        n.toCharArray(sensors[i].name, 33);
    }
    preferences.end();
}

void saveSensorName(int idx, const char *name)
{
    preferences.begin("plantnames", false);
    String key = String("name") + idx;
    preferences.putString(key.c_str(), name);
    preferences.end();
}

void loadSensorPins()
{
    preferences.begin("plantpins", true);
    for (int i = 0; i < NUM_SENSORS; i++)
    {
        String key = String("pin") + i;
        soilMoisturePins[i] = preferences.getInt(key.c_str(), DEFAULT_SOIL_MOISTURE_PINS[i]);
    }
    preferences.end();
}

void saveSensorPin(int idx, int pin)
{
    preferences.begin("plantpins", false);
    String key = String("pin") + idx;
    preferences.putInt(key.c_str(), pin);
    preferences.end();
    soilMoisturePins[idx] = pin;
}

// Helper: Check if a pin is a valid ADC pin for ESP32
bool isValidADCPin(int pin)
{
    // ADC1: 32-39, ADC2: 0, 2, 4, 12-15, 25-27
    return (pin >= 32 && pin <= 39) || (pin == 0) || (pin == 2) || (pin == 4) || (pin >= 12 && pin <= 15) || (pin >= 25 && pin <= 27);
}

void sendStatusUpdate()
{
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.createNestedArray("sensors");
    for (int i = 0; i < NUM_SENSORS; i++)
    {
        JsonObject obj = arr.createNestedObject();
        obj["raw"] = sensors[i].rawValue;
        obj["average_raw"] = sensors[i].averageValue;
        obj["readings_used"] = sensors[i].readingsToAverage;
        obj["moisture"] = sensors[i].moisturePercent;
        obj["name"] = sensors[i].name;
    }
    String output;
    serializeJson(doc, output);
    output.concat("\n"); // Add newline

    Serial.print("Status: ");
    Serial.print(output.c_str());

    featuresCharacteristic.setValue(output.c_str());
}

void sendRawUpdate()
{
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.createNestedArray("sensors");
    for (int i = 0; i < NUM_SENSORS; i++)
    {
        JsonObject obj = arr.createNestedObject();
        obj["raw"] = sensors[i].rawValue;
    }
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
        case 0x10: // Set sensor name
            if (dataLength >= 3)
            {
                uint8_t sensorIdx = value[1];
                if (sensorIdx < NUM_SENSORS)
                {
                    std::string newName = value.substr(2);
                    strncpy(sensors[sensorIdx].name, newName.c_str(), 32);
                    sensors[sensorIdx].name[32] = '\0';
                    saveSensorName(sensorIdx, sensors[sensorIdx].name);
                    Serial.print("Sensor ");
                    Serial.print(sensorIdx);
                    Serial.print(" name set to: ");
                    Serial.println(sensors[sensorIdx].name);
                    sendStatusUpdate();
                }
            }
            break;
        case 0x20: // Set sensor pin
            if (dataLength >= 3)
            {
                uint8_t sensorIdx = value[1];
                uint8_t pin = value[2];
                if (sensorIdx < NUM_SENSORS && isValidADCPin(pin))
                {
                    saveSensorPin(sensorIdx, pin);
                    Serial.print("Sensor ");
                    Serial.print(sensorIdx);
                    Serial.print(" pin set to: ");
                    Serial.println(pin);
                    sendStatusUpdate();
                }
                else
                {
                    Serial.println("Invalid sensor index or pin for ADC");
                }
            }
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
    loadSensorNames();
    loadSensorPins();

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
        for (int i = 0; i < NUM_SENSORS; i++)
        {
            int value = analogRead(soilMoisturePins[i]);
            PlantSensor &sensor = sensors[i];
            sensor.rawValue = value;
            sensor.readings[sensor.readingIndex] = value;
            sensor.readingsToAverage = sensor.hasFullMinuteData ? NUM_READINGS : (sensor.readingIndex + 1);

            long sum = 0L;
            for (int j = 0; j < sensor.readingsToAverage; j++)
            {
                sum += static_cast<long>(sensor.readings[j]);
            }
            sensor.averageValue = sensor.readingsToAverage > 0 ? static_cast<int>(sum / sensor.readingsToAverage) : value;
            sensor.readingIndex = (sensor.readingIndex + 1) % NUM_READINGS;
            if (sensor.readingIndex == 0)
            {
                sensor.hasFullMinuteData = true;
            }
            sensor.moisturePercent = map(sensor.averageValue, WET_VALUE, DRY_VALUE, 100, 0);
            sensor.moisturePercent = constrain(sensor.moisturePercent, 0, 100);
        }

        // Print values for debugging
        for (int i = 0; i < NUM_SENSORS; i++)
        {
            Serial.print("Sensor ");
            Serial.print(i);
            Serial.print(" | Pin: ");
            Serial.print(soilMoisturePins[i]);
            Serial.print(" | Raw Value: ");
            Serial.print(sensors[i].rawValue);
            Serial.print(" | Average Raw: ");
            Serial.print(sensors[i].averageValue);
            Serial.print(" | Readings used: ");
            Serial.print(sensors[i].readingsToAverage);
            Serial.print(" | Moisture: ");
            Serial.print(sensors[i].moisturePercent);
            Serial.println("%");
        }

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