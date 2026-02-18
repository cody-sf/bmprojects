#include "LightShow.h"
#include <FastLED.h>
#include "Palettes.h"
#include <algorithm>

LightShow::LightShow(const std::vector<CLEDController *> &led_controllers, const Clock &clock)
    : led_controllers_(led_controllers), clock_(clock), scene_changed_(false), hue_(0), frame_number_(0), scale_(0), palette_index_(0), palette_size_(0),
      current_palette_(AvailablePalettes::cool), primary_palette_(getPalette(AvailablePalettes::cool)), secondary_palette_(getPalette(AvailablePalettes::earth)), speed_(175), color_(CRGB::Red), direction_(true),
      heat_array_(nullptr), heat_array_size_(0), meteor_positions_(nullptr), meteor_trails_(nullptr), pulse_center_(0), plasma_offset_(0), 
      noise_x_(0), noise_y_(0), explosion_center_(0), spiral_angle_(0)
{
    // Initialize the active scene to default settings
    memset(&active_scene_, 0, sizeof(active_scene_));
    active_scene_.scene_id = LightSceneID::off;

    // Initialize matrix drops
    memset(matrix_drops_, 0, sizeof(matrix_drops_));

    // Initialize the available palettes
    available_palettes_ = {
        &candyPalette,
        &coolPalette,
        &cosmicWavesPalette,
        &earthPalette,
        &eblossomPalette,
        &emeraldPalette,
        &everglowPalette,
        &fatboyPalette,
        &fireicePalette,
        &fireyNightPalette,
        &flamePalette,
        &heartPalette,
        &lavaPalette,
        &meadowPalette,
        &melonballPalette,
        &nebulaPalette,
        &oasisPalette,
        &pinksplashPalette,
        &rPalette,
        &sofiaPalette,
        &sunsetPalette,
        &sunsetFusionPalette,
        &trovePalette,
        &vividPalette,
        &velvetPalette,
        &vgaPalette,
        &wavePalette,
        // New Burning Man palettes
        &electricDesertPalette,
        &psychedelicPlayaPalette,
        &burningRainbowPalette,
        &neonNightsPalette,
        &desertStormPalette,
        &cosmicFirePalette,
        &alienGlowPalette,
        &moltenMetalPalette,
        // Add other palettes as needed
    };
}

LightShow::~LightShow()
{
    // Clean up dynamically allocated memory
    if (heat_array_) delete[] heat_array_;
    if (meteor_positions_) delete[] meteor_positions_;
    if (meteor_trails_) delete[] meteor_trails_;
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
    case candy:
        return candy_palette;
    case cool:
        return cool_palette;
    case cosmicwaves:
        return cosmic_waves_palette;
    case earth:
        return earth_palette;
    case eblossom:
        return electric_blossom_palette;
    case emerald:
        return emerald_palette;
    case everglow:
        return everglow_palette;
    case fatboy:
        return fatboy_palette;
    case fireice:
        return fireice_palette;
    case fireynight:
        return firey_night_palette;
    case flame:
        return flame_palette;
    case heart:
        return heart_palette;
    case lava:
        return lava_palette;
    case meadow:
        return meadow_palette;
    case melonball:
        return melonball_palette;
    case nebula:
        return nebula_palette;
    case oasis:
        return oasis_palette;
    case pinksplash:
        return pinksplash_palette;
    case r:
        return r_palette;
    case sofia:
        return sofia_palette;
    case sunset:
        return sunset_palette;
    case sunsetfusion:
        return sunset_fusion_palette;
    case trove:
        return trove_palette;
    case vivid:
        return vivid_palette;
    case velvet:
        return velvet_palette;
    case vga:
        return vga_palette;
    case wave:
        return wave_palette;
    // New Burning Man palettes
    case electricdesert:
        return electric_desert_palette;
    case psychedelicplaya:
        return psychedelic_playa_palette;
    case burningrainbow:
        return burning_rainbow_palette;
    case neonnights:
        return neon_nights_palette;
    case desertstorm:
        return desert_storm_palette;
    case cosmicfire:
        return cosmic_fire_palette;
    case alienglow:
        return alien_glow_palette;
    case moltenmetal:
        return molten_metal_palette;
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
    new_scene.scenes.solid.color = {color.r, color.g, color.b};
    new_scene.color = color;
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
    new_scene.direction = direction;
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
    new_scene.scenes.strobe.color = {color.r, color.g, color.b};
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
    new_scene.scenes.sparkle.color = {color.r, color.g, color.b};
    apply_scene_updates(new_scene);
}

