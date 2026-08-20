#include "LightShow.h"
#include <FastLED.h>
#include "Palettes.h"
#include <algorithm>
#include <new>

LightShow::LightShow(const std::vector<CLEDController *> &led_controllers, const Clock &clock)
    : led_controllers_(led_controllers), clock_(clock), scene_changed_(false), hue_(0), frame_number_(0), scale_(0), palette_index_(0), palette_size_(0),
      current_palette_(AvailablePalettes::cool), primary_palette_(getPalette(AvailablePalettes::cool)), secondary_palette_(getPalette(AvailablePalettes::earth)), speed_(175), color_(CRGB::Red), direction_(true),
      heat_array_(nullptr), heat_array_size_(0), meteor_positions_(nullptr), meteor_trails_(nullptr), pulse_center_(0),
      explosion_center_(0)
{
    // Initialize the active scene to default settings
    memset(&active_scene_, 0, sizeof(active_scene_));
    active_scene_.scene_id = LightSceneID::off;

    // Initialize matrix drops
    memset(matrix_drops_, 0, sizeof(matrix_drops_));

    // Custom palette slots start empty; getPalette() falls back until the phone
    // uploads one.
    for (uint8_t i = 0; i < CUSTOM_PALETTE_COUNT; ++i)
    {
        custom_palettes_[i] = CRGBPalette16(CRGB::Black);
        custom_palette_filled_[i] = false;
    }

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
        &purpleOrangePalette,
        &orangePurplePalette,
        // Add other palettes as needed
    };
}

LightShow::~LightShow()
{
    // Clean up dynamically allocated memory
    if (heat_array_) delete[] heat_array_;
    if (meteor_positions_) delete[] meteor_positions_;
    if (meteor_trails_) delete[] meteor_trails_;
    if (transition_snapshot_) delete[] transition_snapshot_;
    if (transition_scratch_) delete[] transition_scratch_;
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
    case purpleorange:
        return purple_orange_palette;
    case orangepurple:
        return orange_purple_palette;
    case custom1:
    case custom2:
    case custom3:
    case custom4:
    {
        const int slot = customPaletteSlot(palette);
        if (slot >= 0 && custom_palette_filled_[slot])
        {
            return custom_palettes_[slot];
        }
        // Nothing uploaded to this slot yet. Falling back keeps an unfilled
        // slot from rendering as a strip of black.
        return cool_palette;
    }
    default:
        return vga_palette; // Default or error palette
    }
}

bool LightShow::isCustomPalette(AvailablePalettes palette)
{
    return palette >= AvailablePalettes::custom1 && palette <= AvailablePalettes::custom4;
}

int LightShow::customPaletteSlot(AvailablePalettes palette)
{
    if (!isCustomPalette(palette))
    {
        return -1;
    }
    return static_cast<int>(palette) - static_cast<int>(AvailablePalettes::custom1);
}

AvailablePalettes LightShow::customPaletteId(uint8_t slot)
{
    if (slot >= CUSTOM_PALETTE_COUNT)
    {
        slot = 0;
    }
    return static_cast<AvailablePalettes>(static_cast<int>(AvailablePalettes::custom1) + slot);
}

void LightShow::setCustomPalette(uint8_t slot, const CRGB *entries)
{
    if (slot >= CUSTOM_PALETTE_COUNT || entries == nullptr)
    {
        return;
    }
    for (uint8_t i = 0; i < CUSTOM_PALETTE_ENTRIES; ++i)
    {
        custom_palettes_[slot][i] = entries[i];
    }
    custom_palette_filled_[slot] = true;

    // A palette that is already playing has been copied into primary_palette_,
    // so refresh that too rather than waiting for the next selection.
    if (current_palette_ == customPaletteId(slot))
    {
        primary_palette_ = custom_palettes_[slot];
    }
}

void LightShow::clearCustomPalette(uint8_t slot)
{
    if (slot >= CUSTOM_PALETTE_COUNT)
    {
        return;
    }
    custom_palette_filled_[slot] = false;
    custom_palettes_[slot] = CRGBPalette16(CRGB::Black);
}

bool LightShow::isPaletteAvailable(AvailablePalettes palette) const
{
    const int slot = customPaletteSlot(palette);
    if (slot >= 0)
    {
        return custom_palette_filled_[slot];
    }
    return palette <= AvailablePalettes::orangepurple;
}

void LightShow::add_led_controller(CLEDController *led_controller)
{
    // Dither stays off for the controller's lifetime; setting it here rather
    // than on every render pass keeps the hot loop clean.
    led_controller->setDither(0);
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
    // render() gates this scene on the top-level speed; leaving it zero meant
    // palette_cycle redrew every single pass at 100% CPU.
    new_scene.speed = duration;
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
        active_scene_.scenes.palette_stream.duration = speed;
        scene_changed_ = true;
    }
}

