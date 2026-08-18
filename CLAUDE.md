# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is a Burning Man LED wearables ecosystem with three components:

1. **Firmware** — PlatformIO/Arduino projects for ESP32-based LED devices (backpack, umbrella, cowboy hat, bike, boofer/flamethrower, stoplight, etc.)
2. **Mobile App** (`RNUmbrella/`) — React Native iOS/Android app that controls all devices over BLE
3. **Watch App** (`BMLightsWatch/`) — standalone watchOS app (SwiftUI + CoreBluetooth) for the everyday light controls, with no phone involved

External libraries live at `../libraries/` (sibling to each firmware project directory), not inside this repo. Every `platformio.ini` points there via `lib_extra_dirs = ../libraries/BurningManLEDs` and `../libraries/BMDevice`.

---

## Firmware Build Commands (PlatformIO)

Run from within a firmware project directory (e.g. `BMGenericDevice/`, `BTUmbrellaV3/`, `BatteryCharger/`):

```bash
# Build only
pio run -e <env>

# Build and flash
pio run -e <env> -t upload

# Monitor serial
pio device monitor -e <env>

# Clean
pio run -e <env> -t clean
```

### BMGenericDevice environments
| Env | Hardware |
|-----|----------|
| `esp32` | Generic ESP32 dev board (8 strips) |
| `c6` | Seeed Xiao ESP32-C6 (single strip) |
| `slut` | Custom SLUT board (7 strips + GPS + rotary encoder) |

### Other projects
`BTUmbrellaV3` and `BatteryCharger` each have a single `esp32dev` environment.

No automated test runner is used across any firmware project.

---

## Mobile App Commands (RNUmbrella/)

```bash
cd RNUmbrella

yarn install          # Install dependencies
yarn start            # Start Metro bundler
yarn ios              # Run on iOS simulator
yarn android          # Run on Android emulator
yarn lint             # ESLint
yarn test             # Jest
yarn test -- --testPathPattern=<file>   # Run a single test file
```

---

## Firmware Architecture

### Shared Libraries

**`../libraries/BMDevice`** — Application-level framework for all devices:
- `src/BMDevice.h/cpp` — Main class; call `device.addLEDStrip<...>()`, then `device.begin()` / `device.loop()`
- `src/BMBluetoothHandler.h/cpp` — BLE peripheral setup, characteristic write handling, chunked status updates
- `src/BMDeviceState.h/cpp` — Runtime state (brightness, speed, palette, effect, power)
- `src/BMDeviceDefaults.h/cpp` — NVRAM persistence of default settings via ESP32 Preferences

**`../libraries/BurningManLEDs`** — LED rendering engine:
- `LightShow.cpp/h` — Scene management and all effect implementations (palette stream, meteor shower, fire plasma, etc.)
- `ControlCenter.cpp/h` — Multi-device sync coordination over ESP-NOW
- `SyncController.cpp/h` — Sync state machine
- `NetworkClient/NetworkManagerCore` — UDP-based networking for sync

### Firmware Project Pattern

Every device's `main.cpp` is intentionally thin:
1. Allocate `CRGB` arrays for each LED strip
2. Call `device.addLEDStrip<CHIPSET, PIN, COLOR_ORDER>(array, count)` for each strip
3. Call `device.begin()` in `setup()`
4. Call `device.loop()` in `loop()`

Hardware variants are selected via `build_flags` defines (`TARGET_ESP32_C6`, `TARGET_SLUT`, `TARGET_ESP32_CLASSIC`) — never hardcoded in source. Custom behavior (e.g. sound reactivity in BTUmbrellaV3) is registered via `device.setCustomFeatureHandler(...)`.

### OTA Updates (BMGenericDevice only)

Controlled by `include/OTAConfig.h`. When `OTA_ENABLED=1`, `BMOTA` checks `OTA_VERSION_URL` 30s after boot and every hour thereafter. If the remote version string differs from `FIRMWARE_VERSION` (a build-time define), it fetches the binary via `esp_https_ota`. WiFi credentials are delivered over BLE (feature codes `0x35`/`0x37`) and stored in Preferences namespace `"ota"`. During download, all strips go red.

---

## Mobile App Architecture (RNUmbrella/)

### Provider / Context Layer

Three React Context providers wrap the app (see `App.tsx`):

- **`BluetoothProvider`** — BLE scan/connect/disconnect, characteristic read/write, peripheral map, device status dispatch
- **`SettingsProvider`** — Persisted device state for all device types (backpack, umbrella, boofer, etc.) using AsyncStorage via `store/store.tsx`
- **`WatchProvider`** — Apple Watch connectivity via `react-native-watch-connectivity`

### BLE Protocol

All device types have a fixed service UUID and two characteristics: `features` (write commands to device) and `status` (device pushes JSON back). UUIDs and command codes for every device type are centralised in `constants.ts`.

