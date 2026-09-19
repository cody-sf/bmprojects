#ifndef LIGHTSHOW_H
#define LIGHTSHOW_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <FastLED.h>
#include <Clock.h>

#define MAX_PALETTE_SIZE 8

// Strips that group several LEDs per addressable pixel (12 V glow strip
// WS2811 boards run 3-6 LEDs per IC) cover several times the physical
// distance per pixel, so motion tuned on a dense wearable strip tears across
// them. Motion is slowed per strip via setStripGroupSize() - runtime, from the
// app's strand settings - because one device can mix grouped glow strips with
// normal-density button LEDs or fairy lights on its other outputs. The old
// build flag survives as the default for strips never configured. Frame rate
// and the app's speed slider are unaffected either way.
#ifndef BM_MOTION_PERCENT
#define BM_MOTION_PERCENT 100
#endif

enum LightSceneID : uint8_t
{
    off,
    solid,
    palette_cycle,
    palette_stream,
    spectrum_cycle,
    spectrum_stream,
    spectrum_sparkle,
    strobe,
    sparkle,
    breathe,
    setCHSV,
    position_status,
    color_wheel,
    speedometer,
    jacketDance,
    color_radial,
    pulse_wave,
    meteor_shower,
    fire_plasma,
    kaleidoscope,
    rainbow_comet,
    matrix_rain,
    plasma_clouds,
    lava_lamp,
    aurora_borealis,
    lightning_storm,
    color_explosion,
    spiral_galaxy,
    // Appended so every earlier effect keeps the id it already has on the wire.
    noise_flow,
    twinkle,
    ripple,
    cylon,
    fireworks,
    // Panel-aware effects (Cody's bike chevrons). On rigs without a pixel map
    // they fall back to treating each strip as a straight run of 4 segments.
    chevron_wave,
    chevron_chase,
    chevron_burst,
    chevron_glow,
    chevron_eq,
    // FastLED classics, adapted to the palette system and the shared clock.
    // Appended so every earlier effect keeps the id it already has on the wire.
    confetti,
    juggle,
    sinelon,
    bpm,
    pacifica,
    // True-2D panel effects (the bike's front-rack 8x32 matrix, and anything
    // else with a pixel map). They read PanelPixel x/y as real geometry, so on
    // the chevron panel they follow its shape too. Appended so every earlier
    // effect keeps the id it already has on the wire.
    panel_waves,
    panel_fire,
    panel_rain,
    panel_spin,
    // Matrix display effects: a scrolling marquee of device-stored text and a
    // phone-uploaded 16-colour bitmap. They render through a strip's matrix
    // grid (setMatrixGrid); strips without one play a palette wash alongside.
    panel_text,
    panel_bitmap,
    // Word-at-a-time zoom: each word of the stored text flies at the viewer,
    // growing from a point to fill the panel (reversed: flies away).
    panel_words,
    // Appended so every earlier effect keeps the id it already has on the wire.
    // starfield: stars stream out of the matrix centre (warp); mapped strips
    // without a grid play star streaks along x instead. orbit_comet: a comet
    // laps each strip's PHYSICAL wiring chain - on the bike's frame wrap the
    // chain is laid along the frame, so the comet orbits the silhouette.
    // panel_puddle: rain falls through panel space and floods the low pixels,
    // then drains. panel_anim: flips through the stored animation frames
    // (a display effect like panel_bitmap - the Matrix card switches to it).
    starfield,
    orbit_comet,
    panel_puddle,
    panel_anim
};

/// The last valid effect id - anything above this off the wire is garbage.
#define LIGHT_SCENE_ID_MAX LightSceneID::panel_anim

/// One LED of a shaped panel, for effects that want geometry instead of a strip
/// index. All values are pre-normalised so effects do no per-frame maths:
/// x 0-255 across the panel (rear -> front), y 0-255 bottom -> top, seg the
/// segment (chevron) this LED belongs to, vdist 0 at the segment's focal point
/// (the chevron vertex) rising to 255 at the segment's far end (a leg tip).
struct PanelPixel
{
    uint8_t x;
    uint8_t y;
    uint8_t seg;
    uint8_t vdist;
};

enum AvailablePalettes : uint8_t
{
    candy,
    cool,
    cosmicwaves,
    earth,
    eblossom,
    emerald,
    everglow,
    fatboy,
    fireice,
    fireynight,
    flame,
    heart,
    lava,
    meadow,
    melonball,
    nebula,
    oasis,
    pinksplash,
    r,
    sofia,
    sunset,
    sunsetfusion,
    trove,
    vivid,
    velvet,
    vga,
    wave,
    // New Burning Man palettes
    electricdesert,
    psychedelicplaya,
    burningrainbow,
    neonnights,
    desertstorm,
    cosmicfire,
    alienglow,
    moltenmetal,
    purpleorange,
    orangepurple,
    // Slots the phone fills over BLE. They sit at the tail of the enum so every
    // built-in palette keeps the id it already has on the wire.
    custom1,
    custom2,
    custom3,
    custom4
};

