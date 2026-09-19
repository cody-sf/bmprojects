#!/usr/bin/env python3
"""Generate include/CodysBikeMap.h - the LED maps for Cody's bike.

Both strips are mapped into one shared coordinate system, "bike space":
x 0 at the rear of the bike rising to 255 at the nose; the frame strip's y
runs 0 at the low frame rail up to 255 at the head tube.

* Chevron panel (GPIO 32, 12v 3): geometry from the Fusion 360 "radrunner"
  design (bodies PANEL_LEFT_chev12 / PANEL_RIGHT_chev34): four nested
  chevrons pointing at the front of the bike (+x), 50 x 12 mm holes on a
  45-degree grid, 12.375 mm apart along each leg. Chevrons are numbered
  0 (rear) to 3 (front) by vertex x. The strip is strung serpentine,
  starting at the FRONT chevron:
    seg 3 top->bottom, seg 2 bottom->top, seg 1 top->bottom, seg 0 bottom->top.
  The panel's y stays normalised across its own height - rain/fire
  resolution on the panel matters more than sharing a height scale with
  the frame.

* Frame wrap strip (GPIO 33, 12v 2): 12 V glow neon-flex, 3 LEDs per WS2811
  pixel, 5 cm each, 50 driven pixels (dialled in on the bike). Fed at the
  FRONT, it follows the frame rather than a straight line (see the side
  photo): high at the head tube, diving down the down tube to the low
  step-through rail, flat back past the battery to the chainstay, then a
  small climb to wrap behind the rear wheel; the far side mirrors the path
  forward. FRAME_WAYPOINTS_CM below traces that route; pixels are laid at
  even arc-length steps along it, so every pixel knows its true fore-aft
  position AND height. A front->rear sweep therefore crosses the down-tube
  dive as one near-vertical front, panel_fire burns along the low rail and
  licks up the down tube, and panel_rain runs down it. Each side is cut
  into 4 fore-aft bands carrying the chevron segment ids so the frame
  steps/pulses in sympathy with the panel; like a chevron (vertex forward),
  a band's focal point (vdist 0) is its front edge.

The two strips are tied together by Cody's one measurement: 75% of the way
along each side's strip, the front chevron's vertex sits alongside. That
arc point's fore-aft position anchors the panel into bike space.

Run from anywhere:  python3 BMGenericDevice/scripts/generate_bike_map.py
"""

import math
import os

STEP = 1.2375  # cm between hole centres in x and in y along a leg (45 deg)

# (vertex_x, vertex_y, holes_per_leg) in Fusion cm, rear chevron first.
CHEVRONS = [
    (-1.900, 0.150, 5),   # seg 0, 11 holes (panel narrows at the back)
    (1.700, 0.150, 6),    # seg 1, 13 holes
    (5.300, 0.150, 6),    # seg 2, 13 holes
    (8.900, 0.150, 6),    # seg 3, 13 holes
]

# --- Frame wrap strip (GPIO 33) -------------------------------------------
# Measured/dialled in on the bike, not designed.
FRAME_PIXEL_PITCH_CM = 5.0    # 60 LED/m glow flex: one WS2811 per 3 LEDs
FRAME_GROUP_SIZE = 3          # LEDs per pixel -> setStripGroupSize()
FRAME_CHEVRON_START = 0.75    # fraction of each side where the chevrons start
FRAME_SEGMENTS = 4            # virtual chevrons per side (matches the panel)
# Driven pixel count, dialled in on the bike (2026-08-24): the length/pitch
# estimate (64) ran past the strip's end; walked down to 50. Must be even so
# the two sides mirror.
FRAME_PIXELS = 50

# The strip's route along the frame, per side, eyeballed from the side
# photo. (x, y) in cm: x fore-aft from the rear wrap point toward the nose,
# y above the ground. Only the proportions matter - the pixels are spread at
# even arc steps over the whole polyline - so tweak freely and regenerate.
FRAME_WAYPOINTS_CM = [
    (105.0, 85.0),  # head tube, where the first driven pixel sits
    (65.0, 27.0),   # the frame's low point, ahead of the battery
    (10.0, 27.0),   # chainstay by the rear dropout
    (0.0, 42.0),    # wrap point behind the rear wheel
]