The scan filter in `BluetoothProvider` looks for known service UUIDs (`scanUUIDs`) and device name prefixes from `DEVICE_NAMES`. On connect, the provider reads the `status` characteristic and sets up notifications.

Command bytes are written to the `features` characteristic. Encoding: single byte command code followed by the payload (bool, int as bytes, ASCII string, or binary struct depending on the command).

### Key Files

| File | Purpose |
|------|---------|
| `constants.ts` | All BLE UUIDs, command codes, palette definitions, device types |
| `helpers.ts` | `bytesToString`, `convertRange`, `parseJsonString`, `sleep` |
| `providers/Bluetooth/BluetoothProvider.tsx` | BLE scan/connect logic, event routing |
| `providers/Settings/SettingsProvider.tsx` | Device state persistence and dispatch |
| `pages/BMDevice/BMDevice.tsx` | Generic device control UI |
| `pages/Umbrella/Umbrella.tsx` | Sound-reactive umbrella UI |
| `partials/NavigationTabs/NavigationTabs.tsx` | Bottom tab navigation root |

### Adding a New Device Type

1. Add service/feature/status UUIDs to `constants.ts`
2. Add the device name prefix to `DEVICE_NAMES`
3. Add the service UUID to `scanUUIDs`
4. Create a new entry in `DEVICE_UUIDS`
5. Add device state shape to `SettingsContext` and `SettingsProvider`
6. Create a page under `pages/` and wire it into `NavigationTabs`

---

## Watch App (BMLightsWatch/)

Standalone watchOS app — the watch is its own BLE central, so it needs neither
the phone nor the RNUmbrella app at runtime. Covers power, brightness, palette,
effect, speed and direction only; everything deeper stays on the phone.

```bash
open BMLightsWatch/BMLightsWatch.xcodeproj   # scheme BMLightsWatch, run to the watch

# compile check without hardware
xcodebuild -project BMLightsWatch/BMLightsWatch.xcodeproj -scheme BMLightsWatch \
  -destination 'generic/platform=watchOS Simulator' CODE_SIGNING_ALLOWED=NO build
```

Both the palette/effect catalog and the Xcode project are **generated** — do not
hand-edit `BMLightsWatch/BMLightsWatch/Model/Catalog.swift` or the `.xcodeproj`:

```bash
node BMLightsWatch/scripts/generate-catalog.js   # after changing palettes/effects
ruby BMLightsWatch/scripts/generate-project.rb   # after adding a Swift file
```

The catalog joins the firmware's id strings (`libraries/BurningManLEDs/LightShow.cpp`)
with the app's display names and colors (`RNUmbrella/constants.ts`). The firmware
is authoritative for the wire strings: `paletteNameToId`/`effectNameToId` fall
back to `cool`/`palette_stream` for anything they don't recognise.

Wire protocol notes that matter (see `BMLightsWatch/BMLightsWatch/Protocol/BMProtocol.swift`):
- The `features` characteristic is `BLERead | BLEWrite`, so writes must be
  **with response** — write-without-response is silently dropped.
- Status arrives as several JSON chunks, each a complete object with a subset of
  the keys; merge, never replace.
- Devices do **not** push status on a timer — they report on connect, on a change
  (app or encoder), and when asked via `request_status` (`0x02`). Send that
  request only once the status subscription is confirmed live: the firmware's
  `sendStatusUpdate` silently drops the burst unless `isSubscribed()`.
- The watch connects only on an explicit tap. Auto-connecting saved lights on
  launch exhausts watchOS's small BLE connection ceiling.

### Device naming

Devices advertise as `<identifier> - <owner>` ("BMDevice - Cody",
"Umbrella-CL"). The identifier is what `handleDiscoverPeripheral` matches during
a scan, so it must stay on the wire — but it is never displayed. Two
implementations must agree, and are verified to:

- `RNUmbrella/helpers.ts` → `resolveDeviceName` / `cleanAdvertisedName`
- `BMLightsWatch/BMLightsWatch/Model/BMNaming.swift` → `BMNaming.resolve`

Only the generic `BMDevice` identifier is stripped; real device words (Umbrella,
Backpack, Bike) are descriptive and stay. Order of preference: app-assigned name,
then cleaned advertised name, then `Light <last4 of id>`.

The firmware's `defaults.deviceName` is **not** a name — nothing ever sets it, so
it reads "BMDevice" on every generic device and is treated as a placeholder. The
only name written to hardware is `owner` (`set_owner`, `0x30`); the per-device
name typed into RNUmbrella is phone-local (AsyncStorage keyed by peripheral id),
so the watch cannot see it.
- A 2-byte palette/effect write is read by the firmware as a numeric id, not a
  one-character name.
