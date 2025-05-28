# BTBackpackV2

A Bluetooth-enabled LED backpack controller for ESP32 using PlatformIO. This device integrates with the RNUmbrella React Native app for remote control and monitoring.

## Features

- **Multiple LED Modes:**
  - Palette Stream - Flowing colors from selected palette
  - Palette Cycle - Cycling through palette colors
  - Speedometer - Visual speed indicator using GPS
  - Color Wheel - Color based on direction from origin
  - Color Radial - Color based on distance from origin
  - Position Status - GPS lock indicator

- **Bluetooth LE Control:**
  - Power on/off
  - Mode selection
  - Brightness control (1-100)
  - Speed control (1-100)
  - Direction control (up/down)
  - Palette selection
  - Custom speedometer colors
  - Origin point configuration

- **GPS Integration:**
  - Real-time position tracking
  - Speed calculation
  - Distance and direction from origin

- **Physical Button:**
  - Short press: Cycle through modes
  - Long press: Cycle through palettes
  - Very long press: Toggle power

## Hardware Requirements

- ESP32 development board
- WS2812B LED strip (192 LEDs)
- GPS module (connected via Serial)
- Push button
- 5V power supply

## Pin Configuration

- LED Data Pin: GPIO 27
- Button Pin: GPIO 2
- GPS: Default Serial pins

## Building and Uploading

1. Install PlatformIO IDE or CLI
2. Clone this repository
3. Connect your ESP32 board
4. Build and upload:
   ```bash
   cd BTBackpackV2
   pio run -t upload
   ```

## Bluetooth Protocol

### Service UUID
`be03096f-9322-4360-bc84-0f977c5c3c10`

### Characteristics

#### Features (Write)
UUID: `24dcb43c-d457-4de0-a968-9cdc9d60392c`

Commands:
- `0x01` + `[0/1]` - Power off/on
- `0x02` + `[mode_string]` - Set mode (pstream, pcycle, speedo, cwheel, cradial, pstat)
- `0x03` + `[palette_string]` - Set palette
- `0x04` + `[int32]` - Set brightness (1-100)
- `0x05` + `[int32]` - Set speed (5-200, inverted in app)
- `0x06` + `[0/1]` - Set direction
- `0x07` + `[float][float]` - Set origin (latitude, longitude)
- `0x08` - Request status update
- `0x09` + `[rgb][rgb]` - Set speedometer colors

#### Status (Notify)
UUID: `71a0cb09-7998-4774-83b5-1a5f02f205fb`

JSON status updates containing:
```json
{
  "power": true,
  "currentSceneId": "pstream",
  "brightness": 50,
  "speed": 100,
  "direction": true,
  "palette": "cool",
  "position_available": true,
  "current_speed": 15.5,
  "current_position": {
    "latitude": 40.7868,
    "longitude": -119.2065
  }
}
```

## RNUmbrella App Integration

The device is fully compatible with the RNUmbrella React Native app. Features supported:

- Device discovery and connection
- Real-time control of all settings
- Live status updates
- GPS position and speed monitoring
- Custom color selection for speedometer mode
- Persistent user settings

## Configuration

Default settings can be modified in `main.cpp`:
- `NUM_LEDS`: Number of LEDs in the strip
- `LED_OUTPUT_PIN`: GPIO pin for LED data
- `BUTTON_PIN`: GPIO pin for button input
- Default origin coordinates
- Default colors and palettes

## Troubleshooting

1. **LEDs not lighting up:**
   - Check power supply voltage and current
   - Verify LED_OUTPUT_PIN matches your wiring
   - Ensure COLOR_ORDER matches your LED strip

2. **Bluetooth connection issues:**
   - Ensure BLE is enabled on your phone
   - Check that the device name matches in the app
   - Try forgetting and re-pairing the device

3. **GPS not working:**
   - Ensure GPS module has clear view of sky
   - Check serial connections
   - Verify baud rate matches GPS module

## License

This project is part of the Burning Man LED ecosystem and follows the same licensing terms as the parent project. 