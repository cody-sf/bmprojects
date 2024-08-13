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
    setCHSV
};

enum AvailablePalettes : uint8_t
{
    cool,
    earth,
    everglow,
    fatboy,
    fireice,
    flame,
    heart,
    lava,
    melonball,
    pinksplash,
    r,
    sofia,
    sunset,
    trove,
    velvet,
    vga
};

struct LightScene
{
    LightSceneID scene_id;
    uint8_t brightness;
    uint8_t selected_devices;
    uint32_t reference_time;
    uint16_t speed;
    AvailablePalettes primary_palette;
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

    } scenes;
};

class LightShow
{
public:
    LightShow(const std::vector<CLEDController *> &led_controllers = std::vector<CLEDController *>(), const Clock &clock = Clock());
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
};

#endif // LIGHTSHOW_H
