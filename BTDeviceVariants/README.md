# BT Device Variants

A flexible PlatformIO project for creating different Bluetooth LED device variants from a single codebase.

## Device Naming

Devices follow the naming format: **DEVICENAME-USERINITIALS-UUID4DIGITS**

Examples:
- `CowboyHat-AY-1234`
- `Fannypack-JD-5678`
- `CowboyHat-MK-9999`

## Supported Device Types

### CowboyHat
- **Device Name Format**: CowboyHat-{INITIALS}-{UUID}
- **LED Configuration**: 600 LEDs on GPIO 17
- **UUIDs**: 
  - Service: `a6b43832-0e6e-46df-a80b-e7ab5b4d7c99`
  - Features: `ef399903-6234-4ba8-894b-6b537bc6739b`
  - Status: `8d78c901-7dc0-4e1b-9ac9-34731a1ccf49`

### Fannypack
- **Device Name Format**: Fannypack-{INITIALS}-{UUID}
- **LED Configuration**: 30 LEDs on GPIO 27
- **UUIDs**:
  - Service: `ac34717f-b5d0-45b5-8bbb-7020974bea2f`
  - Features: `dc23128c-6bc0-406e-9725-075e44e73a2d`
  - Status: `ca6b1a3e-7746-4e80-88fe-9b9df934a0bd`

## Build Instructions

### Available Build Environments

| Environment | Device Type | User Initials | UUID | Device Name |
|-------------|-------------|---------------|------|-------------|
| `cowboyhat` | CowboyHat | AY | 1234 | CowboyHat-AY-1234 |
| `fannypack` | Fannypack | AY | 5678 | Fannypack-AY-5678 |
| `cowboyhat-jd` | CowboyHat | JD | 9999 | CowboyHat-JD-9999 |
| `fannypack-mk` | Fannypack | MK | 0001 | Fannypack-MK-0001 |

### Building and Uploading

#### Default Environments
```bash
# Build and upload CowboyHat-AY-1234
pio run -e cowboyhat -t upload

# Build and upload Fannypack-AY-5678
pio run -e fannypack -t upload
```

#### Custom Environments
```bash
# Build and upload CowboyHat-JD-9999
pio run -e cowboyhat-jd -t upload

# Build and upload Fannypack-MK-0001
pio run -e fannypack-mk -t upload
```

### Monitor Serial Output
```bash
# Monitor any environment
pio device monitor -e cowboyhat
pio device monitor -e fannypack
pio device monitor -e cowboyhat-jd
pio device monitor -e fannypack-mk
```

## Features

Both device variants include:
- Full BLE communication compatible with your existing app
- All lighting effects and palettes (GPS-related scenes excluded automatically)
- Automatic status reporting via STATUS_UUID
- Power management
- All parameter controls (brightness, speed, effects, direction, etc.)
- **PERSISTENT DEFAULTS** that survive reboots!
- Owner identification and device naming
- Maximum brightness limits
- Auto-on behavior control
- Optimized for ESP32C6 hardware

## BLE Commands

| Command | Description |
|---------|-------------|
| 0x01    | Set primary palette |
| 0x02    | Set lighting mode/scene |
| 0x05    | Set direction |
| 0x06    | Set power on/off |
| 0x08    | Set speed |
| 0x09    | Set brightness |
| 0x1A    | Get current defaults (returns JSON via status notification) |
| 0x1B    | Set defaults from JSON string |
| 0x1C    | Save current state as defaults |
| 0x1D    | Reset to factory defaults |
| 0x1E    | Set max brightness limit |
| 0x1F    | Set device owner |
| 0x20    | Set auto-on behavior |

## Creating Custom Environments

### Quick Custom Environment
Create a custom environment for any user/device combination by adding to `platformio.ini`:

```ini
[env:my-custom-device]
platform = espressif32
board = seeed_xiao_esp32c6
framework = arduino
lib_deps = 
    fastled/FastLED@^3.5.0
    adafruit/Adafruit BusIO@^1.14.1
    adafruit/Adafruit Unified Sensor@^1.1.9
    adafruit/Adafruit MPU6050@^2.2.4
    adafruit/Adafruit GFX Library@^1.11.3
    adafruit/Adafruit SSD1306@^2.5.7
    adafruit/Adafruit NeoPixel@^1.10.6
    bblanchon/ArduinoJson@^6.21.2
    https://github.com/codycodes/bmdevice
build_flags = 
    -DDEVICE_TYPE=COWBOY_HAT          ; or FANNYPACK
    -DUSER_INITIALS="XX"              ; Your initials
    -DDEVICE_UUID="1111"              ; Your unique 4-digit ID
    -DCORE_DEBUG_LEVEL=3
monitor_speed = 115200
monitor_filters = esp32_exception_decoder
```

Then build with: `pio run -e my-custom-device -t upload`

### Example Custom Environments
- `CowboyHat-AB-2024`: For user AB with UUID 2024
- `Fannypack-CD-0101`: For user CD with UUID 0101
- `CowboyHat-EF-9876`: For user EF with UUID 9876

## Adding New Device Types

To add a completely new device type:

1. Add a new device type definition in `src/main.cpp`:
   ```cpp
   #define NEW_DEVICE 3

   #elif DEVICE_TYPE == NEW_DEVICE
       #define DEVICE_NAME_BASE "NewDevice"
       #define SERVICE_UUID "your-service-uuid"
       #define FEATURES_UUID "your-features-uuid"
       #define STATUS_UUID "your-status-uuid"
       #define LED_PIN 27
       #define NUM_LEDS 50
       #define COLOR_ORDER GRB
   ```

2. Update the setup() function to include the new device type in the serial output.

3. Add environments in `platformio.ini` using the new device type:
   ```ini
   [env:newdevice]
   build_flags = 
       -DDEVICE_TYPE=NEW_DEVICE
       -DUSER_INITIALS="AY"
       -DDEVICE_UUID="1234"
   ```

## Hardware Requirements

- **Board**: Seeed Xiao ESP32-C6 (or compatible ESP32-C6 board)
- **LED Strip**: WS2812B compatible LED strip
- **Power**: Appropriate power supply for your LED count

## Dependencies

- FastLED
- BMDevice library
- Standard ESP32 Arduino libraries

## License

This project is part of the Burning Man LED device ecosystem. 