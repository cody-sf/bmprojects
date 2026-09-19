#!/usr/bin/env python3
"""Generate include/FrontRackMatrixMap.h - the PanelPixel map for the 8x32
WS2812 flex sheet (320 x 80 mm) on the front face of the front rack.

The matrix faces forward, perpendicular to the bike's rear->front sweep axis,
so it can't lie along bike space the way the chevron panel and the frame wrap
do. Instead it *continues* the axis, in one of two styles (STYLE below):

  'chevrons'  (default) The sheet is more chevrons, like the panel: the
              map's progression is a V-shaped field (a front at constant
              progression IS a chevron - vertex mid-height, arms swept back
              CHEVRON_SWEEP columns per row), so chevron_wave rolls off the
              bike and plays as V after V, and seg is the four chevron bands
              stepping in unison with the panel's four. CHEVRON_POINT picks
              the shape: 'out' (default) is << >> blooming out of the
              middle, 'in' converges from the edges, 'right'/'left' is a
              one-direction march (which reads as a turn signal - kept for
              exactly that).
  'radial'    The original splash: progression is the distance from the
              sheet's centre, waves bloom outward as expanding rings. (Looked
              like "a circle expanding" in practice - kept for comparison.)

  x     128 at the start of the progression rising to 255 at its far end -
        the front-most half of bike space, so waves arrive off the bike and
        cross the rack.
  y     true vertical, 0 bottom -> 255 top (panel_rain falls, panel_fire
        rises).
  seg   four progression bands (chevrons or rings), 0 first -> 3 last. seg
        grows with x like the panel's chevrons and the frame's
        quarter-sections, so chevron_chase / chevron_eq step all three rigs
        in unison.
  vdist 0 at a band's leading edge (the chevron's vertex side / a ring's
        outer edge, matching the panel's vertices), 255 at its trailing edge.

The stream order plays the progression start -> end, so every 1D effect
(streams, meteors, cylon...) renders as chevron fronts marching across the
sheet (or, in 'radial', rings expanding out of the middle).

Run from anywhere:  python3 BMGenericDevice/scripts/generate_matrix_map.py
"""

import math
import os

WIDTH = 32
HEIGHT = 8
PITCH_MM = 10.0   # LED pitch, identical on both axes, so rings are true circles
SEGMENTS = 4      # matches CODYS_BIKE_SEGMENTS - the rigs step together

STYLE = 'chevrons'    # 'chevrons' | 'radial' - see the docstring
# Chevron geometry: arms sweep back this many columns per row away from the
# vertical centre. 1.0 = 45 degrees on the physical sheet (the pitch is equal
# on both axes). The vertices lead the march.
CHEVRON_SWEEP = 1.0
# Which way the chevrons face and march:
#   'out'    << >> blooming out of the middle - the radial splash's motion
#            with chevron-shaped fronts. Symmetric, so it never reads as a
#            turn signal. (One-direction marches did: "looks like I'm always
#            turning left".)
#   'in'     >> << converging from the edges into the middle.
#   'right'/'left'  a one-direction march across the sheet - deliberate
#            turn-signal territory, kept for exactly that use.
# The effects' direction toggle still reverses travel at runtime; this picks
# the field's shape and its natural (forward) direction.
CHEVRON_POINT = 'out'

# Wiring of the sheet. To identify it on hardware, run the app's wiring test
# (BLE 0x83), which alternates two patterns:
#
# Rainbow phase - hue = raw chain index, red at LED 0. The macro layout:
#   column_serpentine  one smooth horizontal rainbow, every 8-LED column a
#                      single hue, red at one end of the panel
#   row_serpentine     one smooth vertical rainbow, every 32-LED row roughly
#                      a single hue, red along one long edge
#   tile8_rows[_zigzag] four distinct 8x8 colour blocks (red/green/blue/pink)
#
# The 2026-08-24 bench rainbow was a smooth horizontal gradient, red at the
# left: columns of 8, left to right, LED 0 in the left column. (An earlier
# 'tile8_rows' deduction came from comparing photos against a canvas that had
# been edited after sending - only the rainbow is ground truth.)
#
# Corner phase - chain LEDs 0-7 red, 8-15 green, 16-23 blue, each run's
# first LED white, rest dim. The rainbow cannot see inside a column; this
# can. Read the three white dots (each is the start of its run):
#   whites alternate ends (bottom, top, bottom...)  -> column_serpentine
#   whites all along the SAME edge                  -> column_straight
# and the white-red dot's corner is LED 0: bottom-left = no flips; any other
# corner = the matching FLIP_X / FLIP_Y / both.
#
# Since serpentine top-start vs bottom-start differ by a pure vertical
# mirror, any residual error shows in marquee text as a plain mirror/flip:
# upside-down text = both flips; water-reflection text = FLIP_Y; backwards
# text = FLIP_X. Set, regenerate, reflash.
# SETTLED on hardware, 2026-08-24, by the 0x83 corner phase photographed
# through the diffuser: the three coloured runs are full 8-LED columns at the
# panel's left end (matching the rainbow's red-at-left), their white start
# dots ALTERNATE ends (top, bottom, top) = serpentine, and LED 0's white dot
# is the TOP-left corner = FLIP_Y (the raw layout below starts bottom-left).
LAYOUT = 'column_serpentine'  # 'column_serpentine' | 'column_straight' | 'row_serpentine' | 'tile8_rows' | 'tile8_rows_zigzag'
FLIP_X = False
FLIP_Y = True


