#ifndef LIGHTSHOW_H
#define LIGHTSHOW_H

#include <cstdint>
#include <cstring>
#include <vector>
#include <FastLED.h>
#include <Clock.h>

#define MAX_PALETTE_SIZE 8

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
    spiral_galaxy
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
    moltenmetal
};

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
        struct
        {
            CRGB color;
        } solid;
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
            CRGB color;
        } strobe;
        struct
        {
            uint16_t duration;
            uint8_t density;
            CRGB color;
        } sparkle;
        struct
        {
            uint16_t duration;
            uint8_t dimness;
            CRGB color;
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
            CRGB color;
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
    CRGB color_;
    
    // Variables for new modern effects
    uint8_t *heat_array_;          // For fire plasma effect
    size_t heat_array_size_;       // Size of heat array
    uint8_t *meteor_positions_;    // Positions of meteors
    uint8_t *meteor_trails_;       // Trail intensities
    uint8_t pulse_center_;         // Center position for pulse waves
    uint8_t matrix_drops_[64];     // Matrix rain drop positions (max 64 drops)
    uint8_t plasma_offset_;        // Offset for plasma clouds
    float noise_x_, noise_y_;      // For smooth noise effects
    uint8_t explosion_center_;     // Center of color explosion
    uint8_t spiral_angle_;         // Current spiral rotation
};

#endif // LIGHTSHOW_H
