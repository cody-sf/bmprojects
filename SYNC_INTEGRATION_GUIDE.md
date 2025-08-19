# BMDevice Synchronization Integration Guide

## Overview

The BMDevice library now includes a comprehensive synchronization system that allows multiple devices to automatically sync their lightshow settings in real-time using ESP-NOW communication.

## How It Works

### **Automatic Device Discovery**
- Devices automatically discover each other based on the **device owner** setting
- No need to hardcode MAC addresses or maintain device lists
- Uses ESP-NOW for fast, reliable local communication

### **User-Based Grouping**
- All devices with the same **device owner** automatically form a sync group
- Example: All devices owned by "CL" will sync with each other
- Devices with different owners remain independent

### **Smart Property Sync**
- **✅ Syncs**: Effects, palettes, brightness, speed, direction, effect parameters, colors
- **✅ GPS Speed Sharing**: Devices with GPS share speed data for speedometer effects
- **❌ Doesn't Sync**: GPS position, device-specific settings, LED configuration

## Current Implementation Status

### **✅ BMGenericDevice Integration**
The sync system is now fully integrated into BMGenericDevice:

1. **Default Enabled**: Sync is enabled by default when device owner is set
2. **Automatic Startup**: Sync starts automatically when BMGenericDevice boots (if owner is set)
3. **BLE Control**: Sync can be enabled/disabled via BLE command `0x24`
4. **Persistent Setting**: Sync enabled/disabled state is saved in device preferences
5. **Platform Optimized**: 
   - **ESP32-C6**: Sync enabled, GPS disabled (saves flash space)
   - **ESP32 Classic**: Sync + GPS both enabled (full features)

### **✅ BLE Control Interface**

#### Enable/Disable Sync
```
Feature: 0x24 (BLE_FEATURE_SET_SYNC_ENABLED)
Data: [0x24, enabled] where enabled = 1 (enable) or 0 (disable)
```

#### Current Status Fields
The device status now includes sync information:
```json
{
  "sync": true,        // Current sync state (runtime)
  "syncCfg": true,     // Configured sync state (saved in preferences)
  "owner": "CL",       // Device owner (required for sync)
  // ... other fields
}
```

## RNUmbrella Integration Plan

### **Phase 1: Basic Sync Control** ✅ Ready to Implement

Add sync toggle to the device settings screen:

```typescript
// In device settings screen
const [syncEnabled, setSyncEnabled] = useState(deviceStatus.syncCfg);

const toggleSync = async () => {
  const newState = !syncEnabled;
  await sendBLECommand(0x24, [newState ? 1 : 0]);
  setSyncEnabled(newState);
};

// UI Component
<View style={styles.settingRow}>
  <Text style={styles.settingLabel}>Device Synchronization</Text>
  <Switch
    value={syncEnabled}
    onValueChange={toggleSync}
    disabled={!deviceStatus.owner || deviceStatus.owner === "New"}
  />
  <Text style={styles.settingDescription}>
    {deviceStatus.owner && deviceStatus.owner !== "New" 
      ? `Sync with other "${deviceStatus.owner}" devices`
      : "Set device owner first to enable sync"
    }
  </Text>
</View>
```

### **Phase 2: Sync Status Indicators** 

Show sync status and connected peers:

```typescript
// Sync status indicator
const SyncStatusIndicator = ({ deviceStatus }) => {
  if (!deviceStatus.sync) return null;
  
  return (
    <View style={styles.syncStatus}>
      <Icon name="wifi" color={deviceStatus.sync ? "green" : "gray"} />
      <Text>Sync: {deviceStatus.sync ? "Active" : "Disabled"}</Text>
      {/* Future: Show peer count when available */}
    </View>
  );
};
```

### **Phase 3: Advanced Sync Controls** (Future)

```typescript
// Advanced sync settings
interface SyncSettings {
  syncEffects: boolean;      // Sync effects and palettes
  syncBrightness: boolean;   // Sync brightness (scaled)
  syncSpeed: boolean;        // Sync animation speed
  syncColors: boolean;       // Sync effect colors
  shareGPSSpeed: boolean;    // Share GPS speed data
}
```