void LightShow::breathe(uint16_t duration, uint8_t dimness, CRGB color)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::breathe;
    new_scene.scenes.breathe.duration = duration;
    new_scene.scenes.breathe.dimness = dimness;
    new_scene.scenes.breathe.color = {color.r, color.g, color.b};
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    setup_breathe_palette_(new_scene.scenes.breathe.dimness, CRGB(new_scene.scenes.breathe.color.r, new_scene.scenes.breathe.color.g, new_scene.scenes.breathe.color.b));
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
    brightness_ = new_scene.brightness;
    active_scene_.brightness = brightness_;

    if (active_scene_.color != new_scene.color)
    {
        Serial.println("Color has changed");
        active_scene_.color = new_scene.color;
        color_ = new_scene.color;
        scene_changed_ = true;
    }
    if (active_scene_.primary_palette != new_scene.primary_palette)
    {
        Serial.println("Primary Palette has changed");
        active_scene_.primary_palette = new_scene.primary_palette;
        scene_changed_ = true;
    }
    if (active_scene_.scene_id != new_scene.scene_id)
    // ||
    // memcmp(&active_scene_.scenes, &new_scene.scenes, sizeof(active_scene_.scenes)) != 0)
    {
        Serial.println("Scene ID has changed");
        active_scene_.scene_id = new_scene.scene_id;
        scene_changed_ = true;
    }
    if (active_scene_.speed != new_scene.speed)
    {
        Serial.println("Speed has changed");
        active_scene_.speed = new_scene.speed;
        scene_changed_ = true;
        speed_ = new_scene.speed;
    }
    if (active_scene_.brightness != new_scene.brightness)
    {
        Serial.println("Brightness has changed");
        active_scene_.brightness = new_scene.brightness;
        scene_changed_ = true;
    }
    if (active_scene_.direction != new_scene.direction)
    {
        Serial.println("Direction has changed");
        active_scene_.direction = new_scene.direction;
        scene_changed_ = true;
        direction_ = new_scene.direction;
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
                controller->showColor(CRGB::Black, controller->size(), brightness_);
            }
        }
        break;

    case LightSceneID::solid:
        if (scene_changed_ || (now - last_render_time_ > static_scene_refresh_interval))
        {
            last_render_time_ = now;
            for (auto &controller : led_controllers_)
            {
                controller->showColor(active_scene_.color, controller->size(), active_scene_.brightness);
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
                controller->showLeds(brightness_);
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
                controller->showColor(CHSV(new_hue, 255, 255), controller->size(), active_scene_.brightness);
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
                        controller->showColor(CRGB::Black, controller->size(), brightness_);
                    }

                    current_frame_duration_ = active_scene_.scenes.strobe.duration_off;
                }
                else
                {
                    for (auto &controller : led_controllers_)
                    {
                        controller->showColor(CRGB(active_scene_.scenes.strobe.color.r, active_scene_.scenes.strobe.color.g, active_scene_.scenes.strobe.color.b), controller->size(), active_scene_.brightness);
                    }

                    current_frame_duration_ = active_scene_.scenes.strobe.duration_off;
                }
                frame_number_++;
            }
            else
            {
                for (auto &controller : led_controllers_)
                {
                    controller->showColor(CRGB::Black, controller->size(), brightness_);
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
                    leds[position] = CRGB(active_scene_.scenes.sparkle.color.r, active_scene_.scenes.sparkle.color.g, active_scene_.scenes.sparkle.color.b);
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
                controller->showColor(new_color, controller->size(), active_scene_.brightness);
            }

            scale_ = new_scale;
            palette_index_ = new_palette_index;
        }
    }
    break;
    
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
    break;

    // NEW BURNING MAN EFFECTS RENDERING - SPECTACULAR LIGHT SHOWS!
    
    case LightSceneID::pulse_wave:
        if (now - last_render_time_ > active_scene_.scenes.pulse_wave.duration)
        {
            last_render_time_ = now;
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.pulse_wave.palette);
            
            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                
                for (size_t i = 0; i < num_leds; i++)
                {
                    // Create expanding pulse waves from center
                    uint8_t distance = abs((int)i - (int)pulse_center_);
                    uint8_t wave_val = sin8(distance * active_scene_.scenes.pulse_wave.wave_width + hue_);
                    leds[i] = ColorFromPalette(current_palette, wave_val);
                }
                
                controller->showLeds(active_scene_.brightness);
            }
            
            pulse_center_ = (pulse_center_ + 1) % (led_controllers_.empty() ? 1 : led_controllers_[0]->size());
            hue_ += 4;
        }
        break;

    case LightSceneID::meteor_shower:
        if (now - last_render_time_ > active_scene_.scenes.meteor_shower.duration)
        {
            last_render_time_ = now;
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.meteor_shower.palette);
            
            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                
                // Fade all LEDs
                for (size_t i = 0; i < num_leds; i++)
                {
                    leds[i].fadeToBlackBy(60);
                }
                
                // Update meteors
                for (uint8_t m = 0; m < active_scene_.scenes.meteor_shower.meteor_count; m++)
                {
                    if (meteor_positions_)
                    {
                        uint8_t pos = (meteor_positions_[m] * num_leds) >> 8;
                        if (pos < num_leds)
                        {
                            leds[pos] = ColorFromPalette(current_palette, meteor_positions_[m] + hue_);
                        }
                        meteor_positions_[m] += 2; // Speed of meteors
                    }
                }
                
                controller->showLeds(active_scene_.brightness);
            }
            hue_ += 1;
        }
        break;

    case LightSceneID::fire_plasma:
        if (now - last_render_time_ > active_scene_.scenes.fire_plasma.duration)
        {
            last_render_time_ = now;
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.fire_plasma.palette);
            
            size_t led_idx = 0;
            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                
                for (size_t i = 0; i < num_leds && led_idx < heat_array_size_; i++, led_idx++)
                {
                    // Cool down
                    heat_array_[led_idx] = std::max(0, (int)heat_array_[led_idx] - (int)random(0, 10));
                    
                    // Heat from neighbors (simple diffusion)
                    if (led_idx > 0 && led_idx < heat_array_size_ - 1)
                    {
                        heat_array_[led_idx] = (heat_array_[led_idx - 1] + heat_array_[led_idx] + heat_array_[led_idx + 1]) / 3;
                    }
                    
                    // Add random heat sparks
                    if (random(255) < active_scene_.scenes.fire_plasma.heat_variance)
                    {
                        heat_array_[led_idx] = std::min(255, (int)heat_array_[led_idx] + (int)random(50, 255));
                    }
                    
                    leds[i] = ColorFromPalette(current_palette, heat_array_[led_idx]);
                }
                
                controller->showLeds(active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::kaleidoscope:
        if (now - last_render_time_ > active_scene_.scenes.kaleidoscope.duration)
        {
            last_render_time_ = now;
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.kaleidoscope.palette);
            
            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                
                for (size_t i = 0; i < num_leds; i++)
                {
                    // Create kaleidoscope effect with mirroring
                    uint8_t mirror_section = num_leds / active_scene_.scenes.kaleidoscope.mirror_count;
                    uint8_t mirror_pos = i % mirror_section;
                    uint8_t pattern = sin8(mirror_pos * 8 + hue_) + cos8(mirror_pos * 4 + hue_ * 2);
                    leds[i] = ColorFromPalette(current_palette, pattern);
                }
                
                controller->showLeds(active_scene_.brightness);
            }
            hue_ += 3;
        }
        break;

    case LightSceneID::rainbow_comet:
        if (now - last_render_time_ > active_scene_.scenes.rainbow_comet.duration)
        {
            last_render_time_ = now;
            
            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                
                // Fade existing
                for (size_t i = 0; i < num_leds; i++)
                {
                    leds[i].fadeToBlackBy(80);
                }
                
                // Draw rainbow comets
                for (uint8_t c = 0; c < active_scene_.scenes.rainbow_comet.comet_count; c++)
                {
                    uint8_t comet_pos = (hue_ + c * (256 / active_scene_.scenes.rainbow_comet.comet_count)) % 256;
                    uint8_t led_pos = (comet_pos * num_leds) >> 8;
                    
                    if (led_pos < num_leds)
                    {
                        leds[led_pos] = CHSV(comet_pos + hue_, 255, 255);
                        
                        // Draw trail
                        for (uint8_t t = 1; t < active_scene_.scenes.rainbow_comet.trail_length && led_pos >= t; t++)
                        {
                            leds[led_pos - t] = CHSV(comet_pos + hue_, 255, 255 - (t * 40));
                        }
                    }
                }
                
                controller->showLeds(active_scene_.brightness);
            }
            hue_ += 4;
        }
        break;

    case LightSceneID::matrix_rain:
        if (now - last_render_time_ > active_scene_.scenes.matrix_rain.duration)
        {
            last_render_time_ = now;
            
            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                
                // Fade all
                for (size_t i = 0; i < num_leds; i++)
                {
                    leds[i].fadeToBlackBy(50);
                }
                
                // Add new drops
                if (random(255) < active_scene_.scenes.matrix_rain.drop_rate)
                {
                    for (int d = 0; d < 64; d++)
                    {
                        if (matrix_drops_[d] == 0)
                        {
                            matrix_drops_[d] = 1;
                            break;
                        }
                    }
                }
                
                // Update drops
                for (int d = 0; d < 64; d++)
                {
                    if (matrix_drops_[d] > 0)
                    {
                        uint8_t pos = (matrix_drops_[d] * num_leds) >> 8;
                        if (pos < num_leds)
                        {
                            leds[pos] = CRGB(active_scene_.scenes.matrix_rain.color.r, active_scene_.scenes.matrix_rain.color.g, active_scene_.scenes.matrix_rain.color.b);
                        }
                        matrix_drops_[d] += 3;
                        if (matrix_drops_[d] == 0) matrix_drops_[d] = 0; // Reset when wrapped
                    }
                }
                
                controller->showLeds(active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::plasma_clouds:
        if (now - last_render_time_ > active_scene_.scenes.plasma_clouds.duration)
        {
            last_render_time_ = now;
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.plasma_clouds.palette);
            
            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                
                for (size_t i = 0; i < num_leds; i++)
                {
                    // Create smooth plasma effect
                    uint8_t plasma1 = sin8((i * active_scene_.scenes.plasma_clouds.cloud_scale) + plasma_offset_);
                    uint8_t plasma2 = cos8((i * (active_scene_.scenes.plasma_clouds.cloud_scale / 2)) + plasma_offset_ * 2);
                    uint8_t plasma_combined = (plasma1 + plasma2) / 2;
                    leds[i] = ColorFromPalette(current_palette, plasma_combined);
                }
                
                controller->showLeds(active_scene_.brightness);
            }
            plasma_offset_ += 2;
        }
        break;

    case LightSceneID::lava_lamp:
        if (now - last_render_time_ > active_scene_.scenes.lava_lamp.duration)
        {
            last_render_time_ = now;
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.lava_lamp.palette);
            unsigned long time_offset = (now - start_time_) / 10;
            
            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                
                for (size_t i = 0; i < num_leds; i++)
                {
                    uint8_t blob_influence = 0;
                    
                    // Calculate influence from each blob
                    for (uint8_t b = 0; b < active_scene_.scenes.lava_lamp.blob_count; b++)
                    {
                        uint8_t blob_pos = sin8(time_offset + b * 64) >> 2; // Blob position 0-63
                        blob_pos = map(blob_pos, 0, 63, 0, num_leds - 1);
                        
                        uint8_t distance = abs((int)i - (int)blob_pos);
                        if (distance < 10) // Blob radius
                        {
                            blob_influence = std::max((int)blob_influence, 255 - (distance * 25));
                        }
                    }
                    
                    leds[i] = ColorFromPalette(current_palette, blob_influence);
                }
                
                controller->showLeds(active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::aurora_borealis:
        if (now - last_render_time_ > active_scene_.scenes.aurora_borealis.duration)
        {
            last_render_time_ = now;
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.aurora_borealis.palette);
            
            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                
                for (size_t i = 0; i < num_leds; i++)
                {
                    // Create aurora waves
                    uint8_t wave1 = sin8((i * 4) + (noise_x_ * 2));
                    uint8_t wave2 = cos8((i * 6) + (noise_y_ * 3));
                    uint8_t wave3 = sin8((i * 2) + hue_);
                    
                    uint8_t aurora_intensity = (wave1 + wave2 + wave3) / 3;
                    leds[i] = ColorFromPalette(current_palette, aurora_intensity);
                }
                
                controller->showLeds(active_scene_.brightness);
            }
            
            noise_x_ += 0.1;
            noise_y_ += 0.05;
            hue_ += 1;
        }
        break;

    case LightSceneID::lightning_storm:
        if (now - last_render_time_ > active_scene_.scenes.lightning_storm.flash_frequency)
        {
            last_render_time_ = now;
            
            if (random(100) < 20) // 20% chance of lightning
            {
                // Lightning flash
                for (auto &controller : led_controllers_)
                {
                    controller->showColor(CRGB::White, controller->size(), active_scene_.scenes.lightning_storm.flash_intensity);
                }
                frame_number_ = 3; // Flash duration
            }
            else if (frame_number_ > 0)
            {
                // Continue flash
                for (auto &controller : led_controllers_)
                {
                    uint8_t fade_intensity = (active_scene_.scenes.lightning_storm.flash_intensity * frame_number_) / 3;
                    controller->showColor(CRGB::White, controller->size(), fade_intensity);
                }
                frame_number_--;
            }
            else
            {
                // Storm clouds (dark with occasional flickers)
                for (auto &controller : led_controllers_)
                {
                    size_t num_leds = controller->size();
                    CRGB *leds = controller->leds();
                    
                    for (size_t i = 0; i < num_leds; i++)
                    {
                        if (random(100) < 5)
                        {
                            leds[i] = CRGB(20, 20, 40); // Dim blue-gray flicker
                        }
                        else
                        {
                            leds[i] = CRGB(5, 5, 10); // Dark storm clouds
                        }
                    }
                    
                    controller->showLeds(active_scene_.brightness);
                }
            }
        }
        break;

    case LightSceneID::color_explosion:
        if (now - last_render_time_ > active_scene_.scenes.color_explosion.duration)
        {
            last_render_time_ = now;
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.color_explosion.palette);
            unsigned long time_since_start = now - start_time_;
            
            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                
                for (size_t i = 0; i < num_leds; i++)
                {
                    // Distance from explosion center
                    uint8_t distance = abs((int)i - (int)explosion_center_);
                    
                    // Explosion wave propagation
                    uint8_t wave_position = (time_since_start / 20) % (num_leds * 2);
                    uint8_t explosion_intensity = 0;
                    
                    if (distance <= wave_position && distance >= wave_position - active_scene_.scenes.color_explosion.explosion_size)
                    {
                        explosion_intensity = 255 - ((wave_position - distance) * (255 / active_scene_.scenes.color_explosion.explosion_size));
                    }
                    
                    leds[i] = ColorFromPalette(current_palette, explosion_intensity + hue_);
                }
                
                controller->showLeds(active_scene_.brightness);
            }
            
            // Create new explosion occasionally
            if (time_since_start % 500 == 0)
            {
                explosion_center_ = random(0, led_controllers_.empty() ? 1 : led_controllers_[0]->size());
                start_time_ = now;
            }
            hue_ += 2;
        }
        break;

    case LightSceneID::spiral_galaxy:
        if (now - last_render_time_ > active_scene_.scenes.spiral_galaxy.duration)
        {
            last_render_time_ = now;
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.spiral_galaxy.palette);
            
            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                
                for (size_t i = 0; i < num_leds; i++)
                {
                    // Create spiral pattern
                    uint8_t spiral_position = (i * 256 / num_leds) + spiral_angle_;
                    uint8_t arm_number = (i * active_scene_.scenes.spiral_galaxy.spiral_arms) / num_leds;
                    uint8_t arm_offset = arm_number * (256 / active_scene_.scenes.spiral_galaxy.spiral_arms);
                    
                    uint8_t spiral_intensity = sin8(spiral_position + arm_offset);
                    uint8_t distance_fade = 255 - abs((int)128 - (int)((i * 256) / num_leds)); // Fade from center
                    
                    uint8_t final_intensity = (spiral_intensity * distance_fade) >> 8;
                    leds[i] = ColorFromPalette(current_palette, final_intensity + hue_);
                }
                
                controller->showLeds(active_scene_.brightness);
            }
            
            spiral_angle_ += 2;
            hue_ += 1;
        }
        break;

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
        setup_breathe_palette_(new_scene.scenes.breathe.dimness, CRGB(new_scene.scenes.breathe.color.r, new_scene.scenes.breathe.color.g, new_scene.scenes.breathe.color.b));
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
    // New Burning Man effects initialization
    case LightSceneID::pulse_wave:
        pulse_center_ = 0;
        start_time_ = now;
        hue_ = 0;
        break;
    case LightSceneID::meteor_shower:
        if (meteor_positions_) delete[] meteor_positions_;
        if (meteor_trails_) delete[] meteor_trails_;
        meteor_positions_ = new uint8_t[new_scene.scenes.meteor_shower.meteor_count]();
        meteor_trails_ = new uint8_t[new_scene.scenes.meteor_shower.meteor_count * new_scene.scenes.meteor_shower.trail_length]();
        for (uint8_t i = 0; i < new_scene.scenes.meteor_shower.meteor_count; i++)
        {
            meteor_positions_[i] = random(0, 255);
        }
        hue_ = 0;
        break;
    case LightSceneID::fire_plasma:
        {
            size_t total_leds = 0;
            for (auto &controller : led_controllers_)
            {
                total_leds += controller->size();
            }
            if (heat_array_size_ != total_leds)
            {
                if (heat_array_) delete[] heat_array_;
                heat_array_ = new uint8_t[total_leds]();
                heat_array_size_ = total_leds;
            }
            for (size_t i = 0; i < heat_array_size_; i++)
            {
                heat_array_[i] = random(0, 100);
            }
        }
        break;
    case LightSceneID::kaleidoscope:
        start_time_ = now;
        hue_ = 0;
        break;
    case LightSceneID::rainbow_comet:
        last_render_time_ = 0;
        hue_ = 0;
        break;
    case LightSceneID::matrix_rain:
        memset(matrix_drops_, 0, sizeof(matrix_drops_));
        last_render_time_ = 0;
        break;
    case LightSceneID::plasma_clouds:
        plasma_offset_ = 0;
        noise_x_ = 0;
        noise_y_ = 0;
        break;
    case LightSceneID::lava_lamp:
        start_time_ = now;
        break;
    case LightSceneID::aurora_borealis:
        start_time_ = now;
        noise_x_ = 0;
        noise_y_ = 0;
        hue_ = 0;
        break;
    case LightSceneID::lightning_storm:
        frame_number_ = 0;
        last_render_time_ = 0;
        break;
    case LightSceneID::color_explosion:
        explosion_center_ = 0;
        start_time_ = now;
        hue_ = 0;
        break;
    case LightSceneID::spiral_galaxy:
        spiral_angle_ = 0;
        start_time_ = now;
        hue_ = 0;
        break;
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

