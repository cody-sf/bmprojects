# BTUmbrellaV3 - Clean Sound-Reactive Umbrella

## Overview
BTUmbrellaV3 is a clean rewrite of the sound-reactive umbrella controller, combining the best features of the original Umbrella.ino with the advanced BMDevice framework.

## Key Features
- **Clean Palette Management**: Proper primary/secondary palette visualization like the original
- **BMDevice Integration**: Full compatibility with standard BLE commands and light shows
- **Sound Analysis**: 8-strip FFT-based music visualization with I2S microphone
- **Dual Modes**: Sound-reactive mode and standard BMDevice light show mode
- **Advanced Audio Features**: Configurable sensitivity, smoothing, beat detection, etc.

## Hardware
- 8 LED strips (38 LEDs each) on GPIO pins: 32, 33, 27, 14, 12, 13, 18, 5
- I2S microphone on pins: SCK=26, WS=22, SD=21
- ESP32 microcontroller

## BLE Commands

### Standard BMDevice Commands
- `0x01` - Set primary palette
- `0x04` - Set brightness  
- `0x05` - Set speed
- `0x06` - Set power on/off
- `0x08` - Set direction
- `0x0A` - Set effect/scene

### Custom Sound Commands
- `0x03` - Set sensitivity (int)
- `0x07` - Sound mode on/off (bool)
- `0x09` - Set secondary palette (string) or "off"
- `0x0B` - Set amplitude (int)
- `0x0C` - Set noise threshold (int)
- `0x0D` - Decay on/off (bool)
- `0x0E` - Decay rate (int)
- `0x13` - Bar mode vs dot mode (bool)
- `0x14` - Rainbow mode on/off (bool)
- `0x15` - Color speed (int)

## Sound Visualization
- **Primary Palette**: Used for active LEDs that respond to sound
- **Secondary Palette**: Used for background/inactive LEDs (can be set to "off")
- **Rainbow Mode**: Overrides palettes with rainbow colors
- **Bar Mode**: Fills LEDs from bottom based on frequency intensity
- **Dot Mode**: Shows only the peak frequency as a single LED

## Differences from V2
- Cleaner, more maintainable code structure
- Simplified palette management that actually works
- Better separation of sound features from BMDevice features
- More reliable primary/secondary palette handling
- Reduced complexity while maintaining all functionality

## Status Updates
The device sends JSON status updates via BLE including:
- Power, brightness, speed, direction
- Primary and secondary palette names
- Sound mode state and settings
- Amplitude, noise threshold, sensitivity
- Visual mode settings

## Usage
1. Flash to ESP32 using PlatformIO
2. Connect via BLE (device name: "Umbrella-CL")
3. Send BLE commands to control lighting and sound settings
4. Toggle between sound-reactive mode and standard light shows 