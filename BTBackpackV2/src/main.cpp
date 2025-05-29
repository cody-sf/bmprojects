#include <Arduino.h>
#include <FastLED.h>
#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <math.h>
#include <ctype.h>
#include "LightShow.h"
#include "Clock.h"
#include "Position.h"
// LED Configuration
#define LED_OUTPUT_PIN 27
#define NUM_LEDS 192
#define COLOR_ORDER GRB

// Button Configuration
#define BUTTON_PIN 2

// GPS Configuration
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define GPS_BAUD 9600

// BLE Configuration
#define SERVICE_UUID "be03096f-9322-4360-bc84-0f977c5c3c10"
#define FEATURES_UUID "24dcb43c-d457-4de0-a968-9cdc9d60392c"
#define STATUS_UUID "71a0cb09-7998-4774-83b5-1a5f02f205fb"

BLEService backpackService(SERVICE_UUID);
BLECharacteristic featuresCharacteristic(FEATURES_UUID, BLERead | BLEWrite | BLENotify, 512);
BLECharacteristic statusCharacteristic(STATUS_UUID, BLERead | BLENotify, 512);

// Connection State
bool deviceConnected = false;
unsigned long lastBluetoothSync = 0;
#define DEFAULT_BT_REFRESH_INTERVAL 5000

// LED Setup
CRGB leds[NUM_LEDS];
CLEDController* controller;
Clock backpackClock;
LightShow lightShow(std::vector<CLEDController*>(), backpackClock);

// GPS Setup
TinyGPSPlus gps;
HardwareSerial GPSSerial(2);


// Device State Variables
bool power = true;
uint16_t speed = 100;
int brightness = 50;
bool reverseStrip = true;
Position origin(40.7868, -119.2065); // Default BRC location
Position currentPosition;
bool positionAvailable = false;
float currentSpeed = 0;


// Add effect and palette selection
AvailablePalettes currentPalette = AvailablePalettes::cool;
LightSceneID currentEffect = LightSceneID::palette_stream;

// Effect parameters
int waveWidth = 10;
int meteorCount = 5;
int trailLength = 10;
int heatVariance = 50;
int mirrorCount = 4;
int cometCount = 3;
int dropRate = 25;
int cloudScale = 20;
int blobCount = 8;
int waveCount = 6;
int flashIntensity = 80;
int flashFrequency = 1000;
int explosionSize = 20;
int spiralArms = 4;
CRGB effectColor = CRGB::Green;

// Function Declarations
void sendStatusUpdate();
void setEffect(const char* effectName);
void setEffect(uint8_t effectId);
void featureCallback(BLEDevice central, BLECharacteristic characteristic);
void blePeripheralConnectHandler(BLEDevice central);
void blePeripheralDisconnectHandler(BLEDevice central);
void updateGPS();
void updateLightShow();

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("BTBackpackV2 Starting...");
    
    // Initialize LEDs
    controller = &FastLED.addLeds<WS2812B, LED_OUTPUT_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    
    // Add controller to LightShow
    lightShow.add_led_controller(controller);
    lightShow.brightness(brightness * 2.55);
    
    // Initialize GPS
    GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    
    // Initialize BLE
    if (!BLE.begin()) {
        Serial.println("Starting Bluetooth® Low Energy module failed!");
        while (1);
    }
    
    // Configure BLE
    BLE.setLocalName("Backpack-CL");
    BLE.setAdvertisedService(backpackService);
    backpackService.addCharacteristic(featuresCharacteristic);
    backpackService.addCharacteristic(statusCharacteristic);
    BLE.addService(backpackService);
    
    // Set BLE event handlers
    BLE.setEventHandler(BLEConnected, blePeripheralConnectHandler);
    BLE.setEventHandler(BLEDisconnected, blePeripheralDisconnectHandler);
    featuresCharacteristic.setEventHandler(BLEWritten, featureCallback);
    
    // Start advertising
    BLE.advertise();
    Serial.println("BLE advertising started");
    
    Serial.println("Setup complete");
    updateLightShow();
}

