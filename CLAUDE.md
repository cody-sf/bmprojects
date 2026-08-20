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
`ElkRemote` has a single `cyd` environment.

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

- **`BluetoothProvider`** — BLE scan/connect/disconnect, characteristic read/write, peripheral map, device status dispatch, standing auto-reconnect of every registered device (on an interval and on app foreground)
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

### Third-party ELK light bars (device type `elk`)

Off-the-shelf ELK-BLEDOM/BLEDDM Bluetooth LED bars (generic Amazon controllers)
are controllable as their own device type. They are foreign hardware: stock
`fff0` service, write characteristic `fff3`, fixed 9-byte frames
(`7E 00 <cmd> <args> .. EF` — see `encodeElkCommand` in
`providers/Bluetooth/commandCodec.ts`; the exact frames are pinned by tests and
were verified against the hardware). They never report state, so the app's view
of them is purely optimistic.

Bars are identified by their `ELK-` name prefix — the `fff0` UUID is far too
generic to scan for — and auto-adopted into the device registry on discovery.
There is no setup screen: they cannot store an owner or a name, and clearing app
storage costs nothing because rediscovery re-adopts them. A standing loop in
`BluetoothProvider` reconnects every registered device without scanning — bars
and our own hardware alike (connect-by-id pends on iOS / retries on Android
until the device powers up); a scan is only needed to meet a device the app has
never seen. The loop also re-kicks on app foreground, so paired devices are
connecting before any device page mounts. All bars share one
"Light Bars" tab (`pages/Elk/`), which exists only while at least one bar is
connected.

Their animations run on their own chip, so none of our effects apply. The mode
list is `ELK_MODES` in `constants.ts` (names beyond the verified `0x87` follow
community BLEDOM documentation — rename after eyeballing). A palette tap on the
Light Bars page samples the gradient at one point per connected bar and deals
the colours out across the group, which is the nearest thing a pile of
single-colour bars has to playing a palette.

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

The device owns its own name. `BLE_FEATURE_SET_DEVICE_NAME` (`0x38`, ASCII
string) persists `defaults.deviceName`, reports it in status as `deviceName`, and
folds it into the advertised name via `BMDevice::buildAdvertisedName()` —
`"BMDevice - <name>"`, falling back to the owner and then `"New"`. The
`"BMDevice"` identifier always leads so scanning still matches.

RNUmbrella writes the name over BLE (device setup and the device settings screen)
and keeps its own copy only as an offline cache; what the device reports wins.
A device flashed before `0x38` existed still reports the factory `"BMDevice"`,
which both apps treat as a placeholder and fall back from.
- A 2-byte palette/effect write is read by the firmware as a numeric id, not a
  one-character name.

### Custom palettes

A device has `CUSTOM_PALETTE_COUNT` (4) palette slots, stored in NVRAM and
selectable as `custom1`..`custom4` — ordinary `AvailablePalettes` values at the
tail of the enum, so every built-in palette keeps its id.

The phone owns the library; the slots are a cache. `RNUmbrella` keeps an
unbounded set of palettes in AsyncStorage (`providers/Settings/customPaletteStore.ts`)
shared across every device. Selecting one that a device is not already holding
uploads it into a free slot, or the slot that device has gone longest without
playing, then selects it by name.

The device stores colours, never gradient stops: the phone samples each palette
to `CUSTOM_PALETTE_ENTRIES` (16) colours — one per `CRGBPalette16` entry — so the
firmware does no interpolation. Everything goes through `samplePalette`
(`utils/gradient.ts`), in one of two modes:

- **Blend** (`sampleGradient`) — linear in sRGB, which is both what
  `LinearGradient` previews and what FastLED blends between entries, so the
  editor and the strip agree.
- **Hard edges** (`sampleBands`) — the colours become equal solid bands with no
  intermediates. This is how the banded built-ins (`earth`, `everglow`,
  `melonball`, `heart`, `sofia`, `velvet`) are defined, by stacking two stops on
  one position. Positions are ignored in this mode; only colour order matters.

