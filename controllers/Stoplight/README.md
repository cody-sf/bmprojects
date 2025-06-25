# ESP32 Stoplight Controller

A Bluetooth-enabled ESP32 controller for managing a 3-light stoplight system with 120V bulbs via relay switches.

## Features

- **Dual Mode Support**:
  - American Style: Red → Green → Yellow → Red
  - European Style: Red → Red+Yellow → Green → Yellow → Red

- **Control Options**:
  - Auto-cycling with configurable timing
  - Manual control of individual lights
  - All lights off mode

- **Bluetooth Control**:
  - BLE communication for wireless control
  - Real-time status updates
  - Persistent settings storage

- **Safety Features**:
  - Relay-based switching for 120V isolation
  - Configurable timing for each light phase
  - State management with proper transitions

## Hardware Setup

### Required Components
- ESP32 development board
- 3x Relay modules (capable of switching 120V AC)
- 3x 120V light bulbs (Red, Yellow, Green)
- Appropriate electrical enclosure and wiring

### Pin Configuration
- `RED_RELAY_PIN` (GPIO 18): Controls red light relay
- `YELLOW_RELAY_PIN` (GPIO 19): Controls yellow light relay  
- `GREEN_RELAY_PIN` (GPIO 21): Controls green light relay

⚠️ **SAFETY WARNING**: This project involves 120V AC wiring. Only qualified electricians should handle the high-voltage connections. Always follow local electrical codes and safety practices.

## BLE Command Protocol

The device uses BLE characteristics for communication:

### Commands (Write to Command Characteristic)
- `0x01`: Toggle auto-cycle mode (byte 1: 0=off, 1=on)
- `0x02`: Manual red light
- `0x03`: Manual yellow light
- `0x04`: Manual green light
- `0x05`: Manual red+yellow light (European style)
- `0x06`: All lights off
- `0x07`: Set red duration (4-byte integer in ms)
- `0x08`: Set yellow duration (4-byte integer in ms)
- `0x09`: Set green duration (4-byte integer in ms)
- `0x0A`: Set red+yellow duration (4-byte integer in ms)
- `0x0B`: Set stoplight mode (4-byte integer: 0=American, 1=European)
- `0x0F`: Save settings to flash
- `0x10`: Restore factory defaults

### Status Updates (Read/Notify from Status Characteristic)
JSON format with current state:
```json
{
  "active": true,
  "autoCycle": true,
  "state": 1,
  "mode": 0,
  "redOn": true,
  "yellowOn": false,
  "greenOn": false,
  "timeInState": 15000
}
```

### Configuration Updates
JSON format with current settings:
```json
{
  "redDur": 30000,
  "yellowDur": 3000,
  "greenDur": 25000,
  "redYellowDur": 2000,
  "mode": 0,
  "autoCycle": false
}
```

## Default Settings

- **Red Duration**: 30 seconds
- **Yellow Duration**: 3 seconds  
- **Green Duration**: 25 seconds
- **Red+Yellow Duration**: 2 seconds (European mode only)
- **Default Mode**: American
- **Auto-cycle**: Disabled

## Building and Uploading

1. Install PlatformIO
2. Clone/download this project
3. Connect your ESP32 via USB
4. Run: `pio run -t upload`
5. Monitor serial output: `pio device monitor`

## BLE Device Name
The device advertises as: `Stoplight-CL`

## Circuit Diagram Notes

```
ESP32 GPIO → Relay IN → Relay NO → 120V Hot → Light Bulb → 120V Neutral
                    ↓
               Relay COM → 120V Hot (mains)
```

Each relay should be wired to switch the hot (live) wire to each light bulb. The neutral wire can be connected directly to all bulbs. Always use proper electrical enclosures and follow local codes. 