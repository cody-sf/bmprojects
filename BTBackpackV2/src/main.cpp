#include <Arduino.h>
#include <FastLED.h>
#include <ArduinoBLE.h>
#include <ArduinoJson.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <math.h>
#include <ctype.h>

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

// GPS Setup
TinyGPSPlus gps;
HardwareSerial GPSSerial(2);

// Position class
class Position {
public:
    Position(float lat = 0, float lon = 0) : latitude_(lat), longitude_(lon) {}
    float latitude() { return latitude_; }
    float longitude() { return longitude_; }
    
    float distance_from(Position &origin) {
        float lat1 = degrees_to_radians(latitude_);
        float lon1 = degrees_to_radians(longitude_);
        float lat2 = degrees_to_radians(origin.latitude());
        float lon2 = degrees_to_radians(origin.longitude());
        
        float dlat = lat2 - lat1;
        float dlon = lon2 - lon1;
        
        float a = sin(dlat/2) * sin(dlat/2) + cos(lat1) * cos(lat2) * sin(dlon/2) * sin(dlon/2);
        float c = 2 * atan2(sqrt(a), sqrt(1-a));
        
        return 6372800 * c; // Earth radius in meters
    }
    
    Position operator-(Position &that) {
        return Position(latitude_ - that.latitude(), longitude_ - that.longitude());
    }
    
    float polar_angle() {
        return atan2(longitude_, latitude_);
    }
    
    void toJson(JsonObject &json) const {
        json["latitude"] = latitude_;
        json["longitude"] = longitude_;
    }
    
private:
    float degrees_to_radians(float angle) { return M_PI * angle / 180.0; }
    float latitude_;
    float longitude_;
};

// Light modes
enum LightMode {
    PALETTE_STREAM,
    PALETTE_CYCLE,
    SPEEDOMETER,
    COLOR_WHEEL,
    COLOR_RADIAL,
    POSITION_STATUS
};

// Palettes
enum PaletteType {
    COOL,
    EARTH,
    EVERGLOW,
    HEART,
    LAVA,
    MELONBALL,
    PINKSPLASH,
    R,
    SOFIA,
    SUNSET,
    TROVE,
    VELVET,
    VGA,
    CANDY,
    COSMICWAVES,
    EBLOSSOM,
    EMERALD,
    FATBOY,
    FIREICE,
    FIREYNIGHT,
    FLAME,
    MEADOW,
    NEBULA,
    OASIS,
    PALETTE_COUNT
};

// Device State Variables
LightMode currentMode = PALETTE_STREAM;
bool power = true;
PaletteType currentPalette = COOL;
uint16_t speed = 100;
int brightness = 10;
bool reverseStrip = true;
CRGB speedoColor1 = CRGB::Red;
CRGB speedoColor2 = CRGB::Blue;
Position origin(40.7868, -119.2065); // Default BRC location
Position currentPosition;
bool positionAvailable = false;
float currentSpeed = 0;

// Button State
#define DEBOUNCE_DELAY 50
int lastState = LOW;
int currentState;
unsigned long pressedTime = 0;
unsigned long releasedTime = 0;
const int SHORT_PRESS_TIME = 1000;
const int OFF_TIME = 3000;
bool buttonInitialized = false;

// Animation variables
unsigned long lastUpdate = 0;
uint8_t paletteIndex = 0;
uint8_t cycleHue = 0;

// Function Declarations
void sendStatusUpdate();
void setMode(const char* mode);
void featureCallback(BLEDevice central, BLECharacteristic characteristic);
void blePeripheralConnectHandler(BLEDevice central);
void blePeripheralDisconnectHandler(BLEDevice central);
void handleButton();
void updateGPS();
void updateLightShow();
const char* paletteToString(PaletteType palette);
PaletteType stringToPalette(const char* str);
CRGBPalette16 getPalette(PaletteType palette);

