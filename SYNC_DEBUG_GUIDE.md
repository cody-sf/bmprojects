# BMDevice Sync Debugging Guide

## How the Sync System Works

### **Step-by-Step Sync Process:**

1. **Device Initialization**
   ```
   [BMDevice] Checking sync settings - syncEnabled: true, owner: 'cody'
   [BMDevice] Auto-enabling sync for owner: 'cody'
   [BMSync] *** SYNC INITIALIZED ***
   [BMSync] Owner: 'cody'
   [BMSync] Device: 'cody - Generic'
   [BMSync] Local MAC: AA:BB:CC:DD:EE:FF
   [BMSync] Sending initial discovery broadcast...
   ```

2. **Device Discovery**
   ```
   [BMSync] *** DISCOVERY MESSAGE RECEIVED ***
   [BMSync] From: cody - Generic (11:22:33:44:55:66)
   [BMSync] Owner: 'cody' (our owner: 'cody')
   [BMSync] Device Type: 'Generic'
   [BMSync] Owner matches! Adding peer...
   [BMSync] ✅ Added new peer: cody - Generic
   ```

3. **State Changes & Sync**
   ```
   [BMDevice] *** SYNC TRIGGERED ***
   [BMDevice] Changed properties: 0x00000002
   [BMDevice] - Effects changed
   [BMSync] *** SENDING LIGHTSHOW UPDATE ***
   [BMSync] Changed properties: 0x00000002
   [BMSync] Broadcasting to 1 peers
   [BMSync] Lightshow update sent: ✅ Success
   ```

4. **Receiving Updates**
   ```
   [BMSync] *** LIGHTSHOW UPDATE RECEIVED ***
   [BMSync] From: cody - Generic (11:22:33:44:55:66)
   [BMSync] ✅ Valid peer and owner - processing update...
   [BMSync] Changed properties: 0x00000002
   [BMSync] Applying changes to local device...
   [BMDevice] Sync: Effect -> meteor_shower
   [BMSync] ✅ Applied lightshow update from cody - Generic
   ```

## Debugging Your Sync Issue

### **Step 1: Check Device Owner Settings**

Flash the debug version and check the serial output for:

```
[BMDevice] Checking sync settings - syncEnabled: ?, owner: '?'
```

**Expected**: Both devices should show:
- `syncEnabled: true`
- `owner: 'cody'` (exactly matching, case sensitive)

**If you see**:
- `owner: 'New'` or `owner: ''` → Device owner not set
- `syncEnabled: false` → Sync disabled in preferences

### **Step 2: Check Sync Initialization**

Look for this on both devices:
```
[BMSync] *** SYNC INITIALIZED ***
[BMSync] Owner: 'cody'
[BMSync] Device: 'cody - Generic'
[BMSync] Local MAC: XX:XX:XX:XX:XX:XX
```

**If missing**: Sync not starting - check device owner setting

### **Step 3: Check Discovery Process**

Within 30 seconds, you should see:
```
[BMSync] Sending initial discovery broadcast...
[BMSync] *** DISCOVERY MESSAGE RECEIVED ***
[BMSync] From: cody - Generic (MAC)
[BMSync] Owner: 'cody' (our owner: 'cody')
[BMSync] Owner matches! Adding peer...
[BMSync] ✅ Added new peer: cody - Generic
```

**If you see**:
- No discovery messages → ESP-NOW not working or devices too far apart
- `Owner mismatch` → Device owners don't match exactly
- `Unknown peer` → Discovery working but peer not being added

### **Step 4: Check Periodic Status**

Every 30 seconds you'll see:
```
=== BMDevice Sync Status ===
Sync Enabled: Yes
Sync Controller: Active
[BMSync] Active peers (1):
  cody - Generic (11:22:33:44:55:66) - Owner: cody, GPS: No, MaxBri: 100
[BMSync] Stats - Sent: 5, Received: 3, Conflicts: 0
```

### **Step 5: Test State Changes**

When you change an effect via BLE, you should see:
```
[BMDevice] *** SYNC TRIGGERED ***
[BMDevice] Changed properties: 0x00000002
[BMDevice] - Effects changed
[BMSync] *** SENDING LIGHTSHOW UPDATE ***
[BMSync] Broadcasting to 1 peers
[BMSync] Lightshow update sent: ✅ Success
```

And on the other device:
```
[BMSync] *** LIGHTSHOW UPDATE RECEIVED ***
[BMSync] From: cody - Generic
[BMSync] ✅ Valid peer and owner - processing update...
[BMDevice] Sync: Effect -> meteor_shower
```

## Common Issues & Solutions

### **Issue 1: No Discovery Messages**
**Symptoms**: No `DISCOVERY MESSAGE RECEIVED` logs
**Causes**:
- Devices too far apart (ESP-NOW range ~200m)
- WiFi interference
- ESP-NOW not initialized properly

