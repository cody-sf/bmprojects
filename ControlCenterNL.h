#ifndef CONTROLCENTER_H
#define CONTROLCENTER_H
#include <WiFi.h>
#include <FastLED.h>
#include <LightShow.h>
#include "ControlCenterHTML.h"
#include "LocationService.h"
#include "DeviceRoles.h"

#define DEFAULT_ORIGIN_LATITUDE 40.786331
#define DEFAULT_ORIGIN_LONGITUDE -119.206489
#define DEFAULT_PLAYA_INNER_RADIUS 220
#define DEFAULT_PLAYA_OUTER_RADIUS 2000
#define ORIGIN_COLOR CRGB::White
#define MAX_SPEED 16.0
#define LINEAR_SPECTRUM_MAX_HUE 191
#define COLOR_CLOCK_PERIOD 6
class ControlCenter
{
public:
    ControlCenter(LocationService *location_service, LightShow &light_show, Device &device);
    bool execute_command(char *request_params[], size_t num_params);
    void write_status(WiFiClient &client);
    void update_state();
    uint8_t selected_devices() { return selected_devices_; }
    void update_lightshow(LightShow *light_show);
    bool button_press_cycle_lightshow();
    void change_light_show_mode(const char *requested_mode_str);
    void change_palette(const char *requested_palette_str);
    void change_brightness(uint8_t brightness);
    void change_speed(uint8_t speed);
    void change_direction(bool direction);
    bool change_origin(const char *requested_origin);
    enum class LightShowMode
    {
        palette_cycle,
        color_wheel,
        color_radial,
        palette_stream,
        color_speedometer,
        color_clock,
        solid_color,
        spectrum_cycle,
        spectrum_stream,
        spectrum_sparkle,
        strobe,
        sparkle,
        position_status,
        breathe
    };
    bool isLightShowModeAvailable(LightShowMode mode);
    // I dont know this isn't working with the backpack
    // AvailablePalettes stringToPaletteEnum(const char *str);

private:
    enum class Command
    {
        light_show_mode,
        origin,
        brightness,
        color,
        palette,
        radius,
        devices,
        button_press,
        primary_palette,
        secondary_palette
    };
    template <typename T>
    struct MapEntry
    {
        const char *name;
        T enum_value;
    };

    bool change_light_show_mode_(char *request_params[], size_t num_params);
    bool change_origin_(char *request_params[], size_t num_params);
    bool change_brightness_(char *request_params[], size_t num_params);
    bool change_color_(char *request_params[], size_t num_params);
    bool change_palette_(char *request_params[], size_t num_params);
    bool change_radius_(char *request_params[], size_t num_params);
    bool change_devices_(char *request_params[], size_t num_params);
    bool change_primary_palette_(char *request_params[], size_t num_params);
    bool change_secondary_palette_(char *request_params[], size_t num_params);

    static constexpr MapEntry<Command> command_map_[] = {
        {"mode", Command::light_show_mode},
        {"origin", Command::origin},
        {"brightness", Command::brightness},
        {"color", Command::color},
        {"palette", Command::palette},
        {"radius", Command::radius},
        {"devices", Command::devices},
        {"primary_palette", Command::primary_palette},
        {"secondary_palette", Command::secondary_palette}};

    static constexpr MapEntry<LightShowMode> light_show_mode_map_[] = {
        {"palette-stream", LightShowMode::palette_stream},
        {"color-wheel", LightShowMode::color_wheel},
        {"color-radial", LightShowMode::color_radial},
        {"color-speedometer", LightShowMode::color_speedometer},
        {"palette-cycle", LightShowMode::palette_cycle},
        {"color-clock", LightShowMode::color_clock},
        {"solid-color", LightShowMode::solid_color},
        {"spectrum-cycle", LightShowMode::spectrum_cycle},
        {"spectrum-stream", LightShowMode::spectrum_stream},
        {"spectrum-sparkle", LightShowMode::spectrum_sparkle},
        {"strobe", LightShowMode::strobe},
        {"sparkle", LightShowMode::sparkle},
        {"position-status", LightShowMode::position_status},
        {"breathe", LightShowMode::breathe}};

    LightShowMode light_show_mode_;
    Position origin_;
    CRGB color_;
    AvailablePalettes current_palette_;
    LocationService &location_service_;
    LightShow *light_show_;
    uint8_t brightness_;
    uint8_t selected_devices_;
    uint8_t speed_;
    bool direction_;
    unsigned int radius_inner_;
    unsigned int radius_outer_;
    Device &device_;
    AvailablePalettes primary_palette_;
    AvailablePalettes secondary_palette_;
};

#endif // CONTROLCENTER_H