// Define gradient palettes for complex palettes
DEFINE_GRADIENT_PALETTE(earth_gp) {
    0,   0x49, 0x9C, 0x01,
    64,  0x25, 0x8A, 0x0B,
    128, 0x0E, 0x77, 0x2A,
    192, 0x03, 0x67, 0x67,
    255, 0x01, 0x57, 0xC5
};

DEFINE_GRADIENT_PALETTE(everglow_gp) {
    0,   0xA7, 0xF4, 0x59,
    64,  0x5C, 0xB6, 0x1A,
    128, 0x42, 0x56, 0x0E,
    192, 0x2E, 0x10, 0x0D,
    255, 0x22, 0x05, 0x09
};

DEFINE_GRADIENT_PALETTE(heart_gp) {
    0,   0x23, 0x49, 0x47,
    64,  0x2E, 0x29, 0x25,
    128, 0x3B, 0x12, 0x0F,
    192, 0x2E, 0x04, 0x09,
    255, 0x17, 0x06, 0x07
};

DEFINE_GRADIENT_PALETTE(melonball_gp) {
    0,   0x98, 0xE3, 0x55,
    64,  0xD7, 0xF4, 0x6A,
    128, 0xFF, 0x8E, 0x38,
    192, 0xFF, 0x5A, 0x2D,
    255, 0xE5, 0x24, 0x3E
};

DEFINE_GRADIENT_PALETTE(pinksplash_gp) {
    0,   0x8E, 0x01, 0x10,
    43,  0xE0, 0x01, 0x1B,
    86,  0xC7, 0x55, 0x55,
    128, 0xFC, 0xAD, 0xA4,
    170, 0xC7, 0x55, 0x55,
    213, 0xE0, 0x01, 0x1B,
    255, 0x8E, 0x01, 0x10
};

DEFINE_GRADIENT_PALETTE(sofia_gp) {
    0,   0x00, 0x5A, 0x78,
    64,  0x05, 0x39, 0x39,
    128, 0x23, 0x21, 0x12,
    192, 0x6B, 0x55, 0x03,
    255, 0xEA, 0xA2, 0x00
};

DEFINE_GRADIENT_PALETTE(sunset_gp) {
    0,   0x78, 0x00, 0x00,
    36,  0xB3, 0x16, 0x00,
    73,  0xFF, 0x68, 0x00,
    109, 0xA7, 0x16, 0x12,
    146, 0x64, 0x00, 0x67,
    182, 0x10, 0x00, 0x82,
    255, 0x00, 0x00, 0xA0
};

DEFINE_GRADIENT_PALETTE(trove_gp) {
    0,   0x0C, 0x17, 0x0B,
    25,  0x08, 0x34, 0x1B,
    51,  0x20, 0x8E, 0x40,
    76,  0x37, 0x44, 0x18,
    102, 0xBE, 0x87, 0x2D,
    127, 0xC9, 0xAF, 0x3B,
    153, 0xBA, 0x50, 0x14,
    178, 0xDC, 0x4F, 0x20,
    204, 0xB8, 0x21, 0x0E,
    229, 0x89, 0x10, 0x0F,
    255, 0x3A, 0x1F, 0x6D
};

DEFINE_GRADIENT_PALETTE(velvet_gp) {
    0,   0x01, 0x4F, 0x50,
    64,  0x01, 0x3E, 0x3E,
    128, 0x01, 0x2B, 0x28,
    192, 0x01, 0x1B, 0x17,
    255, 0x01, 0x0D, 0x0A
};

DEFINE_GRADIENT_PALETTE(vga_gp) {
    0,   0xFF, 0xFF, 0xFF,
    16,  0x78, 0x85, 0x7B,
    32,  0xFF, 0x00, 0x00,
    48,  0xFF, 0xFF, 0x00,
    64,  0x00, 0xFF, 0x00,
    80,  0x00, 0xFF, 0xFF,
    96,  0x00, 0x00, 0xFF,
    112, 0xFF, 0x00, 0xFF,
    128, 0x00, 0x00, 0x00,
    144, 0x29, 0x37, 0x2C,
    160, 0x29, 0x00, 0x00,
    176, 0x29, 0x37, 0x00,
    192, 0x00, 0x37, 0x00,
    208, 0x00, 0x37, 0x2C,
    224, 0x00, 0x00, 0x2C,
    255, 0x29, 0x00, 0x2C
};

