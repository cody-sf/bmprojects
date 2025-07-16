# BMGeneric Device

A simple implementation showcasing the generic device configuration capabilities built into the BMDevice library. This demonstrates dynamic device naming, runtime LED configuration, and BLE-based device setup - all handled automatically by the BMDevice library.

## Features (Built into BMDevice Library)

- **Runtime Configuration**: All settings stored in ESP32 Preferences (NVRAM)
- **Dynamic Device Naming**: Shows as "BMDevice - [OWNER]" or "BMDevice - New"
- **Multi-LED Support**: Up to 8 configurable LED strips
- **Flexible Pin Assignment**: Supports common ESP32/ESP32-C6 pins
- **Color Order Configuration**: Support for GRB, RGB, BRG, BGR, RBG, GBR
- **BLE Configuration**: Configure settings via mobile app
- **Persistent Storage**: Settings survive reboots and firmware updates
- **Cross-Platform**: Works on ESP32 and ESP32-C6 boards

**Note**: All these features are now built directly into the BMDevice library and available on every device that uses it!

## Supported Hardware

### ESP32 Boards
- Generic ESP32 development boards
- ESP32 DevKit v1
- Any ESP32 board with sufficient GPIO pins

### ESP32-C6 Boards  
- Seeed Studio Xiao ESP32-C6
- ESP32-C6-DevKitM-1
- Any ESP32-C6 board

### Supported GPIO Pins
The firmware supports the following GPIO pins for LED strips:
- Pin 2, 4, 5 (common for most ESP32 boards)
- Pin 16, 17, 18, 19 (additional ESP32 pins)
- Pin 21, 22 (I2C pins, use with caution)

**Note**: Pin 2 is used as fallback for unsupported pins.

## Build and Upload

### For Generic ESP32
```bash
pio run -e esp32_generic -t upload
```

### For ESP32-C6 (Seeed Xiao)
```bash
pio run -e seeed_xiao_esp32c6 -t upload
```

### Monitor Serial Output
```bash
pio device monitor -e esp32_generic
# or
pio device monitor -e seeed_xiao_esp32c6
```

## Default Configuration

When first flashed, the device starts with these defaults:

| Setting | Default Value |
|---------|---------------|
| Owner | Not Set (shows as "BMDevice - New") |
| Device Type | "Generic" |
| LED Strips | 1 active strip |
| LED Pin | GPIO 2 |
| LED Count | 30 LEDs |
| Color Order | GRB (0) |

## Configuration via BLE

The device can be configured through your mobile app using these BLE commands (defined in BMBluetoothHandler.h):

### Command Reference

| Command | Code | Constant | Description | Data Format |
|---------|------|----------|-------------|-------------|
| Set Owner | 0x30 | BLE_FEATURE_SET_OWNER | Set device owner name | ASCII string |
| Set Device Type | 0x31 | BLE_FEATURE_SET_DEVICE_TYPE | Set device type | ASCII string |
| Configure LED Strip | 0x32 | BLE_FEATURE_CONFIGURE_LED_STRIP | Configure LED strip parameters | See below |
| Get Configuration | 0x33 | BLE_FEATURE_GET_CONFIGURATION | Request current configuration | No data |
| Reset to Defaults | 0x34 | BLE_FEATURE_RESET_TO_DEFAULTS | Reset all settings to defaults | No data |

### LED Strip Configuration (Command 0x32)

Data format: `[stripIndex][pin][numLeds_high][numLeds_low][colorOrder][enabled]`

- **stripIndex** (1 byte): Strip index (0-7)
- **pin** (1 byte): GPIO pin number
- **numLeds_high** (1 byte): High byte of LED count
- **numLeds_low** (1 byte): Low byte of LED count  
- **colorOrder** (1 byte): Color order (0=GRB, 1=RGB, 2=BRG, 3=BGR, 4=RBG, 5=GBR)
- **enabled** (1 byte): Enable strip (0=disabled, 1=enabled)

### Example: Configure Strip 0 for 60 LEDs on Pin 5 with RGB order
```
Command: 0x32
Data: [0x00][0x05][0x00][0x3C][0x01][0x01]
      ^     ^     ^     ^     ^     ^
      |     |     |     |     |     enabled (1)
      |     |     |     |     RGB color order (1)
      |     |     |     LED count low byte (60)
      |     |     LED count high byte (0)
      |     GPIO pin 5
      Strip index 0
```

## Color Order Reference

