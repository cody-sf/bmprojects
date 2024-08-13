#include "LightShow.h"
#include <FastLED.h>
#include "Palettes.h"

LightShow::LightShow(const std::vector<CLEDController *> &led_controllers, const Clock &clock)
    : led_controllers_(led_controllers), clock_(clock), scene_changed_(false), hue_(0), frame_number_(0), scale_(0), palette_index_(0), palette_size_(0),
      current_palette_(AvailablePalettes::cool), primary_palette_(getPalette(AvailablePalettes::cool)), secondary_palette_(getPalette(AvailablePalettes::earth))
{
    // Initialize the active scene to default settings
    memset(&active_scene_, 0, sizeof(active_scene_));
    active_scene_.scene_id = LightSceneID::off;

    // Initialize the available palettes
    available_palettes_ = {
        &coolPalette,
        &earthPalette,
        &everglowPalette,
        &fatboyPalette,
        &fireicePalette,
        &flamePalette,
        &heartPalette,
        &lavaPalette,
        &melonballPalette,
        &pinksplashPalette,
        &rPalette,
        &sofiaPalette,
        &sunsetPalette,
        &trovePalette,
        &velvetPalette,
        &vgaPalette,
        // Add other palettes as needed
    };
}
LightScene LightShow::getCurrentScene() const
{
    return active_scene_;
}
size_t LightShow::getPaletteCount() const
{
    return available_palettes_.size();
}

CRGBPalette16 LightShow::getPalette(AvailablePalettes palette)
{
    switch (palette)
    {
    case cool:
        return cool_palette;
    case earth:
        return earth_palette;
    case everglow:
        return everglow_palette;
    case fatboy:
        return fatboy_palette;
    case fireice:
        return fireice_palette;
    case flame:
        return flame_palette;
    case heart:
        return heart_palette;
    case lava:
        return lava_palette;
    case melonball:
        return melonball_palette;
    case pinksplash:
        return pinksplash_palette;
    case r:
        return r_palette;
    case sofia:
        return sofia_palette;
    case sunset:
        return sunset_palette;
    case trove:
        return trove_palette;
    case velvet:
        return velvet_palette;
    case vga:
        return vga_palette;
    default:
        return vga_palette; // Default or error palette
    }
}

void LightShow::add_led_controller(CLEDController *led_controller)
{
    led_controllers_.push_back(led_controller);
}

void LightShow::brightness(uint8_t brightness)
{
    brightness_ = brightness;
    apply_scene_updates(brightness);
}

void LightShow::setSpeed(uint16_t speed)
{
    speed_ = speed;
    apply_scene_updates(speed);
}

uint8_t LightShow::getBrightness() const
{
    return brightness_;
}

void LightShow::solid(const CRGB &color)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::solid;
    new_scene.scenes.solid.color = color;
    apply_scene_updates(new_scene);
}

void LightShow::palette_cycle(AvailablePalettes palette, uint32_t duration)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::palette_cycle;
    new_scene.scenes.palette_cycle.palette = palette;
    new_scene.scenes.palette_cycle.duration = duration;
    apply_scene_updates(new_scene);
    if (!scene_changed_)
    {
        return;
    }

    // Restart the animation from the beginning.
    start_time_ = clock_.now();
    hue_ = 0;
}

void LightShow::palette_stream(uint16_t duration, AvailablePalettes palette, bool direction)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::palette_stream;
    new_scene.primary_palette = palette;
    new_scene.speed = duration;
    new_scene.scenes.palette_stream.palette = palette;
    new_scene.scenes.palette_stream.duration = duration;
    new_scene.scenes.palette_stream.direction = direction;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    // Restart the animation from the beginning.
    last_render_time_ = 0;
    hue_ = 0;
    setup_palette_stream_(direction);
}

void LightShow::spectrum_cycle(uint32_t duration)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::spectrum_cycle;
    new_scene.scenes.spectrum_cycle.duration = duration;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    // Restart the animation from the beginning.
    start_time_ = clock_.now();
    hue_ = 0;
}

void LightShow::spectrum_stream(uint32_t duration)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::spectrum_stream;
    new_scene.scenes.spectrum_stream.duration = duration;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    // Restart the animation from the beginning.
    last_render_time_ = 0;
    hue_ = 0;
    setup_spectrum_stream_();
}

void LightShow::spectrum_sparkle(uint16_t duration, uint8_t density)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::spectrum_sparkle;
    new_scene.scenes.sparkle.duration = duration;
    new_scene.scenes.sparkle.density = density;
    apply_scene_updates(new_scene);
}

