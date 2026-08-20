# BMDevice Sync Testing Guide

This guide will help you test the new ESP-NOW sync functionality between BMDevice instances.

## What We Built

✅ **BMSyncController** - ESP-NOW based sync system
✅ **Device Discovery** - Automatic peer discovery by device owner
✅ **Sync Integration** - Fully integrated into BMDevice class
✅ **Bluetooth Coexistence** - Manages WiFi/BLE interference
✅ **Testing Interface** - Added to BMGenericDevice for easy testing

## Quick Start Testing

### 1. Flash Two Devices

Flash the **BMGenericDevice** project to two ESP32 devices:

```bash
cd BMGenericDevice
# For ESP32 Classic (recommended for sync testing)
pio run -e esp32 -t upload

# For SLUT target (if you have that hardware)
pio run -e slut -t upload

# ESP32-C6 has some library compatibility issues, use ESP32 classic for now
```

### 2. Set Device Owner (Auto-Enables Sync)

On both devices, open Serial Monitor and set the owner:

Device 1:
```
owner TEST
name Device1
```

Device 2:
```
owner TEST
name Device2
```

**Sync will automatically enable** when you set the owner! You should see:
```
=== Auto-Enabling Sync ===
Found device owner: TEST
Device name: Device1
✓ Sync automatically enabled!
```

### 3. Check Discovery

Wait 30-60 seconds, and the devices will automatically discover each other. You'll see periodic status updates like:
```
[Sync] Owner: TEST | Device: Device1 | Peers: 1
  Active peers:
    - TEST Device2 (5s ago)
```

You can also manually check:
```
peers
```

### 4. Test Sync

The system will automatically test sync every 2 minutes, or you can manually test:
```
test
```

This will change brightness and sync it to all peers. You should see both devices change brightness simultaneously!

## Available Commands

| Command | Description | Example |
|---------|-------------|---------|
| `owner [ID]` | Set device owner (auto-enables sync) | `owner CL` |
| `name [NAME]` | Set device name | `name Backpack` |
| `status` | Show sync status and peer info | `status` |
| `peers` | Show detailed peer list | `peers` |
| `discover` | Manually trigger discovery | `discover` |
| `test` | Test brightness sync | `test` |
| `sync` | Toggle sync on/off manually | `sync` |
| `help` | Show all commands | `help` |

## Expected Behavior

### Automatic Sync Enablement
- **Sync automatically enables** when you set a device owner
- No manual intervention needed - just set owner and name
- Displays clear status messages when sync starts

### Device Discovery  
- Devices with same owner automatically discover each other
- Discovery happens every 30 seconds automatically
- Peers timeout after 90 seconds of no communication
- **Automatic status updates** every 20 seconds showing peer count

### Sync Messages
- When you change settings on one device, it syncs to peers
- Supported settings: brightness, speed, palette, effect, direction, power, color
- Messages are filtered by device owner (only sync with same owner)
- **Auto-test** every 2 minutes if peers are available

### Serial Output
You should see messages like:
```
=== Auto-Enabling Sync ===
Found device owner: TEST
✓ Sync automatically enabled!

[Sync] Owner: TEST | Device: Device1 | Peers: 1
  Active peers:
    - TEST Device2 (5s ago)

[BMSyncController] Synced brightness to: 60
[BMDevice] Received sync message for setting: 1
```

## Troubleshooting

### No Peers Found
1. Check both devices have same owner: `status`
2. Make sure sync is enabled on both: `sync`
3. Try manual discovery: `discover`
4. Check WiFi channel (should be 6)

### Sync Not Working
1. Verify peers are discovered: `peers`
2. Check ESP-NOW initialization in serial output
3. Try restarting sync: `sync` (off) then `sync` (on)

### Compilation Issues
1. Make sure all BMDevice library files are present
2. Check that ESP32_NOW library is installed
3. Verify ArduinoJson library is available

## Real-World Usage

Once basic connectivity works, you can:

1. **Set Real Device Owners**: Use your initials instead of "TEST"
   ```
   owner CL
   ```

2. **Configure Device Types**: 
   ```
   name Backpack
   name Umbrella
   name CowboyHat
   ```

3. **Enable Automatic Sync**: Sync is enabled by default in BMDevice when owner is set

4. **Use BLE Apps**: The RNUmbrella app or other BLE controllers will trigger sync automatically

## Architecture Notes

- **ESP-NOW Channel**: Fixed to channel 6 for stability
- **Message Size**: Max 250 bytes per sync message
- **Peer Limit**: Up to 20 peers per device
- **Discovery Interval**: 30 seconds
- **Heartbeat**: Every 10 seconds to maintain connections
- **Bluetooth Coexistence**: Automatically manages WiFi/BLE conflicts

## Next Steps

After verifying basic connectivity:

1. Test with your actual device configurations (CL, AY, VM, etc.)
2. Test sync with BLE apps changing settings
3. Test with multiple device types (backpack, umbrella, etc.)
4. Test range and reliability in your typical usage environment

## Status

All core functionality is implemented and ready for testing. The system should automatically handle:
- Device discovery by owner
- Setting synchronization
- Connection management
- Bluetooth coexistence
- Error recovery

Let me know how the testing goes!
