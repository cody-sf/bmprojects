// #include <Arduino.h>
#include <time.h>
#include <GlobalDefaults.h>
#include <DeviceRoles.h>
#include "ControlCenter.h"
#include "StringUtils.h"
#include <cstring>
#include "LightShow.h"
#include <algorithm>
#include "Helpers.h"

constexpr ControlCenter::MapEntry<ControlCenter::Command> ControlCenter::command_map_[];
constexpr ControlCenter::MapEntry<ControlCenter::LightShowMode> ControlCenter::light_show_mode_map_[];
char *paletteEnumToString(AvailablePalettes palette)
{
    switch (palette)
    {
    case AvailablePalettes::cool:
        return "cool";
    case AvailablePalettes::earth:
        return "earth";
    case AvailablePalettes::everglow:
        return "everglow";
    case AvailablePalettes::heart:
        return "heart";
    case AvailablePalettes::lava:
        return "lava";
    case AvailablePalettes::melonball:
        return "melonball";
    case AvailablePalettes::pinksplash:
        return "pinksplash";
    case AvailablePalettes::r:
        return "r";
    case AvailablePalettes::sofia:
        return "sofia";
    case AvailablePalettes::sunset:
        return "sunset";
    case AvailablePalettes::trove:
        return "trove";
    case AvailablePalettes::velvet:
        return "velvet";
    case AvailablePalettes::vga:
        return "vga";
    default:
        return "vga";
    }
}
AvailablePalettes stringToPaletteEnum(const char *str)
{
    Serial.println("Running to enum");
    if (strcmp(str, "cool") == 0)
    {
        return AvailablePalettes::cool;
    }
    else if (strcmp(str, "earth") == 0)
    {
        return AvailablePalettes::earth;
    }
    else if (strcmp(str, "everglow") == 0)
    {
        return AvailablePalettes::everglow;
    }
    else if (strcmp(str, "heart") == 0)
    {
        return AvailablePalettes::heart;
    }
    else if (strcmp(str, "lava") == 0)
    {
        return AvailablePalettes::lava;
    }
    else if (strcmp(str, "melonball") == 0)
    {
        return AvailablePalettes::melonball;
    }
    else if (strcmp(str, "pinksplash") == 0)
    {
        return AvailablePalettes::pinksplash;
    }
    else if (strcmp(str, "r") == 0)
    {
        return AvailablePalettes::r;
    }
    else if (strcmp(str, "sofia") == 0)
    {
        return AvailablePalettes::sofia;
    }
    else if (strcmp(str, "sunset") == 0)
    {
        return AvailablePalettes::sunset;
    }
    else if (strcmp(str, "trove") == 0)
    {
        return AvailablePalettes::trove;
    }
    else if (strcmp(str, "velvet") == 0)
    {
        return AvailablePalettes::velvet;
    }
    else if (strcmp(str, "vga") == 0)
    {
        return AvailablePalettes::vga;
    }
    else
    {
        // Handle default or unknown case. Depending on your use case, you might want to throw an error, return a default palette, or handle this case differently.
        return AvailablePalettes::vga; // Default to VGA for this example. Adjust as needed.
    }
}

void ControlCenter::update_lightshow(LightShow *light_show)
{
    this->light_show_ = light_show;
}

static unsigned int parse_int(const char *str)
{
    int value = atoi(str);
    if (value < 0)
    {
        return 0;
    }

    return value;
}

static uint8_t parse_unsigned_byte(const char *str)
{
    int value = atoi(str);
    if (value < 0 || value > 255)
    {
        return 0;
    }

    return value;
}

template <typename DestinationType>
static void print_int_zero_padded(DestinationType &destination, int num)
{
    if (num < 10)
    {
        destination.print(0);
    }
    destination.print(num);
}

template <typename Map, typename EnumType>
static bool map_name_to_enum_value_(Map map, size_t num_entries, const char *search_name, EnumType &result)
{
    for (size_t i = 0; i < num_entries; i++)
    {
        if (StringUtils::strings_match(map[i].name, search_name))
        {
            result = map[i].enum_value;
            return true;
        }
    }

    return false;
}

template <typename Map, typename EnumType>
static bool map_enum_value_to_name_(Map map, size_t num_entries, EnumType search_enum_value, const char *&result)
{
    for (size_t i = 0; i < num_entries; i++)
    {
        if (map[i].enum_value == search_enum_value)
        {
            result = map[i].name;
            return true;
        }
    }

    return false;
}

