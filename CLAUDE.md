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
| `codys_bike` | SLUT board on the bike: chevron panel on GPIO 32 (12v 3), frame wrap glow strip on GPIO 33 (12v 2), front-rack 8x32 matrix on GPIO 5 (5 V 1), encoder, no GPS |

The bike's LED maps (`BMGenericDevice/include/CodysBikeMap.h`) are **generated** —
do not hand-edit. The header covers both strips in one shared "bike space"
coordinate system (x 0 = rear of the bike, 255 = front): the Fusion 360 panel
geometry (four chevrons, 50 holes, serpentine stringing starting at the front
chevron) and the frame wrap glow strip (12 V neon-flex, 3 LEDs per WS2811 pixel,
front → rear down one side, wrapping behind the bike, rear → front up the
other). The frame strip follows the frame, not a straight line: its pixels are
laid at even arc steps along a waypoint polyline (head-tube dive → low
step-through rail → chainstay → rear wrap, eyeballed from a side photo), so
each pixel carries its true fore-aft position and height — sweeps cross the
down-tube dive as one near-vertical front, `panel_fire` burns along the low
rail and licks up the down tube, `panel_rain` runs down it. The chevrons start
75% of the way down each side — that one measurement anchors the panel into
bike space, so effects flow off the frame into the panel at the right place.
The pixel count, pitch, waypoints and the 75% mark are constants at the top of
the script:

```bash
python3 BMGenericDevice/scripts/generate_bike_map.py   # after changing the panel or the strip
```

The front-rack 8×32 matrix map (`BMGenericDevice/include/FrontRackMatrixMap.h`)
is generated too, by `scripts/generate_matrix_map.py`. The matrix faces
forward — perpendicular to the sweep axis — so its map *continues* bike space,
in one of two styles behind the script's `STYLE` constant. The default,
`'chevrons'`, makes the sheet more chevrons like the panel: the map's
progression is a V-shaped field (a front at constant progression IS a
chevron — vertex mid-height leading the march, arms swept back
`CHEVRON_SWEEP` columns per row), so a wave that swept the bike arrives and
plays as V after V, `seg` is the four chevron bands stepping in unison with
the panel's four, `vdist` fills each band from its vertex-side edge, and y
stays true vertical so `panel_rain` falls and `panel_fire` rises.
`CHEVRON_POINT` picks the shape: `'out'` (default) mirrors the field about
the vertical centreline into two back-to-back chevrons — `<< >>` blooming
out of the middle, the radial splash's motion with chevron fronts — `'in'`
converges from the edges, and `'right'`/`'left'` is a one-direction march
(retired as the default because a lateral march on the front of the bike
reads as a permanent turn signal). `'radial'` is the original centre-out
splash — kept for comparison, retired because on hardware it read as "a
circle expanding" rather than anything chevron. The wiring layout is a constant at
the top of the script — SETTLED on hardware by the `0x83` corner-phase test
as `column_serpentine` with `FLIP_Y` (the reading guide is documented at the
top of the script). Mounting flips are constants too: if text comes out
upside down on the rack, set both `FLIP_X`/`FLIP_Y` and regenerate.

The maps feed `LightShow::setPixelMap()`, which gives the `chevron_*` and
`panel_*` effects per-LED coordinates (x/y position, chevron index, distance
from the chevron's point — the frame strip's sides are split into four virtual
chevron sections that step in sympathy with the panel). A device with no map
(`LightShow::hasPanelMap()` false) *renders* a substitute in their place —
`color_explosion` for the chevron effects, and the nearest 1D effect for each
`panel_*` one (waves→plasma_clouds, fire→fire_plasma, rain→matrix_rain,
spin→spiral_galaxy, puddle→matrix_rain, starfield→meteor_shower; the old
straight-strip fallback read as noise) — while
still reporting the chevron/panel id, so a synced group can hold one effect
even when only the bike draws it properly. On a strip with a real map those effects read the shared
`map_phase_()` rather than the grouping-slowed `strip_phase_()` — the map
already encodes physical positions, and per-strip pacing would run the
grouped glow strip out of phase with the panel it is aligned to.

The header also carries `LightShow::setRenderOrder()` tables: effects render in
logical rear→front order and the wiring-order permutation is applied only at
show time (inside `show_now_`), so **every** 1D effect (streams, meteors,
cylon, fire…) sweeps the panel front-to-back instead of snaking the serpentine,
and sweeps the frame strip's two sides in mirror instead of looping around the
rear wrap. The matrix's order plays along the chevron progression, so 1D
effects render on it as `<< >>` fronts blooming out of the middle (rings
under the retired `'radial'` style).

