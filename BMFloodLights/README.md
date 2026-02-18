# BMFloodLights

A specialized Burning Man device controller for managing floodlights with slow, ambient color transitions.

## Features

- **Floodlight Optimized**: Designed specifically for controlling 5-10 high-power floodlights
- **Full Brightness**: Operates at maximum brightness (255) for powerful illumination
- **Slow Fade Transitions**: Extended timing for smooth, ambient color changes
- **12V Compatible**: Optimized for 12V LED strips and floodlights
- **Color Palette Support**: Full palette support with extended transition times
- **BLE Control**: Wireless control via Bluetooth Low Energy

## Hardware Configuration

### LED Configuration
- **LED Count**: 5-10 floodlights per strip
- **Brightness**: Fixed at 255 (maximum)
- **Voltage**: 12V optimized
- **Color Order**: RGB for 12V strips

### Pin Configuration
- **Strip 0**: GPIO 32 (12V floodlight strip 1)
- **Strip 1**: GPIO 33 (12V floodlight strip 2)  
- **Strip 2**: GPIO 27 (12V floodlight strip 3)
- **Strip 3**: GPIO 14 (12V floodlight strip 4)
- **Strip 4**: GPIO 12 (12V floodlight strip 5)

## Timing Characteristics

The floodlight controller uses extended timing for smooth ambient transitions:
- **Base Speed**: Much slower than standard LED strips
- **Fade Duration**: Extended fade times for gradual color changes
- **Effect Speed**: Optimized for ambient lighting rather than dynamic shows

## Usage

1. **Build and Upload**:
   ```bash
   pio run -e floodlights -t upload
   ```

2. **Connect via BLE**: Use the standard BM device BLE interface to control colors, palettes, and effects.

3. **Ambient Control**: The device automatically applies slow timing suitable for floodlight ambiance.

## Differences from BMGenericDevice

- **Reduced LED Count**: 5-10 LEDs per strip instead of 350-450
- **Maximum Brightness**: Always operates at 255 brightness
- **Slow Timing**: Extended fade and transition times
- **12V Optimization**: Pin configuration optimized for 12V strips
- **Ambient Focus**: Effects selected for ambient lighting rather than dynamic shows