/// How many custom palettes a device stores.
#define CUSTOM_PALETTE_COUNT 4
/// A custom palette arrives as this many RGB entries - one per CRGBPalette16
/// slot - so the device never has to interpolate a gradient itself.
#define CUSTOM_PALETTE_ENTRIES 16

/// Longest marquee text the display will store/scroll.
#define MARQUEE_TEXT_MAX 63
/// A matrix bitmap is 4-bit palette-indexed: 16 colours + two pixels per
/// byte keeps a full 8x32 upload (+palette +header) inside one BLE write.
#define MATRIX_BITMAP_COLORS 16
#define MATRIX_BITMAP_MAX_PIXELS 256
/// Frames a stored animation can hold (each one a full bitmap-format frame).
#define MATRIX_ANIM_MAX_FRAMES 8

/// What the matrix display overlay is showing (setMatrixDisplay). The display
/// is NOT an effect: it owns only the grid strip(s) and runs on its own clock
/// (setMatrixSpeed), so the rest of the rig keeps playing the selected effect
/// and the speed knob never paces the content.
#define MATRIX_DISPLAY_OFF 0
#define MATRIX_DISPLAY_TEXT 1
#define MATRIX_DISPLAY_WORDS 2
#define MATRIX_DISPLAY_BITMAP 3
#define MATRIX_DISPLAY_ANIM 4
#define MATRIX_DISPLAY_MAX MATRIX_DISPLAY_ANIM

/// Default and bounds for the display's own pace (ms per animation frame;
/// text/word motion scales off the same value, calibrated so the default
/// scrolls like the old speed knob's midpoint).
#define MATRIX_SPEED_DEFAULT_MS 100
#define MATRIX_SPEED_MIN_MS 20
#define MATRIX_SPEED_MAX_MS 2000

// Trivial RGB for union members (CRGB has non-trivial ctor, breaks union default-init on strict compilers)
struct SceneRGB { uint8_t r, g, b; };

struct LightScene
{
    LightSceneID scene_id;
    uint8_t brightness;
    uint8_t selected_devices;
    uint32_t reference_time;
    uint16_t speed;
    AvailablePalettes primary_palette;
    CRGB color;
    bool direction;
    union
    {
        struct { SceneRGB color; } solid;
        struct
        {
            uint16_t duration;
            AvailablePalettes palette;
        } palette_cycle;
        struct
        {
            uint16_t duration;
            AvailablePalettes palette;
            bool direction;
        } palette_stream;

        struct
        {
            uint16_t duration;
        } spectrum_cycle;
        struct
        {
            uint16_t duration;
        } spectrum_stream;
        struct
        {
            uint8_t density;
        } spectrum_sparkle;
        struct
        {
            uint16_t num_flashes;
            uint16_t duration_on;
            uint16_t duration_off;
            uint16_t duration_between_sets;
            SceneRGB color;
        } strobe;
        struct
        {
            uint16_t duration;
            uint8_t density;
            SceneRGB color;
        } sparkle;
        struct
        {
            uint16_t duration;
            uint8_t dimness;
            SceneRGB color;
        } breathe;

        struct
        {
            int color;
            int saturation;
            int luminosity;
        } setCHSV;

        // New modern effects structures
        struct
        {
            uint16_t duration;
            uint8_t wave_width;
            AvailablePalettes palette;
        } pulse_wave;
        
        struct
        {
            uint16_t duration;
            uint8_t meteor_count;
            uint8_t trail_length;
            AvailablePalettes palette;
        } meteor_shower;
        
        struct
        {
            uint16_t duration;
            uint8_t heat_variance;
            AvailablePalettes palette;
        } fire_plasma;
        
        struct
        {
            uint16_t duration;
            uint8_t mirror_count;
            AvailablePalettes palette;
        } kaleidoscope;
        
        struct
        {
            uint16_t duration;
            uint8_t comet_count;
            uint8_t trail_length;
        } rainbow_comet;
        
        struct
        {
            uint16_t duration;
            uint8_t drop_rate;
            SceneRGB color;
        } matrix_rain;
        
        struct
        {
            uint16_t duration;
            uint8_t cloud_scale;
            AvailablePalettes palette;
        } plasma_clouds;
        
        struct
        {
            uint16_t duration;
            uint8_t blob_count;
            AvailablePalettes palette;
        } lava_lamp;
        
        struct
        {
            uint16_t duration;
            uint8_t wave_count;
            AvailablePalettes palette;
        } aurora_borealis;
        
        struct
        {
            uint16_t duration;
            uint8_t flash_intensity;
            uint16_t flash_frequency;
        } lightning_storm;
        
        struct
        {
            uint16_t duration;
            uint8_t explosion_size;
            AvailablePalettes palette;
        } color_explosion;
        
        struct
        {
            uint16_t duration;
            uint8_t spiral_arms;
            AvailablePalettes palette;
        } spiral_galaxy;

