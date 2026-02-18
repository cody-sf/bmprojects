# Expected Sync Discovery Logs

This document shows what you should expect to see in the serial logs when the aggressive sync discovery system is working.

## Device 1 Startup (owner TEST, name Device1)

```
=== BMGeneric Device Starting ===
... (LED and device setup) ...
=== BMGeneric Device Ready ===

=== Auto-Enabling Sync ===
Found device owner: TEST
Device name: Device1
✓ Sync automatically enabled!
Device is now discoverable by other devices with the same owner.
Use 'peers' to see discovered devices, 'test' to test sync.
=========================

[BMDevice] Auto-enabling sync for owner: TEST, device: Device1

=== BMSyncController Initialization ===
✓ ESP-NOW initialized on channel 6
✓ Device Owner: TEST
✓ Device Name: Device1
✓ Local MAC: AA:BB:CC:DD:EE:FF
✓ Bluetooth coexistence: enabled
✓ Ready for device discovery and sync!
========================================

[BMSyncController] Starting immediate device discovery...

🚀 [STARTUP DISCOVERY] Starting aggressive device discovery...
   Round 1/5
🔍 [DISCOVERY] Broadcasting device discovery...
   Owner: TEST | Device: Device1 | MAC: AA:BB:CC:DD:EE:FF
   Looking for other 'TEST' devices...
   Round 2/5
🔍 [DISCOVERY] Broadcasting device discovery...
   Owner: TEST | Device: Device1 | MAC: AA:BB:CC:DD:EE:FF
   Looking for other 'TEST' devices...
   Round 3/5
🔍 [DISCOVERY] Broadcasting device discovery...
   Owner: TEST | Device: Device1 | MAC: AA:BB:CC:DD:EE:FF
   Looking for other 'TEST' devices...
   Round 4/5
🔍 [DISCOVERY] Broadcasting device discovery...
   Owner: TEST | Device: Device1 | MAC: AA:BB:CC:DD:EE:FF
   Looking for other 'TEST' devices...
   Round 5/5
🔍 [DISCOVERY] Broadcasting device discovery...
   Owner: TEST | Device: Device1 | MAC: AA:BB:CC:DD:EE:FF
   Looking for other 'TEST' devices...
✅ [STARTUP DISCOVERY] Initial discovery rounds complete!
   Continuing with periodic discovery every 5 seconds...

📊 [SYNC STATUS UPDATE]
   Owner: TEST | Device: Device1 | Peers: 0
   🔍 No peers found yet - discovery in progress...
========================
```

## Device 2 Startup (owner TEST, name Device2) - SAME TIME

```
=== BMGeneric Device Starting ===
... (LED and device setup) ...
=== BMGeneric Device Ready ===

=== Auto-Enabling Sync ===
Found device owner: TEST
Device name: Device2
✓ Sync automatically enabled!
Device is now discoverable by other devices with the same owner.
Use 'peers' to see discovered devices, 'test' to test sync.
=========================

[BMDevice] Auto-enabling sync for owner: TEST, device: Device2

=== BMSyncController Initialization ===
✓ ESP-NOW initialized on channel 6
✓ Device Owner: TEST
✓ Device Name: Device2
✓ Local MAC: 11:22:33:44:55:66
✓ Bluetooth coexistence: enabled
✓ Ready for device discovery and sync!
========================================

[BMSyncController] Starting immediate device discovery...

🚀 [STARTUP DISCOVERY] Starting aggressive device discovery...
   Round 1/5
🔍 [DISCOVERY] Broadcasting device discovery...
   Owner: TEST | Device: Device2 | MAC: 11:22:33:44:55:66
   Looking for other 'TEST' devices...

📡 [DISCOVERY RECEIVED] Device discovery message received!
   From: TEST - Device1 (AA:BB:CC:DD:EE:FF)
🎉 [NEW PEER ADDED] Successfully added new sync peer!
   📝 Owner: TEST
   🏷️  Name: Device1
   📍 MAC: AA:BB:CC:DD:EE:FF
   📊 Total peers: 1
   🔄 Peer ready for sync operations!
📤 [DISCOVERY RESPONSE] Sent discovery response
   To: TEST - Device1 at AA:BB:CC:DD:EE:FF

   Round 2/5
🔍 [DISCOVERY] Broadcasting device discovery...
   Owner: TEST | Device: Device2 | MAC: 11:22:33:44:55:66
   Looking for other 'TEST' devices...
   ... (continues for 5 rounds) ...
✅ [STARTUP DISCOVERY] Initial discovery rounds complete!
   Continuing with periodic discovery every 5 seconds...
```

## Device 1 Receiving Device2's Discovery

```
📡 [DISCOVERY RECEIVED] Device discovery message received!
   From: TEST - Device2 (11:22:33:44:55:66)
🎉 [NEW PEER ADDED] Successfully added new sync peer!
   📝 Owner: TEST
   🏷️  Name: Device2
   📍 MAC: 11:22:33:44:55:66
   📊 Total peers: 1
   🔄 Peer ready for sync operations!
📤 [DISCOVERY RESPONSE] Sent discovery response
   To: TEST - Device2 at 11:22:33:44:55:66

📥 [DISCOVERY RESPONSE] Received discovery response!
   From: TEST - Device2 (11:22:33:44:55:66)
✅ [PEER ESTABLISHED] Connection established with TEST - Device2
```

## Ongoing Status Updates (Every 10 seconds for first 2 minutes)

```
📊 [SYNC STATUS UPDATE]
   Owner: TEST | Device: Device1 | Peers: 1
   🔗 Active peer connections:
     • TEST Device2 (last seen 2s ago)
========================

💓 [HEARTBEAT] Sent to 1 peers

🔍 [DISCOVERY] Broadcasting device discovery...
   Owner: TEST | Device: Device1 | MAC: AA:BB:CC:DD:EE:FF
   Looking for other 'TEST' devices...

📊 [SYNC STATUS UPDATE]
   Owner: TEST | Device: Device1 | Peers: 1
   🔗 Active peer connections:
     • TEST Device2 (last seen 7s ago)
========================
```

## Sync Test Messages

```
[Auto-Test] Testing sync with peers...

=== Sync Test ===
Syncing brightness to: 40
[BMSyncController] Synced brightness to: 40
Sync message sent to 1 peers
================

[BMDevice] Received sync message for setting: 1
[BMDevice] Synced brightness to: 40
```

## Summary of Expected Behavior

**🚀 Immediate Startup Discovery:**
- 5 rounds of discovery broadcasts immediately on startup
- 1 second delay between rounds
- Multiple broadcasts per round for reliability

**📡 Aggressive Early Discovery:**
- Discovery every 5 seconds for first 2 minutes
- Status updates every 10 seconds for first 2 minutes
- Then normal intervals (30s discovery, 20s status)

**💓 Frequent Heartbeats:**
- Heartbeat every 5 seconds to all peers
- Maintains connection health

**🎉 Verbose Peer Management:**
- Clear messages when peers are discovered
- Detailed information about each peer
- Real-time connection status

**🔄 Automatic Sync:**
- Settings sync immediately when changed
- Auto-test every 2 minutes
- Clear sync confirmation messages

This system ensures that devices find each other within seconds of startup and maintain robust connections with comprehensive logging for debugging.