DEFINE_GRADIENT_PALETTE(candy_gp) {
    0,   0xFF, 0xA6, 0x1A,
    51,  0x32, 0x1A, 0xFF,
    102, 0xFF, 0x1A, 0xDD,
    153, 0x1A, 0x34, 0xFF,
    204, 0xFF, 0xA6, 0x1A,
    255, 0xFF, 0xA6, 0x1A
};

DEFINE_GRADIENT_PALETTE(cosmicwaves_gp) {
    0,   0xFF, 0x1A, 0xF8,
    51,  0x32, 0x1A, 0xFF,
    102, 0x1A, 0xFF, 0xB1,
    153, 0x1A, 0x34, 0xFF,
    204, 0xFF, 0x1A, 0xF8,
    255, 0xFF, 0x1A, 0xF8
};

DEFINE_GRADIENT_PALETTE(eblossom_gp) {
    0,   0xFF, 0x1A, 0xF8,
    64,  0x2A, 0xFF, 0x13,
    128, 0xFF, 0x1A, 0xF8,
    192, 0x2A, 0xFF, 0x13,
    255, 0xFF, 0x1A, 0xF8
};

DEFINE_GRADIENT_PALETTE(emerald_gp) {
    0,   0x02, 0x73, 0x01,
    51,  0x03, 0xD2, 0x2C,
    102, 0x06, 0x57, 0x00,
    153, 0x29, 0x66, 0x23,
    204, 0x00, 0xEC, 0xAA,
    255, 0x00, 0xEC, 0xAA
};

DEFINE_GRADIENT_PALETTE(fatboy_gp) {
    0,   0xD7, 0x4A, 0x06,
    28,  0x4A, 0x16, 0x35,
    56,  0x12, 0x06, 0x1B,
    85,  0x05, 0x24, 0x55,
    113, 0x01, 0x08, 0x1E,
    142, 0x01, 0x08, 0x1E,
    170, 0x05, 0x24, 0x55,
    198, 0x12, 0x06, 0x1B,
    227, 0x4A, 0x16, 0x35,
    255, 0xD7, 0x4A, 0x06
};

DEFINE_GRADIENT_PALETTE(fireice_gp) {
    0,   0x50, 0x02, 0x01,
    43,  0xCE, 0x0F, 0x01,
    85,  0xF2, 0x22, 0x01,
    128, 0x10, 0x43, 0x84,
    170, 0x02, 0x15, 0x45,
    255, 0x01, 0x02, 0x04
};

DEFINE_GRADIENT_PALETTE(fireynight_gp) {
    0,   0xF7, 0x24, 0x24,
    23,  0x86, 0x1D, 0xFD,
    46,  0xFD, 0x22, 0x22,
    69,  0x67, 0x01, 0x08,
    92,  0xFC, 0x50, 0x41,
    115, 0xEB, 0x4F, 0xF2,
    138, 0xFF, 0x00, 0x00,
    161, 0x77, 0x0A, 0xF2,
    184, 0xFE, 0x1C, 0x20,
    207, 0xE8, 0x27, 0x27,
    255, 0xA7, 0x45, 0xFC
};

DEFINE_GRADIENT_PALETTE(meadow_gp) {
    0,   0x0C, 0xCB, 0xA3,
    36,  0x00, 0x5C, 0x31,
    73,  0x00, 0x57, 0x2C,
    109, 0x3A, 0xB6, 0x33,
    146, 0x7C, 0xFF, 0x39,
    182, 0x83, 0xD1, 0x0A,
    255, 0x93, 0xEC, 0x00
};

