# BMDevice Persistent Defaults Guide

The BMDevice library now includes a comprehensive persistent defaults system that allows you to save device settings that persist across reboots. This guide covers everything you need to know about using this feature.

## Overview

The defaults system stores settings in ESP32 flash memory using the Preferences library. Settings are automatically loaded on startup and can be modified via code or BLE commands.

## Supported Default Settings

### Core Device Settings
- **Brightness** (1-100): Default brightness level
- **Max Brightness** (1-100): Maximum allowed brightness 
- **Speed** (5-200): Default animation speed
- **Palette**: Default color palette
- **Effect**: Default lighting effect
- **Direction**: Forward/reverse direction
- **Effect Color**: RGB color for solid color effects

### Device Identity
- **Owner**: Device owner identifier (e.g., "CL", "AY")
- **Device Name**: Custom device name

### Behavior Settings
- **Auto On**: Whether device powers on automatically
- **GPS Enabled**: Whether GPS is enabled by default
- **Status Interval**: How often to send BLE status updates (1-60 seconds)

## Basic Usage

### 1. Automatic Loading

Defaults are automatically loaded when you call `device.begin()`:

```cpp
#include <BMDevice.h>

BMDevice device("MyDevice", SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

void setup() {
    device.addLEDStrip<WS2812B, 27, GRB>(leds, NUM_LEDS);
    
    // This automatically loads and applies saved defaults
    device.begin();
}
```

### 2. Setting Defaults Programmatically

```cpp
void setup() {
    device.begin();
    
    // Get access to defaults system
    BMDeviceDefaults& defaults = device.getDefaults();
    
    // Set individual defaults
    defaults.setOwner("CL");
    defaults.setMaxBrightness(85);
    defaults.setBrightness(70);
    defaults.setPalette(AvailablePalettes::rainbow);
    defaults.setEffect(LightSceneID::aurora_borealis);
    defaults.setSpeed(120);
    defaults.setAutoOn(true);
    defaults.setEffectColor(CRGB::Purple);
    
    // Print current defaults
    defaults.printCurrentDefaults();
}
```

### 3. Save Current State as Defaults

```cpp
// After adjusting settings via BLE or code
device.saveCurrentAsDefaults();
```

### 4. Reset to Factory Defaults

```cpp
device.resetToFactoryDefaults();
```

### 5. Load and Apply Defaults

```cpp
device.loadDefaults();  // Loads and applies saved defaults
```

## BLE Commands

The defaults system adds several new BLE commands:

### Get Current Defaults (0x1A)
Returns current defaults as JSON via status notification.

**Command:** `[0x1A]`

**Response:** JSON object with all current defaults

### Set Defaults from JSON (0x1B)
Update defaults from a JSON string.

**Command:** `[0x1B][JSON_STRING]`

**Example JSON:**
```json
{
  "brightness": 75,
  "maxBrightness": 90,
  "speed": 100,
  "paletteId": 2,
  "effectId": 3,
  "owner": "CL",
  "autoOn": true
}
```

### Save Current as Defaults (0x1C)
Save the current device state as defaults.

**Command:** `[0x1C]`

**Response:** `{"defaultsSaved": true/false}`

### Reset to Factory (0x1D)
Reset all defaults to factory values.

**Command:** `[0x1D]`

**Response:** `{"factoryReset": true/false}`

### Set Max Brightness (0x1E)
Set the maximum brightness limit.

**Command:** `[0x1E][int32_brightness]`

### Set Device Owner (0x1F)
Set the device owner string.

**Command:** `[0x1F][owner_string]`

### Set Auto-On (0x20)
Set whether device auto-powers on.

**Command:** `[0x20][0x00/0x01]`

## JSON Format

The defaults system uses JSON for data exchange. Here's the complete format:

```json
{
  "brightness": 50,
  "maxBrightness": 100,
  "speed": 100,
  "palette": "cool",
  "paletteId": 1,
  "effect": "palette_stream",
  "effectId": 0,
  "reverseDirection": true,
  "owner": "CL",
  "deviceName": "BMDevice",
  "autoOn": true,
  "statusInterval": 5000,
  "gpsEnabled": false,
  "version": 1,
  "effectColor": {
    "r": 0,
    "g": 255,
    "b": 0
  }
}
```