        // New effects keep `duration` as the first member like everything
        // above: apply_scene_updates(speed) and msUntilNextFrame() rely on the
        // union members sharing that leading field.
        struct
        {
            uint16_t duration;
            uint8_t scale;
            AvailablePalettes palette;
        } noise_flow;

        struct
        {
            uint16_t duration;
            uint8_t density;
            AvailablePalettes palette;
        } twinkle;

        struct
        {
            uint16_t duration;
            uint8_t width;
            AvailablePalettes palette;
        } ripple;

        struct
        {
            uint16_t duration;
            uint8_t trail;
            AvailablePalettes palette;
        } cylon;

        struct
        {
            uint16_t duration;
            uint8_t size;
            AvailablePalettes palette;
        } fireworks;

        struct
        {
            uint16_t duration;
            uint8_t width;
            AvailablePalettes palette;
            bool direction;
        } chevron_wave;

        struct
        {
            uint16_t duration;
            uint8_t trail;
            AvailablePalettes palette;
            bool direction;
        } chevron_chase;

        struct
        {
            uint16_t duration;
            uint8_t width;
            AvailablePalettes palette;
            bool direction;
        } chevron_burst;

        struct
        {
            uint16_t duration;
            AvailablePalettes palette;
        } chevron_glow;

        struct
        {
            uint16_t duration;
            AvailablePalettes palette;
        } chevron_eq;

        struct
        {
            uint16_t duration;
            uint8_t density;
            AvailablePalettes palette;
        } confetti;

        struct
        {
            uint16_t duration;
            uint8_t ball_count;
            AvailablePalettes palette;
        } juggle;

        struct
        {
            uint16_t duration;
            uint8_t trail;
            AvailablePalettes palette;
        } sinelon;

        struct
        {
            uint16_t duration;
            AvailablePalettes palette;
        } bpm;

        struct
        {
            uint16_t duration;
        } pacifica;

        struct
        {
            uint16_t duration;
            uint8_t scale;
            AvailablePalettes palette;
        } panel_waves;

        struct
        {
            uint16_t duration;
            uint8_t variance;
            AvailablePalettes palette;
        } panel_fire;

        struct
        {
            uint16_t duration;
            uint8_t density;
            AvailablePalettes palette;
        } panel_rain;

        struct
        {
            uint16_t duration;
            uint8_t arms;
            AvailablePalettes palette;
        } panel_spin;

        struct
        {
            uint16_t duration;
            uint8_t density;
            AvailablePalettes palette;
            bool direction;
        } starfield;

        struct
        {
            uint16_t duration;
            uint8_t trail;
            AvailablePalettes palette;
            bool direction;
        } orbit_comet;

        struct
        {
            uint16_t duration;
            uint8_t density;
            AvailablePalettes palette;
        } panel_puddle;

    } scenes;
};

class LightShow
{
public:
    LightShow(const std::vector<CLEDController *> &led_controllers = std::vector<CLEDController *>(), const Clock &clock = Clock());
    ~LightShow();
    void add_led_controller(CLEDController *led_controller);
    void brightness(uint8_t brightness);
    void speed(uint16_t speed);
    void setSpeed(uint16_t speed);
    void solid(const CRGB &color);
    void spectrum_cycle(uint32_t duration);
    void spectrum_stream(uint32_t duration);
    void spectrum_sparkle(uint16_t duration, uint8_t density);
    void palette_cycle(AvailablePalettes palette, uint32_t duration = 100);
    void palette_stream(uint16_t duration, AvailablePalettes palette, bool direction = true);
    void strobe(uint16_t num_flashes, uint16_t duration_on, uint16_t duration_off, uint16_t duration_between_sets, CRGB color);
    void sparkle(uint16_t duration, uint8_t density, CRGB color);
    void breathe(uint16_t duration, uint8_t dimness, CRGB color);
    void setCHSV(int color, int saturation, int luminosity);
    
    // New modern effects for Burning Man!
    void pulse_wave(uint16_t duration, uint8_t wave_width, AvailablePalettes palette);
    void meteor_shower(uint16_t duration, uint8_t meteor_count, uint8_t trail_length, AvailablePalettes palette);
    void fire_plasma(uint16_t duration, uint8_t heat_variance, AvailablePalettes palette);
    void kaleidoscope(uint16_t duration, uint8_t mirror_count, AvailablePalettes palette);
    void rainbow_comet(uint16_t duration, uint8_t comet_count, uint8_t trail_length);
    void matrix_rain(uint16_t duration, uint8_t drop_rate, CRGB color = CRGB::Green);
    void plasma_clouds(uint16_t duration, uint8_t cloud_scale, AvailablePalettes palette);
    void lava_lamp(uint16_t duration, uint8_t blob_count, AvailablePalettes palette);
    void aurora_borealis(uint16_t duration, uint8_t wave_count, AvailablePalettes palette);
    void lightning_storm(uint16_t duration, uint8_t flash_intensity, uint16_t flash_frequency);
    void color_explosion(uint16_t duration, uint8_t explosion_size, AvailablePalettes palette);
    void spiral_galaxy(uint16_t duration, uint8_t spiral_arms, AvailablePalettes palette);
    void noise_flow(uint16_t duration, uint8_t scale, AvailablePalettes palette);
    void twinkle(uint16_t duration, uint8_t density, AvailablePalettes palette);
    void ripple(uint16_t duration, uint8_t width, AvailablePalettes palette);
    void cylon(uint16_t duration, uint8_t trail, AvailablePalettes palette);
    void fireworks(uint16_t duration, uint8_t size, AvailablePalettes palette);
    void chevron_wave(uint16_t duration, uint8_t width, AvailablePalettes palette, bool direction = true);
    void chevron_chase(uint16_t duration, uint8_t trail, AvailablePalettes palette, bool direction = true);
    void chevron_burst(uint16_t duration, uint8_t width, AvailablePalettes palette, bool direction = true);
    void chevron_glow(uint16_t duration, AvailablePalettes palette);
    void chevron_eq(uint16_t duration, AvailablePalettes palette);