void LightShow::strobe(uint16_t num_flashes, uint16_t duration_on, uint16_t duration_off, uint16_t duration_between_sets, CRGB color)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::strobe;
    new_scene.scenes.strobe.num_flashes = num_flashes;
    new_scene.scenes.strobe.duration_on = duration_on;
    new_scene.scenes.strobe.duration_off = duration_off;
    new_scene.scenes.strobe.duration_between_sets = duration_between_sets;
    new_scene.scenes.strobe.color = color;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    frame_number_ = 0;
    current_frame_duration_ = 0;
}

void LightShow::sparkle(uint16_t duration, uint8_t density, CRGB color)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::sparkle;
    new_scene.scenes.sparkle.duration = duration;
    new_scene.scenes.sparkle.density = density;
    new_scene.scenes.sparkle.color = color;
    apply_scene_updates(new_scene);
}

void LightShow::breathe(uint16_t duration, uint8_t dimness, CRGB color)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::breathe;
    new_scene.scenes.breathe.duration = duration;
    new_scene.scenes.breathe.dimness = dimness;
    new_scene.scenes.breathe.color = color;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    setup_breathe_palette_(new_scene.scenes.breathe.dimness, new_scene.scenes.breathe.color);
    start_time_ = clock_.now();
    scale_ = 0;
    palette_index_ = 0;
}

void LightShow::setCHSV(int color, int saturation, int luminosity)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::setCHSV;
    new_scene.scenes.setCHSV.color = color;
    new_scene.scenes.setCHSV.saturation = saturation;
    new_scene.scenes.setCHSV.luminosity = luminosity;
    apply_scene_updates(new_scene);
}

void LightShow::apply_scene_updates(uint8_t brightness)
{
    if (active_scene_.brightness != brightness)
    {
        active_scene_.brightness = brightness;
        scene_changed_ = true;
    }
}

void LightShow::apply_scene_updates(uint16_t speed)
{
    active_scene_.speed = speed;
    if (active_scene_.scenes.palette_cycle.duration != speed)
    {
        active_scene_.scenes.palette_cycle.duration = speed;
        scene_changed_ = true;
    }
    if (active_scene_.scenes.palette_stream.duration != speed)
    {
        active_scene_.scenes.palette_cycle.duration = speed;
        scene_changed_ = true;
    }
}

void LightShow::apply_scene_updates(LightScene &new_scene)
{
    new_scene.brightness = active_scene_.brightness;

    if (active_scene_.scene_id != new_scene.scene_id ||
        memcmp(&active_scene_.scenes, &new_scene.scenes, sizeof(active_scene_.scenes)) != 0)
    {
        active_scene_ = new_scene;
        scene_changed_ = true;
    }
}

void LightShow::apply_sync_updates(LightScene &new_scene)
{
    Serial.println("Weve reached the correct apply_sync_update");
    if (active_scene_.primary_palette != new_scene.primary_palette)
    {
        Serial.println("Primary Palette has changed, setting active scene palete to it!");
        active_scene_.primary_palette = new_scene.primary_palette;
        scene_changed_ = true;
    }
    if (active_scene_.scene_id != new_scene.scene_id ||
        memcmp(&active_scene_.scenes, &new_scene.scenes, sizeof(active_scene_.scenes)) != 0)
    {
        Serial.println("Scene ID has changed, setting active scene to new scene!");
        active_scene_ = new_scene;
        scene_changed_ = true;
    }
    if (active_scene_.speed != new_scene.speed)
    {
        Serial.println("Speed has changed, setting active scene speed to new scene speed!");
        active_scene_.speed = new_scene.speed;
        scene_changed_ = true;
    }
    if (active_scene_.brightness != new_scene.brightness)
    {
        Serial.println("Brightness has changed, setting active scene brightness to new scene brightness!");
        active_scene_.brightness = new_scene.brightness;
        scene_changed_ = true;
    }
}