Stop positions are derived from order, not authored — `stopsFromColors` respaces
them evenly on every add/remove — so the two modes above are the full range of
what the editor can express. `sampleGradient` does honour arbitrary positions,
including two stops stacked on one position, if per-stop positions are ever
exposed.

- `BLE_FEATURE_SET_CUSTOM_PALETTE` (`0x7C`): `[slot][nameLen][name ASCII][16 * rgb]`
- `BLE_FEATURE_DELETE_CUSTOM_PALETTE` (`0x7D`): `[slot]`
- Status: one `{"type":"cpal","i":<slot>,"n":<name>,"c":"<16 packed rrggbb>"}`
  chunk per slot. A slot reports itself with an empty name when empty — that is
  how a deletion reaches an app that did not make it. One slot per chunk because
  all four together overflow the 512-byte characteristic.

Two things this constrains:

- An upload is ~67 bytes, so `BleManager.write` is passed an explicit
  `maxByteSize`. At its 20-byte default it splits the command into several
  writes, and the firmware reads each fragment as its own command. Android also
  needs `requestMTU` on connect, which `BluetoothProvider` now does.
- An empty slot is not selectable (`LightShow::isPaletteAvailable`); selecting
  one would render black. The encoder skips empty slots for the same reason.

---

## Elk Remote (`ElkRemote/`)

