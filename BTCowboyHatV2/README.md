# BTCowboyHatV2

A modernized CowboyHat LED device using the BMDevice framework, designed for the Seeed Studio ESP32C3.

## Hardware Specifications

- **Board**: Seeed Xiao ESP32C3
- **LED Pin**: D7
- **LED Count**: 350 LEDs
- **LED Type**: WS2812B (GRB color order)
- **GPS**: Not included (automatically excludes GPS-related light scenes)

## Features

- **Bluetooth LE Communication**: Full BLE support with status updates
- **Persistent Settings**: All configurations survive reboots
- **LED Effects**: All standard lighting effects and palettes (excluding GPS-dependent scenes)
- **Power Management**: Remote on/off control
- **Parameter Control**: Brightness, speed, direction, palette selection
- **Device Management**: Owner identification, max brightness limits, auto-on behavior

## BLE Configuration

- **Service UUID**: `a6b43832-0e6e-46df-a80b-e7ab5b4d7c99`
- **Features UUID**: `ef399903-6234-4ba8-894b-6b537bc6739b`
- **Status UUID**: `8d78c901-7dc0-4e1b-9ac9-34731a1ccf49` (new for this device)
- **Device Name**: `CowboyHat-AY`

## Installation

1. Install PlatformIO
2. Open this project in PlatformIO
3. Build and upload to your Seeed Xiao ESP32C3

## BLE Commands

The device responds to all standard BMDevice BLE commands:

### Basic Controls
- `0x01` - Set primary palette
- `0x02` - Set lighting mode/scene  
- `0x05` - Set direction
- `0x06` - Set power on/off
- `0x08` - Set speed
- `0x09` - Set brightness

### Advanced Controls
- `0x1A` - Get current defaults
- `0x1B` - Set defaults from JSON
- `0x1C` - Save current state as defaults
- `0x1D` - Reset to factory defaults
- `0x1E` - Set max brightness limit
- `0x1F` - Set device owner
- `0x20` - Set auto-on behavior

## Differences from Original

This version replaces 257 lines of custom code with ~70 lines using the BMDevice framework, providing:

- Automatic BLE handling
- Persistent settings storage
- Standardized command interface
- Better error handling
- Consistent behavior across all devices
- Automatic exclusion of GPS-dependent features

## Compatibility

Fully compatible with existing mobile apps and control interfaces that work with other BMDevice-based devices. 