def grid_position(p):
    """Physical LED index -> (col, row) with col 0 left, row 0 bottom."""
    if LAYOUT == 'column_straight':
        col = p // HEIGHT
        row = p % HEIGHT
    elif LAYOUT == 'column_serpentine':
        col = p // HEIGHT
        row = p % HEIGHT
        if col % 2 == 1:              # serpentine: odd columns run backwards
            row = HEIGHT - 1 - row
    elif LAYOUT == 'row_serpentine':
        row_top, k = divmod(p, WIDTH)
        col = WIDTH - 1 - k if row_top % 2 == 1 else k
        row = HEIGHT - 1 - row_top
    else:                             # four 8x8 tiles, row-major inside each
        tile, q = divmod(p, 64)
        row_top, k = divmod(q, 8)
        if LAYOUT == 'tile8_rows_zigzag' and row_top % 2 == 1:
            k = 7 - k
        col = tile * 8 + k
        row = HEIGHT - 1 - row_top
    if FLIP_X:
        col = WIDTH - 1 - col
    if FLIP_Y:
        row = HEIGHT - 1 - row
    return col, row


def progression(col, row):
    """0..1 along the map's sweep: chevron field or radial distance."""
    cy = (HEIGHT - 1) / 2
    if STYLE == 'chevrons':
        # A front at constant progression is a V: cells farther from the
        # mid-height line sit later in the progression, so the boundary of
        # everything at-or-before an instant bulges forward at mid-height -
        # a vertex that LEADS the march, arms swept back CHEVRON_SWEEP
        # columns per row, exactly the panel's chevron read. 'out'/'in' run
        # the same field mirrored about the vertical centreline, giving two
        # back-to-back chevrons (<< >>) instead of one lateral march.
        cx = (WIDTH - 1) / 2
        if CHEVRON_POINT in ('out', 'in'):
            edge_dist = abs(col - cx)  # 0 at the centreline -> cx at the edges
            lead = edge_dist if CHEVRON_POINT == 'out' else (cx - edge_dist)
            d = lead + CHEVRON_SWEEP * abs(row - cy)
            return d / (cx + CHEVRON_SWEEP * cy)
        lead = col if CHEVRON_POINT == 'right' else (WIDTH - 1 - col)
        d = lead + CHEVRON_SWEEP * abs(row - cy)
        return d / (WIDTH - 1 + CHEVRON_SWEEP * cy)
    cx = (WIDTH - 1) / 2
    r_max = math.hypot(cx * PITCH_MM, cy * PITCH_MM)  # centre -> corner
    return math.hypot((col - cx) * PITCH_MM, (row - cy) * PITCH_MM) / r_max


