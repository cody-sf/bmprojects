#include <Arduino.h>
#include <ArduinoBLE.h>

// BLE UUIDs (must match PlantMonitor)
#define PLANTMONITOR_SERVICE_UUID "f8e3a1b2-c4d5-6e7f-8a9b-0c1d2e3f4a5b"
#define PLANTMONITOR_FEATURES_UUID "a1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d"

// Pump control
#define PUMP_PWM_PIN 18 // GPIO for MOSFET/relay
#define PWM_CHANNEL 0
#define PWM_FREQ 5000
#define PWM_RES 8

// BLE service for threshold and pump status
#define PUMP_SERVICE_UUID "b1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d"
#define THRESHOLD_CHAR_UUID "c1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d"
#define PUMPSTATUS_CHAR_UUID "d1b2c3d4-e5f6-7a8b-9c0d-1e2f3a4b5c6d"

BLEService pumpService(PUMP_SERVICE_UUID);
BLEFloatCharacteristic thresholdChar(THRESHOLD_CHAR_UUID, BLERead | BLEWrite);
BLEByteCharacteristic pumpStatusChar(PUMPSTATUS_CHAR_UUID, BLERead | BLENotify);

float moistureThreshold = 30.0; // Default threshold (%)
bool pumpOn = false;
int monitoredSensor = 0; // Index of sensor to monitor (can be made configurable)

BLEDevice plantPeripheral;
BLECharacteristic plantFeaturesChar;

void setPump(bool on)
{
    if (on)
    {
        ledcWrite(PWM_CHANNEL, 255); // Full power
        pumpOn = true;
    }
    else
    {
        ledcWrite(PWM_CHANNEL, 0);
        pumpOn = false;
    }
    pumpStatusChar.writeValue(pumpOn ? 1 : 0);
}

void thresholdWritten(BLEDevice central, BLECharacteristic characteristic)
{
    moistureThreshold = thresholdChar.value();
    Serial.print("Threshold set to: ");
    Serial.println(moistureThreshold);
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("\nPump Controller starting up...");

    // Set up PWM for pump
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);
    ledcAttachPin(PUMP_PWM_PIN, PWM_CHANNEL);
    setPump(false);

    // Set up BLE Peripheral (for threshold and pump status)
    if (!BLE.begin())
    {
        Serial.println("Starting BLE failed!");
        while (1)
            ;
    }
    BLE.setLocalName("PumpController-CL");
    BLE.setAdvertisedService(pumpService);
    pumpService.addCharacteristic(thresholdChar);
    pumpService.addCharacteristic(pumpStatusChar);
    BLE.addService(pumpService);
    thresholdChar.writeValue(moistureThreshold);
    pumpStatusChar.writeValue(0);
    thresholdChar.setEventHandler(BLEWritten, thresholdWritten);
    BLE.advertise();
    Serial.println("BLE Pump Controller Ready!");

    // Set up BLE Central (to connect to PlantMonitor)
    BLE.scanForUuid(PLANTMONITOR_SERVICE_UUID);
}

void loop()
{
    BLEDevice foundDevice = BLE.available();
    if (!plantPeripheral && foundDevice)
    {
        if (foundDevice.hasServiceUuid(PLANTMONITOR_SERVICE_UUID))
        {
            Serial.print("Connecting to PlantMonitor: ");
            Serial.println(foundDevice.address());
            if (foundDevice.connect())
            {
                plantPeripheral = foundDevice;
                plantFeaturesChar = plantPeripheral.characteristic(PLANTMONITOR_FEATURES_UUID);
                if (plantFeaturesChar)
                {
                    plantFeaturesChar.subscribe();
                    Serial.println("Subscribed to PlantMonitor features characteristic.");
                }
            }
        }
    }

    // Handle BLE events
    BLE.poll();

    // If connected, check for notifications
    if (plantPeripheral && plantFeaturesChar && plantFeaturesChar.valueUpdated())
    {
        String json = plantFeaturesChar.value();
        Serial.print("Received sensor data: ");
        Serial.println(json);
        // Parse JSON for monitored sensor
        float avgMoisture = -1;
        int numSensors = 0;
        DynamicJsonDocument doc(1024);
        DeserializationError err = deserializeJson(doc, json);
        if (!err && doc["sensors"])
        {
            JsonArray arr = doc["sensors"].as<JsonArray>();
            numSensors = arr.size();
            if (monitoredSensor < numSensors)
            {
                avgMoisture = arr[monitoredSensor]["moisture"];
                Serial.print("Sensor ");
                Serial.print(monitoredSensor);
                Serial.print(" moisture: ");
                Serial.println(avgMoisture);
            }
        }
        if (avgMoisture >= 0)
        {
            if (avgMoisture < moistureThreshold)
            {
                setPump(true);
            }
            else
            {
                setPump(false);
            }
        }
    }

    delay(100);
}