Standalone controller for the third-party ELK light bars, on the 1-USB
"Cheap Yellow Display" (ESP32-2432S028R / ILI9341, resistive XPT2046 touch on
its own SPI bus). It is the app's Light Bars tab in hardware: BLE central
only (NimBLE, `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=9` so all 8 bars stay
connected; NimBLE passes that through to the controller's `ble_max_conn`),
no WiFi ever, and the same ELK frames - `src/ElkCodec.h` mirrors
`encodeElkCommand` in `RNUmbrella/providers/Bluetooth/commandCodec.ts` byte
for byte, so a frame change must land in both.

`src/Palettes.h` is **generated** from the app's palette table - do not
hand-edit:

```bash
node ElkRemote/scripts/generate-palettes.js   # after changing palettes in RNUmbrella/constants.ts
```

Behaviour is ported from `pages/Elk/Elk.tsx`: auto-adopt by `ELK-` name
prefix, standing reconnect scan while any registered bar is missing,
ensure-on before anything visual, and a palette tap deals one sampled colour
per connected bar in label order (jittered for 3 or fewer). Bar labels and
touch calibration live in NVS (Preferences namespace `elkremote`) because
the bars can store nothing. All displayed state is optimistic - the bars
never report.

The TFT_eSPI pin map is passed entirely as `build_flags` (no library edits);
the 2-USB / ST7789 CYD variant needs `-DST7789_DRIVER` and
`-DTFT_INVERSION_ON` instead. The XPT2046 library comes from a GitHub tag in
`lib_deps` because the registry package lacks a darwin_arm64 manifest.

---

## Battery Charger (`BatteryCharger/`, device type `batterycharger`)

An 8-port single-cell Li-ion charge monitor: each port has a battery under
charge behind a /2 voltage divider on an ADC pin, plus one addressable WS2812
status LED. It is a *monitor*, not a light — the app shows per-port voltage,
charge %, and state; there is no palette/effect/brightness control.

It has its own service UUID (`BATTERYCHARGER_UUIDS` in `constants.ts`, mirrored
by `SERVICE_UUID` in `BatteryCharger/src/main.cpp` — keep the two in step), so
the app detects it as its own type rather than a generic `BMDevice`.

### Firmware notes

Built on `BMDevice` for the BLE plumbing (chunked status, subscription gating,
owner/name persistence, the app's `request_status` poll on `0x02`), but with two
deliberate departures because it has no light show:

- **The status LEDs are driven directly with FastLED, not registered with the
  framework's `LightShow`.** `LightShow::render()` drives each of *its*
  controllers with per-controller `showLeds()`, so a strip it never learned
  about is left alone. The device is forced powered-on at boot
  (`getState().power = true`) so the framework never runs its blank-the-strip
  path (`FastLED.clear()/show()`, the only place it touches FastLED globally),
  which would fight the status LEDs.
- **The inherited lighting status chunks are cleared and replaced.** After
  `begin()`, `clearStatusChunks()` drops the basic/effect/palette chunks, and a
  single `batt` chunk is registered in their place. Status is pushed on a port
  *state* change and in answer to the app's poll; live voltage drift rides the
  poll so a stable box stays quiet on the radio.

`PORTS[]` at the top of `main.cpp` is the single source of truth: each row maps
an ADC pin → position in the WS2812 chain (`ledIndex`) → silk-screen label, and
its order is the order the app displays ports in. Reorder/relabel there to match
the box; nothing else changes. `STATUS_LED_PIN` is the WS2812 data GPIO. See the
`battery-charger-pending-wiring` memory — these are placeholders until the
EasyEDA export confirms them.

Calibration is live and persisted in NVRAM (Preferences namespace `battcal`),
adjustable over BLE. The custom feature codes are device-specific (not common),
so they can reuse low numbers:

- `0x50` get: trigger a battery status burst now
- `0x51` set calibration: float LE (multimeter / ESP correction factor)
- `0x52` reset calibration: back to the factory factor
- `0x53` rescan ports: clear the per-port "seen charging" latches (see below)

The framework hands the custom handler the whole write including the feature
byte, so a float payload starts at `data[1]` and makes `length == 5` (as the
framework's own handlers do — `buffer + 1`, `length >= 5`).

### Wire protocol

One status chunk covers the whole box, parallel arrays in `PORTS` order:

```
{"type":"batt","p":8,"cal":1.073,"lbl":["1",..],"mv":[4050,..],"st":[1,..]}
```

`st` is the `PortState` enum, matching `BATTERY_PORT_STATES` in `constants.ts`:
0 empty (no battery), 1 charging, 2 full, 3 fault (over-voltage). It drives both
the app's colours and the LED colours (empty off, charging amber, full green,
fault blinking red).

**Empty ports read *higher* than any battery.** Open-circuit, a CN3791's BAT
output drifts up to the module's setpoint (~4.25 V on this board); a cell clamps
its terminal to ≤ ~4.2 V, so a battery always reads below the empty float. So
empty is a plain threshold: `classifyPort()` reports NONE at/above
`MV_EMPTY_FLOAT` (4230 mV) — which also detects removal for free (pull a battery,
the port floats back to 4.25). Below that a battery is present; a `reachedFull`
latch (set at `MV_FULL_MARK` 4180 mV, cleared when the port reads empty) keeps a
finished battery reading FULL as it relaxes rather than flipping to charging.
`0x53` (the app's "Re-scan Ports") just clears the full latches. Thresholds are
in the app-mV domain, so trim the calibration slider until an empty port reads
~4.25 V in the app; the ADC itself uses `analogReadMilliVolts()` (factory
calibration), so the trim only covers the divider resistor tolerance.

### App notes

Routed like the stoplight — `updateAppDeviceStatus('batterycharger', ...)` in
`BluetoothProvider`, keyed on the chunk's own `type === 'batt'` (robust to the
characteristic UUID coming back in different cases on iOS vs Android) rather than
the generic `mapStatusPayload` path, because the per-port array is a composite
the key/value table cannot express. `SettingsProvider` turns it into a `ports`
array and derives each charge % from `BATTERY_MV_EMPTY`/`BATTERY_MV_FULL` (the
firmware reports raw millivolts and never computes a percentage). The page lives
at `pages/BatteryCharger/` and polls via `useStatusPolling` while on screen.