void LightShow::apply_scene_updates(LightScene &new_scene)
{
    new_scene.brightness = active_scene_.brightness;

    if (active_scene_.scene_id != new_scene.scene_id ||
        memcmp(&active_scene_.scenes, &new_scene.scenes, sizeof(active_scene_.scenes)) != 0)
    {
        // Snapshot the outgoing frame before the new scene's setup repaints
        // the buffers, so the change dissolves instead of hard-cutting.
        begin_transition_();
        active_scene_ = new_scene;
        scene_changed_ = true;
        // Field effects start their motion from phase zero. (Speed-only
        // changes go through apply_scene_updates(uint16_t) and deliberately
        // keep the phase, so pace changes stay continuous.)
        phase_acc_ = 0;
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
        begin_transition_();
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

// ── Crossfade transitions ────────────────────────────────────────────────────

bool LightShow::ensure_transition_buffers_()
{
    size_t total = 0;
    size_t max_strip = 0;
    for (auto &controller : led_controllers_)
    {
        total += controller->size();
        if ((size_t)controller->size() > max_strip) max_strip = controller->size();
    }
    if (total == 0) return false;

    if (total != transition_total_leds_)
    {
        if (transition_snapshot_) delete[] transition_snapshot_;
        transition_snapshot_ = new (std::nothrow) CRGB[total];
        transition_total_leds_ = transition_snapshot_ ? total : 0;
    }
    if (max_strip != transition_max_strip_)
    {
        if (transition_scratch_) delete[] transition_scratch_;
        transition_scratch_ = new (std::nothrow) CRGB[max_strip];
        transition_max_strip_ = transition_scratch_ ? max_strip : 0;
    }
    return transition_snapshot_ != nullptr && transition_scratch_ != nullptr;
}

uint8_t LightShow::transition_progress_(unsigned long now) const
{
    unsigned long elapsed = now - transition_start_;
    if (transition_duration_ == 0 || elapsed >= transition_duration_) return 255;
    return ease8InOutQuad((uint8_t)((elapsed * 255) / transition_duration_));
}

void LightShow::begin_transition_()
{
    if (transition_duration_ == 0 || !ensure_transition_buffers_()) return;

    unsigned long now = clock_.now();
    size_t offset = 0;
    if (transitioning_)
    {
        // Interrupted mid-fade: freeze what is actually on the strips right
        // now (old snapshot blended with the current frame), so the new fade
        // starts from what the eye sees rather than jumping.
        uint8_t t = transition_progress_(now);
        for (auto &controller : led_controllers_)
        {
            CRGB *leds = controller->leds();
            size_t n = controller->size();
            for (size_t i = 0; i < n; i++)
            {
                transition_snapshot_[offset + i] = blend(transition_snapshot_[offset + i], leds[i], t);
            }
            offset += n;
        }
    }
    else
    {
        for (auto &controller : led_controllers_)
        {
            memcpy(transition_snapshot_ + offset, controller->leds(), controller->size() * sizeof(CRGB));
            offset += controller->size();
        }
    }
    transitioning_ = true;
    transition_start_ = now;
    last_transition_tick_ = 0;
}

void LightShow::show_now_(CLEDController *controller, uint8_t brightness)
{
    if (power_budget_ma_ > 0)
    {
        brightness = calculate_max_brightness_for_power_vmA(
            controller->leds(), controller->size(), brightness, 5, power_budget_ma_);
    }
    controller->showLeds(brightness);
}

void LightShow::show_(CLEDController *controller, uint8_t brightness)
{
    // While a transition runs the ticker owns the output; the effect has just
    // refreshed its buffer, which the next tick blends in.
    if (!transitioning_)
    {
        show_now_(controller, brightness);
    }
}

void LightShow::show_color_(CLEDController *controller, const CRGB &color, uint8_t brightness)
{
    // Mirror into the buffer even outside a transition: a later snapshot must
    // capture what is displayed, and showColor alone leaves the buffer stale.
    fill_solid(controller->leds(), controller->size(), color);
    show_(controller, brightness);
}

void LightShow::transition_tick_(unsigned long now)
{
    if (!transitioning_) return;

    if (now - transition_start_ >= transition_duration_)
    {
        transitioning_ = false;
        // Land exactly on the new effect's frame.
        for (auto &controller : led_controllers_)
        {
            show_now_(controller, brightness_);
        }
        return;
    }
    if (now - last_transition_tick_ < transition_tick_ms) return;
    last_transition_tick_ = now;

    uint8_t t = transition_progress_(now);
    size_t offset = 0;
    for (auto &controller : led_controllers_)
    {
        size_t n = controller->size();
        CRGB *leds = controller->leds();
        // Blend into the live buffer for the show, then put the effect's true
        // frame back so trails, heat maps and shift-registers stay intact.
        memcpy(transition_scratch_, leds, n * sizeof(CRGB));
        for (size_t i = 0; i < n; i++)
        {
            leds[i] = blend(transition_snapshot_[offset + i], transition_scratch_[i], t);
        }
        show_now_(controller, brightness_);
        memcpy(leds, transition_scratch_, n * sizeof(CRGB));
        offset += n;
    }
}

uint16_t LightShow::field_frame_elapsed_(unsigned long now, uint16_t duration)
{
    if (now - last_render_time_ <= field_frame_period_(duration))
    {
        return 0;
    }
    unsigned long since = now - last_render_time_;
    // First frame after a scene change (or a stall) must not lurch the phase.
    if (since > 250) since = 250;
    last_render_time_ = now;
    return (uint16_t)since;
}

unsigned long LightShow::msUntilNextFrame(unsigned long now) const
{
    if (transitioning_)
    {
        unsigned long due = last_transition_tick_ + transition_tick_ms;
        return (now >= due) ? 0 : due - now;
    }

    unsigned long period;
    switch (active_scene_.scene_id)
    {
    case LightSceneID::off:
    case LightSceneID::solid:
    case LightSceneID::setCHSV:
        period = static_scene_refresh_interval;
        break;
    case LightSceneID::strobe:
        period = current_frame_duration_;
        break;
    case LightSceneID::lightning_storm:
        period = active_scene_.scenes.lightning_storm.flash_frequency;
        break;
    // Field effects run at a fixed smooth cadence; their speed knob scales the
    // motion, not the frame period.
    case LightSceneID::pulse_wave:
    case LightSceneID::kaleidoscope:
    case LightSceneID::plasma_clouds:
    case LightSceneID::lava_lamp:
    case LightSceneID::aurora_borealis:
    case LightSceneID::color_explosion:
    case LightSceneID::spiral_galaxy:
    case LightSceneID::noise_flow:
    case LightSceneID::twinkle:
    case LightSceneID::cylon:
    case LightSceneID::fireworks:
        period = field_frame_period_(active_scene_.scenes.palette_stream.duration);
        break;
    default:
        // Every animated scene keys its frame gate off a leading uint16_t
        // duration in its union struct (palette_cycle/palette_stream gate off
        // the top-level speed, which their setters keep equal to it).
        period = active_scene_.scenes.palette_stream.duration;
        break;
    }
    unsigned long due = last_render_time_ + period;
    return (now >= due) ? 0 : due - now;
}

// Twinkle envelope (after Kriegsman's TwinkleFox): a quick rise over the first
// third of the cycle, then a slow decay - reads as a sparkle rather than a
// symmetric throb.
static uint8_t attackDecayWave8(uint8_t i)
{
    if (i < 86)
    {
        return i * 3;
    }
    i -= 86;
    return 255 - (i + (i / 2));
}

// ─────────────────────────────────────────────────────────────────────────────

void LightShow::render()
{
    unsigned long now = clock_.now();

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
                show_color_(controller, CRGB::Black, brightness_);
            }
        }
        break;

    case LightSceneID::solid:
        if (scene_changed_ || (now - last_render_time_ > static_scene_refresh_interval))
        {
            last_render_time_ = now;
            for (auto &controller : led_controllers_)
            {
                show_color_(controller, active_scene_.color, active_scene_.brightness);
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

                show_(controller, active_scene_.brightness);
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
            // One hue step per frame, shared by every strip. Advancing it
            // inside the controller loop made an 8-strip rig cycle its colours
            // 8x faster than a single-strip build at the same speed setting,
            // and the `% 255` skipped a hue - uint8_t wraps on its own.
            CRGB incoming = ColorFromPalette(current_palette, hue_);

            for (auto &controller : led_controllers_)
            {
                int last = controller->size() - 1;
                if (!direction_)
                {
                    for (int i = last; i > 0; i--)
                    {
                        controller->leds()[i] = controller->leds()[i - 1];
                    }
                    controller->leds()[0] = incoming;
                }
                else
                {

                    for (int i = 0; i < last; i++)
                    {
                        controller->leds()[i] = controller->leds()[i + 1];
                    }

                    controller->leds()[last] = incoming;
                }

                show_(controller, brightness_);
            }
            hue_++;
        }
        break;

    case LightSceneID::spectrum_cycle:
    {
        uint8_t new_hue = ((now - start_time_) / active_scene_.scenes.spectrum_cycle.duration) % 256;

        if (new_hue != hue_)
        {
            last_render_time_ = now;
            for (auto &controller : led_controllers_)
            {
                show_color_(controller, CHSV(new_hue, 255, 255), active_scene_.brightness);
            }

            hue_ = new_hue;
        }
    }
    break;

    case LightSceneID::spectrum_stream:
        if (now - last_render_time_ > active_scene_.scenes.spectrum_stream.duration)
        {
            last_render_time_ = now;
            // Advance once per frame so every strip streams the same rainbow
            // at the same rate regardless of how many strips the device has.
            CRGB incoming = CHSV(hue_, 255, 255);

            for (auto &controller : led_controllers_)
            {
                int last = controller->size() - 1;

                for (int i = 0; i < last; i++)
                {
                    controller->leds()[i] = controller->leds()[i + 1];
                }

                controller->leds()[last] = incoming;
                show_(controller, active_scene_.brightness);
            }
            hue_ += 3;
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

                show_(controller, active_scene_.brightness);
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
                        show_color_(controller, CRGB::Black, brightness_);
                    }

                    current_frame_duration_ = active_scene_.scenes.strobe.duration_off;
                }
                else
                {
                    for (auto &controller : led_controllers_)
                    {
                        show_color_(controller, CRGB(active_scene_.scenes.strobe.color.r, active_scene_.scenes.strobe.color.g, active_scene_.scenes.strobe.color.b), active_scene_.brightness);
                    }

                    current_frame_duration_ = active_scene_.scenes.strobe.duration_off;
                }
                frame_number_++;
            }
            else
            {
                for (auto &controller : led_controllers_)
                {
                    show_color_(controller, CRGB::Black, brightness_);
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

                show_(controller, active_scene_.brightness);
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
            last_render_time_ = now;
            CRGB &from_color = palette_[new_palette_index];
            CRGB &to_color = palette_[(new_palette_index + 1) % palette_size_];
            CRGB new_color = from_color.lerp8(to_color, new_scale);
            for (auto &controller : led_controllers_)
            {
                show_color_(controller, new_color, active_scene_.brightness);
            }

            scale_ = new_scale;
            palette_index_ = new_palette_index;
        }
    }
    break;
    
    case LightSceneID::setCHSV:
        // Static like solid: re-rendering an unchanged colour every pass just
        // burned the CPU and the data lines.
        if (scene_changed_ || (now - last_render_time_ > static_scene_refresh_interval))
        {
            last_render_time_ = now;
            for (auto &controller : led_controllers_)
            {
                show_color_(controller,
                            CHSV(active_scene_.scenes.setCHSV.color, active_scene_.scenes.setCHSV.saturation, active_scene_.scenes.setCHSV.luminosity),
                            active_scene_.brightness);
            }
        }
        break;

    // NEW BURNING MAN EFFECTS RENDERING - SPECTACULAR LIGHT SHOWS!
    
    case LightSceneID::pulse_wave:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.pulse_wave.duration))
        {
            advance_phase_(dt, 4, active_scene_.scenes.pulse_wave.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.pulse_wave.palette);
            uint8_t p = (uint8_t)phase_();
            size_t span = led_controllers_.empty() ? 1 : led_controllers_[0]->size();
            pulse_center_ = (phase_() / 4) % span;

            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();

                for (size_t i = 0; i < num_leds; i++)
                {
                    // Create expanding pulse waves from center
                    uint16_t distance = abs((int)i - (int)pulse_center_);
                    uint8_t wave_val = sin8(distance * active_scene_.scenes.pulse_wave.wave_width + p);
                    leds[i] = ColorFromPalette(current_palette, wave_val);
                }

                show_(controller, active_scene_.brightness);
            }
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
                
                // Update meteors. The stored position is a 0-255 phase; the
                // LED index it maps to must be wide enough for a 450-LED strip.
                for (uint8_t m = 0; m < active_scene_.scenes.meteor_shower.meteor_count; m++)
                {
                    if (meteor_positions_)
                    {
                        size_t pos = ((size_t)meteor_positions_[m] * num_leds) >> 8;
                        if (pos < num_leds)
                        {
                            leds[pos] = ColorFromPalette(current_palette, meteor_positions_[m] + hue_);
                        }
                        meteor_positions_[m] += 2; // Speed of meteors
                    }
                }
                
                show_(controller, active_scene_.brightness);
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
                
                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::kaleidoscope:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.kaleidoscope.duration))
        {
            advance_phase_(dt, 3, active_scene_.scenes.kaleidoscope.duration);
            uint8_t p = (uint8_t)phase_();
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.kaleidoscope.palette);

            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();

                // Section length is up to a whole strip, so it cannot live in
                // a byte; the guards keep a zeroed scene from dividing by zero.
                size_t mirrors = active_scene_.scenes.kaleidoscope.mirror_count;
                if (mirrors == 0) mirrors = 1;
                size_t mirror_section = num_leds / mirrors;
                if (mirror_section == 0) mirror_section = 1;

                for (size_t i = 0; i < num_leds; i++)
                {
                    // Create kaleidoscope effect with mirroring
                    size_t mirror_pos = i % mirror_section;
                    uint8_t pattern = sin8(mirror_pos * 8 + p) + cos8(mirror_pos * 4 + p * 2);
                    leds[i] = ColorFromPalette(current_palette, pattern);
                }

                show_(controller, active_scene_.brightness);
            }
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
                
                // Draw rainbow comets. comet_pos is a 0-255 phase; led_pos is a
                // real LED index and must not be truncated to a byte.
                for (uint8_t c = 0; c < active_scene_.scenes.rainbow_comet.comet_count; c++)
                {
                    uint8_t comet_pos = (hue_ + c * (256 / active_scene_.scenes.rainbow_comet.comet_count)) % 256;
                    size_t led_pos = ((size_t)comet_pos * num_leds) >> 8;

                    if (led_pos < num_leds)
                    {
                        leds[led_pos] = CHSV(comet_pos + hue_, 255, 255);

                        // Draw trail; the fade bottoms out at black instead of
                        // wrapping back to full brightness on long trails.
                        for (size_t t = 1; t < active_scene_.scenes.rainbow_comet.trail_length && led_pos >= t; t++)
                        {
                            uint8_t fade = (t * 40 > 255) ? 0 : 255 - (t * 40);
                            leds[led_pos - t] = CHSV(comet_pos + hue_, 255, fade);
                        }
                    }
                }
                
                show_(controller, active_scene_.brightness);
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
                
                // Update drops. A drop's stored value is a 0-255 phase that
                // frees its slot when it wraps back to 0.
                for (int d = 0; d < 64; d++)
                {
                    if (matrix_drops_[d] > 0)
                    {
                        size_t pos = ((size_t)matrix_drops_[d] * num_leds) >> 8;
                        if (pos < num_leds)
                        {
                            leds[pos] = CRGB(active_scene_.scenes.matrix_rain.color.r, active_scene_.scenes.matrix_rain.color.g, active_scene_.scenes.matrix_rain.color.b);
                        }
                        matrix_drops_[d] += 3;
                    }
                }
                
                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::plasma_clouds:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.plasma_clouds.duration))
        {
            advance_phase_(dt, 2, active_scene_.scenes.plasma_clouds.duration);
            uint8_t p = (uint8_t)phase_();
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.plasma_clouds.palette);

            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();

                for (size_t i = 0; i < num_leds; i++)
                {
                    // Create smooth plasma effect
                    uint8_t plasma1 = sin8((i * active_scene_.scenes.plasma_clouds.cloud_scale) + p);
                    uint8_t plasma2 = cos8((i * (active_scene_.scenes.plasma_clouds.cloud_scale / 2)) + p * 2);
                    uint8_t plasma_combined = (plasma1 + plasma2) / 2;
                    leds[i] = ColorFromPalette(current_palette, plasma_combined);
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::lava_lamp:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.lava_lamp.duration))
        {
            // Blob motion used to run off the wall clock at a fixed pace, so
            // the speed slider only changed how choppy it looked. Now it
            // actually paces the blobs.
            advance_phase_(dt, 3, active_scene_.scenes.lava_lamp.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.lava_lamp.palette);
            unsigned long time_offset = phase_();

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
                        uint8_t blob_wave = sin8(time_offset + b * 64) >> 2; // Blob phase 0-63
                        size_t blob_pos = map(blob_wave, 0, 63, 0, num_leds - 1);

                        int distance = abs((int)i - (int)blob_pos);
                        if (distance < 10) // Blob radius
                        {
                            blob_influence = std::max((int)blob_influence, 255 - (distance * 25));
                        }
                    }

                    leds[i] = ColorFromPalette(current_palette, blob_influence);
                }
                
                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::aurora_borealis:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.aurora_borealis.duration))
        {
            advance_phase_(dt, 1, active_scene_.scenes.aurora_borealis.duration);
            uint32_t p = phase_();
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.aurora_borealis.palette);

            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();

                for (size_t i = 0; i < num_leds; i++)
                {
                    // Three waves drifting at different paces, as before, but
                    // driven by the shared phase so slow speeds stay fluid.
                    uint8_t wave1 = sin8((i * 4) + (p / 5));
                    uint8_t wave2 = cos8((i * 6) + (p * 3 / 20));
                    uint8_t wave3 = sin8((i * 2) + p);

                    uint8_t aurora_intensity = (wave1 + wave2 + wave3) / 3;
                    leds[i] = ColorFromPalette(current_palette, aurora_intensity);
                }

                show_(controller, active_scene_.brightness);
            }
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
                    show_color_(controller, CRGB::White, active_scene_.scenes.lightning_storm.flash_intensity);
                }
                frame_number_ = 3; // Flash duration
            }
            else if (frame_number_ > 0)
            {
                // Continue flash
                for (auto &controller : led_controllers_)
                {
                    uint8_t fade_intensity = (active_scene_.scenes.lightning_storm.flash_intensity * frame_number_) / 3;
                    show_color_(controller, CRGB::White, fade_intensity);
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
                    
                    show_(controller, active_scene_.brightness);
                }
            }
        }
        break;

    case LightSceneID::color_explosion:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.color_explosion.duration))
        {
            // Bespoke speed curve: the shared rate/duration mapping spans 40x
            // across the slider, which made full speed a blink. This gives
            // ~0.35 LED/ms at 100% (a 450-LED strip in ~1.3 s) easing to
            // ~0.05 LED/ms at the slowest (~8 s) - dramatic, not strobing.
            // Grouped-pixel rigs scale it down further via BM_MOTION_PERCENT.
            uint32_t wave_v_fp = 25600 / (250 + (uint32_t)active_scene_.scenes.color_explosion.duration * 8);
            wave_v_fp = wave_v_fp * BM_MOTION_PERCENT / 100;
            if (wave_v_fp == 0) wave_v_fp = 1;
            phase_acc_ += (uint32_t)dt * wave_v_fp;
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.color_explosion.palette);

            // How far the shock front has travelled, in LEDs. Not wrapped: the
            // wave runs off the end of the strip once, then a fresh explosion
            // is launched below.
            size_t wave_position = phase_();
            uint8_t hue_drift = (uint8_t)(phase_() / 5);
            size_t explosion_size = active_scene_.scenes.color_explosion.explosion_size;
            if (explosion_size == 0) explosion_size = 1;
            size_t longest_strip = 0;

            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                if (num_leds > longest_strip) longest_strip = num_leds;
                CRGB *leds = controller->leds();

                for (size_t i = 0; i < num_leds; i++)
                {
                    // Distance from explosion center
                    size_t distance = abs((int)i - (int)explosion_center_);

                    uint8_t explosion_intensity = 0;
                    // Written as distance + size >= wave to stay in unsigned
                    // maths without underflowing when the wave is young.
                    if (distance <= wave_position && distance + explosion_size >= wave_position)
                    {
                        explosion_intensity = 255 - (uint8_t)((wave_position - distance) * (255 / explosion_size));
                    }

                    leds[i] = ColorFromPalette(current_palette, explosion_intensity + hue_drift);
                }

                show_(controller, active_scene_.brightness);
            }

            // Launch the next explosion once the wave has cleared the strip.
            // (The old `time_since_start % 500 == 0` check only fired if a
            // frame happened to land on an exact multiple of 500 ms, so a new
            // centre was almost never picked.)
            if (wave_position > longest_strip + explosion_size)
            {
                explosion_center_ = random(0, longest_strip ? longest_strip : 1);
                phase_acc_ = 0;
            }
        }
        break;

    case LightSceneID::spiral_galaxy:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.spiral_galaxy.duration))
        {
            advance_phase_(dt, 2, active_scene_.scenes.spiral_galaxy.duration);
            uint8_t angle = (uint8_t)phase_();
            uint8_t hue_drift = (uint8_t)(phase_() / 2);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.spiral_galaxy.palette);

            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();

                for (size_t i = 0; i < num_leds; i++)
                {
                    // Create spiral pattern
                    uint8_t spiral_position = (i * 256 / num_leds) + angle;
                    uint8_t arm_number = (i * active_scene_.scenes.spiral_galaxy.spiral_arms) / num_leds;
                    uint8_t arm_offset = arm_number * (256 / active_scene_.scenes.spiral_galaxy.spiral_arms);

                    uint8_t spiral_intensity = sin8(spiral_position + arm_offset);
                    uint8_t distance_fade = 255 - abs((int)128 - (int)((i * 256) / num_leds)); // Fade from center

                    uint8_t final_intensity = (spiral_intensity * distance_fade) >> 8;
                    leds[i] = ColorFromPalette(current_palette, final_intensity + hue_drift);
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::noise_flow:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.noise_flow.duration))
        {
            advance_phase_(dt, 4, active_scene_.scenes.noise_flow.duration);
            uint16_t flow = (uint16_t)phase_();
            // A slow palette drift keeps a calm noise field from looking parked.
            uint8_t hue_drift = (uint8_t)(phase_() >> 4);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.noise_flow.palette);
            uint8_t scale = active_scene_.scenes.noise_flow.scale;
            if (scale == 0) scale = 1;

            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();

                for (size_t i = 0; i < num_leds; i++)
                {
                    // A Perlin noise field drifting along the strip. inoise8
                    // clusters around mid-range, so stretch it or the palette's
                    // ends never show up.
                    uint8_t n = inoise8((uint16_t)(i * scale * 3), flow);
                    n = qsub8(n, 16);
                    n = qadd8(n, scale8(n, 39));
                    leds[i] = ColorFromPalette(current_palette, n + hue_drift);
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::twinkle:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.twinkle.duration))
        {
            // Temporal, not spatial: breathing pace shouldn't slow on
            // grouped-pixel rigs.
            advance_phase_(dt, 8, active_scene_.scenes.twinkle.duration, false);
            uint16_t clock16 = (uint16_t)phase_();
            uint8_t hue_drift = (uint8_t)(phase_() >> 6);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.twinkle.palette);
            // density 1-100 -> what fraction of pixels ever take part
            uint8_t threshold = (uint8_t)(((uint16_t)active_scene_.scenes.twinkle.density * 255) / 100);
            // Resting pixels hold a dim wash of the palette rather than dead
            // black, so the strip reads as embers with sparks over the top.
            CRGB background = ColorFromPalette(current_palette, hue_drift);
            background.nscale8(20);

            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();

                for (size_t i = 0; i < num_leds; i++)
                {
                    // A cheap stable per-pixel hash gives each LED its own
                    // identity: whether it twinkles, its colour, pace and phase.
                    uint16_t prng = (uint16_t)((i + 1) * 30087);
                    prng = (prng >> 8) | (prng << 8);
                    uint8_t salt = prng & 0xFF;
                    if (salt > threshold)
                    {
                        leds[i] = background;
                        continue;
                    }
                    uint8_t pace = 3 + (prng >> 13); // 3-10, per pixel
                    uint8_t cycle = (uint8_t)((clock16 * pace) >> 4) + salt;
                    // Fast rise, slow fall - a sparkle, not a throb.
                    uint8_t wave = attackDecayWave8(cycle);
                    CRGB c = ColorFromPalette(current_palette, salt + hue_drift, wave);
                    if (wave > 224)
                    {
                        // White-hot flash right at the peak makes it glint.
                        uint8_t w = (wave - 224) * 6;
                        c += CRGB(w, w, w);
                    }
                    // Never dimmer than the resting wash.
                    c |= background;
                    leds[i] = c;
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::ripple:
        if (now - last_render_time_ > active_scene_.scenes.ripple.duration)
        {
            last_render_time_ = now;
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.ripple.palette);
            uint8_t width = active_scene_.scenes.ripple.width;
            if (width == 0) width = 1;
            size_t span = led_controllers_.empty() ? 1 : led_controllers_[0]->size();

            // Now and then, drop a new stone into a free slot
            if (random8() < 20)
            {
                for (uint8_t r = 0; r < max_ripples; r++)
                {
                    if (ripple_age_[r] == 0)
                    {
                        ripple_center_[r] = random16(span);
                        ripple_age_[r] = 1;
                        ripple_hue_[r] = random8();
                        break;
                    }
                }
            }

            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                fadeToBlackBy(leds, num_leds, 60);

                for (uint8_t r = 0; r < max_ripples; r++)
                {
                    if (ripple_age_[r] == 0) continue;
                    uint16_t radius = ripple_age_[r];
                    // The ring dims as it spreads, dying as it leaves the strip
                    uint8_t energy = 255 - (uint8_t)std::min<size_t>(255, (size_t)radius * 255 / span);

                    for (uint8_t w = 0; w < width && w <= radius; w++)
                    {
                        uint8_t v = scale8(energy, 255 - (w * 255 / width));
                        CRGB c = ColorFromPalette(current_palette, ripple_hue_[r] + radius / 2, v);
                        size_t reach = radius - w;
                        size_t right = (size_t)ripple_center_[r] + reach;
                        if (right < num_leds) leds[right] += c;
                        if (ripple_center_[r] >= reach) leds[ripple_center_[r] - reach] += c;
                    }
                }

                show_(controller, active_scene_.brightness);
            }

            for (uint8_t r = 0; r < max_ripples; r++)
            {
                if (ripple_age_[r] == 0) continue;
                if (++ripple_age_[r] >= span) ripple_age_[r] = 0;
            }
        }
        break;

    case LightSceneID::cylon:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.cylon.duration))
        {
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.cylon.palette);
            uint8_t trail = active_scene_.scenes.cylon.trail;
            if (trail == 0) trail = 1;
            size_t span = led_controllers_.empty() ? 1 : led_controllers_[0]->size();

            // Time-based sweep with sub-pixel position: one end-to-end pass
            // takes 100 ms at full speed up to ~4 s at the slowest setting
            // (stretched by BM_MOTION_PERCENT on grouped-pixel rigs), and the
            // eye glides instead of stepping.
            uint32_t sweep_ms = (100 + (uint32_t)active_scene_.scenes.cylon.duration * 20) * 100 / BM_MOTION_PERCENT;
            uint32_t top = ((uint32_t)(span - 1)) << 8;
            uint32_t step = (uint64_t)top * dt / sweep_ms;
            if (cylon_dir_ > 0)
            {
                cylon_pos_ += step;
                if (cylon_pos_ >= top) { cylon_pos_ = top; cylon_dir_ = -1; }
            }
            else
            {
                if (step >= cylon_pos_) { cylon_pos_ = 0; cylon_dir_ = 1; }
                else cylon_pos_ -= step;
            }
            hue_ += 1 + (dt >> 5);
            CRGB eye = ColorFromPalette(current_palette, hue_);

            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                // Longer trail = gentler fade behind the eye
                fadeToBlackBy(leds, num_leds, 255 / trail);

                size_t i0 = std::min((size_t)(cylon_pos_ >> 8), num_leds - 1);
                uint8_t frac = cylon_pos_ & 0xFF;
                // The eye straddles two pixels by its fractional position, with
                // dim shoulders either side - a glow, not a lone pixel.
                leds[i0] += eye.scale8(255 - frac);
                if (i0 + 1 < num_leds) leds[i0 + 1] += eye.scale8(frac ? frac : 1);
                if (i0 >= 1) leds[i0 - 1] += eye.scale8(48);
                if (i0 + 2 < num_leds) leds[i0 + 2] += eye.scale8(48);

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::fireworks:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.fireworks.duration))
        {
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.fireworks.palette);
            uint8_t size = active_scene_.scenes.fireworks.size;
            if (size == 0) size = 1;
            size_t span = led_controllers_.empty() ? 1 : led_controllers_[0]->size();
            uint16_t duration = active_scene_.scenes.fireworks.duration;

            // ── advance the show (shared across strips) ──
            if (fw_stage_ == 0) // launch
            {
                // Rocket speed scales with the speed knob (and the rig's
                // motion scale, like every other physical velocity here).
                uint32_t rocket_v = ((((uint32_t)span << 8) / (200 + (uint32_t)duration * 12)) * BM_MOTION_PERCENT / 100) + 1;
                fw_pos_ += rocket_v * dt;
                if ((fw_pos_ >> 8) >= fw_apex_)
                {
                    // Burst: sparks fly from the apex, spread set by `size`.
                    fw_stage_ = 1;
                    for (uint8_t s = 0; s < max_sparks; s++)
                    {
                        spark_pos_[s] = (int32_t)((uint32_t)fw_apex_ << 8);
                        spark_vel_[s] = (int16_t)random16(0, size * 2 + 1) - size;
                        spark_heat_[s] = 200 + random8(56);
                    }
                }
            }
            else if (fw_stage_ == 1) // burst
            {
                bool alive = false;
                for (uint8_t s = 0; s < max_sparks; s++)
                {
                    if (spark_heat_[s] == 0) continue;
                    spark_pos_[s] += (int32_t)spark_vel_[s] * dt * BM_MOTION_PERCENT / 100;
                    // Air drag, then cooling with a little per-spark chaos.
                    spark_vel_[s] -= spark_vel_[s] * (int16_t)dt / 300;
                    spark_heat_[s] = qsub8(spark_heat_[s], dt / 6 + random8(3));
                    if (spark_heat_[s] > 0) alive = true;
                }
                if (!alive)
                {
                    fw_stage_ = 2;
                    fw_next_launch_ = now + 300 + random16(900);
                }
            }
            else if (now >= fw_next_launch_) // rest -> relaunch
            {
                fw_stage_ = 0;
                fw_pos_ = 0;
                fw_apex_ = span / 2 + random16(span / 2 > 10 ? span / 2 - 10 : 1);
            }

            // ── draw ──
            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                // Lingering smoke: everything decays between frames.
                fadeToBlackBy(leds, num_leds, 40);

                if (fw_stage_ == 0)
                {
                    size_t pos = std::min((size_t)(fw_pos_ >> 8), num_leds - 1);
                    // Gold rocket head with a flickering tail.
                    leds[pos] += CRGB(255, 180, 40);
                    if (pos >= 1) leds[pos - 1] += CRGB(120, 70, 10);
                    if (pos >= 2 && random8() < 90) leds[pos - 2] += CRGB(60, 30, 0);
                }
                else if (fw_stage_ == 1)
                {
                    for (uint8_t s = 0; s < max_sparks; s++)
                    {
                        if (spark_heat_[s] == 0) continue;
                        int32_t p = spark_pos_[s] >> 8;
                        if (p < 0 || (size_t)p >= num_leds) continue;
                        uint8_t heat = spark_heat_[s];
                        CRGB c = ColorFromPalette(current_palette, heat + s * 21, heat);
                        if (heat > 200)
                        {
                            // Fresh sparks flash white before taking colour.
                            uint8_t w = (heat - 200) * 4;
                            c += CRGB(w, w, w);
                        }
                        leds[p] += c;
                    }
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    default:
        break;
    }

    // Dissolve the previous scene into whatever the switch above just drew.
    transition_tick_(now);

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
        break;
    case LightSceneID::lava_lamp:
        start_time_ = now;
        break;
    case LightSceneID::aurora_borealis:
        start_time_ = now;
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
        start_time_ = now;
        break;
    case LightSceneID::noise_flow:
    case LightSceneID::twinkle:
        break;
    case LightSceneID::ripple:
        memset(ripple_age_, 0, sizeof(ripple_age_));
        break;
    case LightSceneID::cylon:
        cylon_pos_ = 0;
        cylon_dir_ = 1;
        break;
    case LightSceneID::fireworks:
        fw_stage_ = 2;
        fw_next_launch_ = 0;
        memset(spark_heat_, 0, sizeof(spark_heat_));
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

    last_render_time_ = 0;
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

    start_time_ = clock_.now();
}