    // FastLED classics. bpm has no knob beyond the speed slider - the slider
    // *is* the tempo - and pacifica plays its own layered ocean palettes.
    void confetti(uint16_t duration, uint8_t density, AvailablePalettes palette);
    void juggle(uint16_t duration, uint8_t ball_count, AvailablePalettes palette);
    void sinelon(uint16_t duration, uint8_t trail, AvailablePalettes palette);
    void bpm(uint16_t duration, AvailablePalettes palette);
    void pacifica(uint16_t duration);

    // True-2D panel effects: interference waves, rising palette fire, falling
    // palette rain, and a spinning radial sweep. All stateless fields off the
    // shared phase, so a synced group holds them in step.
    void panel_waves(uint16_t duration, uint8_t scale, AvailablePalettes palette);
    void panel_fire(uint16_t duration, uint8_t variance, AvailablePalettes palette);
    void panel_rain(uint16_t duration, uint8_t density, AvailablePalettes palette);
    void panel_spin(uint16_t duration, uint8_t arms, AvailablePalettes palette);

    // Starfield warp (stars out of the matrix centre; star streaks along x on
    // mapped strips without a grid; direction reverses into falling inward),
    // the orbiting comet (laps each strip's physical wiring chain), and the
    // flooding rain.
    void starfield(uint16_t duration, uint8_t density, AvailablePalettes palette, bool direction = true);
    void orbit_comet(uint16_t duration, uint8_t trail, AvailablePalettes palette, bool direction = true);
    void panel_puddle(uint16_t duration, uint8_t density, AvailablePalettes palette);

    /// The matrix display overlay: marquee text, word zoom, the stored bitmap
    /// or the stored animation, drawn OVER whatever effect is running - but
    /// only on strips with a display grid, so the rest of the rig keeps
    /// playing the selected effect in the selected palette. Runs on its own
    /// clock (setMatrixSpeed), untouched by the effect speed knob. Modes with
    /// no content behind them (empty marquee, no stored frames) simply don't
    /// engage, and neither does a rig without a grid.
    void setMatrixDisplay(uint8_t mode);
    uint8_t matrixDisplay() const { return matrix_display_mode_; }

    /// The display's own pace: exactly ms per animation frame; the marquee
    /// scroll and word zoom scale off the same value (100 matches the old
    /// speed knob's midpoint). Clamped to MATRIX_SPEED_MIN/MAX_MS.
    void setMatrixSpeed(uint16_t ms);
    uint16_t matrixSpeed() const { return matrix_speed_ms_; }

    /// The palette the marquee's glyph fills draw from and the direction the
    /// text scrolls (words: fly at you vs recede). Pushed by the device layer
    /// whenever its palette/direction change so the text recolors live; the
    /// bitmap and animation carry their own colours and ignore both.
    void setMatrixStyle(AvailablePalettes palette, bool forward);

    /// Text rendering style for the display effects: 0 normal, 1 bold
    /// (strokes doubled one column to the right). Live, like the text.
    void setTextStyle(uint8_t style) { text_style_ = style ? 1 : 0; }
    uint8_t textStyle() const { return text_style_; }

    /// What the marquee glyphs are filled with: 0 the sliding palette
    /// gradient, 1 rising fire, 2 falling rain streaks, 3 plasma - all in
    /// the current palette, run off the overlay clock. Applies to the
    /// marquee and the word zoom alike. Live, like the text.
    void setTextFill(uint8_t fill) { text_fill_ = fill < 4 ? fill : 0; }
    uint8_t textFill() const { return text_fill_; }

    /// Give a strip a real display grid for the matrix effects:
    /// grid[row * width + col] = the LED's LOGICAL playback position (the
    /// same pre-render-order space every effect writes into; show time
    /// applies the wiring permutation once for everyone), row 0 the TOP row
    /// as mounted. The table is not copied. Distinct from the pixel map -
    /// the bike's radial map deliberately erases columns.
    void setMatrixGrid(size_t strip_index, uint8_t width, uint8_t height, const uint16_t *grid);
    /// True when any strip has a display grid (the matrix display overlay
    /// has somewhere to draw; without one it simply never engages).
    bool hasMatrixGrid() const;