ControlCenter::ControlCenter(LocationService &location_service, LightShow &light_show, Device &device) : light_show_mode_(LightShowMode::position_status),
                                                                                                         origin_(Position(DEFAULT_ORIGIN_LATITUDE, DEFAULT_ORIGIN_LONGITUDE)),
                                                                                                         color_(CRGB::Purple),
                                                                                                         current_palette_(cool),
                                                                                                         location_service_(location_service),
                                                                                                         light_show_(&light_show),
                                                                                                         brightness_(DEFAULT_BRIGHTNESS),
                                                                                                         selected_devices_(DEVICE_ROLE_ALL),
                                                                                                         radius_inner_(DEFAULT_PLAYA_INNER_RADIUS),
                                                                                                         radius_outer_(DEFAULT_PLAYA_OUTER_RADIUS),
                                                                                                         device_(device),
                                                                                                         primary_palette_(cool),
                                                                                                         secondary_palette_(earth),
                                                                                                         speed_(100),
                                                                                                         direction_(true)
{
}

bool ControlCenter::execute_command(char *request_params[], size_t num_params)
{
    if (num_params <= 0)
    {
        return false;
    }

    Serial.print("Received command:");
    for (size_t i = 0; i < num_params; i++)
    {
        Serial.print(" ");
        Serial.print(request_params[i]);
    }
    Serial.println();

    const char *requested_command_str = request_params[0];
    Command requested_command;
    bool command_found = map_name_to_enum_value_(command_map_, sizeof(command_map_) / sizeof(command_map_[0]), requested_command_str, requested_command);

    if (!command_found)
    {
        return false;
    }

    switch (requested_command)
    {
    case Command::light_show_mode:
        return change_light_show_mode_(request_params + 1, num_params - 1);
    case Command::origin:
        return change_origin_(request_params + 1, num_params - 1);
    case Command::brightness:
        return change_brightness_(request_params + 1, num_params - 1);
    case Command::color:
        return change_color_(request_params + 1, num_params - 1);
    case Command::palette:
        return change_palette_(request_params + 1, num_params - 1);
    case Command::primary_palette:
        return change_primary_palette_(request_params + 1, num_params - 1);
    case Command::secondary_palette:
        return change_secondary_palette_(request_params + 1, num_params - 1);
    case Command::radius:
        return change_radius_(request_params + 1, num_params - 1);
    case Command::devices:
        return change_devices_(request_params + 1, num_params - 1);
    }

    return false;
}

bool ControlCenter::button_press_cycle_lightshow()
{
    // Handle the palette modes separately
    if (light_show_mode_ == LightShowMode::palette_stream)
    {
        // Attempt to change the palette
        current_palette_ = static_cast<AvailablePalettes>((static_cast<int>(current_palette_) + 1));

        // If we're still within the palette range
        if (current_palette_ <= AvailablePalettes::vga)
        {
            char *params[] = {paletteEnumToString(current_palette_)};
            Serial.println(params[0]);
            return change_palette_(params, 1);
        }
        // If we've cycled through all palettes, reset and increment the mode
        else
        {
            current_palette_ = AvailablePalettes::cool; // Reset the palette
        }
    }

    // Handle mode cycling
    do
    {
        light_show_mode_ = static_cast<LightShowMode>((static_cast<int>(light_show_mode_) + 1) % (static_cast<int>(LightShowMode::breathe) + 1));

    } while (!isLightShowModeAvailable(light_show_mode_));

    char *params[] = {const_cast<char *>(light_show_mode_map_[static_cast<int>(light_show_mode_)].name)};
    return change_light_show_mode_(params, 1);
}

bool ControlCenter::isLightShowModeAvailable(LightShowMode mode)
{
    switch (mode)
    {
    default:
        return true;
    }
}

void ControlCenter::change_light_show_mode(const char *requested_mode_str)
{
    LightShowMode requested_mode;
    bool mode_found = map_name_to_enum_value_(light_show_mode_map_,
                                              sizeof(light_show_mode_map_) / sizeof(light_show_mode_map_[0]), requested_mode_str, requested_mode);

    if (mode_found)
    {
        Serial.println('Changing light show mode to: ');
        Serial.println(requested_mode_str);
        light_show_mode_ = requested_mode;
    }
    else
    {
        Serial.println("Mode not found");
    }
}

void ControlCenter::change_palette(const char *requested_palette_str)
{
    primary_palette_ = stringToPalette(requested_palette_str);
    current_palette_ = stringToPalette(requested_palette_str);
    return;
}

void ControlCenter::change_brightness(uint8_t brightness)
{
    brightness_ = brightness;
    return;
}