**Solutions**:
- Move devices closer (within 10m for testing)
- Check for WiFi interference
- Restart both devices

### **Issue 2: Owner Mismatch**
**Symptoms**: `Owner mismatch` in discovery logs
**Causes**:
- Device owners set differently ("cody" vs "Cody" vs "CODY")
- One device has owner "New" or empty

**Solutions**:
- Use RNUmbrella to set identical owner on both devices
- Owner is case-sensitive: "cody" ≠ "Cody"
- Check: `device.getDefaults().getCurrentDefaults().owner`

### **Issue 3: Sync Not Enabled**
**Symptoms**: `Sync Enabled: No` in status
**Causes**:
- Sync disabled in device preferences
- Device owner not set or set to "New"

**Solutions**:
```cpp
// Via BLE command 0x24
sendBLECommand(0x24, [1]); // Enable sync

// Or reset to factory defaults (sync enabled by default)
sendBLECommand(0x1D, []); // Reset to factory
```

### **Issue 4: Messages Sent But Not Received**
**Symptoms**: `Lightshow update sent: ✅ Success` but no corresponding received message
**Causes**:
- ESP-NOW channel mismatch
- Message corruption
- Peer not properly added

**Solutions**:
- Check peer list in sync status
- Restart devices to refresh ESP-NOW
- Verify both devices on same WiFi channel

## Manual Testing Commands

### **Force Sync Status Print**
Add this to your loop() for immediate debugging:
```cpp
// In main.cpp loop()
static bool debugPrinted = false;
if (!debugPrinted && millis() > 10000) { // 10 seconds after boot
    device.printSyncStatus();
    debugPrinted = true;
}
```

### **Force Effect Change**
Add a button handler to test sync:
```cpp
// Test sync by changing effect
static int testEffectIndex = 0;
LightSceneID testEffects[] = {
    LightSceneID::meteor_shower,
    LightSceneID::fire_plasma,
    LightSceneID::rainbow_comet
};

if (buttonPressed) {
    testEffectIndex = (testEffectIndex + 1) % 3;
    device.setEffect(testEffects[testEffectIndex]);
    Serial.printf("Manual effect change to: %s\n", 
                 LightShow::effectIdToName(testEffects[testEffectIndex]));
}
```

## Expected Debug Output (Working Sync)

### **Device 1 (Initiator)**
```
[BMDevice] Auto-enabling sync for owner: 'cody'
[BMSync] *** SYNC INITIALIZED ***
[BMSync] Owner: 'cody'
[BMSync] Local MAC: AA:BB:CC:DD:EE:FF
[BMSync] Sending initial discovery broadcast...

[BMSync] *** DISCOVERY MESSAGE RECEIVED ***
[BMSync] From: cody - Generic (11:22:33:44:55:66)
[BMSync] Owner: 'cody' (our owner: 'cody')
[BMSync] Owner matches! Adding peer...
[BMSync] ✅ Added new peer: cody - Generic

[BMDevice] *** SYNC TRIGGERED ***
[BMDevice] - Effects changed
[BMSync] *** SENDING LIGHTSHOW UPDATE ***
[BMSync] Broadcasting to 1 peers
[BMSync] Lightshow update sent: ✅ Success
```

### **Device 2 (Receiver)**
```
[BMDevice] Auto-enabling sync for owner: 'cody'
[BMSync] *** SYNC INITIALIZED ***
[BMSync] Owner: 'cody'
[BMSync] Local MAC: 11:22:33:44:55:66
[BMSync] Sending initial discovery broadcast...

[BMSync] *** DISCOVERY MESSAGE RECEIVED ***
[BMSync] From: cody - Generic (AA:BB:CC:DD:EE:FF)
[BMSync] Owner: 'cody' (our owner: 'cody')
[BMSync] Owner matches! Adding peer...
[BMSync] ✅ Added new peer: cody - Generic

[BMSync] *** LIGHTSHOW UPDATE RECEIVED ***
[BMSync] From: cody - Generic (AA:BB:CC:DD:EE:FF)
[BMSync] ✅ Valid peer and owner - processing update...
[BMDevice] Sync: Effect -> meteor_shower
[BMSync] ✅ Applied lightshow update from cody - Generic
```

## Quick Debugging Checklist

1. **✅ Device Owner Set**: Check both devices have owner "cody" (not "New")
2. **✅ Sync Enabled**: Check `syncEnabled: true` in defaults
3. **✅ ESP-NOW Working**: Check for discovery messages between devices
4. **✅ Peer Discovery**: Check peer count > 0 in sync status
5. **✅ Change Detection**: Check for "SYNC TRIGGERED" when changing effects
6. **✅ Message Transmission**: Check for "Lightshow update sent: ✅ Success"
7. **✅ Message Reception**: Check for "LIGHTSHOW UPDATE RECEIVED" on other device

Flash the debug version to both devices and check the serial output against this guide to identify where the sync process is failing!

