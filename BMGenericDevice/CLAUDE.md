# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What This Is

A PlatformIO/Arduino firmware project for ESP32-based Burning Man LED wearables. It targets multiple hardware variants and relies on two sibling libraries (`../libraries/BMDevice` and `../libraries/BurningManLEDs`) that must exist alongside this repo on disk.

## Build & Flash Commands

```bash
# Build for a specific target
pio run -e esp32          # Generic ESP32 (8 LED strips)
pio run -e c6             # Seeed Xiao ESP32-C6 (single strip)
pio run -e slut           # Custom SLUT hardware (7 strips + GPS + encoder)

# Build and upload
pio run -e esp32 -t upload
pio run -e c6 -t upload
pio run -e slut -t upload

# Monitor serial output
pio device monitor -e esp32
pio device monitor -e c6

# Clean build
pio run -e esp32 -t clean
```

There are no automated tests — PlatformIO's native test runner is not used here.

## Architecture

### Multi-target compile-time configuration

All hardware differences are handled through preprocessor guards in `src/main.cpp`:

- `TARGET_ESP32_C6` — Seeed Xiao, single WS2812B strip on GPIO 17
- `TARGET_SLUT` — Custom board: 7 strips, rotary encoder (pins 16/17/18), GPS (RX:21 TX:22), startup brightness fade-up to avoid power spike
- Default (no define) — Generic ESP32 dev board, 8 strips on GPIOs 32/33/27/14/12/13/18/5

These defines are set via `build_flags` in `platformio.ini`, not in source.

### BMDevice library (external dependency)

`BMDevice` (in `../libraries/BMDevice`) is the core library that handles everything at the application level: BLE advertising, LED configuration via NVRAM preferences, light show effects, palettes, and state management. `main.cpp` is intentionally thin — it calls `device.addLEDStrip<...>()` to register each strip, then `device.begin()` and `device.loop()`. All BLE commands (owner name, device type, LED strip config, factory reset) are handled inside BMDevice.

Custom BLE feature handlers are registered via `device.setCustomFeatureHandler(...)` — this is how OTA WiFi credentials flow from the app to the device.

### OTA update system (`BMOTA`)

Controlled entirely by `include/OTAConfig.h`. When `OTA_ENABLED=1`:

1. `BMOTA::begin()` connects to WiFi (credentials from ESP32 Preferences, then fallback to build-time defines)
2. After `OTA_BOOT_DELAY_MS` (30s), `BMOTA::loop()` starts checking `OTA_VERSION_URL` for a version string
3. If the remote version differs from `FIRMWARE_VERSION` (set at build time via `-D FIRMWARE_VERSION=...`), it downloads the per-target binary from `OTA_FIRMWARE_URL` using `esp_https_ota`
4. During OTA download, `ota.isUpdating()` returns true and `main.cpp` fills all strips red, then reboots on success

WiFi credentials reach the device via two BLE feature codes: `BLE_FEATURE_SET_WIFI_SSID` and `BLE_FEATURE_SET_WIFI_PASSWORD` (sent sequentially from the app). Credentials persist in `Preferences` namespace `"ota"`.

The HTTPS certificate bundle in `OTAConfig.h` covers both the GitHub ECC cert chain and the Comodo RSA CDN chain (needed for release asset redirects).

### Versioning

`include/version.h` defines `FIRMWARE_VERSION` as `"dev"` unless overridden by a build flag (e.g., `-DFIRMWARE_VERSION='"v1.2.3"'`). The OTA system compares this string against `version.txt` on the release server.

### Partition table

All environments use `min_spiffs.csv` for OTA-capable dual-partition layout. The `partitions_c6.csv` file in the repo root is available but not currently active.

## Key Files

| File | Purpose |
|------|---------|
| `src/main.cpp` | Entry point — hardware setup, LED strip registration, OTA hook |
| `src/BMOTA.cpp` | Non-blocking OTA state machine; runs OTA task on FreeRTOS |
| `include/OTAConfig.h` | **Edit this** to enable OTA, set WiFi credentials, firmware URL |
| `include/version.h` | Firmware version string (override via build flag for releases) |
| `platformio.ini` | All three build environments and their flags |