| Code | Order | Description |
|------|-------|-------------|
| 0 | GRB | Green-Red-Blue (WS2812B default) |
| 1 | RGB | Red-Green-Blue |
| 2 | BRG | Blue-Red-Green |
| 3 | BGR | Blue-Green-Red |
| 4 | RBG | Red-Blue-Green |
| 5 | GBR | Green-Blue-Red |

## Serial Configuration Examples

Monitor the serial output to see configuration status and debug information:

### First Boot (Unconfigured)
```
=== BMGeneric Device Starting ===
No saved configuration found, using defaults
=== Device Configuration ===
Owner: Not Set
Device Type: Generic
Active LED Strips: 1
Configured: No
Strip 0: Pin 2, LEDs 30, Color Order 0
=============================
Device name: BMDevice - New
```

### After Owner Configuration
```
=== BMGeneric Device Starting ===
Configuration loaded from preferences
=== Device Configuration ===
Owner: CODY
Device Type: Generic
Active LED Strips: 1
Configured: Yes
Strip 0: Pin 2, LEDs 30, Color Order 0
=============================
Device name: BMDevice - CODY
```

### Multiple LED Strips Configuration
```
=== Device Configuration ===
Owner: ALEX
Device Type: Backpack
Active LED Strips: 3
Configured: Yes
Strip 0: Pin 2, LEDs 60, Color Order 0
Strip 1: Pin 5, LEDs 30, Color Order 1
Strip 2: Pin 16, LEDs 120, Color Order 0
=============================
Device name: BMDevice - ALEX
```

## Configuration Workflow

1. **Flash Firmware**: Upload to your ESP32/ESP32-C6 board
2. **Connect via App**: Device appears as "BMDevice - New"
3. **Set Owner**: Use command 0x30 to set owner name
4. **Configure LEDs**: Use command 0x32 to configure LED strips
5. **Set Device Type**: Use command 0x31 to set device type (optional)
6. **Verify**: Device now appears as "BMDevice - [OWNER]"

## Storage Locations

Settings are stored in ESP32 NVRAM under namespace "bmgeneric":

- `owner`: Device owner name
- `deviceType`: Device type string
- `ledCount`: Number of active LED strips
- `ledPins`: JSON array of LED strip configurations
- `colorOrders`: JSON array of color orders
- `configured`: Boolean flag indicating if device is configured

## Troubleshooting

### Device Not Appearing in App
- Check serial output for errors
- Verify BLE is working: look for "BMDevice - New" or "BMDevice - [OWNER]"
- Try resetting to defaults (command 0x34)

### LEDs Not Working
- Check pin configuration in serial output
- Verify pin is supported (see Supported GPIO Pins)
- Check power supply capacity for LED count
- Verify color order setting matches your LED strip

### Configuration Not Saving
- Check serial output for "Configuration saved to preferences"
- Try power cycling the device
- Use command 0x33 to verify current configuration

### Reset to Factory Defaults
Send BLE command 0x34 or:
```cpp
// In Arduino Serial Monitor, you can force a reset by modifying code temporarily
preferences.clear(); // Add this line temporarily in setup()
```

## Advanced Usage

### Custom Device Types
Set descriptive device types for organization:
- "Backpack", "Hat", "Jacket", "Umbrella", "Sign", etc.

### Multi-Strip Configurations
Examples of common setups:

**Backpack with Shoulder Straps**:
- Strip 0: Main panel (Pin 2, 200 LEDs)
- Strip 1: Left strap (Pin 5, 30 LEDs) 
- Strip 2: Right strap (Pin 16, 30 LEDs)

**Hat with Brim**:
- Strip 0: Crown (Pin 2, 60 LEDs)
- Strip 1: Brim (Pin 4, 40 LEDs)

**Large Installation**:
- Multiple strips on different pins for zone control
- Up to 8 strips supported

## Mobile App Integration

When implementing mobile app features:

1. **Device Discovery**: Look for devices advertising "BMDevice - " prefix
2. **Owner Display**: Parse owner from device name  
3. **Configuration UI**: Provide interface for setting owner, device type, and LED configurations
4. **Status Display**: Use BLE_FEATURE_GET_CONFIGURATION to show current configuration
5. **Factory Reset**: Provide BLE_FEATURE_RESET_TO_DEFAULTS for troubleshooting

**All BMDevice-based devices now support these features automatically!**

## Future Enhancements

Planned features for future versions:
- Web-based configuration interface
- OTA firmware updates with configuration preservation
- Additional LED chipset support (APA102, SK6812, etc.)
- GPIO pin validation and conflict detection
- Configuration backup/restore via JSON import/export

## License

This project is part of the Burning Man LED device ecosystem. 