// NEW BURNING MAN EFFECTS - GET READY TO LIGHT UP THE PLAYA!

void LightShow::pulse_wave(uint16_t duration, uint8_t wave_width, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::pulse_wave;
    new_scene.scenes.pulse_wave.duration = duration;
    new_scene.scenes.pulse_wave.wave_width = wave_width;
    new_scene.scenes.pulse_wave.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    pulse_center_ = 0;
    start_time_ = clock_.now();
}

void LightShow::meteor_shower(uint16_t duration, uint8_t meteor_count, uint8_t trail_length, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::meteor_shower;
    new_scene.scenes.meteor_shower.duration = duration;
    new_scene.scenes.meteor_shower.meteor_count = meteor_count;
    new_scene.scenes.meteor_shower.trail_length = trail_length;
    new_scene.scenes.meteor_shower.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    // Initialize meteor arrays if needed
    if (meteor_positions_) delete[] meteor_positions_;
    if (meteor_trails_) delete[] meteor_trails_;
    
    meteor_positions_ = new uint8_t[meteor_count]();
    meteor_trails_ = new uint8_t[meteor_count * trail_length]();
    
    // Randomize initial positions
    for (uint8_t i = 0; i < meteor_count; i++)
    {
        meteor_positions_[i] = random(0, 255);
    }
}