void loop() {
    BLE.poll();
    updateGPS();

    if ((millis() - lastBluetoothSync) > DEFAULT_BT_REFRESH_INTERVAL && deviceConnected) {
        sendStatusUpdate();
        lastBluetoothSync = millis();
    }
    
    if (!power) {
        FastLED.clear();
        FastLED.show();
        return;
    }
    
    // lightShow.brightness(brightness * 2.55);
    lightShow.render(); 
}

void updateGPS() {
    while (GPSSerial.available() > 0) {
        if (gps.encode(GPSSerial.read())) {
            if (gps.location.isValid()) {
                currentPosition = Position(gps.location.lat(), gps.location.lng());
                positionAvailable = true;
                
                if (gps.speed.isValid()) {
                    currentSpeed = gps.speed.mph();
                }
            }
        }
    }
}

void updateLightShow() {
    // Map LightSceneID to LightShow effect
    switch (currentEffect) {
        case LightSceneID::palette_stream:
            lightShow.palette_stream(speed, currentPalette, reverseStrip);
            break;
        case LightSceneID::pulse_wave:
            lightShow.pulse_wave(speed, waveWidth, currentPalette);
            break;
        case LightSceneID::meteor_shower:
            lightShow.meteor_shower(speed, meteorCount, trailLength, currentPalette);
            break;
        case LightSceneID::fire_plasma:
            lightShow.fire_plasma(speed, heatVariance, currentPalette);
            break;
        case LightSceneID::kaleidoscope:
            lightShow.kaleidoscope(speed, mirrorCount, currentPalette);
            break;
        case LightSceneID::rainbow_comet:
            lightShow.rainbow_comet(speed, cometCount, trailLength);
            break;
        case LightSceneID::matrix_rain:
            lightShow.matrix_rain(speed, dropRate, effectColor);
            break;
        case LightSceneID::plasma_clouds:
            lightShow.plasma_clouds(speed, cloudScale, currentPalette);
            break;
        case LightSceneID::lava_lamp:
            lightShow.lava_lamp(speed, blobCount, currentPalette);
            break;
        case LightSceneID::aurora_borealis:
            lightShow.aurora_borealis(speed, waveCount, currentPalette);
            break;
        case LightSceneID::lightning_storm:
            lightShow.lightning_storm(speed, flashIntensity, flashFrequency);
            break;
        case LightSceneID::color_explosion:
            lightShow.color_explosion(speed, explosionSize, currentPalette);
            break;
        case LightSceneID::spiral_galaxy:
            lightShow.spiral_galaxy(speed, spiralArms, currentPalette);
            break;
        default:
            lightShow.palette_stream(speed, currentPalette, reverseStrip);
            break;
    }
}

void sendStatusUpdate() {
    StaticJsonDocument<256> doc;
    doc["pwr"] = power;
    doc["fx"] = LightShow::effectIdToName(currentEffect);
    doc["fxId"] = (uint8_t)currentEffect;
    doc["pal"] = LightShow::paletteIdToName(currentPalette);
    doc["palId"] = (uint8_t)currentPalette;
    doc["bri"] = brightness;
    doc["spd"] = speed;
    doc["dir"] = reverseStrip;
    doc["posAvail"] = positionAvailable;
    doc["spdCur"] = currentSpeed;
    JsonObject posObj = doc.createNestedObject("pos");
    posObj["lat"] = currentPosition.latitude();
    posObj["lon"] = currentPosition.longitude();
    String output;
    serializeJson(doc, output);
    Serial.print("Sending status update: ");
    Serial.println(output);
    statusCharacteristic.setValue(output.c_str());
}

void setEffect(const char* effectName) {
    Serial.print("[setEffect] Setting effect to: ");
    Serial.println(effectName);
    
    // Try to parse as an effect name directly
    LightSceneID parsedEffect = LightShow::effectNameToId(effectName);
    currentEffect = parsedEffect;
    
    Serial.print("[setEffect] Effect set to ID: ");
    Serial.println((uint8_t)currentEffect);
    Serial.print("[setEffect] Effect name: ");
    Serial.println(LightShow::effectIdToName(currentEffect));
}

