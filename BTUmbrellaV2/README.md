# BTUmbrellaV2 - Sound-Reactive Umbrella with BMDevice Integration

This is the upgraded version of the Umbrella controller, now using PlatformIO and the BMDevice framework for enhanced functionality while preserving the original sound-reactive capabilities.

## Features

### Enhanced BMDevice Integration
- **Shared Palettes**: Uses the same palette system as other BMDevice projects
- **Persistent Defaults**: Settings are saved automatically and survive reboots
- **Advanced BLE Communication**: Full status reporting and command handling
- **Power Management**: Proper on/off states with automatic LED management
- **Multiple Light Scenes**: Access to all BMDevice light effects when not in sound mode

### Sound Analysis Capabilities
- **Real-time FFT Analysis**: Processes audio input via I2S microphone
- **8 LED Strip Support**: Each strip responds to different frequency bands
- **Customizable Sound Settings**: Adjustable sensitivity, amplitude, noise threshold
- **Dual Mode Operation**: Switch between sound-reactive and regular light shows

### Hardware Support
- **ESP32 Based**: Optimized for ESP32 development boards
- **I2S Microphone**: Professional audio sampling with INMP441 or similar
- **8 WS2812B LED Strips**: 30 LEDs per strip (240 total LEDs)
- **Multi-pin Configuration**: Uses 8 different GPIO pins for LED control

## Hardware Connections

### LED Strips
- Strip 0: GPIO 13
- Strip 1: GPIO 12
- Strip 2: GPIO 14
- Strip 3: GPIO 27
- Strip 4: GPIO 16
- Strip 5: GPIO 25
- Strip 6: GPIO 33
- Strip 7: GPIO 32

### I2S Microphone (INMP441)
- SCK (Bit Clock): GPIO 26
- WS (Word Select): GPIO 22
- SD (Data): GPIO 21
- 3.3V: 3.3V
- GND: GND

## BLE Commands

### Standard BMDevice Commands
- `0x01` - Set primary palette
- `0x02` - Set lighting mode/scene
- `0x05` - Set direction (reverse strips)
- `0x06` - Set power on/off
- `0x08` - Set speed
- `0x09` - Set brightness
- `0x1A` - Get current defaults (JSON)
- `0x1B` - Set defaults from JSON
- `0x1C` - Save current state as defaults
- `0x1D` - Reset to factory defaults
- `0x1E` - Set max brightness limit
- `0x1F` - Set device owner
- `0x20` - Set auto-on behavior

### Custom Sound Commands
- `0x03` - Set sensitivity (int)
- `0x07` - Set sound sensitivity on/off (bool)
- `0x0B` - Set amplitude (int)
- `0x0C` - Set noise threshold (int)
- `0x0D` - Set decay on/off (bool)
- `0x0E` - Set decay rate (int)

## Usage

### Building and Uploading
1. Install PlatformIO
2. Open the project in PlatformIO
3. Connect your ESP32 board
4. Update the upload port in `platformio.ini` if needed
5. Build and upload: `pio run -t upload`

### Operation Modes

#### Sound-Reactive Mode (Default)
- The umbrella analyzes audio input and creates visual effects
- Each LED strip responds to different frequency bands
- Height of the LED bars corresponds to audio intensity
- Uses primary palette for active LEDs, secondary for inactive

#### Regular Light Show Mode
- Disable sound sensitivity via BLE command `0x07`
- Access to all BMDevice light scenes and effects
- Standard palette streaming and other effects available

### Configuration
- All settings can be controlled via BLE from compatible apps
- Settings are automatically saved and restored on reboot
- Factory reset available via BLE command `0x1D`

## Differences from Original

### Improvements
- **Reduced Code Size**: ~250 lines vs 441 in original
- **Better BLE Integration**: Full status reporting and more commands
- **Persistent Settings**: No manual preference management needed
- **Shared Palette System**: Compatible with other BMDevice projects
- **Enhanced Error Handling**: Better initialization and error recovery

### Maintained Features
- Full FFT sound analysis
- 8 LED strip support
- I2S microphone integration
- All original sound parameters
- Same hardware pinout

## Troubleshooting

### Common Issues
1. **I2S Initialization Failed**: Check microphone connections and power
2. **No BLE Connection**: Ensure device name "Umbrella-CL" is advertised
3. **LEDs Not Working**: Verify GPIO pin connections and power supply
4. **Sound Not Reactive**: Check microphone placement and sensitivity settings

### Debug Output
- Enable serial monitor at 115200 baud
- Debug output shows BLE commands, sound parameters, and system status
- BMDevice provides detailed initialization logging

## Dependencies
- BMDevice library (custom)
- BurningManLEDs library (custom)
- FastLED
- ArduinoBLE
- ArduinoFFT
- ArduinoJson

## License
This project is part of the Burning Man LED ecosystem and follows the same licensing as the parent project. 