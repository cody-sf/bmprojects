# BTBeacon Test Program

A comprehensive ESP32 Bluetooth Low Energy (BLE) client program for testing connections to Bluetooth beacons with button functionality.

## Features

- **BLE Device Scanning**: Discover and list all nearby Bluetooth devices
- **Device Connection**: Connect to specific beacons by name or manually selected
- **Automatic Service Discovery**: Explore all available services and characteristics
- **Button Detection**: Automatically detect and monitor button presses from connected beacons
- **Interactive Menu**: Easy-to-use serial interface for testing different functions

## Hardware Requirements

- ESP32 development board (any ESP32 variant)
- Bluetooth beacon with button capability
- USB cable for programming and serial communication

## Software Requirements

- PlatformIO IDE or PlatformIO CLI
- ArduinoBLE library (automatically installed)

## Quick Start

1. **Build and Flash**:
   ```bash
   pio run --target upload
   ```

2. **Open Serial Monitor**:
   ```bash
   pio device monitor
   ```

3. **Follow the Interactive Menu**:
   - The program will show a menu with 5 options
   - Start by scanning for devices (option 1)
   - Connect to your beacon by name (option 2)

## How to Use

### Step 1: Scan for Devices
- Press `1` to scan for nearby BLE devices
- The program will scan for 10 seconds and display all found devices
- Each device shows: name, MAC address, signal strength, and advertised services

### Step 2: Connect to Your Beacon
- Press `2` to connect to a specific device by name
- Enter the name of your beacon when prompted
- The program will automatically find and connect to the device

### Step 3: Monitor Button Presses
- Once connected, the program automatically:
  - Discovers all services and characteristics
  - Subscribes to all notification characteristics
  - Monitors for button press events
- Press the button on your beacon to see the detected events

## Understanding the Output

### Device Discovery
```
Device #1: Name: MyBeacon | Address: aa:bb:cc:dd:ee:ff | RSSI: -45
  Advertised Service: 0x1800
  ---
```

### Service and Characteristic Discovery
```
Service 0: 0x1800
  Characteristic 0: 0x2a00 (Properties: 0x02) [R]
  Characteristic 1: 0x2a01 (Properties: 0x02) [R]
Service 1: 0x1801
  Characteristic 0: 0x2a05 (Properties: 0x20) [N]
    -> This characteristic supports notifications!
    -> This could be where button presses are reported
```

### Button Press Detection
```
🔔 BUTTON PRESS DETECTED! 🔔
Service: 0x1801
Characteristic: 0x2a05
Value length: 1
Raw value: 0x01 
As byte: 1 (binary: 1)
---
```

## Menu Options

1. **Scan for nearby BLE devices** - Discover all BLE devices in range
2. **Connect to device by name** - Connect to a specific beacon
3. **Disconnect current device** - Disconnect from the currently connected device
4. **Show connection status** - Display current connection information
5. **Show this menu** - Redisplay the menu options

## Common Beacon Types

### Generic BLE Beacons
- Usually have services like 0x1800 (Generic Access) and 0x1801 (Generic Attribute)
- Button notifications often come through characteristics with notification properties

### iBeacon/Eddystone
- May require specific service UUIDs for button functionality
- Check manufacturer documentation for button notification characteristics

### Custom Beacons
- May use proprietary service UUIDs
- The program will automatically discover and monitor all available characteristics

## Troubleshooting

### No Devices Found
- Ensure your beacon is powered on and in advertising mode
- Check if your beacon is within range (typically 10-50 feet)
- Some beacons require a button press to start advertising

### Connection Fails
- Try scanning first to verify the device is visible
- Make sure the beacon is not connected to another device
- Some beacons have connection limits

### Button Presses Not Detected
- The program subscribes to ALL notification characteristics automatically
- If no notifications are received, your beacon may use a different mechanism
- Check the characteristic discovery output for clues about which characteristics support notifications

### No Notification Characteristics Found
- Your beacon may not support button notifications
- Try pressing the button while connected to see if any characteristics change
- Check with your beacon manufacturer for button notification specifications

## Configuration

You can modify these settings in `main.cpp`:
- `SCAN_DURATION_MS`: How long to scan for devices (default: 10 seconds)
- `CONNECT_TIMEOUT_MS`: Connection timeout (default: 5 seconds)

## Technical Details

### BLE Configuration
- The ESP32 is configured as a BLE Central device (client)
- It connects TO other devices rather than advertising itself
- Supports multiple simultaneous characteristic subscriptions

### Button Detection Method
The program uses a comprehensive approach:
1. Discovers all services and characteristics
2. Subscribes to every characteristic that supports notifications
3. Monitors all subscribed characteristics for value updates
4. Reports any changes as potential button presses

## Example Session

```
BTBeacon Test Program
=====================
BLE initialized successfully!
Local MAC address: 24:0a:c4:xx:xx:xx

=== BTBeacon Test Menu ===
1. Scan for nearby BLE devices
2. Connect to device by name
3. Disconnect current device
4. Show connection status
5. Show this menu

Enter command number:
1
Scanning for BLE devices...
Device #1: Name: MyBeacon | Address: aa:bb:cc:dd:ee:ff | RSSI: -45
  Advertised Service: 0x1800
  ---
Scan complete. Found 1 devices.

Enter command number:
2
Enter device name to connect to:
MyBeacon
Scanning for device: MyBeacon
Found target device! Attempting to connect...
Connected successfully!
Discovering attributes...
Attributes discovered:
  Service 0: 0x1800
    Characteristic 0: 0x2a00 (Properties: 0x02) [R]
    Characteristic 1: 0x2a01 (Properties: 0x02) [R]
  Service 1: 0x1801
    Characteristic 0: 0x2a05 (Properties: 0x20) [N]
      -> This characteristic supports notifications!
      -> This could be where button presses are reported

=== Button Press Monitor ===
Looking for characteristics that can send notifications...
Subscribing to characteristic 0x2a05 in service 0x1801...
  -> Successfully subscribed!

Monitoring for button presses...
Press the button on your beacon!

🔔 BUTTON PRESS DETECTED! 🔔
Service: 0x1801
Characteristic: 0x2a05
Value length: 1
Raw value: 0x01 
As byte: 1 (binary: 1)
---
```

## Notes

- This program is designed to work with any BLE beacon that supports button notifications
- The automatic discovery approach means it should work with most beacon types
- For optimal results, consult your beacon's documentation for specific button notification details
- The program will continue monitoring until the device disconnects or you manually disconnect 