void ControlCenter::change_speed(uint8_t speed)
{
    speed_ = speed;
    return;
}

void ControlCenter::change_direction(bool direction)
{
    direction_ = direction;
    return;
}

// Location
bool ControlCenter::change_origin(const char *requested_origin)
{

    if (StringUtils::strings_match(requested_origin, "default"))
    {
        origin_ = Position(DEFAULT_ORIGIN_LATITUDE, DEFAULT_ORIGIN_LONGITUDE);
        return true;
    }
    else if (StringUtils::strings_match(requested_origin, "initial"))
    {
        if (location_service_.is_initial_position_available())
        {
            origin_ = location_service_.initial_position();
        }
        return true;
    }
    else if (StringUtils::strings_match(requested_origin, "recenter"))
    {
        if (location_service_.is_current_position_available())
        {
            origin_ = location_service_.current_position();
        }
        return true;
    }

    return false;
}

// Deprecate These
bool ControlCenter::change_light_show_mode_(char *request_params[], size_t num_params)
{
    if (num_params <= 0)
    {
        return false;
    }

    const char *requested_mode_str = request_params[0];
    LightShowMode requested_mode;

    bool mode_found = map_name_to_enum_value_(light_show_mode_map_,
                                              sizeof(light_show_mode_map_) / sizeof(light_show_mode_map_[0]), requested_mode_str, requested_mode);

    if (!mode_found)
    {
        Serial.println("mode not found");
        Serial.println(requested_mode_str);
        return false;
    }

    light_show_mode_ = requested_mode;

    Serial.print("Requested Mode: ");
    Serial.println(requested_mode_str);

    return true;
}

bool ControlCenter::change_origin_(char *request_params[], size_t num_params)
{
    if (num_params < 1)
    {
        return false;
    }

    const char *requested_origin = request_params[0];

    if (StringUtils::strings_match(requested_origin, "default"))
    {
        origin_ = Position(DEFAULT_ORIGIN_LATITUDE, DEFAULT_ORIGIN_LONGITUDE);
        return true;
    }
    else if (StringUtils::strings_match(requested_origin, "initial"))
    {
        if (location_service_.is_initial_position_available())
        {
            origin_ = location_service_.initial_position();
        }
        return true;
    }
    else if (StringUtils::strings_match(requested_origin, "recenter"))
    {
        if (location_service_.is_current_position_available())
        {
            origin_ = location_service_.current_position();
        }
        return true;
    }

    return false;
}

bool ControlCenter::change_brightness_(char *request_params[], size_t num_params)
{
    if (num_params < 1)
    {
        return false;
    }

    brightness_ = parse_unsigned_byte(request_params[0]);
    return true;
}

bool ControlCenter::change_color_(char *request_params[], size_t num_params)
{
    if (num_params < 1)
    {
        return false;
    }

    const char *color_type = request_params[0];

    if (StringUtils::strings_match(color_type, "rgb"))
    {
        if (num_params < 4)
        {
            return false;
        }

        uint8_t red = parse_unsigned_byte(request_params[1]);
        uint8_t green = parse_unsigned_byte(request_params[2]);
        uint8_t blue = parse_unsigned_byte(request_params[3]);
        color_ = CRGB(red, green, blue);
        return true;
    }
    else if (StringUtils::strings_match(color_type, "hue"))
    {
        if (num_params < 2)
        {
            return false;
        }

        int hue = parse_unsigned_byte(request_params[1]);
        color_ = CHSV(hue, 255, 255);
        return true;
    }

    return false;
}

bool ControlCenter::change_palette_(char *request_params[], size_t num_params)
{
    Serial.println("Change Palette_");
    if (num_params < 1)
    {
        Serial.println("returning false");
        return false;
    }
    const char *paletteName = request_params[0];
    current_palette_ = stringToPaletteEnum(paletteName);
    return true;
}

bool ControlCenter::change_primary_palette_(char *request_params[], size_t num_params)
{
    Serial.println("Change Primary Palette_");
    if (num_params < 1)
    {
        Serial.println("returning false");
        return false;
    }
    const char *paletteName = request_params[0];
    primary_palette_ = stringToPaletteEnum(paletteName);
    return true;
}

bool ControlCenter::change_secondary_palette_(char *request_params[], size_t num_params)
{
    Serial.println("Change Secondary Palette_");
    if (num_params < 1)
    {
        Serial.println("returning false");
        return false;
    }
    const char *paletteName = request_params[0];
    secondary_palette_ = stringToPaletteEnum(paletteName);
    return true;
}