void LightShow::fire_plasma(uint16_t duration, uint8_t heat_variance, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::fire_plasma;
    new_scene.scenes.fire_plasma.duration = duration;
    new_scene.scenes.fire_plasma.heat_variance = heat_variance;
    new_scene.scenes.fire_plasma.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    // Calculate total LED count for heat array
    size_t total_leds = 0;
    for (auto &controller : led_controllers_)
    {
        total_leds += controller->size();
    }

    if (heat_array_size_ != total_leds)
    {
        if (heat_array_) delete[] heat_array_;
        heat_array_ = new uint8_t[total_leds]();
        heat_array_size_ = total_leds;
    }

    // Initialize with random heat values
    for (size_t i = 0; i < heat_array_size_; i++)
    {
        heat_array_[i] = random(0, 100);
    }
}

void LightShow::kaleidoscope(uint16_t duration, uint8_t mirror_count, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::kaleidoscope;
    new_scene.scenes.kaleidoscope.duration = duration;
    new_scene.scenes.kaleidoscope.mirror_count = mirror_count;
    new_scene.scenes.kaleidoscope.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    start_time_ = clock_.now();
    hue_ = 0;
}

void LightShow::rainbow_comet(uint16_t duration, uint8_t comet_count, uint8_t trail_length)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::rainbow_comet;
    new_scene.scenes.rainbow_comet.duration = duration;
    new_scene.scenes.rainbow_comet.comet_count = comet_count;
    new_scene.scenes.rainbow_comet.trail_length = trail_length;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
    hue_ = 0;
}