    /// Diagnostic frame for the wiring test: a red top edge, green left
    /// column and a white 'F' drawn THROUGH the display grid and shown
    /// immediately. Firmware-only - no app content involved - so one glance
    /// separates "the grid is wrong" from "the uploaded content is wrong":
    /// a readable F with red on top and green at the left means the whole
    /// grid pipeline is correct; a mirrored F names the flip; a shredded F
    /// means the layout constant is wrong.
    void renderMatrixTestCard(uint8_t brightness);
    /// The first display grid's dimensions, 0 with no grid. Reported in the
    /// devConfig chunk so the app knows whether to offer the display effects.
    uint8_t matrixGridWidth() const;
    uint8_t matrixGridHeight() const;

    /// The marquee text the display scrolls. Stored copy, truncated to
    /// MARQUEE_TEXT_MAX; live - the next frame picks it up mid-scroll.
    void setMarqueeText(const char *text);
    const char *marqueeText() const { return marquee_text_; }

    /// Store the bitmap the display draws: a 16-colour palette and 4-bit
    /// pixels packed two per byte (high nibble first), row-major from the top
    /// row. Copied; w*h capped at MATRIX_BITMAP_MAX_PIXELS.
    void setMatrixBitmap(uint8_t w, uint8_t h, const uint8_t *palette_rgb, const uint8_t *pixels);
    void clearMatrixBitmap();
    bool hasMatrixBitmap() const { return bitmap_set_; }

    /// Store one animation frame (same format as the bitmap) into a slot.
    /// Playback runs the contiguous run of set frames from 0; a gap ends
    /// the animation, so the phone clears before uploading a shorter one.
    void setAnimFrame(uint8_t index, uint8_t w, uint8_t h, const uint8_t *palette_rgb, const uint8_t *pixels);
    void clearAnimFrames();
    uint8_t animFrameCount() const;

    /// Attach a geometry map to a strip (by the order strips were added). The
    /// map is not copied - point it at a static table sized to the strip. The
    /// chevron effects read it; strips without one get a synthesized fallback
    /// that divides the strip into 4 straight segments.
    void setPixelMap(size_t strip_index, const PanelPixel *map, uint8_t segment_count);

    /// True when any strip carries real panel geometry (setPixelMap). The
    /// chevron effects only look like anything on such a rig; callers use
    /// this to substitute a straight-strip effect everywhere else.
    bool hasPanelMap() const;

    /// Give a strip a playback order: order[k] = the physical (wiring-order)
    /// LED of logical position k. Every effect then renders in logical space -
    /// for a shaped panel, sorted by geometry so 1D effects sweep the panel
    /// rear -> front instead of following the wiring serpentine - and the
    /// permutation is applied only at show time. The table is not copied.
    /// Call after the strip has been added (the scratch is sized from it).
    void setRenderOrder(size_t strip_index, const uint16_t *order);

    /// How many physical LEDs one addressable pixel drives on this strip
    /// (1 = normal density, 6 = 12 V glow strip). Field effects slow their
    /// motion across the strip proportionally, so a pattern crosses the same
    /// physical distance per second as it would on a dense strip. Live: takes
    /// effect on the next frame, no scene restart.
    void setStripGroupSize(size_t strip_index, uint8_t leds_per_pixel);

    /// Per-strip brightness ceiling (255 = uncapped). Not a clamp: the strip's
    /// output is the master brightness scaled by cap/255, so a capped strand
    /// dims in step with the rest of the rig instead of flat-lining until the
    /// master drops below it. Applied at show time, before the power budget,
    /// so mixed hardware can run dim accents beside bright strips - or hold a
    /// strand under the level where its first pixel starts to glitch. Live.
    void setStripMaxBrightness(size_t strip_index, uint8_t max_brightness);

    /// Milliseconds until the active scene wants its next frame (0 = due now),
    /// so the caller can sleep instead of spinning. A running transition
    /// reports its own faster tick.
    unsigned long msUntilNextFrame(unsigned long now) const;

    /// Crossfade length when the scene or palette changes. 0 disables
    /// transitions and restores hard cuts.
    void setTransitionDuration(uint16_t ms) { transition_duration_ = ms; }

    /// Force the next render pass to repaint even a static scene - used after
    /// something outside the show (the find-me strobe) drove the strips.
    void requestRepaint()
    {
        scene_changed_ = true;
        last_render_time_ = 0;
    }

    /// Per-strip current budget in mA at 5 V (0 = no limiting). Applied at show
    /// time via FastLED's power maths - a frame that would draw more is dimmed,
    /// which both protects the supply and puts a hard ceiling on battery draw.
    void setPowerBudgetPerStrip(uint32_t milliamps) { power_budget_ma_ = milliamps; }