bool ControlCenter::change_radius_(char *request_params[], size_t num_params)
{
    if (num_params < 2)
    {
        return false;
    }

    const char *radius_type = request_params[0];
    const char *radius_str = request_params[1];
    bool set_default = false;

    if (StringUtils::strings_match(radius_str, "default"))
    {
        set_default = true;
    }

    if (StringUtils::strings_match(radius_type, "inner"))
    {
        if (set_default)
        {
            radius_inner_ = DEFAULT_PLAYA_INNER_RADIUS;
        }
        else
        {
            radius_inner_ = parse_int(radius_str);
        }

        return true;
    }
    else if (StringUtils::strings_match(radius_type, "outer"))
    {
        if (set_default)
        {
            radius_outer_ = DEFAULT_PLAYA_OUTER_RADIUS;
        }
        else
        {
            radius_outer_ = parse_int(radius_str);
        }
        return true;
    }

    return false;
}

bool ControlCenter::change_devices_(char *request_params[], size_t num_params)
{
    if (num_params < 1)
    {
        return false;
    }

    selected_devices_ = parse_unsigned_byte(request_params[0]);
    return true;
}

void ControlCenter::update_state()
{
    light_show_->brightness(brightness_);

    switch (light_show_mode_)
    {
    case LightShowMode::color_wheel:
        if (location_service_.is_current_position_available())
        {
            Position current_position = location_service_.current_position();
            Position offset = current_position - origin_;
            uint8_t hue = 255 * offset.polar_angle() / (2 * PI);
            CRGB new_color = CHSV(hue, 255, 255);
            float distance = current_position.distance_from(origin_);
            unsigned int half_radius_inner = radius_inner_ >> 1;

            if (distance < half_radius_inner)
            {
                new_color = ORIGIN_COLOR;
            }
            else if (distance >= half_radius_inner && distance <= radius_inner_)
            {
                uint8_t scale_factor = 255 * (distance - half_radius_inner) / half_radius_inner;
                new_color = CRGB(ORIGIN_COLOR).lerp8(new_color, scale_factor);
            }

            light_show_->solid(new_color);
        }
        else
        {
            light_show_->spectrum_cycle(30);
        }
        break;

    case LightShowMode::color_radial:
        if (location_service_.is_current_position_available())
        {
            Position current_position = location_service_.current_position();
            float distance = current_position.distance_from(origin_);
            unsigned int half_radius_inner = radius_inner_ >> 1;
            CRGB new_color = ORIGIN_COLOR;

            if (distance >= half_radius_inner && distance <= radius_inner_)
            {
                CRGB first_color = CHSV(0, 255, 255);
                uint8_t scale_factor = 255 * (distance - half_radius_inner) / half_radius_inner;
                new_color = CRGB(ORIGIN_COLOR).lerp8(first_color, scale_factor);
            }
            else if (distance > radius_inner_)
            {
                unsigned int range = radius_outer_ - radius_inner_;
                uint8_t hue = LINEAR_SPECTRUM_MAX_HUE * std::min(static_cast<float>(distance - radius_inner_), static_cast<float>(range)) / static_cast<float>(range);
                new_color = CHSV(hue, 255, 255);
            }

            light_show_->solid(new_color);
        }
        else
        {
            light_show_->spectrum_stream(30);
        }
        break;

    case LightShowMode::color_speedometer:
        if (location_service_.is_current_position_available())
        {
            float speed = location_service_.recent_average_speed();
            uint8_t hue = LINEAR_SPECTRUM_MAX_HUE * std::min(speed, static_cast<float>(MAX_SPEED)) / MAX_SPEED;
            CRGB new_color = CHSV(hue, 255, 255);
            light_show_->solid(new_color);
        }
        else
        {
            light_show_->spectrum_cycle(30);
        }
        break;

    case LightShowMode::color_clock:
        if (location_service_.is_time_available())
        {
            struct tm timeinfo;
            if (getLocalTime(&timeinfo))
            {
                uint16_t minutes = ((timeinfo.tm_hour % COLOR_CLOCK_PERIOD) * 60) + timeinfo.tm_min;
                uint8_t hue = 255 * minutes / (COLOR_CLOCK_PERIOD * 60);
                CRGB new_color = CHSV(hue, 255, 255);
                light_show_->solid(new_color);
            }
            else
            {
                Serial.println("Failed to obtain time");
                light_show_->spectrum_cycle(30);
            }
        }
        else
        {
            light_show_->spectrum_cycle(30);
        }
        break;

    case LightShowMode::solid_color:
        light_show_->solid(color_);
        break;
    case LightShowMode::palette_cycle:
        light_show_->palette_cycle(current_palette_, speed_);
        break;
    case LightShowMode::palette_stream:
        light_show_->palette_stream(speed_, current_palette_, direction_);
        break;
    case LightShowMode::spectrum_cycle:
        light_show_->spectrum_cycle(100);
        break;

    case LightShowMode::spectrum_stream:
        light_show_->spectrum_stream(30);
        break;

    case LightShowMode::spectrum_sparkle:
        light_show_->spectrum_sparkle(75, 50);
        break;

    case LightShowMode::strobe:
        light_show_->strobe(2, 150, 150, 1200, color_);
        break;

    case LightShowMode::sparkle:
        light_show_->sparkle(75, 50, color_);
        break;

    case LightShowMode::position_status:
        light_show_->solid(location_service_.is_current_position_available() ? CRGB::Green : CRGB::Red);
        break;

    case LightShowMode::breathe:
        light_show_->breathe(20, 230, color_);
        break;

    default:
        break;
    }
}