void LightShow::matrix_rain(uint16_t duration, uint8_t drop_rate, CRGB color)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::matrix_rain;
    new_scene.scenes.matrix_rain.duration = duration;
    new_scene.scenes.matrix_rain.drop_rate = drop_rate;
    new_scene.scenes.matrix_rain.color = {color.r, color.g, color.b};
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    // Initialize matrix drops
    memset(matrix_drops_, 0, sizeof(matrix_drops_));
    last_render_time_ = 0;
}

void LightShow::plasma_clouds(uint16_t duration, uint8_t cloud_scale, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::plasma_clouds;
    new_scene.scenes.plasma_clouds.duration = duration;
    new_scene.scenes.plasma_clouds.cloud_scale = cloud_scale;
    new_scene.scenes.plasma_clouds.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    plasma_offset_ = 0;
    noise_x_ = 0;
    noise_y_ = 0;
}

void LightShow::lava_lamp(uint16_t duration, uint8_t blob_count, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::lava_lamp;
    new_scene.scenes.lava_lamp.duration = duration;
    new_scene.scenes.lava_lamp.blob_count = blob_count;
    new_scene.scenes.lava_lamp.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    start_time_ = clock_.now();
}

void LightShow::aurora_borealis(uint16_t duration, uint8_t wave_count, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::aurora_borealis;
    new_scene.scenes.aurora_borealis.duration = duration;
    new_scene.scenes.aurora_borealis.wave_count = wave_count;
    new_scene.scenes.aurora_borealis.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    start_time_ = clock_.now();
    noise_x_ = 0;
    noise_y_ = 0;
}

