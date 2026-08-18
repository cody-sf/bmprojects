#ifndef BM_DEVICE_LOG_H
#define BM_DEVICE_LOG_H

#include <Arduino.h>

// Verbose tracing for per-command and per-status-chunk activity.
//
// Serial writes are blocking once the TX buffer fills: a full status burst is
// ~1.5 KB of text, which is >100 ms of stalled main loop at 115200 baud. That
// stall shows up as dropped BLE polls and stuttering LED frames, so this
// tracing is compiled out unless it is explicitly asked for.
//
// Enable with -DBM_VERBOSE_LOGS=1 in platformio.ini build_flags when debugging.
#ifndef BM_VERBOSE_LOGS
#define BM_VERBOSE_LOGS 0
#endif

#if BM_VERBOSE_LOGS
#define BM_LOGV(...) Serial.printf(__VA_ARGS__)
#else
#define BM_LOGV(...) do {} while (0)
#endif

#endif // BM_DEVICE_LOG_H