DEFINE_GRADIENT_PALETTE(nebula_gp) {
    0,   0x69, 0x00, 0xC1,
    43,  0x09, 0x0F, 0x79,
    85,  0x7B, 0x08, 0x89,
    128, 0x25, 0x31, 0x99,
    170, 0xA2, 0x22, 0x58,
    255, 0x00, 0xF5, 0xFF
};

DEFINE_GRADIENT_PALETTE(oasis_gp) {
    0,   0x10, 0x5B, 0x13,
    64,  0x1D, 0xED, 0xFD,
    128, 0x1D, 0xED, 0xFD,
    192, 0xA7, 0x45, 0xFC,
    255, 0xA7, 0x45, 0xFC
};

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("BTBackpackV2 Starting...");
    
    // Initialize LEDs
    FastLED.addLeds<WS2812B, LED_OUTPUT_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(brightness * 2.55);
    
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
    
    // Setup Button
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    Serial.println("Setup complete");
}

void loop() {
    BLE.poll();
    handleButton();
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
    
    updateLightShow();
    FastLED.show();
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
    unsigned long now = millis();
    
    switch (currentMode) {
        case PALETTE_STREAM: {
            if (now - lastUpdate > speed) {
                lastUpdate = now;
                paletteIndex++;
                
                CRGBPalette16 palette = getPalette(currentPalette);
                for (int i = 0; i < NUM_LEDS; i++) {
                    leds[i] = ColorFromPalette(palette, paletteIndex + i * 2, 255, LINEARBLEND);
                }
            }
            break;
        }
            
        case PALETTE_CYCLE: {
            if (now - lastUpdate > speed) {
                lastUpdate = now;
                cycleHue++;
                
                CRGBPalette16 palette = getPalette(currentPalette);
                CRGB color = ColorFromPalette(palette, cycleHue, 255, LINEARBLEND);
                fill_solid(leds, NUM_LEDS, color);
            }
            break;
        }
            
        case SPEEDOMETER: {
            // Map speed (0-30 mph) to LED position
            int ledPosition = map(currentSpeed, 0, 30, 0, NUM_LEDS);
            ledPosition = constrain(ledPosition, 0, NUM_LEDS);
            
            // Clear LEDs
            fill_solid(leds, NUM_LEDS, CRGB::Black);
            
            // Fill LEDs up to current speed
            for (int i = 0; i < ledPosition; i++) {
                float ratio = (float)i / NUM_LEDS;
                leds[i] = blend(speedoColor1, speedoColor2, ratio * 255);
            }
            break;
        }
            
        case COLOR_WHEEL: {
            if (positionAvailable) {
                Position diff = currentPosition - origin;
                float angle = diff.polar_angle() * 180.0 / M_PI; // Convert radians to degrees
                if (angle < 0) angle += 360;
                uint8_t hue = (uint8_t)(angle * 255.0 / 360.0);
                fill_solid(leds, NUM_LEDS, CHSV(hue, 255, 255));
            } else {
                fill_solid(leds, NUM_LEDS, CRGB::Red);
            }
            break;
        }
            
        case COLOR_RADIAL: {
            if (positionAvailable) {
                float distance = currentPosition.distance_from(origin);
                // Map distance (0-1000 meters) to color blend
                float ratio = constrain(distance / 1000.0, 0.0, 1.0);
                CRGB color = blend(CRGB::White, CRGB::Red, ratio * 255);
                fill_solid(leds, NUM_LEDS, color);
            } else {
                fill_solid(leds, NUM_LEDS, CRGB::Red);
            }
            break;
        }
            
        case POSITION_STATUS: {
            // Show GPS status with colors
            if (positionAvailable) {
                fill_solid(leds, NUM_LEDS, CRGB::Green);
            } else {
                fill_solid(leds, NUM_LEDS, CRGB::Red);
            }
            break;
        }
    }
    
    // Apply brightness
    FastLED.setBrightness(brightness * 2.55);
    
    // Apply direction
    if (!reverseStrip) {
        // Reverse the LED array
        for (int i = 0; i < NUM_LEDS / 2; i++) {
            CRGB temp = leds[i];
            leds[i] = leds[NUM_LEDS - 1 - i];
            leds[NUM_LEDS - 1 - i] = temp;
        }
    }
}

