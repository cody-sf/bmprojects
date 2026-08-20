#ifndef LIGHTSHOW_H
#define LIGHTSHOW_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <FastLED.h>
#include <Clock.h>

#define MAX_PALETTE_SIZE 8

// Rigs whose strips group several LEDs per addressable pixel (12 V WS2811
// boards run 3-6 LEDs per IC) cover several times the physical distance per
// pixel, so motion tuned on a dense wearable strip tears across them. Build
// those targets with e.g. -DBM_MOTION_PERCENT=25 to slow every field effect's
// motion to a quarter; frame rate and the app's speed slider are unaffected.
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
    fireworks
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
    /// `spatial` phases (positions, waves) honour BM_MOTION_PERCENT; temporal
    /// ones (twinkle breathing) don't - a grouped-pixel strip changes how far
    /// a pixel is, not how fast time passes.
    void advance_phase_(uint16_t elapsed, uint16_t rate, uint16_t duration, bool spatial = true)
    {
        uint32_t step = (uint32_t)elapsed * rate * 256 / (duration ? duration : 1);
        if (spatial)
        {
            step = step * BM_MOTION_PERCENT / 100;
        }
        phase_acc_ += step;
    }
    uint32_t phase_() const { return phase_acc_ >> 8; }
    uint32_t phase_acc_ = 0;

    // State for the ripple / cylon / fireworks effects (noise_flow and twinkle
    // run entirely off the shared phase)
    uint32_t cylon_pos_ = 0;       // Q8.8 for sub-pixel motion
    int8_t cylon_dir_ = 1;
    static constexpr uint8_t max_ripples = 6;
    uint16_t ripple_center_[max_ripples] = {0};
    uint16_t ripple_age_[max_ripples] = {0}; // 0 = free slot
    uint8_t ripple_hue_[max_ripples] = {0};
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