    void apply_scene_updates(uint8_t brightness);
    void apply_scene_updates(uint16_t speed);
    void apply_scene_updates(LightScene &new_scene);
    void apply_sync_updates(LightScene &new_scene);
    void render();
    bool scene_changed();
    void import_scene(const LightScene *buffer);
    void export_scene(LightScene *buffer) const;
    CRGBPalette16 getPalette(AvailablePalettes palette);
    void setPrimaryPalette(AvailablePalettes palette);
    void setSecondaryPalette(AvailablePalettes palette);
    std::pair<CRGBPalette16, CRGBPalette16> getPrimarySecondaryPalettes() const
    {
        return std::make_pair(primary_palette_, secondary_palette_);
    }
    uint8_t getBrightness() const;
    size_t getPaletteCount() const;
    // For cycles
    CRGBPalette16 getPalette(size_t index) const;
    AvailablePalettes getPrimaryPalette() const;
    void setPrimaryPalette(size_t index);
    LightScene getCurrentScene() const;

    // --- Custom palettes ---
    // Four slots the phone fills with a gradient it has already sampled to
    // CUSTOM_PALETTE_ENTRIES colours (BLE_FEATURE_SET_CUSTOM_PALETTE). The
    // device stores them and plays them like any other palette.
    void setCustomPalette(uint8_t slot, const CRGB *entries);
    void clearCustomPalette(uint8_t slot);
    /// True for any built-in palette, and for a custom slot that has been
    /// filled. An empty slot must never be selected - it would render black.
    bool isPaletteAvailable(AvailablePalettes palette) const;

    static bool isCustomPalette(AvailablePalettes palette);
    /// The slot behind a custom palette id, or -1 for a built-in one.
    static int customPaletteSlot(AvailablePalettes palette);
    static AvailablePalettes customPaletteId(uint8_t slot);

    // --- Static mapping functions for effect/palette names <-> enums ---
    static LightSceneID effectNameToId(const char* name);
    static const char* effectIdToName(LightSceneID id);
    static AvailablePalettes paletteNameToId(const char* name);
    static const char* paletteIdToName(AvailablePalettes id);

private:
    static constexpr unsigned long static_scene_refresh_interval = 10000;
    void setup_breathe_palette_(uint8_t dimness, CRGB color);
    void setup_spectrum_stream_();
    void setup_palette_stream_(bool direction);

    // --- Crossfade transitions ---
    // On a scene/palette change the outgoing frame is snapshotted and dissolved
    // into the incoming effect. Effects keep rendering into their own buffers
    // untouched (trail/heat state survives); the ticker blends snapshot+frame
    // into the buffer, shows it, then restores the frame.
    static constexpr unsigned long transition_tick_ms = 30;
    void begin_transition_();
    void transition_tick_(unsigned long now);
    uint8_t transition_progress_(unsigned long now) const;
    bool ensure_transition_buffers_();
    /// The one place pixels leave the building. Applies the power budget and,
    /// during a transition, defers to the ticker instead of showing directly.
    void show_(CLEDController *controller, uint8_t brightness);
    /// showColor for static scenes, but always mirrored into the LED buffer so
    /// a transition starting later has a true picture of what was displayed.
    void show_color_(CLEDController *controller, const CRGB &color, uint8_t brightness);
    void show_now_(CLEDController *controller, uint8_t brightness);
    CRGB *transition_snapshot_ = nullptr; // outgoing frame, all strips end to end
    CRGB *transition_scratch_ = nullptr;  // one strip, for the blend/restore dance
    size_t transition_total_leds_ = 0;
    size_t transition_max_strip_ = 0;
    bool transitioning_ = false;
    unsigned long transition_start_ = 0;
    unsigned long last_transition_tick_ = 0;
    uint16_t transition_duration_ = 600;
    uint32_t power_budget_ma_ = 0;
    std::vector<CLEDController *> led_controllers_;
    LightScene active_scene_;
    bool scene_changed_;
    unsigned long last_render_time_;
    unsigned long start_time_;
    unsigned long current_frame_duration_;
    uint8_t hue_;
    uint8_t frame_number_;
    uint8_t scale_;
    size_t palette_index_;
    CRGB palette_[MAX_PALETTE_SIZE];
    const Clock &clock_;
    size_t palette_size_;
    bool direction_;
    // For Umbrella
    CRGBPalette16 primary_palette_;
    CRGBPalette16 secondary_palette_;
    uint8_t brightness_;
    uint16_t speed_;
    std::vector<CRGBPalette16 *> available_palettes_;
    AvailablePalettes current_palette_;
    // Custom palettes live here rather than in available_palettes_: that vector
    // is what the sync controller cycles through, and an empty slot in the
    // rotation would show up as a stretch of black.
    CRGBPalette16 custom_palettes_[CUSTOM_PALETTE_COUNT];
    bool custom_palette_filled_[CUSTOM_PALETTE_COUNT];
    CRGB color_;
    