// Deprecate These
// Generate one string and call client.println() once at the end. Calling it a lot seems to be really slow.
void ControlCenter::write_status(WiFiClient &client)
{
    size_t buffer_size = 1024;
    size_t bytes_written = 0;
    char buffer[buffer_size];
    char latitude_str[32];
    char longitude_str[32];
    char float_str[32];

    bytes_written += snprintf(buffer + bytes_written, buffer_size - bytes_written, "{");

    const char *light_show_mode_str = "unknown";
    bool mode_found = map_enum_value_to_name_(light_show_mode_map_,
                                              sizeof(light_show_mode_map_) / sizeof(light_show_mode_map_[0]), light_show_mode_, light_show_mode_str);

    if (!mode_found)
    {
        bytes_written += snprintf(buffer + bytes_written, buffer_size - bytes_written, "%s", light_show_mode_str);
    }

    bytes_written += snprintf(buffer + bytes_written, buffer_size - bytes_written, "\"light_show_mode\":\"%s\"", light_show_mode_str);
    bytes_written += snprintf(buffer + bytes_written, buffer_size - bytes_written, ",\"color\":{\"r\":%u,\"g\":%u,\"b\":%u}",
                              color_.red, color_.green, color_.blue);
    bytes_written += snprintf(buffer + bytes_written, buffer_size - bytes_written, ",\"brightness\":\"%u\"", brightness_);
    bytes_written += snprintf(buffer + bytes_written, buffer_size - bytes_written, ",\"center_radius\":\"%u\"", radius_inner_);
    bytes_written += snprintf(buffer + bytes_written, buffer_size - bytes_written, ",\"perimeter_radius\":\"%u\"", radius_outer_);
    bytes_written += snprintf(buffer + bytes_written, buffer_size - bytes_written, ",\"origin\":{\"latitude\":%s,\"longitude\":%s}",
                              dtostrf(origin_.latitude(), 2, 6, latitude_str), dtostrf(origin_.longitude(), 2, 6, longitude_str));

    if (location_service_.is_current_position_available())
    {
        Position current = location_service_.current_position();
        bytes_written += snprintf(buffer + bytes_written, buffer_size - bytes_written, ",\"position\":{\"latitude\":%s,\"longitude\":%s}",
                                  dtostrf(current.latitude(), 2, 6, latitude_str), dtostrf(current.longitude(), 2, 6, longitude_str));
        bytes_written += snprintf(buffer + bytes_written, buffer_size - bytes_written, ",\"speed\":\"%s\"",
                                  dtostrf(location_service_.recent_average_speed(), 2, 2, float_str));
        bytes_written += snprintf(buffer + bytes_written, buffer_size - bytes_written, ",\"distance_to_origin\":\"%s\"",
                                  dtostrf(current.distance_from(origin_), 2, 2, float_str));
    }

    if (location_service_.is_time_available())
    {
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            bytes_written += snprintf(buffer + bytes_written, buffer_size - bytes_written, ",\"date_time\":\"%04u-%02u-%02u / %02u:%02u:%02u\"",
                                      (timeinfo.tm_year + 1900), (timeinfo.tm_mon + 1), timeinfo.tm_mday,
                                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        }
        else
        {
            Serial.println("Failed to obtain time");
        }
    }

    bytes_written += snprintf(buffer + bytes_written, buffer_size - bytes_written, ",\"synced_devices\":\"%u\"", selected_devices_);

    bytes_written += snprintf(buffer + bytes_written, buffer_size - bytes_written, "}");
    client.println(buffer);
}