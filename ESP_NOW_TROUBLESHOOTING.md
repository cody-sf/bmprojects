# ESP-NOW Discovery Troubleshooting Guide

## Enhanced Diagnostics Added

The firmware now includes comprehensive diagnostics to help identify ESP-NOW issues:

### New Commands Available:
- `wifi` - Show WiFi diagnostics (mode, status, channel, MAC)
- `ping` - Test ESP-NOW broadcast (sends 3 discovery rounds)
- `status` - Enhanced sync status with more details
- `ble on/off` - Control BLE advertising

### Enhanced Logging:
- **WiFi Setup**: Detailed WiFi initialization logging
- **ESP-NOW TX**: Shows if messages are sent successfully or failed
- **ESP-NOW RX**: Shows all received messages with RSSI, MAC, content
- **Message Validation**: Shows checksum validation results

## Step-by-Step Troubleshooting

### 1. Flash and Setup Both Devices
```bash
pio run -e esp32 -t upload
```

### 2. Check Basic Setup
On both devices, run these commands and compare outputs:

```
wifi
```
Expected output:
```
=== WiFi Diagnostics ===
WiFi Mode: STA (Station)
WiFi Status: DISCONNECTED
WiFi Channel: 6
WiFi MAC: XX:XX:XX:XX:XX:XX
========================
```

### 3. Set Owners and Check Sync Status
```
owner TEST
name Device1    (on first device)
name Device2    (on second device)
status
```

Expected output:
```
🔧 [WIFI] Setting up WiFi for ESP-NOW...
   WiFi mode: STA
   WiFi channel: 6
   WiFi status: 0
   Local MAC: XX:XX:XX:XX:XX:XX
🔧 [ESP-NOW] Initializing ESP-NOW...
✅ ESP-NOW initialized successfully
=== BMSyncController Initialization ===
✓ ESP-NOW initialized on channel 6
✓ Device Owner: TEST
✓ Device Name: Device1
✓ Local MAC: XX:XX:XX:XX:XX:XX
✓ Bluetooth coexistence: enabled
⚠️  Note: BLE advertising should be disabled for best ESP-NOW performance!
✓ Ready for device discovery and sync!
========================================
```

### 4. Test ESP-NOW Transmission
On one device, run:
```
ping
```

**What to look for:**
- **On sending device**: Should see `📡 [ESP-NOW TX] Send SUCCESS` messages
- **On receiving device**: Should see `🔍 [ESP-NOW RX] Data received!` messages

### 5. Manual Discovery Test
```
discover
```

Should trigger discovery broadcast and show detailed TX/RX logs.

## Common Issues and Solutions

### Issue 1: No TX Success Messages
If you see `📡 [ESP-NOW TX] Send FAILED` messages:

**Possible causes:**
- ESP-NOW not properly initialized
- WiFi interference
- Hardware issue

**Solutions:**
1. Check WiFi diagnostics: `wifi`
2. Try different WiFi channel (modify `ESPNOW_WIFI_CHANNEL` in code)
3. Move devices closer together
4. Restart both devices

### Issue 2: TX Success but No RX on Other Device
If Device A shows successful sends but Device B receives nothing:

**Possible causes:**
- Different WiFi channels
- MAC address issues
- Distance/interference
- One device not in STA mode

**Solutions:**
1. Compare `wifi` output on both devices
2. Check MAC addresses are different
3. Move devices within 1 meter of each other
4. Ensure both devices show same WiFi channel

### Issue 3: RX Messages but Wrong Content
If receiving messages but wrong owner/name:

**Possible causes:**
- Data corruption
- Checksum failures
- Buffer issues

**Solutions:**
1. Look for checksum validation messages
2. Check raw data dump in RX logs
3. Verify owner/name settings with `status`

### Issue 4: Intermittent Discovery
If discovery works sometimes but not always:

**Possible causes:**
- WiFi interference
- Timing issues
- Power supply problems

**Solutions:**
1. Use `ping` command multiple times
2. Check for consistent TX success rates
3. Monitor RSSI values in RX logs
4. Try different locations

## Expected Working Logs

### Device 1 Sending Discovery:
```
🚀 [STARTUP DISCOVERY] Starting aggressive device discovery...
   Round 1/5
🔍 [DISCOVERY] Broadcasting device discovery...
   Owner: TEST | Device: Device1 | MAC: AA:BB:CC:DD:EE:FF
   Looking for other 'TEST' devices...
📡 [ESP-NOW TX] Send SUCCESS to FF:FF:FF:FF:FF:FF
```

### Device 2 Receiving Discovery:
```
🔍 [ESP-NOW RX] Data received!
   Length: 284 bytes (expected: 284)
   From MAC: AA:BB:CC:DD:EE:FF
   RSSI: -45
   Message type: 1
   Device owner: TEST
   Device name: Device1
   ✅ Checksum valid
📡 [DISCOVERY RECEIVED] Device discovery message received!
   From: TEST - Device1 (AA:BB:CC:DD:EE:FF)
🎉 [NEW PEER ADDED] Successfully added new sync peer!
```

## Advanced Diagnostics

### Check ESP-NOW Packet Structure
The RX logs show the raw packet data. A valid packet should:
- Be exactly 284 bytes (size of BMSyncData struct)
- Have valid checksum
- Contain readable owner/name strings

### Monitor Signal Strength
RSSI values in RX logs indicate signal strength:
- **-30 to -50 dBm**: Excellent
- **-50 to -70 dBm**: Good  
- **-70 to -85 dBm**: Fair
- **Below -85 dBm**: Poor (may cause issues)

### Check for Interference
If you see intermittent failures:
1. Turn off other 2.4GHz devices (routers, microwaves, etc.)
2. Try different physical locations
3. Check WiFi scanner for channel 6 congestion

## Next Steps if Still Not Working

1. **Hardware Test**: Try with different ESP32 boards
2. **Channel Test**: Modify code to use channel 1 or 11
3. **Range Test**: Test with devices 10cm apart
4. **Isolation Test**: Test in area with no WiFi networks
5. **Library Version**: Check ESP-NOW library compatibility

The enhanced diagnostics should help identify exactly where the communication is failing!