    // Variables for new modern effects
    uint8_t *heat_array_;          // For fire plasma effect
    size_t heat_array_size_;       // Size of heat array
    uint8_t *meteor_positions_;    // Positions of meteors
    uint8_t *meteor_trails_;       // Trail intensities
    // LED positions are uint16_t: strips run to 450 LEDs, and a uint8_t here
    // wrapped every position past 255 back onto the front of the strip.
    uint16_t pulse_center_;        // Center position for pulse waves
    uint8_t matrix_drops_[64];     // Matrix rain drop positions (max 64 drops)
    uint16_t explosion_center_;    // Center of color explosion

    // --- Smooth motion for field effects ---
    // Effects whose pattern is a function of a phase (spiral, plasma, noise…)
    // render at a fixed smooth cadence and advance that phase by elapsed time
    // scaled by the speed knob. Tying motion to the frame period - the old
    // scheme - meant a slow speed setting didn't slow the pattern, it strobed
    // it at 5 fps.
    /// Frame gate for field effects: 0 when no frame is due yet, otherwise the
    /// elapsed ms since the last frame (clamped so stalls don't lurch).
    uint16_t field_frame_elapsed_(unsigned long now, uint16_t duration);
    static uint16_t field_frame_period_(uint16_t duration)
    {
        return duration < 16 ? 16 : (duration > 33 ? 33 : duration);
    }
    /// Advance the shared Q8.8 phase: `rate` phase units per frame of the old
    /// scheme, so motion speed matches what each speed setting used to give.
    /// Accumulated unscaled; *spatial* consumers (positions, waves) read it
    /// through strip_phase_(), which applies that strip's grouping - a
    /// grouped-pixel strip changes how far a pixel is, not how fast time
    /// passes, so temporal reads (twinkle breathing, the bpm throb) use the
    /// raw phase_().
    void advance_phase_(uint16_t elapsed, uint16_t rate, uint16_t duration)
    {
        phase_acc_ += (uint32_t)elapsed * rate * 256 / (duration ? duration : 1);
    }
    uint32_t phase_() const { return phase_acc_ >> 8; }
    /// The phase as strip `i` sees it: slowed by the strip's grouping.
    uint32_t strip_phase_(size_t i) const
    {
        return (uint32_t)(((uint64_t)phase_acc_ * strip_motion_pct_of_(i) / 100) >> 8);
    }
    uint8_t strip_motion_pct_of_(size_t i) const
    {
        return i < strip_motion_pct_.size() ? strip_motion_pct_[i] : (uint8_t)BM_MOTION_PERCENT;
    }
    /// The phase the map-driven (chevron) effects use for strip `i`. A real
    /// pixel map renders in physical panel coordinates, where the grouping
    /// correction must not apply: the map already encodes where each pixel
    /// is, and slowing a mapped glow strip would run it out of phase with
    /// the panel it is aligned to. Only the synthesized fallback, which
    /// fakes its geometry from LED indices, still paces by grouping.
    uint32_t map_phase_(size_t i) const
    {
        bool mapped = i < pixel_maps_.size() && pixel_maps_[i].pixels != nullptr;
        return mapped ? phase_() : strip_phase_(i);
    }
    /// For effects whose motion state is shared across strips (fireworks'
    /// rocket, the explosion front, pacifica's clock): the slowest strip's
    /// percent, so grouped strips never tear even if dense ones run slow.
    uint8_t device_motion_pct_() const;
    std::vector<uint8_t> strip_motion_pct_; // parallel to led_controllers_
    uint32_t phase_acc_ = 0;

    // State for the ripple / cylon / fireworks effects (noise_flow and twinkle
    // run entirely off the shared phase)
    // Per strip: a grouped and a dense strip sweep at different physical
    // paces (and different lengths get their own turnarounds for free).
    std::vector<uint32_t> cylon_pos_; // Q8.8 for sub-pixel motion
    std::vector<int8_t> cylon_dir_;
    static constexpr uint8_t max_ripples = 6;
    uint16_t ripple_center_[max_ripples] = {0};
    uint16_t ripple_age_[max_ripples] = {0}; // 0 = free slot
    uint8_t ripple_hue_[max_ripples] = {0};
    // --- Panel geometry for the chevron effects ---
    struct StripMap
    {
        const PanelPixel *pixels = nullptr;
        const uint16_t *order = nullptr; // logical -> physical, see setRenderOrder
        uint8_t segments = 0;
        const uint16_t *grid = nullptr;  // row-major display grid, see setMatrixGrid
        uint8_t grid_w = 0;
        uint8_t grid_h = 0;
    };
    std::vector<StripMap> pixel_maps_; // parallel to led_controllers_

    // --- Marquee text and bitmap state for the matrix display ---
    char marquee_text_[MARQUEE_TEXT_MAX + 1] = "";
    uint8_t text_style_ = 0; // 0 normal, 1 bold
    uint8_t text_fill_ = 0;  // 0 gradient, 1 fire, 2 rain, 3 plasma