void sendStatusUpdate() {
    StaticJsonDocument<512> doc;
    
    doc["power"] = power;
    
    // Send the mode string
    switch (currentMode) {
        case PALETTE_STREAM:
            doc["currentSceneId"] = "pstream";
            break;
        case PALETTE_CYCLE:
            doc["currentSceneId"] = "pcycle";
            break;
        case SPEEDOMETER:
            doc["currentSceneId"] = "speedo";
            break;
        case COLOR_WHEEL:
            doc["currentSceneId"] = "cwheel";
            break;
        case COLOR_RADIAL:
            doc["currentSceneId"] = "cradial";
            break;
        case POSITION_STATUS:
            doc["currentSceneId"] = "pstat";
            break;
    }
    
    doc["brightness"] = brightness;
    doc["speed"] = speed;
    doc["direction"] = reverseStrip;
    doc["palette"] = paletteToString(currentPalette);
    doc["position_available"] = positionAvailable;
    doc["current_speed"] = currentSpeed;
    
    JsonObject positionObject = doc.createNestedObject("current_position");
    currentPosition.toJson(positionObject);
    
    String output;
    serializeJson(doc, output);
    
    Serial.print("Sending status update: ");
    Serial.println(output);
    
    statusCharacteristic.setValue(output.c_str());
}

void setMode(const char* mode) {
    Serial.print("Setting mode to: ");
    Serial.println(mode);
    
    if (strcmp(mode, "pstream") == 0) {
        currentMode = PALETTE_STREAM;
    } else if (strcmp(mode, "pcycle") == 0) {
        currentMode = PALETTE_CYCLE;
    } else if (strcmp(mode, "speedo") == 0) {
        currentMode = SPEEDOMETER;
    } else if (strcmp(mode, "cwheel") == 0) {
        currentMode = COLOR_WHEEL;
    } else if (strcmp(mode, "cradial") == 0) {
        currentMode = COLOR_RADIAL;
    } else if (strcmp(mode, "pstat") == 0) {
        currentMode = POSITION_STATUS;
    }
    
    // Don't send status update here - let the regular interval handle it
}