void LightShow::noise_flow(uint16_t duration, uint8_t scale, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::noise_flow;
    new_scene.scenes.noise_flow.duration = duration;
    new_scene.scenes.noise_flow.scale = scale;
    new_scene.scenes.noise_flow.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::twinkle(uint16_t duration, uint8_t density, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::twinkle;
    new_scene.scenes.twinkle.duration = duration;
    new_scene.scenes.twinkle.density = density;
    new_scene.scenes.twinkle.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::ripple(uint16_t duration, uint8_t width, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::ripple;
    new_scene.scenes.ripple.duration = duration;
    new_scene.scenes.ripple.width = width;
    new_scene.scenes.ripple.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    memset(ripple_age_, 0, sizeof(ripple_age_));
    last_render_time_ = 0;
}

void LightShow::cylon(uint16_t duration, uint8_t trail, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::cylon;
    new_scene.scenes.cylon.duration = duration;
    new_scene.scenes.cylon.trail = trail;
    new_scene.scenes.cylon.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    cylon_pos_ = 0;
    cylon_dir_ = 1;
    last_render_time_ = 0;
}

void LightShow::fireworks(uint16_t duration, uint8_t size, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::fireworks;
    new_scene.scenes.fireworks.duration = duration;
    new_scene.scenes.fireworks.size = size;
    new_scene.scenes.fireworks.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    // Start in the rest stage with an expired timer: the first frame launches
    // a rocket with a fresh apex sized to the actual strip.
    fw_stage_ = 2;
    fw_next_launch_ = 0;
    memset(spark_heat_, 0, sizeof(spark_heat_));
    last_render_time_ = 0;
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
    {"noise_flow", LightSceneID::noise_flow},
    {"twinkle", LightSceneID::twinkle},
    {"ripple", LightSceneID::ripple},
    {"cylon", LightSceneID::cylon},
    {"fireworks", LightSceneID::fireworks},
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
    {"purpleorange", AvailablePalettes::purpleorange},
    {"purpleOrangePalette", AvailablePalettes::purpleorange},
    {"orangepurple", AvailablePalettes::orangepurple},
    {"orangePurplePalette", AvailablePalettes::orangepurple},
    {"custom1", AvailablePalettes::custom1},
    {"custom2", AvailablePalettes::custom2},
    {"custom3", AvailablePalettes::custom3},
    {"custom4", AvailablePalettes::custom4},
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