    // --- The matrix display overlay ---
    // Its own Q8.8 phase clock, advanced by wall time against matrix_speed_ms_
    // (never the effect duration), so display pace and effect pace are
    // independent knobs. The overlay paints inside show_now_, which every
    // effect's show funnels through - the effect renders the grid strip as
    // usual and the overlay replaces the pixels on the way out.
    uint8_t matrix_display_mode_ = MATRIX_DISPLAY_OFF;
    uint16_t matrix_speed_ms_ = MATRIX_SPEED_DEFAULT_MS;
    AvailablePalettes matrix_palette_ = AvailablePalettes::cool;
    bool matrix_dir_ = true;
    uint32_t matrix_acc_ = 0;              // Q8.8 overlay phase accumulator
    unsigned long matrix_last_tick_ = 0;   // clock advance
    unsigned long matrix_last_show_ = 0;   // last frame that reached the panel
    uint32_t matrix_phase_() const { return matrix_acc_ >> 8; }
    /// Content exists for the active mode (text stored / bitmap set / frames).
    bool matrix_overlay_content_() const;
    /// Mode set, a grid exists, content exists, and the show isn't off.
    bool matrix_overlay_active_() const;
    /// Advance the overlay clock; push frames to the grid strip(s) when the
    /// running effect isn't already repainting them fast enough.
    void matrix_overlay_tick_(unsigned long now);
    /// Draw the current display frame into a grid strip's logical buffer.
    void matrix_overlay_paint_(const StripMap &m, CRGB *leds, size_t n);
    void paint_marquee_(const StripMap &m, CRGB *leds, size_t n);
    void paint_word_zoom_(const StripMap &m, CRGB *leds, size_t n);
    void paint_grid_bitmap_(const StripMap &m, CRGB *leds, size_t n,
                            uint8_t w, uint8_t h, const CRGB *palette,
                            const uint8_t *pixels);
    /// Column bits of the glyph column at text-space column `tc` (advance
    /// MATRIX_FONT_ADVANCE per character), honouring the bold style.
    uint8_t text_col_bits_(const char *text, size_t len, int32_t tc) const;
    /// Colour for a lit glyph pixel at display cell (r, c) under the active
    /// text fill. `base_index` is the sliding-gradient palette index the
    /// caller would have used for fill 0.
    CRGB text_fill_color_(const CRGBPalette16 &pal, uint8_t r, uint8_t c,
                          uint8_t grid_h, uint32_t ph, uint8_t base_index) const;
    CRGB bitmap_palette_[MATRIX_BITMAP_COLORS];
    uint8_t bitmap_pixels_[MATRIX_BITMAP_MAX_PIXELS / 2] = {0}; // 4bpp, high nibble first
    uint8_t bitmap_w_ = 0;
    uint8_t bitmap_h_ = 0;
    bool bitmap_set_ = false;

    // --- Animation frames for panel_anim (bitmap format, one per slot) ---
    struct AnimFrame
    {
        CRGB palette[MATRIX_BITMAP_COLORS];
        uint8_t pixels[MATRIX_BITMAP_MAX_PIXELS / 2];
        uint8_t w = 0;
        uint8_t h = 0;
        bool set = false;
    };
    AnimFrame anim_frames_[MATRIX_ANIM_MAX_FRAMES];
    /// The strip's map entry (for the LED at *logical* position i when a render
    /// order is set), or a synthesized straight-line fallback so the chevron
    /// effects render something sensible on unmapped rigs.
    PanelPixel panel_pixel_(size_t strip_index, size_t i, size_t num_leds, uint8_t &segment_count) const;
    /// The controller's position in led_controllers_, or its size when unknown.
    size_t strip_index_of_(CLEDController *controller) const;
    /// The caller's brightness scaled by the strip's ceiling (see
    /// setStripMaxBrightness). Every showLeds goes through this.
    uint8_t strip_brightness_(size_t strip_index, uint8_t brightness) const;
    std::vector<uint8_t> strip_max_brightness_; // parallel to led_controllers_, 255 = uncapped
    CRGB *order_scratch_ = nullptr; // show-time permute buffer, largest ordered strip
    size_t order_scratch_size_ = 0;
    static constexpr uint8_t fallback_segments_ = 4;

    // Per-segment bar state for chevron_eq (levels chase noise-driven targets)
    static constexpr uint8_t max_panel_segments_ = 8;
    uint8_t eq_level_[max_panel_segments_] = {0};

    // Pacifica's four scrolling colour-index layers and its own speed-scaled
    // clock (the classic runs off millis(); ours runs off the frame delta so
    // the speed knob and BM_MOTION_PERCENT apply).
    uint16_t pacifica_ci_[4] = {0};
    uint32_t pacifica_ms_ = 0;

    static constexpr uint8_t max_sparks = 12;
    uint8_t fw_stage_ = 0;           // 0 launch, 1 burst, 2 rest
    uint32_t fw_pos_ = 0;            // rocket position, Q8.8
    uint16_t fw_apex_ = 0;
    unsigned long fw_next_launch_ = 0;
    int32_t spark_pos_[max_sparks] = {0};  // Q8.8
    int16_t spark_vel_[max_sparks] = {0};  // Q8.8 per ms
    uint8_t spark_heat_[max_sparks] = {0};
};

#endif // LIGHTSHOW_H
