# ElkRemote

Standalone controller for the ELK-BLEDOM light bars, running on the 1-USB
"Cheap Yellow Display" (ESP32-2432S028R with the ILI9341 panel). It is the
app's Light Bars tab in hardware: same palettes, same modes, same 9-byte
frames, no phone required.

## Build & flash

```bash
pio run -e cyd -t upload
pio device monitor -e cyd
```

No `upload_port` is pinned; PlatformIO auto-detects the CYD's CH340.
If you bought the 2-USB (ST7789) variant instead, swap the driver flags in
`platformio.ini`: `-DST7789_DRIVER` in place of `-DILI9341_2_DRIVER`, plus
`-DTFT_INVERSION_ON`.

## First boot

1. Touch calibration runs automatically (4 targets; the 4th verifies).
   Redo it any time from the gear menu.
2. Power a bar on - it is adopted by its `ELK-` name and reconnected on
   every boot after that, exactly like the phone app's standing loop.
   The board's RGB LED shows link health: red none, amber some, green all.

## Screens

- **Home** - group power, brightness, mode speed, solid color.
- **Palettes** - the app's palette catalog; a tap deals one sampled color
  per connected bar (jittered for 3 or fewer, even spread otherwise).
- **Modes** - the bars' built-in animations (`ELK_MODES`).
- **Bars** - per-bar power/color, `ID` flashes a bar white so you can tell
  which is which, tap a name to rename it (stored on the CYD; the bars
  cannot hold a name).

## Generated palette catalog

`src/Palettes.h` is generated - do not hand-edit:

```bash
node scripts/generate-palettes.js   # after changing palettes in RNUmbrella/constants.ts
```

## Notes

- BLE only, central only, `CONFIG_BT_NIMBLE_MAX_CONNECTIONS=9` for the 8
  bars (ESP32 controller ceiling is 9). WiFi is never initialised.
- The bars report nothing, so all state shown is optimistic - the last
  thing we told them (same as the app).
- Registry and touch calibration live in NVS (Preferences namespace
  `elkremote`); custom palettes from the phone's editor are not available
  here, only the built-ins.