void LightShow::lightning_storm(uint16_t duration, uint8_t flash_intensity, uint16_t flash_frequency)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::lightning_storm;
    new_scene.scenes.lightning_storm.duration = duration;
    new_scene.scenes.lightning_storm.flash_intensity = flash_intensity;
    new_scene.scenes.lightning_storm.flash_frequency = flash_frequency;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    frame_number_ = 0;
    last_render_time_ = 0;
}

void LightShow::color_explosion(uint16_t duration, uint8_t explosion_size, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::color_explosion;
    new_scene.scenes.color_explosion.duration = duration;
    new_scene.scenes.color_explosion.explosion_size = explosion_size;
    new_scene.scenes.color_explosion.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    explosion_center_ = 0;
    start_time_ = clock_.now();
}

void LightShow::spiral_galaxy(uint16_t duration, uint8_t spiral_arms, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::spiral_galaxy;
    new_scene.scenes.spiral_galaxy.duration = duration;
    new_scene.scenes.spiral_galaxy.spiral_arms = spiral_arms;
    new_scene.scenes.spiral_galaxy.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    spiral_angle_ = 0;
    start_time_ = clock_.now();
}

// --- Static mapping arrays and functions for effect/palette names <-> enums ---
namespace {
struct EffectNameMapEntry {
    const char* name;
    LightSceneID id;
};
const EffectNameMapEntry effectNameMap[] = {
    {"pstream", LightSceneID::palette_stream},
    {"pcycle", LightSceneID::palette_cycle},
    {"pulse_wave", LightSceneID::pulse_wave},
    {"meteor_shower", LightSceneID::meteor_shower},
    {"fire_plasma", LightSceneID::fire_plasma},
    {"kaleidoscope", LightSceneID::kaleidoscope},
    {"rainbow_comet", LightSceneID::rainbow_comet},
    {"matrix_rain", LightSceneID::matrix_rain},
    {"plasma_clouds", LightSceneID::plasma_clouds},
    {"lava_lamp", LightSceneID::lava_lamp},
    {"aurora_borealis", LightSceneID::aurora_borealis},
    {"lightning_storm", LightSceneID::lightning_storm},
    {"color_explosion", LightSceneID::color_explosion},
    {"spiral_galaxy", LightSceneID::spiral_galaxy},
    {"cradial", LightSceneID::color_radial},
    {"cwheel", LightSceneID::color_wheel},
    {"speedo", LightSceneID::speedometer},
    {"pstat", LightSceneID::position_status},
    {"off", LightSceneID::off},
    // Add more as needed
};
const int effectNameMapSize = sizeof(effectNameMap) / sizeof(effectNameMap[0]);

struct PaletteNameMapEntry {
    const char* name;
    AvailablePalettes id;
};
const PaletteNameMapEntry paletteNameMap[] = {
    {"candy", AvailablePalettes::candy},
    {"candyPalette", AvailablePalettes::candy},
    {"cool", AvailablePalettes::cool},
    {"coolPalette", AvailablePalettes::cool},
    {"cosmicwaves", AvailablePalettes::cosmicwaves},
    {"cosmicwavesPalette", AvailablePalettes::cosmicwaves},
    {"earth", AvailablePalettes::earth},
    {"earthPalette", AvailablePalettes::earth},
    {"eblossom", AvailablePalettes::eblossom},
    {"eblossomPalette", AvailablePalettes::eblossom},
    {"emerald", AvailablePalettes::emerald},
    {"emeraldPalette", AvailablePalettes::emerald},
    {"everglow", AvailablePalettes::everglow},
    {"everglowPalette", AvailablePalettes::everglow},
    {"fatboy", AvailablePalettes::fatboy},
    {"fatboyPalette", AvailablePalettes::fatboy},
    {"fireice", AvailablePalettes::fireice},
    {"fireicePalette", AvailablePalettes::fireice},
    {"fireynight", AvailablePalettes::fireynight},
    {"fireynightPalette", AvailablePalettes::fireynight},
    {"flame", AvailablePalettes::flame},
    {"flamePalette", AvailablePalettes::flame},
    {"heart", AvailablePalettes::heart},
    {"heartPalette", AvailablePalettes::heart},
    {"lava", AvailablePalettes::lava},
    {"lavaPalette", AvailablePalettes::lava},
    {"meadow", AvailablePalettes::meadow},
    {"meadowPalette", AvailablePalettes::meadow},
    {"melonball", AvailablePalettes::melonball},
    {"melonballPalette", AvailablePalettes::melonball},
    {"nebula", AvailablePalettes::nebula},
    {"nebulaPalette", AvailablePalettes::nebula},
    {"oasis", AvailablePalettes::oasis},
    {"oasisPalette", AvailablePalettes::oasis},
    {"pinksplash", AvailablePalettes::pinksplash},
    {"pinksplashPalette", AvailablePalettes::pinksplash},
    {"r", AvailablePalettes::r},
    {"rPalette", AvailablePalettes::r},
    {"sofia", AvailablePalettes::sofia},
    {"sofiaPalette", AvailablePalettes::sofia},
    {"sunset", AvailablePalettes::sunset},
    {"sunsetPalette", AvailablePalettes::sunset},
    {"sunsetfusion", AvailablePalettes::sunsetfusion},
    {"sunsetfusionPalette", AvailablePalettes::sunsetfusion},
    {"trove", AvailablePalettes::trove},
    {"trovePalette", AvailablePalettes::trove},
    {"vivid", AvailablePalettes::vivid},
    {"vividPalette", AvailablePalettes::vivid},
    {"velvet", AvailablePalettes::velvet},
    {"velvetPalette", AvailablePalettes::velvet},
    {"vga", AvailablePalettes::vga},
    {"vgaPalette", AvailablePalettes::vga},
    {"wave", AvailablePalettes::wave},
    {"wavePalette", AvailablePalettes::wave},
    {"electricdesert", AvailablePalettes::electricdesert},
    {"electricDesertPalette", AvailablePalettes::electricdesert},
    {"psychedelicplaya", AvailablePalettes::psychedelicplaya},
    {"psychedelicPlayaPalette", AvailablePalettes::psychedelicplaya},
    {"burningrainbow", AvailablePalettes::burningrainbow},
    {"burningRainbowPalette", AvailablePalettes::burningrainbow},
    {"neonnights", AvailablePalettes::neonnights},
    {"neonNightsPalette", AvailablePalettes::neonnights},
    {"desertstorm", AvailablePalettes::desertstorm},
    {"desertStormPalette", AvailablePalettes::desertstorm},
    {"cosmicfire", AvailablePalettes::cosmicfire},
    {"cosmicFirePalette", AvailablePalettes::cosmicfire},
    {"alienglow", AvailablePalettes::alienglow},
    {"alienGlowPalette", AvailablePalettes::alienglow},
    {"moltenmetal", AvailablePalettes::moltenmetal},
    {"moltenMetalPalette", AvailablePalettes::moltenmetal},
    // Add more as needed
};
const int paletteNameMapSize = sizeof(paletteNameMap) / sizeof(paletteNameMap[0]);
} // anonymous namespace

LightSceneID LightShow::effectNameToId(const char* name) {
    for (int i = 0; i < effectNameMapSize; ++i) {
        if (strcasecmp(name, effectNameMap[i].name) == 0) return effectNameMap[i].id;
    }
    return LightSceneID::palette_stream;
}

const char* LightShow::effectIdToName(LightSceneID id) {
    for (int i = 0; i < effectNameMapSize; ++i) {
        if (effectNameMap[i].id == id) return effectNameMap[i].name;
    }
    return "palette_stream";
}

AvailablePalettes LightShow::paletteNameToId(const char* name) {
    for (int i = 0; i < paletteNameMapSize; ++i) {
        if (strcasecmp(name, paletteNameMap[i].name) == 0) return paletteNameMap[i].id;
    }
    return AvailablePalettes::cool;
}

const char* LightShow::paletteIdToName(AvailablePalettes id) {
    for (int i = 0; i < paletteNameMapSize; ++i) {
        if (paletteNameMap[i].id == id) return paletteNameMap[i].name;
    }
    return "cool";
}