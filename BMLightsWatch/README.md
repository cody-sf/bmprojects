# BM Lights (Apple Watch)

A standalone watchOS app for the everyday light controls — power, brightness,
palette, effect, speed, direction — with no phone in the loop. The watch is its
own BLE central: it scans for the lights, connects, and writes to the same
`features` characteristic the RNUmbrella app uses.

The deep settings (GPS, sound reactivity, effect parameters, defaults/NVRAM,
OTA, boofer, stoplight) stay on the phone.

## Build and install

```bash
open BMLightsWatch.xcodeproj
```

Pick the **BMLightsWatch** scheme, choose your Apple Watch as the run
destination, and hit Run. It is a watch-only app (`WKWatchOnly`), so it installs
straight to the watch and does not need the iPhone app to be present.

From the command line:

```bash
# compile check against the simulator
xcodebuild -project BMLightsWatch.xcodeproj -scheme BMLightsWatch \
  -destination 'generic/platform=watchOS Simulator' \
  CODE_SIGNING_ALLOWED=NO build
```

The simulator has no Bluetooth radio, so the device list is empty there. A
**Demo Light** row appears in simulator builds only, driving the control screens
from local state so layout can be worked on without hardware.

## Using it

The root screen groups lights into **Connected**, **Saved** (used before, ready
to tap) and **Nearby** (just found by a scan). Tap a light to connect; tap it
again once connected to open its controls.

**Nothing connects on its own.** The watch has few BLE slots, and spending them
on lights you did not pick is what leaves the one you did pick with nowhere to
go. Saved lights are listed and idle until tapped — connecting to one is what
saves it in the first place.

Swipe left on any row for **Remove** (forget it entirely) and, when it is
connected, **Disconnect** (drop the link, stay saved). **Remove All Saved** at
the bottom of the list clears the lot after a confirmation. Anything removed
reappears under Nearby if it is still advertising.

A light that drops out mid-use is chased for a few attempts and then left alone.
A connect that gets no answer gives up after 10 seconds rather than holding its
slot forever. With two or more lights connected, an **All Lights** row appears at
the top that fans every command out to all of them.

The controls are four swipeable pages. Each page owns the Digital Crown for its
own value, which is why they are pages rather than rows in one long list.

| Page | Crown | Also |
|------|-------|------|
| Power | Brightness | Tap the ring's center to toggle power |
| Palette | Scroll | 33 palette swatches |
| Effect | Scroll | 14 effects |
| Speed | Speed | Forward/reversed toggle |

## How it talks to the lights

Same wire protocol as the phone app — see `Protocol/BMProtocol.swift`:

- Scans for the backpack, umbrella, bike, and generic BMDevice service UUIDs.
- Writes `[feature byte][payload]` to the `features` characteristic, **with
  response** (the firmware declares it `BLERead | BLEWrite`, so a
  write-without-response would be dropped).
- Subscribes to the `status` characteristic and merges the JSON chunks the
  firmware pushes. Each chunk is a complete object holding a subset of the
  keys, so only the keys actually present are applied.
- Sends `request_status` (`0x02`) to make the device report. These devices do
  **not** push status on a timer — they report on connect, on a change made
  from an app or the encoder, and when asked.

  The request goes out from `didUpdateNotificationStateFor`, **not** straight
  after `setNotifyValue`. `BMBluetoothHandler::sendStatusUpdate` drops the burst
  unless `isSubscribed()` is already true, so asking before the CCCD write lands
  loses the reply silently. One retry 1.5s later covers a burst that still goes
  missing.

Payload shapes: booleans are one byte, ints are little-endian `Int32`, palette
and effect are raw ASCII names. Brightness is 1-100. Speed goes over the wire
as the firmware's frame duration (5 fastest … 200 slowest) and is shown as a
1-100 percentage, matching `handleSpeedChange` in the phone app.

## Naming

Devices advertise as `<identifier> - <owner>` — "BMDevice - Cody",
"Umbrella-CL". That leading identifier is how a scan recognises our gear, so it
stays on the wire, but "BMDevice" is a category rather than a name and is never
shown. `Model/BMNaming.swift` resolves what a person actually sees, and is kept
in step with `resolveDeviceName` in `RNUmbrella/helpers.ts` — both produce the
same answer for the same input:

| Advertised | Shown |
|---|---|
| `BMDevice - Cody` | Cody |
| `BMDevice - New`, `BMDevice` | Light 4A2F |
| `Umbrella-CL`, `Backpack-CL` | unchanged |

Real device words (Umbrella, Backpack, Bike) are descriptive, so they stay; only
the generic identifier is dropped. The firmware's own `deviceName` field is
ignored unless it says something — nothing ever sets it, so it reads "BMDevice"
on every generic device.

**The per-device name you type into RNUmbrella lives on the phone**
(AsyncStorage, keyed by peripheral id) and never reaches the hardware — only
`owner` is written over BLE. So the watch cannot show it, and falls back to the
owner or a short id. See the note at the end of this file.

## Troubleshooting

**"The system has reached the maximum number of connections."** That is
CoreBluetooth error 11 (`CBErrorConnectionLimitReached`): the watch has a much
smaller ceiling on simultaneous BLE links than a phone does. It shows as
*"Watch is at its BLE connection limit (11)"* with a banner explaining what to
free up.

The app no longer opens any connection you did not ask for, so this should be
rare. If it does appear, swipe a light you are not using and disconnect it, then
tap the one you want.

**A light shows up but will not connect.** The ESP32 firmware accepts a single
central at a time, so if the phone app is already connected to that light, the
watch cannot have it. Disconnect it on the phone first.

**Any other error.** Every failure is now shown with its numeric CBError code in
parentheses; that number is what identifies it. Unmapped codes fall back to
CoreBluetooth's own wording, wrapped rather than clipped.

## Regenerating

The palette and effect lists are generated, not hand-written. The firmware owns
the id strings (a name it does not recognise silently falls back to `cool` /
`palette_stream`), and the phone app owns the display names and swatch colors:

```bash
node scripts/generate-catalog.js     # -> BMLightsWatch/Model/Catalog.swift
```

Run it after touching `palettes` / `BACKPACK_MODES` / `modeMapping` in
`RNUmbrella/constants.ts`, or the name tables in
`libraries/BurningManLEDs/LightShow.cpp`. It reports any palette that exists on
only one side.

Two palettes in the phone app — `purpleorange` and `orangepurple` — have no
entry in the firmware's table, so they are left out here; sending them would
silently play `cool` instead. (The phone app has the same problem with the eight
underscored palettes, which it sends without underscores. This app sends the
firmware's spelling.)

The Xcode project is generated too, so adding a Swift file anywhere under
`BMLightsWatch/` just needs:

```bash
ruby scripts/generate-project.rb     # -> BMLightsWatch.xcodeproj
swift scripts/make-icon.swift BMLightsWatch/Assets.xcassets/AppIcon.appiconset/AppIcon.png
```