## Device Owner Management

### **Setting Device Owner**
The device owner must be set before sync can be enabled:

```typescript
// Set device owner (existing BLE command)
const setDeviceOwner = async (owner: string) => {
  const ownerBytes = new TextEncoder().encode(owner);
  await sendBLECommand(0x1F, ownerBytes); // BLE_FEATURE_SET_DEVICE_OWNER
};
```

### **Owner Validation**
- Owner must be set and not "New" for sync to work
- Recommended format: 2-3 character initials (e.g., "CL", "AY", "VM")
- Case sensitive matching

## Technical Details

### **Sync Architecture**
```
Device A (Owner: "CL") ←→ ESP-NOW ←→ Device B (Owner: "CL")
     ↓                                      ↓
   Change effect                      Receives change
   Broadcasts update                  Updates effect
```

### **Message Flow**
1. User changes effect on Device A via RNUmbrella
2. Device A detects state change
3. Device A broadcasts sync message via ESP-NOW
4. All "CL" devices receive message
5. Devices update their lightshow to match
6. Brightness is scaled based on each device's max brightness setting

### **Conflict Resolution**
- Last change wins (simple conflict resolution)
- Changes are timestamped to handle simultaneous updates
- Each device tracks its last sync state to detect changes

### **GPS Speed Sharing**
- Devices with GPS automatically share speed data
- Non-GPS devices can use shared speed for speedometer effects
- Only speed is shared, not position (privacy/performance)

## Error Handling

### **Common Issues & Solutions**

1. **Sync Not Working**
   - Check device owner is set and not "New"
   - Verify sync is enabled (`syncCfg: true`)
   - Ensure devices are within ESP-NOW range (~200m)

2. **Devices Not Finding Each Other**
   - Check all devices have identical owner string
   - Restart devices to refresh discovery
   - Verify WiFi channel consistency

3. **Brightness Scaling Issues**
   - Each device scales brightness based on its max brightness setting
   - 50% brightness on 100-max device = 25 brightness
   - 50% brightness on 200-max device = 100 brightness

## Testing Sync

### **Basic Test Procedure**
1. Set same device owner on 2+ devices (via RNUmbrella BLE app)
2. Sync is enabled by default - verify in device status
3. Change effect on one device via RNUmbrella
4. Verify all devices update to match
5. Test brightness scaling with different max brightness settings

### **Platform-Specific Testing**
- **ESP32-C6**: Test sync functionality (no GPS features available)
- **ESP32 Classic**: Test sync + GPS speed sharing for speedometer effects
- **Mixed Groups**: C6 + ESP32 devices can sync together (C6 receives GPS speed from ESP32)

### **Debug Information**
Enable serial monitoring to see sync messages:
```
[BMSync] Initialized for owner 'CL', device 'Generic'
[BMSync] Discovered new peer: CL - Backpack
[BMSync] Applied lightshow update from CL - Umbrella
```

## Future Enhancements

### **Planned Features**
- [ ] Peer count and device list in status
- [ ] Selective sync (choose what properties to sync)
- [ ] Sync groups (multiple sync groups per owner)
- [ ] Conflict resolution improvements
- [ ] Sync statistics and diagnostics

### **RNUmbrella Features**
- [ ] Visual sync status indicators
- [ ] Peer device discovery and listing
- [ ] Advanced sync configuration
- [ ] Sync diagnostics screen
- [ ] Group management interface

## Implementation Notes

### **BMGenericDevice Changes**
- Sync is automatically initialized when device owner is set
- No code changes needed - sync works out of the box
- BLE command 0x24 controls sync enable/disable
- Status updates include sync information

### **Backwards Compatibility**
- Existing devices without sync continue to work normally
- Sync is optional - devices work independently if sync is disabled
- No breaking changes to existing BLE API

### **Performance Impact**
- Minimal memory usage (~2KB for sync controller)
- Low CPU impact (sync messages only sent when changes occur)
- ESP-NOW is very fast (<10ms latency for local sync)

This sync system provides a solid foundation for coordinated lightshows across multiple devices while maintaining the flexibility and reliability of the BMDevice ecosystem.