## Advanced Usage

### Custom Default Values

You can set custom factory defaults by modifying the `setFactoryDefaults()` method in `BMDeviceDefaults.h`:

```cpp
void setFactoryDefaults() {
    brightness = 75;                    // Custom default brightness
    maxBrightness = 85;                // Custom max brightness
    speed = 120;                       // Custom default speed
    palette = AvailablePalettes::fire; // Custom default palette
    effect = LightSceneID::meteor_shower; // Custom default effect
    owner = "CL";                      // Your initials
    deviceName = "MyCustomDevice";     // Custom device name
    // ... other settings
}
```

### Validation and Constraints

The system automatically validates and constrains all values:

- Brightness: 1-100
- Speed: 5-200
- Status Interval: 1000-60000ms
- String lengths: max 32 characters
- Enum values: checked against valid ranges

### Migration Support

The system includes version-based migration for future updates. When you upgrade the library, existing settings are preserved and migrated as needed.

### Storage Information

- **Storage Location**: ESP32 NVS (Non-Volatile Storage)
- **Namespace**: "bmdefaults"
- **Storage Size**: ~500 bytes per device
- **Durability**: 100,000+ write cycles typical

## Best Practices

### 1. Set Identity on First Boot

```cpp
void setup() {
    device.begin();
    
    BMDeviceDefaults& defaults = device.getDefaults();
    
    // Set device identity that persists
    defaults.setOwner("CL");
    defaults.setMaxBrightness(85);  // Prevent eye damage!
}
```

### 2. Provide User Feedback

```cpp
bool success = device.saveCurrentAsDefaults();
if (success) {
    Serial.println("Settings saved!");
    // Maybe blink LEDs to confirm
} else {
    Serial.println("Save failed!");
}
```

### 3. Use Safe Defaults

Always set reasonable factory defaults:
- Moderate brightness (not max)
- Safe speed settings
- Known-good palette/effect combinations

### 4. Handle Reset Gracefully

```cpp
void factoryReset() {
    Serial.println("Resetting to factory defaults...");
    device.resetToFactoryDefaults();
    Serial.println("Reset complete!");
}
```

## Troubleshooting

### Defaults Not Loading
- Check Serial output for error messages
- Verify flash memory isn't corrupted
- Try factory reset if needed

### Settings Not Persisting
- Ensure you're calling `saveDefaults()` or individual setters
- Check return values for success/failure
- Verify flash memory isn't full

### BLE Commands Not Working
- Verify command format and data length
- Check Serial output for command acknowledgment
- Ensure proper BLE connection

### JSON Parsing Errors
- Validate JSON format before sending
- Check string length limits
- Ensure all required fields are present

## Example Projects

### Smart Bike Light
```cpp
// Set bike-specific defaults
defaults.setOwner("CL");
defaults.setMaxBrightness(100);  // Need max visibility
defaults.setPalette(AvailablePalettes::rainbow);
defaults.setEffect(LightSceneID::pulse_wave);
defaults.setAutoOn(true);  // Turn on when powered
```

### Festival Backpack
```cpp
// Set party-friendly defaults
defaults.setOwner("CL");
defaults.setMaxBrightness(75);   // Battery conservation
defaults.setPalette(AvailablePalettes::party);
defaults.setEffect(LightSceneID::aurora_borealis);
defaults.setSpeed(150);          // Fast and energetic
```

### Camp Decoration
```cpp
// Set ambient defaults
defaults.setOwner("CAMP");
defaults.setMaxBrightness(50);   // Ambient lighting
defaults.setPalette(AvailablePalettes::warm);
defaults.setEffect(LightSceneID::fire_plasma);
defaults.setSpeed(80);           // Slow and relaxing
```

The persistent defaults system makes your BMDevice smart and user-friendly, remembering preferences across power cycles and providing a much better user experience! 