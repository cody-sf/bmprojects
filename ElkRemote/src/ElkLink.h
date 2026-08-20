#pragma once
#include <stdint.h>

/**
 * BLE central for the bars, the CYD equivalent of BluetoothProvider's
 * standing reconnect loop: every registered bar is held connected, and a
 * scan runs whenever one is missing (or on request) - which is also how a
 * never-seen bar gets adopted, by its ELK- name prefix.
 *
 * All NimBLE calls live on one FreeRTOS task; the UI talks to it through a
 * write queue and a few request flags, and never blocks on the radio.
 */

void elkInit();

// Queue one 9-byte frame for a bar. Fire-and-forget, like the app.
void elkSend(int barIdx, const uint8_t* frame);

// Keep scanning for the next little while even if everyone is connected -
// the Rescan button, for meeting brand-new bars.
void elkRequestScan();
bool elkIsScanning();

// Drop every connection and wipe the registry (Settings > Forget).
void elkForgetAll();