The `panel_*` effects (`panel_waves`, `panel_fire`, `panel_rain`,
`panel_spin`, `panel_puddle`) are the true-2D companions to the `chevron_*`
set: they read PanelPixel x/y as real geometry, run off the shared phase
clock, and reuse existing effect-parameter BLE codes (scale rides cloudScale,
flame height heatVariance, rain density dropRate, spokes spiralArms).
`panel_puddle` is rain that floods: y is true height, so the water pools
along the frame's low rail first, rises to ~44% of the rig, then drains.
Two more bike-space effects round out the set: `starfield` warps stars out
of the matrix centre (2D on the grid; star streaks along x on the other
mapped strips; direction flips inward, density rides dropRate) and
`orbit_comet` laps each strip's PHYSICAL wiring chain — on the frame wrap
the chain is laid along the frame, so the comet orbits the bike's silhouette
(tail rides trailLength; one lap per 4096 phase units so synced rigs lap
together; works on any plain strip as a wrapping comet, so no substitute
needed).

The matrix is also a display — and the display is an **overlay, not an
effect**. `LightShow::setMatrixGrid()` gives a strip real rows and columns
(the generated `FRONT_RACK_MATRIX_GRID`; the radial map deliberately erases
columns), and `setMatrixDisplay()` (BLE `0x89`, persisted) puts content on
it: 1 scrolls device-stored marquee text (5×7 font in
`libraries/BurningManLEDs/src/MatrixFont.h`), 2 zooms the text
word-at-a-time out of the centre ("fly at you"; direction reverses it),
3 shows a device-stored 16-colour image, 4 plays device-stored animation
frames (same bitmap format, up to `MATRIX_ANIM_MAX_FRAMES` = 8), 0 turns it
off. While it shows, the overlay owns ONLY the grid strip(s): every other
strip keeps playing the selected effect in the selected palette (no more
image-colour wash), group sync keeps sharing the real effect, and the
overlay paints inside `show_now_` — the one funnel every show goes through —
so effects, transitions and repaints all display the content without knowing
it exists. It runs on its own clock too: `0x8A` (int LE ms, clamped 20–2000,
persisted, default 100) is exactly ms-per-animation-frame, and the marquee
scroll/word zoom scale off the same value; the effect speed knob never paces
the display. A mode with no content behind it, or a rig with no grid, simply
doesn't engage. The old `panel_text`/`panel_bitmap`/`panel_words`/
`panel_anim` effect ids survive only as wire aliases: `BMDevice::setEffect`
translates them into `setMatrixDisplay`, so old clients and old NVRAM
defaults still work, sync from old firmware skips them, and the catalog
generators (watch + BikeRemote) exclude them from the mode lists.
`0x84` sets the text style (0 normal,
1 bold — strokes doubled, persisted); `0x85` sets the glyph *fill* — what
the letters are made of (0 sliding palette gradient, 1 fire, 2 rain,
3 plasma; applies to both text looks, persisted, drawn in the device's
current palette so the marquee recolors with it). Content persists in NVRAM
and arrives over BLE:
`0x80` text (ASCII), `0x81` bitmap (`[w][h][16×RGB][packed 4bpp pixels]`, one
~179-byte write), `0x82` clear, `0x86` anim frame (`[slot]` then the bitmap
payload; playback runs the contiguous run of slots from 0, so the app clears
with `0x87` first and uploads 0..N-1). `0x83` runs a 30 s wiring diagnostic — a
rainbow along the raw LED chain that identifies the panel's real layout
(tells documented at the top of `generate_matrix_map.py`; button in the
Matrix Display card).
Status reports the grid size as `mtxW`/`mtxH` — the app's capability signal:
a dedicated Matrix Display card (`partials/DeviceControls/MatrixCard.tsx`)
appears on the device page only when `mtxW` > 0, so strip-only devices and
pre-display firmware never see it. The matrix keys ride their own `matrix`
status chunk and the sync diagnostics (`syncSt`/`syncTx`/`syncRx`) their own
`radio` chunk — they used to ride `devConfig`, which the `txtFill`/`anim`/
counter additions pushed past the ~239-byte notify payload; a clipped chunk
parses as nothing, so every central silently lost `mtxW`. Clients merge
status keys regardless of chunk type, so the split needed no app changes —
but anything added to a chunk must keep its worst-case serialization under
that ceiling. The card's "Show" actions deliver the content *and* switch the
display overlay to it (`0x89`); because the display is independent of the
mode list, the card grows an explicit "⏹ Stop · back to effects" button (and
on the BikeRemote, tapping the lit Show chip again) — picking a mode no
longer touches the matrix. Status reports the display mode as `disp` and its
pace as `mtxMs`, alongside the text as `marquee`,
a stored-bitmap flag as `bmp`, the fill as `txtFill` and the stored frame
count as `anim`. The card has a TEXT section (marquee input, scroll/fly
looks, direction, bold, fill chips) and an ART section over the pixel-art
*project* (`canvasStore.ts`): one frame shows as a bitmap, several send as
an animation ("▶ Animate"), and the Pixel Art editor grows a frame strip —
tap to switch, ＋ duplicates the current frame (draw-add-nudge), Send
delivers a still or the whole set. Photo import goes through a crop screen —
drag/pinch plus a zoom slider — with a live quantized preview
(`partials/PixelArtEditor/`, quantizer in `utils/pixelArt.ts`, pinned by
`__tests__/pixelArt.test.tsx`). The same picker takes GIFs: one that reaches
the app un-recoded (magic-byte sniff, not MIME) is decoded pure-JS (omggif),
its frames composited then cut delay-weighted to `MATRIX_ANIM_MAX_FRAMES` —
a held frame keeps extra slots so the rhythm survives constant-rate
playback — cropped once for all frames (the preview strip animates; RN's
`Image` can't be trusted to), and lands as the whole project — including
`frameMs`, the loop's duration spread over the sampled frames, which
"▶ Animate" sends via `0x8A` so the GIF plays at its native pace (pinned by
`__tests__/gifImport.test.tsx`). A platform that
flattens GIFs to JPEG/PNG degrades to the still-photo path on its own.
Devices without a grid store the content; nothing on them changes when the
display is switched on.

### Other projects
`BTUmbrellaV3` and `BatteryCharger` each have a single `esp32dev` environment.
`ElkRemote` and `BikeRemote` each have a single `cyd` environment.

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
- `src/BMSync.h/cpp` — ESP-NOW group sync: same-owner devices broadcast their
  look (power/brightness/speed/direction/effect/palette) to FF:FF:FF:FF:FF:FF
  and adopt what they hear, plus the sender's `Clock::now()` so shows run in
  phase. No peer roster — this replaces the legacy
  `libraries/BurningManLEDs/SyncController` (hardcoded MAC map, never compiled
  for the C6). The radio is only up while the device is powered on and
  `syncEnabled` (BLE `0x24`) is set — always-on ESP-NOW receive costs tens of
  mA — and a device that powers on late announces itself with a QUERY packet so
  the group re-broadcasts its current look. BMSync only claims the radio when
  WiFi is fully off and yields whenever BMOTA associates, so OTA still works
  (on `OTA_KEEP_WIFI_ALIVE` sign builds, sync therefore never runs). Devices
  that are not light shows opt out with `setSyncAvailable(false)` (the battery
  charger does — a synced power-off would blank its status LEDs). Brightness
  syncs as a fraction of each device's own max, so the group dims
  proportionally instead of slamming into the weakest cap.

### Strand grouping (per-strip motion pace)

Strips that gang several LEDs per addressable pixel (the 12 V glow strips run
6 per WS2811) cover 6× the physical distance per pixel, so motion tuned on a
dense strip tears across them — but one device can mix glow strips with
normal-density button LEDs or fairy lights. Grouping is keyed by **the order
strips were registered with the show** — `BMDevice` records every
`addLEDStrip` into `registeredStrips_`, whether the sketch hardcoded it (bike,
SLUT, signs) or it came from NVRAM rows — *not* by the `LEDStripConfig` rows,
which static targets never populate. Group sizes persist as their own NVRAM
blob (`stripGroups`), are set over BLE `0x7F` `[index][ledsPerPixel]`, and are
reported in the dedicated `strips` status chunk as a packed
`"index,pin,count,group;…"` string (eight JSON object rows overflow both the
512-byte doc and a notify — that's why the old `leds` array was dropped from
devConfig). The app's Advanced Settings "LED Strands" section lists that chunk
and toggles Glow Strip per strand.

Each strand also carries a **max-brightness ceiling** (BLE `0x88`
`[index][max 1-255]`, NVRAM blob `stripBri`, fifth field of the `strips`
rows — absent means uncapped, so old firmware and old apps interop). It is
a scale, not a clamp: `show_now_` multiplies the master brightness by
cap/255 for that strip (before the power budget, which must see the true
output level), so a capped strand dims in step with the rest of the rig —
dim accents beside bright strips, or a strand held under the level where
its LEDs start to glitch. The Advanced tab's strand rows each get a
Max brightness slider (percent, sent on release). `LightShow::setStripGroupSize()` slows that
strip's *spatial* phase reads (`strip_phase_()`) proportionally; temporal
rhythms (twinkle breathing, the bpm throb) stay full pace, and effects with
one shared cross-strip position (fireworks, color_explosion, pacifica's clock)
pace to the slowest strip. The old `-DBM_MOTION_PERCENT` build flag survives
only as the default for strips never configured.

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

### Home Assistant bridge (mains-powered signs)

Keep-WiFi-alive builds (`hotel_sign`, `takoyaki_sign`) publish themselves to
Home Assistant as **one MQTT-discovery light entity — on/off + brightness
only**, deliberately: effects, palettes and everything deeper stay with the
app/watch/encoder. `BMGenericDevice/src/BMMQTT.{h,cpp}`, compiled in only when
the gitignored `WiFiCreds.h` defines `MQTT_BROKER_HOST` (CI release binaries
never carry a broker) and the build sets `OTA_KEEP_WIFI_ALIVE`; every other
target gets an empty stub. Commands are applied through
`BMDevice::injectFeature()` — the same dispatch BLE writes land in — so the
percent conversion and the max-brightness cap match the app exactly. State
mirrors back with the same settle idea as the BLE status (an encoder spin or
the boot fade produces one retained publish, not a stream), availability rides
an MQTT LWT, and discovery/state re-announce on Home Assistant's
`homeassistant/status` birth message. The device id everywhere is the espota
hostname (`bm-<mac4>`): discovery at `homeassistant/light/<id>/light/config`,
topics under `bmlights/<id>/…`. esp-mqtt (bundled in the IDF 5 / arduino-esp32
3.x core) runs its own task; MQTT events only queue work that `loop()` applies
on the main task. A `.local` broker name is resolved via mDNS before connect
(lwip DNS can't); prefer an IP or plain DNS name.

---

## Mobile App Architecture (RNUmbrella/)

### Provider / Context Layer

Three React Context providers wrap the app (see `App.tsx`):

- **`BluetoothProvider`** — BLE scan/connect/disconnect, characteristic read/write, peripheral map, device status dispatch, standing auto-reconnect of every registered device (on an interval and on app foreground)
- **`SettingsProvider`** — Persisted device state for all device types (BMDevice, umbrella, boofer, etc.) using AsyncStorage via `store/store.tsx`
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

### Retired device types

`backpack`, `bike`, `fannypack` and `cowboyhat` used to be device types with
their own service UUIDs. That hardware is all generic `BMDevice` now, so the
types, their UUIDs and the dead `BACKPACK_FEATURES` table are gone from
`constants.ts`, and the watch's `BMProfile.all` is down to `bmDevice` and
`umbrella`.

Two things deliberately survive the removal:

- **The words, wherever they match a device *name* rather than a type** — the
  icon and emoji keyword tables (`suggestIconKey`, `Connection.tsx`) and the
  name suggestions in `InitialSetup.tsx`. A generic device called "Bike" should
  still get 🚲.
- **`BACKPACK_MODES`** in `constants.ts`, which despite the name is the effect
  display-name table for every device, and feeds the watch catalog generator.

A phone that paired one of these before the removal still has the old string in
AsyncStorage, and a type with no `DEVICE_UUIDS` entry makes `encodeCommand`
return null — the device connects and then ignores every button. `deviceRegistry`
rewrites those to `BMDevice` on load (`RETIRED_DEVICE_TYPES`).

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
Light Bars page deals one colour per connected bar off the palette's *vivid
track* (`sampleVividSpread` in `utils/gradient.ts`): the gradient sampled
densely, dark stretches dropped and dim colours boosted, because a black band
is texture on a strip but a dead fixture on a bar — the nearest thing a pile
of single-colour bars has to playing a palette. The track's constants and
maths are mirrored in `ElkRemote/src/Gradient.cpp` and pinned by
`__tests__/vividSpread.test.tsx`; change both together or the phone and the
CYD remote deal different colours.

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

Devices advertise as `<identifier> - <label>` ("BMDevice - Codys Bike",
"Umbrella-CL"), where the label is the device's friendly name, falling back to
the owner only for never-named devices. Because of that ambiguity the suffix
must never be parsed as "the owner" — the status report (`owner`/`deviceName`,
landing in the app as `deviceOwner`/`deviceName`) is the authoritative source
for both, and is what prefills the setup and settings screens. The identifier
is what `handleDiscoverPeripheral` matches during a scan, so it must stay on
the wire — but it is never displayed. Two implementations must agree, and are
verified to:

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
per connected bar in label order (jittered for 3 or fewer). Bars are dealt
from the palette's "vivid track" (`Gradient.cpp`) - dark stretches dropped,
dim colours boosted - since a black band is texture on a strip but a dead
fixture on a bar; an optional cycle (on by default) walks the bars along
that track at the Speed slider's pace. Bar labels and
touch calibration live in NVS (Preferences namespace `elkremote`) because
the bars can store nothing. All displayed state is optimistic - the bars
never report.

The TFT_eSPI pin map is passed entirely as `build_flags` (no library edits);
the 2-USB / ST7789 CYD variant needs `-DST7789_DRIVER` and
`-DTFT_INVERSION_ON` instead. The XPT2046 library comes from a GitHub tag in
`lib_deps` because the registry package lacks a darwin_arm64 manifest.

---

## Bike Remote (`BikeRemote/`)

ElkRemote's sibling on a second CYD, but pointed at our own hardware: a BLE
central for **one** BMDevice (the bike), speaking the same protocol as the
phone and watch — same UUIDs, writes with response, ASCII palette/effect ids
(never 1 char), int32 LE numbers, JSON status chunks merged never replaced,
subscribe-then-request-status with one 1.5 s retry. MTU is raised to 255
because a status chunk runs to ~239 bytes. Unlike the bars, state is only
optimistic until the next status chunk: the firmware pushes one on every
change (phone, watch, encoder), so the remote tracks reality without polling.

`src/Catalog.h` is **generated** — do not hand-edit:

```bash
node BikeRemote/scripts/generate-catalog.js   # after changing palettes/effects/EFFECT_PARAMETERS
```

Like the watch catalog it joins the firmware's wire ids (`src/LightShow.cpp`,
not the drifted root copy) with the app's display names and palette colors,
and additionally bakes in `EFFECT_PARAMETERS` (slider ranges + BLE codes) for
the Tweaks screen. Effects carry `FX_NEEDS_GPS` / `FX_NEEDS_MATRIX` flags and
are hidden unless the device's status reports `gps` / `mtxW>0`; "solid" is
absent on purpose (no firmware wire string — the color swatch sends `0x19`
instead). Custom palette slots arrive via the `cpal` chunks and join the
palette grid; the phone still owns authoring them.

Screens: Home (power/brightness capped by `maxBri`/speed/reverse/Find/color),
Palettes, Modes, Tweaks (live values from the `effectParams` chunk), Setup
(choose/forget light, sync toggle `0x24`, save-as-defaults `0x1C`, touch
recal), and a Matrix screen behind a grid-gated chip on Modes — the app's
Matrix Display card in hardware: marquee text on an on-screen keyboard
(`0x80`), Show Text/Words/Art/Anim chips driving the display overlay
(`0x89`; highlight tracks the `disp` status key, tapping the lit chip sends
mode 0 to hand the panel back to the effect; Art/Anim enabled only when
status reports `bmp` / `anim` > 0 — that content uploads from the phone),
glyph fill `0x85`, bold `0x84`, wiring test `0x83`. The one saved target (address
+ name) and the touch calibration live in NVS namespace `bikeremote`; a
discovery scan matches the advertised service UUID or the `BMDevice` name
prefix, and the shown name strips the `BMDevice` identifier exactly like
`resolveDeviceName`/`BMNaming.resolve`. The backlight sleeps after 15 s
idle (BLE stays up; the waking tap is swallowed).

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
  path (`FastLED.clear()` plus per-strip `showLeds(0)`, re-clocked once a
  second while off — the only place it touches FastLED globally), which would
  fight the status LEDs. The blank is sequential and repeating on purpose: it
  was one parallel `FastLED.show()`, and with WiFi busy the RMT latency of
  that burst could cost a strip its frame — the hotel sign's VACANCY strip
  reliably missed the one-shot blank and held its last frame, lit, until the
  next power-on.
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
