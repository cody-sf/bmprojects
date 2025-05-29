# BMDevice Library

A unified device controller library for Burning Man LED projects. This library abstracts away all the common functionality needed for ESP32-based LED devices including BLE communication, device state management, GPS integration, and LED controller setup.

## Features

- **BLE Communication**: Full Bluetooth Low Energy protocol with feature commands
- **Multi-Strip LED Support**: Easy addition of multiple LED strips with different pins and configurations
- **GPS Integration**: Optional GPS tracking using internal TinyGPS++ or external LocationService
- **Device State Management**: Automatic handling of device parameters and settings
- **Effect Control**: Complete integration with BurningManLEDs LightShow library
- **Status Reporting**: Automatic JSON status updates via BLE
- **Plug-and-Play**: Reduces 600+ lines of boilerplate to ~30 lines

## Installation

1. Copy the `BMDevice` folder to your Arduino `libraries` directory
2. Ensure you have the required dependencies:
   - FastLED
   - ArduinoBLE
   - ArduinoJson
   - BurningManLEDs
   - TinyGPSPlus (optional, for GPS)

## Quick Start

### Simple Single Strip Device

```cpp
#include <BMDevice.h>

#define LED_PIN 27
#define NUM_LEDS 192
CRGB leds[NUM_LEDS];

BMDevice device("My-Device", "be03096f-9322-4360-bc84-0f977c5c3c10");

void setup() {
    device.addLEDStrip<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    device.enableGPS(); // Optional
    device.begin();
}

void loop() {
    device.loop(); // Handles everything!
}
```

### Multi-Strip Device

```cpp
#include <BMDevice.h>

#define NUM_LEDS 200
CRGB leds1[NUM_LEDS], leds2[NUM_LEDS], leds3[NUM_LEDS];

BMDevice device("Multi-Strip", "your-uuid-here");

void setup() {
    device.addLEDStrip<WS2812B, 27, RGB>(leds1, NUM_LEDS);
    device.addLEDStrip<WS2812B, 25, RGB>(leds2, NUM_LEDS);  
    device.addLEDStrip<WS2812B, 23, GRB>(leds3, NUM_LEDS);
    device.begin();
}

void loop() {
    device.loop();
}
```

## API Reference

### Constructor

```cpp
BMDevice(const char* deviceName, const char* serviceUUID)
```

### LED Strip Management

```cpp
template<template<uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER>
void addLEDStrip(CRGB* ledArray, int numLeds)
```

Add an LED strip to the device. Supports all FastLED chipset types.

### GPS Integration

```cpp
void enableGPS(int rxPin = 16, int txPin = 17, int baud = 9600)
void setLocationService(LocationService* locationService)
```

Enable GPS tracking either with built-in TinyGPS++ or external LocationService.

### Device Control

```cpp
bool begin()                    // Initialize the device
void loop()                     // Main loop - call this in your loop()
void setBrightness(int brightness)
void setEffect(LightSceneID effect)
void setPalette(AvailablePalettes palette)
```

### State Access

```cpp
BMDeviceState& getState()              // Access device state
LightShow& getLightShow()              // Access light show controller
BMBluetoothHandler& getBluetoothHandler() // Access BLE handler
```

### Configuration

```cpp
void setStatusUpdateInterval(unsigned long interval)
void setCustomFeatureHandler(std::function<bool(uint8_t, const uint8_t*, size_t)> handler)
void setCustomConnectionHandler(std::function<void(bool)> handler)
```

## Supported BLE Features

The library handles all these BLE commands automatically:

- **0x01**: Power on/off
- **0x04**: Brightness control (1-100)
- **0x05**: Speed control (5-200)
- **0x06**: Direction (forward/reverse)
- **0x07**: GPS origin setting
- **0x08**: Palette selection
- **0x0A**: Effect selection
- **0x0B-0x19**: Effect parameters (wave width, meteor count, etc.)

## Device State

The `BMDeviceState` class manages all device parameters:

```cpp
// Core state
bool power;
int brightness;
uint16_t speed;
bool reverseStrip;

// Effects
AvailablePalettes currentPalette;
LightSceneID currentEffect;

// Effect parameters
int waveWidth, meteorCount, trailLength, heatVariance;
// ... and many more

// Location (if GPS enabled)
Position origin, currentPosition;
bool positionAvailable;
float currentSpeed;
```

## Advanced Usage

### Custom Feature Handling

```cpp
device.setCustomFeatureHandler([](uint8_t feature, const uint8_t* data, size_t length) -> bool {
    if (feature == 0x99) { // Custom feature
        // Handle custom feature
        Serial.println("Custom feature received!");
        return true; // Handled
    }
    return false; // Let BMDevice handle it
});
```

### Using External LocationService

```cpp
LocationService locationService;
device.setLocationService(&locationService);
// No need to call device.enableGPS()
```

### Accessing Components

```cpp
BMDeviceState& state = device.getState();
state.brightness = 75;
device.setBrightness(state.brightness);

LightShow& lightShow = device.getLightShow();
lightShow.solid(CRGB::Red);
```

## Migration from Original Code

Replace this (600+ lines):
```cpp
// All the BLE setup, GPS handling, state management, 
// feature callbacks, status updates, etc.
```

With this (~30 lines):
```cpp
#include <BMDevice.h>
BMDevice device("Name", "UUID");
// Add LED strips
device.begin();
// In loop: device.loop();
```

## Examples

See the `examples/` directory for:
- `simple_backpack.cpp` - Basic single-strip device
- `multi_strip.cpp` - Multiple LED strips
- `custom_features.cpp` - Custom BLE feature handling
- `external_gps.cpp` - Using external LocationService

## Dependencies

- **FastLED**: LED control
- **ArduinoBLE**: Bluetooth communication  
- **ArduinoJson**: JSON serialization
- **BurningManLEDs**: Effect library (LightShow, Position, etc.)
- **TinyGPSPlus**: GPS parsing (optional)

## License

Part of the Burning Man LED project suite. 