def chevron_path(vx, vy, legs, top_to_bottom):
    """Hole centres along one chevron in wiring order, with leg distance."""
    top = [(vx - k * STEP, vy + k * STEP, k) for k in range(legs, 0, -1)]
    bottom = [(vx - k * STEP, vy - k * STEP, k) for k in range(1, legs + 1)]
    path = top + [(vx, vy, 0)] + bottom
    if not top_to_bottom:
        path.reverse()
    return path


def main():
    leds = []  # (x_cm, y_cm, seg, vdist_steps, legs)
    for order, (seg, top_first) in enumerate([(3, True), (2, False), (1, True), (0, False)]):
        vx, vy, legs = CHEVRONS[seg]
        for x, y, k in chevron_path(vx, vy, legs, top_first):
            leds.append((x, y, seg, k, legs))

    assert FRAME_PIXELS % 2 == 0, "frame pixel count must split evenly across sides"
    per_side = FRAME_PIXELS // 2

    pts = FRAME_WAYPOINTS_CM
    seg_lens = [math.dist(a, b) for a, b in zip(pts, pts[1:])]
    path_total = sum(seg_lens)

    def along(frac):
        """(x_cm, y_cm) at arc fraction frac of a side's path (0 feed, 1 wrap)."""
        d = min(max(frac, 0.0), 1.0) * path_total
        for a, b, seg_len in zip(pts, pts[1:], seg_lens):
            if d <= seg_len:
                t = d / seg_len
                return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)
            d -= seg_len
        return pts[-1]

    def frame_pixel(i):
        """Physical pixel i -> (x_cm, y_cm, side)."""
        if i < per_side:                    # side A, front -> rear
            k, side = i, 0
        else:                               # side B, rear -> front
            k, side = 2 * per_side - 1 - i, 1
        x, y = along((k + 0.5) / per_side)
        return x, y, side

    frame_pts = [frame_pixel(i) for i in range(FRAME_PIXELS)]

    # Cody's measurement: the front chevron's vertex sits alongside the strip
    # 75% of the way down each side. That arc point places the panel.
    panel_front_x, _ = along(FRAME_CHEVRON_START)

    xs = [l[0] for l in leds]
    ys = [l[1] for l in leds]
    x_fusion_front = max(xs)  # the front chevron's vertex in Fusion cm
    y0, y1 = min(ys), max(ys)

    def panel_x_cm(x_fusion):
        return panel_front_x - (x_fusion_front - x_fusion)

    assert panel_x_cm(min(xs)) > 0, "panel would extend past the wrap point"

    all_x = [p[0] for p in frame_pts] + [panel_x_cm(x) for x in xs]
    x_lo, x_hi = min(all_x), max(all_x)
    fy_lo = min(p[1] for p in frame_pts)
    fy_hi = max(p[1] for p in frame_pts)

    def norm(v, lo, hi):
        return round((v - lo) / (hi - lo) * 255)

    out = os.path.join(os.path.dirname(__file__), "..", "include", "CodysBikeMap.h")
    with open(out, "w") as f:
        f.write("// GENERATED by scripts/generate_bike_map.py - do not hand-edit.\n")
        f.write("// LED maps for Cody's bike: the 4-chevron panel (Fusion 'radrunner'\n")
        f.write("// design) and the frame wrap glow strip, in one shared coordinate\n")
        f.write("// system. x: 0 rear of the BIKE -> 255 nose (the panel spans "
                f"{norm(panel_x_cm(min(xs)), x_lo, x_hi)}..{norm(panel_front_x, x_lo, x_hi)},\n")
        f.write("// anchored by the chevrons starting 75% of the way down each side).\n")
        f.write("// The frame strip's pixels carry its real route along the frame\n")
        f.write("// (head-tube dive, low rail, rear wrap - a waypoint polyline in the\n")
        f.write("// generator), so y is true height: 0 at the low rail, 255 at the\n")
        f.write("// head tube. Panel y stays 0 bottom -> 255 top across its own\n")
        f.write("// height. seg: chevron 0 (rear) .. 3 (front); the frame's fore-aft\n")
        f.write("// quarter-bands carry the seg of the chevron they step with.\n")
        f.write("// vdist: 0 at a segment's focal point (chevron vertex / band front\n")
        f.write("// edge), 255 at its far end (leg tip / band rear edge).\n")
        f.write("#ifndef CODYS_BIKE_MAP_H\n#define CODYS_BIKE_MAP_H\n\n")
        f.write("#include <LightShow.h>\n\n")
        f.write(f"#define CODYS_BIKE_LED_COUNT {len(leds)}\n")
        f.write("#define CODYS_BIKE_SEGMENTS 4\n\n")
        f.write("const PanelPixel CODYS_BIKE_MAP[CODYS_BIKE_LED_COUNT] = {\n")
        for i, (x, y, seg, k, legs) in enumerate(leds):
            vdist = round(k / legs * 255)
            f.write(
                f"    {{{norm(panel_x_cm(x), x_lo, x_hi):3d}, {norm(y, y0, y1):3d}, {seg}, {vdist:3d}}},"
                f"  // led {i:2d}  ({x:7.3f}, {y:7.3f}) cm\n"
            )
        f.write("};\n\n")

        # Playback order for LightShow::setRenderOrder(): logical position k
        # (0 = rear-most, ascending toward the front vertex) -> physical LED.
        # With this, every 1D effect sweeps the panel rear -> front instead of
        # following the wiring serpentine.
        order = sorted(range(len(leds)), key=lambda i: (leds[i][0], leds[i][1]))
        f.write("// Logical rear->front playback order (see setRenderOrder):\n")
        f.write("// streams, meteors, cylon etc. travel the bike's axis instead\n")
        f.write("// of snaking the wiring path.\n")
        f.write("const uint16_t CODYS_BIKE_STREAM_ORDER[CODYS_BIKE_LED_COUNT] = {\n")
        for row in range(0, len(order), 10):
            f.write("    " + ", ".join(f"{p:2d}" for p in order[row:row + 10]) + ",\n")
        f.write("};\n\n")

        # --- Frame wrap strip ---
        f.write(f"// Frame wrap glow strip: {FRAME_PIXEL_PITCH_CM:.0f} cm/pixel "
                f"({FRAME_GROUP_SIZE} LEDs each), {FRAME_PIXELS} pixels\n")
        f.write(f"// ({per_side} per side, count dialled in on the bike). Pixels walk\n")
        f.write("// the frame route at even arc steps: head tube down to the low\n")
        f.write("// rail, flat to the chainstay, wrap behind the rear wheel.\n")
        f.write(f"#define CODYS_BIKE_FRAME_LED_COUNT {FRAME_PIXELS}\n")
        f.write(f"#define CODYS_BIKE_FRAME_GROUP_SIZE {FRAME_GROUP_SIZE}\n\n")
        f.write("const PanelPixel CODYS_BIKE_FRAME_MAP[CODYS_BIKE_FRAME_LED_COUNT] = {\n")
        for i, (x, y, side) in enumerate(frame_pts):
            fx = (x - x_lo) / (x_hi - x_lo)  # 0 rear .. 1 front, true fore-aft
            band = min(FRAME_SEGMENTS - 1, int(fx * FRAME_SEGMENTS))
            band_lo = band / FRAME_SEGMENTS
            band_hi = (band + 1) / FRAME_SEGMENTS
            vdist = round((band_hi - fx) / (band_hi - band_lo) * 255)
            vdist = min(255, max(0, vdist))
            f.write(
                f"    {{{norm(x, x_lo, x_hi):3d}, {norm(y, fy_lo, fy_hi):3d}, {band}, {vdist:3d}}},"
                f"  // px {i:2d}  side {'A' if side == 0 else 'B'}, "
                f"x {x:5.1f} cm, y {y:4.1f} cm\n"
            )
        f.write("};\n\n")

        # Same rear->front logical order for the frame: rear-most first, the
        # two sides interleaved, so 1D effects sweep both sides in mirror
        # instead of running down one side and back up the other.
        forder = sorted(range(FRAME_PIXELS), key=lambda i: (frame_pts[i][0], i))
        f.write("// Logical rear->front playback order, both sides in mirror:\n")
        f.write("const uint16_t CODYS_BIKE_FRAME_STREAM_ORDER[CODYS_BIKE_FRAME_LED_COUNT] = {\n")
        for row in range(0, len(forder), 10):
            f.write("    " + ", ".join(f"{p:2d}" for p in forder[row:row + 10]) + ",\n")
        f.write("};\n\n#endif // CODYS_BIKE_MAP_H\n")
    print(f"wrote {os.path.normpath(out)}: {len(leds)} panel LEDs, "
          f"{FRAME_PIXELS} frame pixels along a {path_total:.0f} cm/side route "
          f"(driven length {per_side * FRAME_PIXEL_PITCH_CM:.0f} cm/side; "
          f"proportions only), chevrons start at x {panel_front_x:.0f} cm")


if __name__ == "__main__":
    main()