void featureCallback(BLEDevice central, BLECharacteristic characteristic) {
    const uint8_t *buffer = featuresCharacteristic.value();
    unsigned int dataLength = featuresCharacteristic.valueLength();
    
    if (dataLength > 0) {
        uint8_t feature = buffer[0];
        Serial.print("Received feature: 0x");
        Serial.println(feature, HEX);
        
        switch (feature) {
            case 0x01: // Power
                power = buffer[1] != 0;
                Serial.print("Power: ");
                Serial.println(power ? "On" : "Off");
                // Don't send status update here - let the regular interval handle it
                break;
                
            case 0x02: // Mode
                {
                    char modeStr[32] = {0};
                    memcpy(modeStr, buffer + 1, min(dataLength - 1, sizeof(modeStr) - 1));
                    Serial.print("Mode: ");
                    Serial.println(modeStr);
                    setMode(modeStr);  // setMode already sends status update
                }
                break;
                
            case 0x03: // Palette
                {
                    char paletteStr[32] = {0};
                    memcpy(paletteStr, buffer + 1, min(dataLength - 1, sizeof(paletteStr) - 1));
                    Serial.print("Palette: ");
                    Serial.println(paletteStr);
                    currentPalette = stringToPalette(paletteStr);
                    // Don't send status update here
                }
                break;
                
            case 0x04: // Brightness
                {
                    int b = 0;
                    memcpy(&b, buffer + 1, sizeof(int));
                    Serial.print("Brightness: ");
                    Serial.println(b);
                    brightness = constrain(b, 1, 100);
                    // Don't send status update here
                }
                break;
                
            case 0x05: // Speed
                {
                    int s = 0;
                    memcpy(&s, buffer + 1, sizeof(int));
                    Serial.print("Speed: ");
                    Serial.println(s);
                    speed = constrain(s, 5, 200);
                    // Don't send status update here
                }
                break;
                
            case 0x06: // Direction
                reverseStrip = buffer[1] != 0;
                Serial.print("Direction: ");
                Serial.println(reverseStrip ? "Up" : "Down");
                // Don't send status update here
                break;
                
            case 0x07: // Origin
                if (dataLength == 9) {
                    float latitude, longitude;
                    memcpy(&latitude, buffer + 1, sizeof(float));
                    memcpy(&longitude, buffer + 5, sizeof(float));
                    Serial.print("New origin - Lat: ");
                    Serial.print(latitude, 6);
                    Serial.print(", Lon: ");
                    Serial.println(longitude, 6);
                    origin = Position(latitude, longitude);
                    // Don't send status update here
                } else {
                    Serial.println("Invalid origin data length");
                }
                break;
                
            case 0x08: // Ping location
                sendStatusUpdate();  // This one should send status
                break;
                
            case 0x09: // Speedometer colors
                if (dataLength == 7) {
                    speedoColor1 = CRGB(buffer[1], buffer[2], buffer[3]);
                    speedoColor2 = CRGB(buffer[4], buffer[5], buffer[6]);
                    Serial.println("Updated speedometer colors");
                    // Don't send status update here
                } else {
                    Serial.println("Invalid speedometer color data");
                }
                break;
                
            default:
                Serial.print("Unknown feature: 0x");
                Serial.println(feature, HEX);
                break;
        }
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

void handleButton() {
    currentState = digitalRead(BUTTON_PIN);
    
    if (!buttonInitialized) {
        lastState = currentState;
        buttonInitialized = true;
        return;
    }
    
    if ((millis() - pressedTime) > DEBOUNCE_DELAY) {
        if (lastState == HIGH && currentState == LOW) {
            pressedTime = millis();
        } else if (lastState == LOW && currentState == HIGH) {
            releasedTime = millis();
            long pressDuration = releasedTime - pressedTime;
            
            if (pressDuration < SHORT_PRESS_TIME) {
                // Short press - cycle through modes
                Serial.println("Short button press");
                switch (currentMode) {
                    case PALETTE_STREAM:
                        setMode("pcycle");
                        break;
                    case PALETTE_CYCLE:
                        setMode("speedo");
                        break;
                    case SPEEDOMETER:
                        setMode("cwheel");
                        break;
                    case COLOR_WHEEL:
                        setMode("cradial");
                        break;
                    case COLOR_RADIAL:
                        setMode("pstat");
                        break;
                    case POSITION_STATUS:
                        setMode("pstream");
                        break;
                }
            } else if (pressDuration > SHORT_PRESS_TIME && pressDuration < OFF_TIME) {
                // Long press - cycle through palettes
                Serial.println("Long button press");
                currentPalette = (PaletteType)((currentPalette + 1) % PALETTE_COUNT);
                sendStatusUpdate();
            } else {
                // Very long press - toggle power
                power = !power;
                Serial.print("Power toggled: ");
                Serial.println(power ? "On" : "Off");
                sendStatusUpdate();
            }
        }
    }
    
    lastState = currentState;
}

const char* paletteToString(PaletteType palette) {
    switch (palette) {
        case COOL: return "cool";
        case EARTH: return "earth";
        case EVERGLOW: return "everglow";
        case HEART: return "heart";
        case LAVA: return "lava";
        case MELONBALL: return "melonball";
        case PINKSPLASH: return "pinksplash";
        case R: return "r";
        case SOFIA: return "sofia";
        case SUNSET: return "sunset";
        case TROVE: return "trove";
        case VELVET: return "velvet";
        case VGA: return "vga";
        case CANDY: return "candy";
        case COSMICWAVES: return "cosmicwaves";
        case EBLOSSOM: return "eblossom";
        case EMERALD: return "emerald";
        case FATBOY: return "fatboy";
        case FIREICE: return "fireice";
        case FIREYNIGHT: return "fireynight";
        case FLAME: return "flame";
        case MEADOW: return "meadow";
        case NEBULA: return "nebula";
        case OASIS: return "oasis";
        default: return "cool";
    }
}

PaletteType stringToPalette(const char* str) {
    // Create a temporary string to work with
    char temp[32];
    strncpy(temp, str, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    // Convert to lowercase and remove "Palette" suffix if present
    for (int i = 0; temp[i]; i++) {
        temp[i] = tolower(temp[i]);
    }
    
    // Remove "palette" suffix if present
    char* paletteSuffix = strstr(temp, "palette");
    if (paletteSuffix != NULL) {
        *paletteSuffix = '\0';
    }
    
    // Now match the cleaned string
    if (strcmp(temp, "cool") == 0) return COOL;
    if (strcmp(temp, "earth") == 0) return EARTH;
    if (strcmp(temp, "everglow") == 0) return EVERGLOW;
    if (strcmp(temp, "heart") == 0) return HEART;
    if (strcmp(temp, "lava") == 0) return LAVA;
    if (strcmp(temp, "melonball") == 0) return MELONBALL;
    if (strcmp(temp, "pinksplash") == 0) return PINKSPLASH;
    if (strcmp(temp, "r") == 0) return R;
    if (strcmp(temp, "sofia") == 0) return SOFIA;
    if (strcmp(temp, "sunset") == 0) return SUNSET;
    if (strcmp(temp, "trove") == 0) return TROVE;
    if (strcmp(temp, "velvet") == 0) return VELVET;
    if (strcmp(temp, "vga") == 0) return VGA;
    if (strcmp(temp, "candy") == 0) return CANDY;
    if (strcmp(temp, "cosmicwaves") == 0) return COSMICWAVES;
    if (strcmp(temp, "eblossom") == 0) return EBLOSSOM;
    if (strcmp(temp, "emerald") == 0) return EMERALD;
    if (strcmp(temp, "fatboy") == 0) return FATBOY;
    if (strcmp(temp, "fireice") == 0) return FIREICE;
    if (strcmp(temp, "fireynight") == 0 || strcmp(temp, "firey night") == 0) return FIREYNIGHT;
    if (strcmp(temp, "flame") == 0) return FLAME;
    if (strcmp(temp, "meadow") == 0) return MEADOW;
    if (strcmp(temp, "nebula") == 0) return NEBULA;
    if (strcmp(temp, "oasis") == 0) return OASIS;
    return COOL;
}

CRGBPalette16 getPalette(PaletteType palette) {
    switch (palette) {
        case COOL:
            return CRGBPalette16(CRGB(0x00, 0x00, 0xFF), CRGB(0xFF, 0x00, 0xFF));
            
        case EARTH:
            return earth_gp;
            
        case EVERGLOW:
            return everglow_gp;
            
        case HEART:
            return heart_gp;
            
        case LAVA:
            return LavaColors_p;
            
        case MELONBALL:
            return melonball_gp;
            
        case PINKSPLASH:
            return pinksplash_gp;
            
        case R:
            return RainbowColors_p;
            
        case SOFIA:
            return sofia_gp;
            
        case SUNSET:
            return sunset_gp;
            
        case TROVE:
            return trove_gp;
            
        case VELVET:
            return velvet_gp;
            
        case VGA:
            return vga_gp;
            
        case CANDY:
            return candy_gp;
            
        case COSMICWAVES:
            return cosmicwaves_gp;
            
        case EBLOSSOM:
            return eblossom_gp;
            
        case EMERALD:
            return emerald_gp;
            
        case FATBOY:
            return fatboy_gp;
            
        case FIREICE:
            return fireice_gp;
            
        case FIREYNIGHT:
            return fireynight_gp;
            
        case FLAME:
            return HeatColors_p;
            
        case MEADOW:
            return meadow_gp;
            
        case NEBULA:
            return nebula_gp;
            
        case OASIS:
            return oasis_gp;
            
        default:
            return RainbowColors_p;
    }
} 