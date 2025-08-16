# GPS Speed-Based Lightshow Implementation

## Overview

I've successfully implemented GPS speed-based lightshow control for your BMDevice library. This allows lightshow speed to be dynamically adjusted based on your movement speed, creating more immersive effects for activities like biking or walking at Burning Man.

## New Features Added

### 1. GPS Speed Preferences
- **GPS Low Speed** (`gpsLowSpeed`): The speed (in km/h) that corresponds to the slowest lightshow (highest delay)
- **GPS Top Speed** (`gpsTopSpeed`): The speed (in km/h) that corresponds to the fastest lightshow (lowest delay)
- **GPS Lightshow Speed Enabled** (`gpsLightshowSpeedEnabled`): Boolean flag to enable/disable GPS speed control

### 2. Bluetooth Commands
New BLE feature commands for controlling GPS speed settings:
- `BLE_FEATURE_SET_GPS_LOW_SPEED` (0x21): Set the low speed threshold
- `BLE_FEATURE_SET_GPS_TOP_SPEED` (0x22): Set the top speed threshold  
- `BLE_FEATURE_SET_GPS_LIGHTSHOW_SPEED_ENABLED` (0x23): Enable/disable GPS speed control

### 3. Speed Mapping Logic
- **Inverse Relationship**: Higher GPS speed = faster lightshow (lower delay)
- **Speed Range**: 20ms (fastest) to 200ms (slowest) lightshow delay
- **GPS Speed Range**: Configurable between your low and top speed settings
- **Default Values**: Low speed = 5 km/h (walking), Top speed = 25 km/h (biking)

## How It Works

1. **When GPS Lightshow Speed is Enabled**:
   - The system monitors your current GPS speed
   - Maps your speed between the configured low and top speed thresholds
   - Converts GPS speed to lightshow delay using inverse mapping:
     - At low speed (5 km/h): Lightshow runs at ~200ms delay (slow/relaxed)
     - At top speed (25 km/h): Lightshow runs at ~20ms delay (fast/energetic)

2. **When GPS Lightshow Speed is Disabled**:
   - Uses the normal manual speed setting from the device state
   - No GPS speed influence on lightshow timing

3. **Fallback Behavior**:
   - If GPS is not available or no fix: Uses manual speed setting
   - If GPS speed is outside configured range: Constrains to min/max values

## Usage Examples

### Setting Up GPS Speed Control via Bluetooth

```cpp
// Enable GPS speed control
uint8_t enableCmd[] = {BLE_FEATURE_SET_GPS_LIGHTSHOW_SPEED_ENABLED, 1};
// Send via BLE characteristic

// Set low speed to 3 km/h (walking pace)
float lowSpeed = 3.0;
uint8_t lowSpeedCmd[5] = {BLE_FEATURE_SET_GPS_LOW_SPEED};
memcpy(lowSpeedCmd + 1, &lowSpeed, sizeof(float));
// Send via BLE characteristic

// Set top speed to 30 km/h (fast biking)
float topSpeed = 30.0;
uint8_t topSpeedCmd[5] = {BLE_FEATURE_SET_GPS_TOP_SPEED};
memcpy(topSpeedCmd + 1, &topSpeed, sizeof(float));
// Send via BLE characteristic
```

### Programmatic Setup

```cpp
// In your device setup code
bmDevice.getDefaults().setGPSLowSpeed(2.0);   // 2 km/h for very slow walking
bmDevice.getDefaults().setGPSTopSpeed(40.0);  // 40 km/h for fast biking
bmDevice.getDefaults().setGPSLightshowSpeedEnabled(true);
bmDevice.enableGPS(); // Make sure GPS is enabled
```

## Integration with Existing Lightshows

All existing lightshow effects automatically support GPS speed control:
- `palette_stream`
- `pulse_wave`
- `meteor_shower`
- `fire_plasma`
- `kaleidoscope`
- `rainbow_comet`
- `matrix_rain`
- `plasma_clouds`
- `lava_lamp`
- `aurora_borealis`
- `lightning_storm`
- `color_explosion`
- `spiral_galaxy`

## JSON Status Updates

The device status now includes GPS speed settings:
```json
{
  "gpsLowSpd": 5.0,
  "gpsTopSpd": 25.0,
  "gpsLightSpdEn": true,
  "spdCur": 12.5,
  "posAvail": true
}
```

## Debug Output

When GPS speed control is active, you'll see debug output every 5 seconds:
```
[BMDevice] GPS Speed Mapping: GPS=12.5 km/h, Lightshow Speed=85 ms
```

## Files Modified

1. **BMBluetoothHandler.h**: Added new BLE feature constants
2. **BMDeviceDefaults.h/.cpp**: Added GPS speed preferences and storage
3. **BMDeviceState.h/.cpp**: Added GPS speed state variables
4. **BMDevice.h/.cpp**: Added feature handlers and speed mapping logic

## Benefits

- **Immersive Experience**: Lightshows respond to your movement, creating dynamic visual feedback
- **Contextual Lighting**: Slow, relaxing effects when stationary/walking; energetic effects when biking
- **Customizable**: Adjust speed thresholds to match your activity preferences
- **Backward Compatible**: Existing functionality unchanged when GPS speed control is disabled

## Default Configuration

- **GPS Low Speed**: 5.0 km/h (walking pace)
- **GPS Top Speed**: 25.0 km/h (moderate biking)
- **GPS Lightshow Speed Enabled**: false (opt-in feature)
- **Lightshow Speed Range**: 20ms to 200ms delay

The implementation provides a smooth, responsive connection between your physical movement and the visual effects of your LED devices!