void LightShow::render()
{
    unsigned long now = clock_.now();

    for (auto &controller : led_controllers_)
    {
        controller->setDither(0);
    }

    // Static scenes like solid colors don't need to be rendered if there are no changes.
    // However, render them at a slow default interval in case you plug the LEDs in after the
    // scene has been set. Animated scenes need to be rendered constantly even if the
    // configuration hasn't changed.
    switch (active_scene_.scene_id)
    {
    case LightSceneID::off:
        if (scene_changed_ || (now - last_render_time_ > static_scene_refresh_interval))
        {
            last_render_time_ = now;
            for (auto &controller : led_controllers_)
            {
                controller->showColor(CRGB::Black);
            }
        }
        break;

    case LightSceneID::solid:
        if (scene_changed_ || (now - last_render_time_ > static_scene_refresh_interval))
        {
            last_render_time_ = now;
            for (auto &controller : led_controllers_)
            {
                controller->showColor(active_scene_.scenes.solid.color, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::palette_cycle:
    {
        CRGBPalette16 current_palette = getPalette(active_scene_.primary_palette);
        uint8_t hueStep = 256 / led_controllers_.size(); // Assuming even distribution of hue over the number of controllers.
        if (now - last_render_time_ > active_scene_.speed)
        {
            last_render_time_ = now;

            for (auto &controller : led_controllers_)
            {
                int last = controller->size() - 1;

                for (int i = 0; i <= last; i++) // Also include the last LED.
                {
                    uint8_t ledHue = hue_ + i * hueStep;
                    controller->leds()[i] = ColorFromPalette(current_palette, ledHue);
                }

                controller->showLeds(active_scene_.brightness);
            }

            hue_ += 5; // Increment the starting hue for the next iteration. You can adjust the value 5 as needed.
        }
    }

    break;

    case LightSceneID::palette_stream:
        if (now - last_render_time_ > active_scene_.speed)
        {
            last_render_time_ = now;
            CRGBPalette16 current_palette = getPalette(active_scene_.primary_palette);

            for (auto &controller : led_controllers_)
            {
                int last = controller->size() - 1;
                if (!direction_)
                {
                    for (int i = last; i > 0; i--)
                    {
                        controller->leds()[i] = controller->leds()[i - 1];
                    }
                    controller->leds()[0] = ColorFromPalette(current_palette, hue_);
                }
                else
                {

                    for (int i = 0; i < last; i++)
                    {
                        controller->leds()[i] = controller->leds()[i + 1];
                    }

                    controller->leds()[last] = ColorFromPalette(current_palette, hue_);
                }

                hue_ = (hue_ + 1) % 255;
                controller->showLeds(active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::spectrum_cycle:
    {
        uint8_t new_hue = ((now - start_time_) / active_scene_.scenes.spectrum_cycle.duration) % 256;

        if (new_hue != hue_)
        {
            for (auto &controller : led_controllers_)
            {
                controller->showColor(CHSV(new_hue, 255, 255), active_scene_.brightness);
            }

            hue_ = new_hue;
        }
    }
    break;

    case LightSceneID::spectrum_stream:
        if (now - last_render_time_ > active_scene_.scenes.spectrum_stream.duration)
        {
            last_render_time_ = now;

            for (auto &controller : led_controllers_)
            {
                int last = controller->size() - 1;

                for (int i = 0; i < last; i++)
                {
                    controller->leds()[i] = controller->leds()[i + 1];
                }

                controller->leds()[last] = CHSV(hue_, 255, 255);
                hue_ += 3;
                controller->showLeds(active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::spectrum_sparkle:
        if (now - last_render_time_ > active_scene_.scenes.sparkle.duration)
        {
            last_render_time_ = now;

            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                size_t leds_to_light = num_leds * active_scene_.scenes.sparkle.density / 255;
                CRGB *leds = controller->leds();
                for (size_t i = 0; i < num_leds; i++)
                {
                    leds[i] = CRGB::Black;
                }

                for (size_t i = 0; i < leds_to_light; i++)
                {
                    size_t position = random(0, num_leds);
                    uint8_t hue = random(0, 256);
                    leds[position] = CHSV(hue, 255, 255);
                }

                controller->showLeds(active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::strobe:
        if (now - last_render_time_ > current_frame_duration_)
        {
            last_render_time_ = now;

            if (frame_number_ < active_scene_.scenes.strobe.num_flashes * 2)
            {
                if (frame_number_ & 0x1)
                {
                    for (auto &controller : led_controllers_)
                    {
                        controller->showColor(CRGB::Black);
                    }

                    current_frame_duration_ = active_scene_.scenes.strobe.duration_off;
                }
                else
                {
                    for (auto &controller : led_controllers_)
                    {
                        controller->showColor(active_scene_.scenes.strobe.color, active_scene_.brightness);
                    }

                    current_frame_duration_ = active_scene_.scenes.strobe.duration_off;
                }
                frame_number_++;
            }
            else
            {
                for (auto &controller : led_controllers_)
                {
                    controller->showColor(CRGB::Black);
                }

                current_frame_duration_ = active_scene_.scenes.strobe.duration_between_sets;
                frame_number_ = 0;
            }
        }
        break;

    case LightSceneID::sparkle:
        if (now - last_render_time_ > active_scene_.scenes.sparkle.duration)
        {
            last_render_time_ = now;
            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                size_t leds_to_light = num_leds * active_scene_.scenes.sparkle.density / 255;
                CRGB *leds = controller->leds();
                for (size_t i = 0; i < num_leds; i++)
                {
                    leds[i] = CRGB::Black;
                }

                for (size_t i = 0; i < leds_to_light; i++)
                {
                    size_t position = random(0, num_leds);
                    leds[position] = active_scene_.scenes.sparkle.color;
                }

                controller->showLeds(active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::breathe:
    {
        unsigned long intervals = (now - start_time_) / active_scene_.scenes.breathe.duration;
        uint8_t new_scale = intervals % 256;
        size_t new_palette_index = (intervals / 256) % palette_size_;

        if ((new_scale != scale_) || (new_palette_index != palette_index_))
        {
            CRGB &from_color = palette_[new_palette_index];
            CRGB &to_color = palette_[(new_palette_index + 1) % palette_size_];
            CRGB new_color = from_color.lerp8(to_color, new_scale);
            for (auto &controller : led_controllers_)
            {
                controller->showColor(new_color, active_scene_.brightness);
            }

            scale_ = new_scale;
            palette_index_ = new_palette_index;
        }
    }
    case LightSceneID::setCHSV:
    {
        for (auto &controller : led_controllers_)
        {
            size_t num_leds = controller->size();
            CRGB *leds = controller->leds();
            for (size_t i = 0; i < num_leds; i++)
            {
                leds[i] = CHSV(active_scene_.scenes.setCHSV.color, active_scene_.scenes.setCHSV.saturation, active_scene_.scenes.setCHSV.luminosity);
            }

            controller->showLeds(active_scene_.brightness);
        }
    }

    default:
        break;
    }

    scene_changed_ = false;
}

bool LightShow::scene_changed()
{
    return scene_changed_;
}

void LightShow::import_scene(const LightScene *buffer)
{
    LightScene new_scene = {};
    std::memcpy(&new_scene, buffer, sizeof(LightScene));
    apply_scene_updates(new_scene.brightness);
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    unsigned long now = clock_.now();

    switch (active_scene_.scene_id)
    {
    case LightSceneID::spectrum_cycle:
        hue_ = 0;
        start_time_ = now;
        break;
    case LightSceneID::spectrum_stream:
        hue_ = 0;
        last_render_time_ = 0;
        setup_spectrum_stream_();
        break;
    case LightSceneID::breathe:
        setup_breathe_palette_(new_scene.scenes.breathe.dimness, new_scene.scenes.breathe.color);
        start_time_ = now;
        scale_ = 0;
        palette_index_ = 0;
        break;
    case LightSceneID::palette_cycle:
        start_time_ = now;
        scale_ = 0;
        palette_index_ = 0;
        break;
    case LightSceneID::palette_stream:
    {
        hue_ = 0;
        last_render_time_ = 0;
        bool direction = new_scene.scenes.palette_stream.direction;
        setup_palette_stream_(direction);
        break;
    }
    default:
        break;
    }

    // Restart animations from the beginning.
    last_render_time_ = 0;
    frame_number_ = 0;
    current_frame_duration_ = 0;
}

void LightShow::export_scene(LightScene *buffer) const
{
    size_t bytes = sizeof(LightScene);
    std::memcpy(buffer, &active_scene_, bytes);
}

void LightShow::setup_breathe_palette_(uint8_t dimness, CRGB color)
{
    palette_size_ = 2;
    palette_[0] = color;
    palette_[1] = color.lerp8(CRGB::Black, dimness);
}

void LightShow::setup_spectrum_stream_()
{
    for (auto &controller : led_controllers_)
    {
        for (int i = 0; i < controller->size(); i++)
        {
            controller->leds()[i] = CHSV(hue_, 255, 255);
            hue_ += 3;
        }
    }
}

void LightShow::setup_palette_stream_(bool direction)
{
    CRGBPalette16 current_palette = getPalette(active_scene_.scenes.palette_stream.palette);
    const size_t palette_size = sizeof(current_palette) / sizeof(current_palette[0]);
    this->direction_ = direction;
    for (auto &controller : led_controllers_)
    {
        for (int i = 0; i < controller->size(); i++)
        {
            controller->leds()[i] = current_palette[static_cast<uint8_t>(hue_ % palette_size)];
            hue_++;
        }
    }
}

// UMBRELLA PRIMARY AND SECONDARY PALETTES
// Set the primary palette to a predefined palette
void LightShow::setPrimaryPalette(AvailablePalettes palette)
{
    Serial.println("Setting primary palette");
    primary_palette_ = getPalette(palette);
    current_palette_ = palette;
}

// Set primary palette by index
void LightShow::setPrimaryPalette(size_t index)
{
    if (index < available_palettes_.size())
    {
        primary_palette_ = *(available_palettes_[index]);
        current_palette_ = static_cast<AvailablePalettes>(index); // Map index to the enum value
    }
}

// Set the secondary palette to a predefined palette
void LightShow::setSecondaryPalette(AvailablePalettes palette)
{
    secondary_palette_ = getPalette(palette);
}

AvailablePalettes LightShow::getPrimaryPalette() const
{
    return current_palette_;
}