void setEffect(uint8_t effectId) {
    Serial.print("[setEffect] Setting effect to ID: ");
    Serial.println(effectId);
    
    if (effectId <= (uint8_t)LightSceneID::spiral_galaxy) {
        currentEffect = (LightSceneID)effectId;
        Serial.print("[setEffect] Effect set to: ");
        Serial.println(LightShow::effectIdToName(currentEffect));
    } else {
        Serial.println("[setEffect] Effect ID out of range, defaulting to palette_stream");
        currentEffect = LightSceneID::palette_stream;
    }
}

void featureCallback(BLEDevice central, BLECharacteristic characteristic) {
    const uint8_t *buffer = featuresCharacteristic.value();
    unsigned int dataLength = featuresCharacteristic.valueLength();
    
    Serial.print("[featureCallback] Data length: ");
    Serial.println(dataLength);
    Serial.print("[featureCallback] Raw buffer: ");
    for (unsigned int i = 0; i < dataLength; ++i) {
        Serial.print("0x"); Serial.print(buffer[i], HEX); Serial.print(" ");
    }
    Serial.println();
    
    if (dataLength > 0) {
        uint8_t feature = buffer[0];
        Serial.print("[featureCallback] Received feature: 0x");
        Serial.println(feature, HEX);
        
        switch (feature) {
            case 0x01: // Power
                power = buffer[1] != 0;
                Serial.print("[featureCallback] Power set to: ");
                Serial.println(power ? "On" : "Off");
                // Don't send status update here - let the regular interval handle it
                break;
                
            case 0x04: // Brightness (following constants)
                if (dataLength >= 5) {
                    int b = 0;
                    memcpy(&b, buffer + 1, sizeof(int));
                    Serial.print("[featureCallback] Brightness value received: ");
                    Serial.println(b);
                    brightness = constrain(b, 1, 100);
                    Serial.print("[featureCallback] Brightness set to: ");
                    Serial.println(brightness);
                    lightShow.brightness(brightness * 2.55);
                } else {
                    Serial.println("[featureCallback] Brightness data too short!");
                }
                break;
                
            case 0x05: // Speed (following constants)
                {
                    int s = 0;
                    memcpy(&s, buffer + 1, sizeof(int));
                    Serial.print("[featureCallback] Speed value received: ");
                    Serial.println(s);
                    speed = constrain(s, 5, 200);
                    Serial.print("[featureCallback] Speed set to: ");
                    Serial.println(speed);
                    updateLightShow();
                }
                break;
                
            case 0x06: // Direction (following constants)
                reverseStrip = buffer[1] != 0;
                Serial.print("[featureCallback] Direction set to: ");
                Serial.println(reverseStrip ? "Up" : "Down");
                // Don't send status update here
                updateLightShow();
                break;
                
            case 0x07: // Origin (following constants - note this conflicts with effect in constants!)
                if (dataLength == 9) {
                    float latitude, longitude;
                    memcpy(&latitude, buffer + 1, sizeof(float));
                    memcpy(&longitude, buffer + 5, sizeof(float));
                    Serial.print("[featureCallback] New origin received - Lat: ");
                    Serial.print(latitude, 6);
                    Serial.print(", Lon: ");
                    Serial.println(longitude, 6);
                    origin = Position(latitude, longitude);
                } else {
                    Serial.print("[featureCallback] Invalid origin data length: ");
                    Serial.println(dataLength);
                }
                break;
                
            case 0x08: // Palette (paletteNew from constants)
                if (dataLength > 1) {
                    // If data length is 2, it's likely an ID (1 byte feature + 1 byte ID)
                    // If data length is > 2, it's likely a string
                    if (dataLength == 2) { // ID
                        uint8_t paletteId = buffer[1];
                        Serial.print("[featureCallback] Palette ID received: ");
                        Serial.println(paletteId);
                        if (paletteId < (uint8_t)AvailablePalettes::moltenmetal + 1) {
                            currentPalette = (AvailablePalettes)paletteId;
                            Serial.print("[featureCallback] Palette set to ID: ");
                            Serial.println(paletteId);
                            updateLightShow(); // Apply the palette change
                        } else {
                            Serial.println("[featureCallback] Palette ID out of range!");
                        }
                    } else { // String
                        char paletteStr[32] = {0};
                        memcpy(paletteStr, buffer + 1, min(dataLength - 1, sizeof(paletteStr) - 1));
                        Serial.print("[featureCallback] Palette string received: ");
                        Serial.println(paletteStr);
                        currentPalette = LightShow::paletteNameToId(paletteStr);
                        Serial.print("[featureCallback] Palette set to: ");
                        Serial.println(paletteStr);
                        updateLightShow(); // Apply the palette change
                    }
                } else {
                    Serial.println("[featureCallback] Palette data too short!");
                }
                break;
                
            case 0x09: // Speedometer (following constants)
                if (dataLength >= 7) {
                    // Extract two RGB colors from buffer
                    uint8_t r1 = buffer[1], g1 = buffer[2], b1 = buffer[3];
                    uint8_t r2 = buffer[4], g2 = buffer[5], b2 = buffer[6];
                    Serial.print("[featureCallback] Speedometer colors received: RGB1(");
                    Serial.print(r1); Serial.print(","); Serial.print(g1); Serial.print(","); Serial.print(b1);
                    Serial.print(") RGB2(");
                    Serial.print(r2); Serial.print(","); Serial.print(g2); Serial.print(","); Serial.print(b2);
                    Serial.println(")");
                    // Store speedometer colors for future use
                } else {
                    Serial.println("[featureCallback] Speedometer data too short!");
                }
                break;
                
            case 0x0A: // Effect (new effect feature code)
                if (dataLength > 1) {
                    // If data length is 2, it's likely an ID (1 byte feature + 1 byte ID)
                    // If data length is > 2, it's likely a string
                    if (dataLength == 2) { // ID
                        uint8_t effectId = buffer[1];
                        Serial.print("[featureCallback] Effect ID received: ");
                        Serial.println(effectId);
                        setEffect(effectId);
                        updateLightShow(); // Apply the effect change
                    } else { // String
                        char effectStr[32] = {0};
                        memcpy(effectStr, buffer + 1, min(dataLength - 1, sizeof(effectStr) - 1));
                        Serial.print("[featureCallback] Effect string received: ");
                        Serial.println(effectStr);
                        setEffect(effectStr);
                        updateLightShow(); // Apply the effect change
                    }
                } else {
                    Serial.println("[featureCallback] Effect data too short!");
                }
                break;
                
            // Effect Parameter Cases (new feature codes from constants)
            case 0x0B: // wave_width
                if (dataLength >= 5) {
                    int value = 0;
                    memcpy(&value, buffer + 1, sizeof(int));
                    waveWidth = constrain(value, 1, 50);
                    Serial.print("[featureCallback] Wave width set to: ");
                    Serial.println(waveWidth);
                    updateLightShow();
                }
                break;
                
            case 0x0C: // meteor_count
                if (dataLength >= 5) {
                    int value = 0;
                    memcpy(&value, buffer + 1, sizeof(int));
                    meteorCount = constrain(value, 1, 20);
                    Serial.print("[featureCallback] Meteor count set to: ");
                    Serial.println(meteorCount);
                    updateLightShow();
                }
                break;
                
            case 0x0D: // trail_length
                if (dataLength >= 5) {
                    int value = 0;
                    memcpy(&value, buffer + 1, sizeof(int));
                    trailLength = constrain(value, 1, 30);
                    Serial.print("[featureCallback] Trail length set to: ");
                    Serial.println(trailLength);
                    updateLightShow();
                }
                break;
                
            case 0x0E: // heat_variance
                if (dataLength >= 5) {
                    int value = 0;
                    memcpy(&value, buffer + 1, sizeof(int));
                    heatVariance = constrain(value, 1, 100);
                    Serial.print("[featureCallback] Heat variance set to: ");
                    Serial.println(heatVariance);
                    updateLightShow();
                }
                break;
                
            case 0x0F: // mirror_count
                if (dataLength >= 5) {
                    int value = 0;
                    memcpy(&value, buffer + 1, sizeof(int));
                    mirrorCount = constrain(value, 2, 16);
                    Serial.print("[featureCallback] Mirror count set to: ");
                    Serial.println(mirrorCount);
                    updateLightShow();
                }
                break;
                
            case 0x10: // comet_count
                if (dataLength >= 5) {
                    int value = 0;
                    memcpy(&value, buffer + 1, sizeof(int));
                    cometCount = constrain(value, 1, 10);
                    Serial.print("[featureCallback] Comet count set to: ");
                    Serial.println(cometCount);
                    updateLightShow();
                }
                break;
                
            case 0x11: // drop_rate
                if (dataLength >= 5) {
                    int value = 0;
                    memcpy(&value, buffer + 1, sizeof(int));
                    dropRate = constrain(value, 1, 100);
                    Serial.print("[featureCallback] Drop rate set to: ");
                    Serial.println(dropRate);
                    updateLightShow();
                }
                break;
                
            case 0x12: // cloud_scale
                if (dataLength >= 5) {
                    int value = 0;
                    memcpy(&value, buffer + 1, sizeof(int));
                    cloudScale = constrain(value, 1, 50);
                    Serial.print("[featureCallback] Cloud scale set to: ");
                    Serial.println(cloudScale);
                    updateLightShow();
                }
                break;
                
            case 0x13: // blob_count
                if (dataLength >= 5) {
                    int value = 0;
                    memcpy(&value, buffer + 1, sizeof(int));
                    blobCount = constrain(value, 1, 20);
                    Serial.print("[featureCallback] Blob count set to: ");
                    Serial.println(blobCount);
                    updateLightShow();
                }
                break;
                
            case 0x14: // wave_count
                if (dataLength >= 5) {
                    int value = 0;
                    memcpy(&value, buffer + 1, sizeof(int));
                    waveCount = constrain(value, 1, 15);
                    Serial.print("[featureCallback] Wave count set to: ");
                    Serial.println(waveCount);
                    updateLightShow();
                }
                break;
                
            case 0x15: // flash_intensity
                if (dataLength >= 5) {
                    int value = 0;
                    memcpy(&value, buffer + 1, sizeof(int));
                    flashIntensity = constrain(value, 1, 100);
                    Serial.print("[featureCallback] Flash intensity set to: ");
                    Serial.println(flashIntensity);
                    updateLightShow();
                }
                break;
                
            case 0x16: // flash_frequency
                if (dataLength >= 5) {
                    int value = 0;
                    memcpy(&value, buffer + 1, sizeof(int));
                    flashFrequency = constrain(value, 100, 5000);
                    Serial.print("[featureCallback] Flash frequency set to: ");
                    Serial.println(flashFrequency);
                    updateLightShow();
                }
                break;
                
            case 0x17: // explosion_size
                if (dataLength >= 5) {
                    int value = 0;
                    memcpy(&value, buffer + 1, sizeof(int));
                    explosionSize = constrain(value, 1, 50);
                    Serial.print("[featureCallback] Explosion size set to: ");
                    Serial.println(explosionSize);
                    updateLightShow();
                }
                break;
                
            case 0x18: // spiral_arms
                if (dataLength >= 5) {
                    int value = 0;
                    memcpy(&value, buffer + 1, sizeof(int));
                    spiralArms = constrain(value, 2, 12);
                    Serial.print("[featureCallback] Spiral arms set to: ");
                    Serial.println(spiralArms);
                    updateLightShow();
                }
                break;
                
            case 0x19: // color (for single-color effects)
                if (dataLength >= 4) {
                    uint8_t r = buffer[1], g = buffer[2], b = buffer[3];
                    effectColor = CRGB(r, g, b);
                    Serial.print("[featureCallback] Effect color set to: RGB(");
                    Serial.print(r); Serial.print(","); Serial.print(g); Serial.print(","); Serial.print(b);
                    Serial.println(")");
                    updateLightShow();
                }
                break;
                
            default:
                Serial.print("[featureCallback] Unknown feature: 0x");
                Serial.println(feature, HEX);
                break;
        }
    } else {
        Serial.println("[featureCallback] No data received!");
    }
}

void blePeripheralConnectHandler(BLEDevice central) {
    Serial.print("Connected to central: ");
    Serial.println(central.address());
    deviceConnected = true;
    sendStatusUpdate();
}

void blePeripheralDisconnectHandler(BLEDevice central) {
    Serial.print("Disconnected from central: ");
    Serial.println(central.address());
    deviceConnected = false;
}