def main():
    leds = []  # (col, row, progression 0..1)
    for p in range(WIDTH * HEIGHT):
        col, row = grid_position(p)
        leds.append((col, row, progression(col, row)))

    out = os.path.join(os.path.dirname(__file__), "..", "include", "FrontRackMatrixMap.h")
    with open(out, "w") as f:
        f.write("// GENERATED by scripts/generate_matrix_map.py - do not hand-edit.\n")
        f.write(f"// {WIDTH}x{HEIGHT} front-rack matrix, layout '{LAYOUT}'\n")
        f.write(f"// (FLIP_X={FLIP_X}, FLIP_Y={FLIP_Y}), style '{STYLE}'.\n")
        if STYLE == 'chevrons':
            f.write(f"// Chevron map: four V-shaped bands (vertices '{CHEVRON_POINT}', arms swept\n")
            f.write(f"// {CHEVRON_SWEEP} col/row) stepping with the panel's chevrons - 'out' plays\n")
            f.write("// << >> out of the middle. x 128 -> 255 along the progression (the front\n")
            f.write("// half of bike space), y true vertical 0 bottom -> 255 top, seg the four\n")
            f.write("// chevron bands, vdist 0 at each band's vertex-side edge.\n")
        else:
            f.write("// Radial map: x 128 centre -> 255 outer edge (the front end of bike\n")
            f.write("// space, so waves bloom off the front of the bike), y true vertical\n")
            f.write("// 0 bottom -> 255 top, seg four concentric rings stepping with the\n")
            f.write("// chevrons, vdist 0 at a ring's outer edge.\n")
        f.write("#ifndef FRONT_RACK_MATRIX_MAP_H\n#define FRONT_RACK_MATRIX_MAP_H\n\n")
        f.write("#include <LightShow.h>\n\n")
        f.write(f"#define FRONT_RACK_MATRIX_WIDTH {WIDTH}\n")
        f.write(f"#define FRONT_RACK_MATRIX_HEIGHT {HEIGHT}\n")
        f.write(f"#define FRONT_RACK_MATRIX_LED_COUNT {WIDTH * HEIGHT}\n")
        f.write(f"#define FRONT_RACK_MATRIX_SEGMENTS {SEGMENTS}\n\n")

        f.write("// {x, y, seg, vdist} per physical LED, 8 per line = one wired column.\n")
        f.write("const PanelPixel FRONT_RACK_MATRIX_MAP[FRONT_RACK_MATRIX_LED_COUNT] = {\n")
        for p0 in range(0, len(leds), HEIGHT):
            entries = []
            for col, row, rn in leds[p0:p0 + HEIGHT]:
                x = 128 + round(rn * 127)
                y = round(row * 255 / (HEIGHT - 1))
                seg = min(SEGMENTS - 1, int(rn * SEGMENTS))
                frac = rn * SEGMENTS - seg
                vdist = round((1 - frac) * 255)
                entries.append(f"{{{x},{y},{seg},{vdist}}}")
            f.write("    " + ", ".join(entries) + f",  // col {p0 // HEIGHT}\n")
        f.write("};\n\n")

        # Progression-order playback for setRenderOrder(): 1D effects render
        # as marching chevron fronts (or expanding rings) instead of snaking
        # the serpentine.
        order = sorted(range(len(leds)), key=lambda p: (leds[p][2], leds[p][0], leds[p][1]))
        f.write("// Logical progression-order playback (see setRenderOrder):\n")
        f.write("const uint16_t FRONT_RACK_MATRIX_STREAM_ORDER[FRONT_RACK_MATRIX_LED_COUNT] = {\n")
        for row0 in range(0, len(order), 16):
            f.write("    " + ", ".join(f"{p:3d}" for p in order[row0:row0 + 16]) + ",\n")
        f.write("};\n\n")

        # Display grid for setMatrixGrid(): the radial map deliberately erases
        # columns, so panel_text / panel_bitmap get the real rows and columns
        # here. Row-major with row 0 the TOP row as mounted (display
        # convention - the map's y stays bottom-up).
        #
        # LOGICAL positions, not physical: the display effects render into the
        # same pre-render-order space as every other effect, and show time
        # applies the stream-order permutation exactly once. (Emitting
        # physical indices here double-mapped every pixel through the radial
        # order - the great wiring goose chase of 2026-08-24.)
        inverse_order = [0] * len(order)
        for logical, physical in enumerate(order):
            inverse_order[physical] = logical
        grid = [0] * (WIDTH * HEIGHT)
        for p, (col, row, _) in enumerate(leds):
            grid[(HEIGHT - 1 - row) * WIDTH + col] = inverse_order[p]
        f.write("// grid[row * width + col] -> LOGICAL playback position (pre-render-order),\n")
        f.write("// row 0 = top row (see setMatrixGrid - drives panel_text and panel_bitmap).\n")
        f.write("const uint16_t FRONT_RACK_MATRIX_GRID[FRONT_RACK_MATRIX_LED_COUNT] = {\n")
        for row0 in range(0, len(grid), 16):
            f.write("    " + ", ".join(f"{p:3d}" for p in grid[row0:row0 + 16]) + ",\n")
        f.write("};\n\n#endif // FRONT_RACK_MATRIX_MAP_H\n")

    print(f"wrote {os.path.normpath(out)}: {WIDTH * HEIGHT} LEDs, "
          f"style '{STYLE}', {SEGMENTS} bands")


if __name__ == "__main__":
    main()
