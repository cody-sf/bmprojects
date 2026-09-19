# BikeRemote

A standalone handlebar controller for one BMDevice — built for the bike — on
the 1-USB "Cheap Yellow Display" (ESP32-2432S028R / ILI9341, resistive XPT2046
touch). It mirrors the RNUmbrella device page over the same BLE protocol the
phone and watch speak, so nothing on the bike changes: to the firmware this is
just another central.

Sibling of `ElkRemote/` — same board, same touch driver, same
RNUmbrella-flavoured widget set — but where the ELK bars are write-only
foreign hardware, a BMDevice talks back: state here is optimistic only until
the next JSON status chunk lands, and the firmware pushes one on every change
(phone, watch, or the bike's encoder included), so the remote tracks reality
without polling.

## Build & flash

```bash
cd BikeRemote
pio run -e cyd -t upload
pio device monitor -e cyd
```

First boot walks through touch calibration (persisted in NVS). Then open
Setup (gear) → **Choose Light**, power the bike on, and tap it when it
appears. The choice is saved; from then on the remote reconnects by itself
whenever the bike is in range (green RGB LED = connected, amber = searching).

## Screens

- **Home** — power, brightness (capped by the device's own max), speed,
  reverse direction, Find (5 s locate strobe), and the effect-color swatch
  (opens the picker; feeds Color Wheel / Color Radial and tinted effects).
- **Palettes** — every built-in palette as a gradient card, plus whatever
  custom slots the device reports (`cpal` chunks); tap to play. Custom
  palettes are made on the phone — the remote only selects them.
- **Modes** — the effect catalog. GPS effects (Speedometer, Position Status)
  appear only when the device reports GPS; the matrix display effects
  (Marquee Text, Pixel Art, Word Zoom, Animation) only when it reports a
  matrix grid. A **Matrix** chip (also grid-gated) opens the display screen.
- **Matrix** — the app's Matrix Display card in hardware: edit the scrolling
  text on an on-screen keyboard (`0x80`, 63 chars), Show Text / Words / Art /
  Anim buttons (Art and Anim light up only when the device reports stored
  content — pixel art and animation frames are authored and uploaded from the
  phone), the glyph fill (`0x85`: gradient / fire / rain / plasma), bold
  (`0x84`), and the 30 s wiring-diagnostic rainbow (`0x83`).
- **Tweaks** — the selected mode's extra sliders (wave width, meteor count,
  flame height…), same ranges as the app, opened on the device's live values.
- **Setup** — recalibrate touch, choose/forget the light, group-sync toggle
  (ESP-NOW, BLE `0x24`), and Save Look As Power-On (`0x1C`).

The display sleeps (backlight off) after 15 s without a touch
(`SLEEP_TIMEOUT_MS` in `main.cpp`); the BLE link stays up and status keeps
merging underneath. The waking tap is swallowed, so it can never also press
whatever button happens to be under it.

## Generated code

`src/Catalog.h` is **generated** — do not hand-edit:

```bash
node BikeRemote/scripts/generate-catalog.js   # after changing palettes/effects
```

It joins the firmware's wire id strings
(`libraries/BurningManLEDs/src/LightShow.cpp` — the `src` copy, the root one
has drifted) with RNUmbrella's display names, palette colors, and the
`EFFECT_PARAMETERS` slider table (+ its BLE command codes). The firmware is
authoritative for the ids: anything it doesn't recognise falls back to
`cool` / `palette_stream`, which is why unknown entries are skipped rather
than shipped.

## Protocol notes that matter

- The `features` characteristic is `BLERead | BLEWrite`: every write goes
  **with response**, or the firmware never sees it.
- Palette/effect selection is by ASCII id string, never 1 character — the
  firmware reads a 2-byte write as a numeric id.
- Ints (brightness percent, raw speed 5–200, tweak params) ride as int32 LE.
- Status arrives as several JSON chunks, each a complete object with a subset
  of keys: merge, never replace. Chunks run to ~239 bytes, so the MTU is
  raised to 255 — the default 23 would truncate all of them.
- Subscribe **before** requesting status (`0x02`): the firmware drops the
  burst unless the CCCD write has landed. One retry after 1.5 s covers a lost
  burst without turning into a poll.
- "Solid Color" is not in the mode list: the firmware has no `solid` wire
  string (`effectNameToId` would fall back to `palette_stream`). The color
  swatch sets the effect color (`0x19`) instead.

The TFT_eSPI pin map is passed entirely as `build_flags`; the 2-USB / ST7789
CYD variant needs `-DST7789_DRIVER` and `-DTFT_INVERSION_ON` instead. The
XPT2046 library comes from a GitHub tag because the registry package lacks a
darwin_arm64 manifest.
