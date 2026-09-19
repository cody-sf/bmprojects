#pragma once
#include <stdint.h>
#include <stddef.h>

/**
 * BLE central for the one saved BMDevice, the CYD equivalent of
 * BluetoothProvider's standing reconnect: while the target is missing a scan
 * runs, and the moment its address is seen we connect, subscribe to status
 * notifications and ask for the full chunk burst (0x02) - the same handshake
 * the phone and watch do. Status arrives as JSON chunks, each a complete
 * object with a subset of keys; they are merged into AppState here.
 *
 * All NimBLE calls live on one FreeRTOS task; the UI talks to it through a
 * write queue and request flags, and never blocks on the radio. Writes go
 * with response - the features characteristic accepts nothing else.
 */

void bmInit();

// Queue one encoded command (BmCodec.h). Fire-and-forget from the UI.
void bmQueueWrite(const uint8_t* data, size_t len);

// Discovery mode for the Choose Light screen: scan and fill app.found with
// every BMDevice in range, not just the saved target.
void bmSetDiscover(bool on);
bool bmIsScanning();

// Adopt app.found[idx] as the saved target (replacing any previous one) and
// connect to it.
void bmChooseTarget(int idx);

// Drop the connection and wipe the saved target (Settings > Forget).
void bmForgetTarget();
