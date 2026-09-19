#include "LightShow.h"
#include <FastLED.h>
#include "Palettes.h"
#include "MatrixFont.h"
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
    if (order_scratch_) delete[] order_scratch_;
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
    strip_motion_pct_.push_back((uint8_t)BM_MOTION_PERCENT);
    strip_max_brightness_.push_back(255);
    cylon_pos_.push_back(0);
    cylon_dir_.push_back(1);
}

void LightShow::setPixelMap(size_t strip_index, const PanelPixel *map, uint8_t segment_count)
{
    if (pixel_maps_.size() <= strip_index)
    {
        pixel_maps_.resize(strip_index + 1);
    }
    pixel_maps_[strip_index].pixels = map;
    pixel_maps_[strip_index].segments = segment_count;
}

bool LightShow::hasPanelMap() const
{
    for (const auto &map : pixel_maps_)
    {
        if (map.pixels != nullptr)
        {
            return true;
        }
    }
    return false;
}

void LightShow::setRenderOrder(size_t strip_index, const uint16_t *order)
{
    if (pixel_maps_.size() <= strip_index)
    {
        pixel_maps_.resize(strip_index + 1);
    }
    pixel_maps_[strip_index].order = order;
    // One shared show-time scratch, sized for the largest ordered strip.
    if (order && strip_index < led_controllers_.size())
    {
        size_t n = led_controllers_[strip_index]->size();
        if (n > order_scratch_size_)
        {
            delete[] order_scratch_;
            order_scratch_ = new CRGB[n];
            order_scratch_size_ = n;
        }
    }
}

size_t LightShow::strip_index_of_(CLEDController *controller) const
{
    for (size_t s = 0; s < led_controllers_.size(); s++)
    {
        if (led_controllers_[s] == controller)
        {
            return s;
        }
    }
    return led_controllers_.size();
}

uint8_t LightShow::strip_brightness_(size_t strip_index, uint8_t brightness) const
{
    if (strip_index >= strip_max_brightness_.size())
    {
        return brightness;
    }
    uint8_t cap = strip_max_brightness_[strip_index];
    return cap < 255 ? scale8(brightness, cap) : brightness;
}

PanelPixel LightShow::panel_pixel_(size_t strip_index, size_t i, size_t num_leds, uint8_t &segment_count) const
{
    if (strip_index < pixel_maps_.size() && pixel_maps_[strip_index].pixels != nullptr)
    {
        const StripMap &m = pixel_maps_[strip_index];
        segment_count = m.segments ? m.segments : 1;
        // The map is indexed by physical LED; with a render order set the
        // caller's i is a logical position, so translate before looking up.
        return m.pixels[m.order != nullptr && i < num_leds ? m.order[i] : i];
    }
    // No map: pretend the strip is a straight run of equal segments, each
    // "vertex" at its centre, so the chevron effects stay watchable anywhere.
    segment_count = fallback_segments_;
    PanelPixel px;
    px.x = (uint8_t)(num_leds > 1 ? i * 255 / (num_leds - 1) : 0);
    px.y = 128;
    size_t seg_len = num_leds / fallback_segments_;
    if (seg_len < 1) seg_len = 1;
    size_t seg = i / seg_len;
    if (seg >= fallback_segments_) seg = fallback_segments_ - 1;
    px.seg = (uint8_t)seg;
    size_t pos = i - seg * seg_len;
    size_t half = seg_len / 2;
    size_t d = (pos > half) ? pos - half : half - pos;
    px.vdist = (uint8_t)(half ? std::min<size_t>(255, d * 255 / half) : 255);
    return px;
}

void LightShow::setMatrixGrid(size_t strip_index, uint8_t width, uint8_t height, const uint16_t *grid)
{
    if (pixel_maps_.size() <= strip_index)
    {
        pixel_maps_.resize(strip_index + 1);
    }
    pixel_maps_[strip_index].grid = grid;
    pixel_maps_[strip_index].grid_w = width;
    pixel_maps_[strip_index].grid_h = height;
}

bool LightShow::hasMatrixGrid() const
{
    for (const auto &map : pixel_maps_)
    {
        if (map.grid != nullptr)
        {
            return true;
        }
    }
    return false;
}

void LightShow::renderMatrixTestCard(uint8_t brightness)
{
    for (size_t s = 0; s < led_controllers_.size(); s++)
    {
        CLEDController *ctrl = led_controllers_[s];
        CRGB *leds = ctrl->leds();
        fill_solid(leds, ctrl->size(), CRGB(2, 2, 4));
        const StripMap *m = s < pixel_maps_.size() ? &pixel_maps_[s] : nullptr;
        if (m && m->grid)
        {
            size_t n = ctrl->size();
            // The grid holds LOGICAL positions; this direct-show path skips
            // the render-order permutation, so resolve it here.
            const uint16_t *ord = m->order;
            auto phys = [&](uint16_t logical) -> uint16_t {
                return (ord && logical < n) ? ord[logical] : logical;
            };
            for (uint8_t c = 0; c < m->grid_w; c++)
            {
                uint16_t p = phys(m->grid[c]); // row 0 = the top edge
                if (p < n) leds[p] = CRGB(255, 0, 0);
            }
            for (uint8_t r = 0; r < m->grid_h; r++)
            {
                uint16_t p = phys(m->grid[r * m->grid_w]); // column 0 = the left edge
                if (p < n) leds[p] = CRGB(0, 255, 0);
            }
            // A white 'F' at rows 1-7, columns 3-7: asymmetric on both axes,
            // so any mirror or shred is unmistakable.
            const uint8_t *glyph = MATRIX_FONT['F' - MATRIX_FONT_FIRST_CHAR];
            for (uint8_t k = 0; k < MATRIX_FONT_WIDTH; k++)
            {
                for (uint8_t r = 0; r < MATRIX_FONT_HEIGHT && r + 1 < m->grid_h; r++)
                {
                    if (glyph[k] & (1 << r))
                    {
                        uint16_t p = phys(m->grid[(r + 1) * m->grid_w + 3 + k]);
                        if (p < n) leds[p] = CRGB(255, 255, 255);
                    }
                }
            }
        }
        ctrl->showLeds(strip_brightness_(s, brightness));
    }
}

uint8_t LightShow::matrixGridWidth() const
{
    for (const auto &map : pixel_maps_)
    {
        if (map.grid != nullptr)
        {
            return map.grid_w;
        }
    }
    return 0;
}

uint8_t LightShow::matrixGridHeight() const
{
    for (const auto &map : pixel_maps_)
    {
        if (map.grid != nullptr)
        {
            return map.grid_h;
        }
    }
    return 0;
}

uint8_t LightShow::text_col_bits_(const char *text, size_t len, int32_t tc) const
{
    auto raw = [&](int32_t col) -> uint8_t {
        if (col < 0 || col >= (int32_t)len * MATRIX_FONT_ADVANCE)
        {
            return 0;
        }
        char ch = text[col / MATRIX_FONT_ADVANCE];
        if (ch < MATRIX_FONT_FIRST_CHAR || ch > MATRIX_FONT_LAST_CHAR)
        {
            ch = '?';
        }
        uint8_t k = (uint8_t)(col % MATRIX_FONT_ADVANCE);
        return k < MATRIX_FONT_WIDTH ? MATRIX_FONT[ch - MATRIX_FONT_FIRST_CHAR][k] : 0;
    };
    uint8_t bits = raw(tc);
    // Bold doubles every stroke one column to the right, but never into the
    // spacer column, so letters keep their 1-pixel gap.
    if (text_style_ == 1 && (tc % MATRIX_FONT_ADVANCE) != MATRIX_FONT_ADVANCE - 1)
    {
        bits |= raw(tc - 1);
    }
    return bits;
}

void LightShow::setMarqueeText(const char *text)
{
    if (text == nullptr)
    {
        marquee_text_[0] = '\0';
        return;
    }
    strncpy(marquee_text_, text, MARQUEE_TEXT_MAX);
    marquee_text_[MARQUEE_TEXT_MAX] = '\0';
}

void LightShow::setMatrixBitmap(uint8_t w, uint8_t h, const uint8_t *palette_rgb, const uint8_t *pixels)
{
    if (w == 0 || h == 0 || (size_t)w * h > MATRIX_BITMAP_MAX_PIXELS ||
        palette_rgb == nullptr || pixels == nullptr)
    {
        return;
    }
    for (int k = 0; k < MATRIX_BITMAP_COLORS; k++)
    {
        bitmap_palette_[k] = CRGB(palette_rgb[k * 3], palette_rgb[k * 3 + 1], palette_rgb[k * 3 + 2]);
    }
    memcpy(bitmap_pixels_, pixels, ((size_t)w * h + 1) / 2);
    bitmap_w_ = w;
    bitmap_h_ = h;
    bitmap_set_ = true;
    // If the bitmap is on screen right now, repaint it with the new pixels.
    requestRepaint();
}

void LightShow::clearMatrixBitmap()
{
    bitmap_set_ = false;
    bitmap_w_ = 0;
    bitmap_h_ = 0;
    requestRepaint();
}

void LightShow::setAnimFrame(uint8_t index, uint8_t w, uint8_t h, const uint8_t *palette_rgb, const uint8_t *pixels)
{
    if (index >= MATRIX_ANIM_MAX_FRAMES || w == 0 || h == 0 ||
        (size_t)w * h > MATRIX_BITMAP_MAX_PIXELS || palette_rgb == nullptr || pixels == nullptr)
    {
        return;
    }
    AnimFrame &f = anim_frames_[index];
    for (int k = 0; k < MATRIX_BITMAP_COLORS; k++)
    {
        f.palette[k] = CRGB(palette_rgb[k * 3], palette_rgb[k * 3 + 1], palette_rgb[k * 3 + 2]);
    }
    memcpy(f.pixels, pixels, ((size_t)w * h + 1) / 2);
    f.w = w;
    f.h = h;
    f.set = true;
    requestRepaint();
}

void LightShow::clearAnimFrames()
{
    for (uint8_t k = 0; k < MATRIX_ANIM_MAX_FRAMES; k++)
    {
        anim_frames_[k].set = false;
    }
    requestRepaint();
}

uint8_t LightShow::animFrameCount() const
{
    uint8_t n = 0;
    while (n < MATRIX_ANIM_MAX_FRAMES && anim_frames_[n].set)
    {
        n++;
    }
    return n;
}

CRGB LightShow::text_fill_color_(const CRGBPalette16 &pal, uint8_t r, uint8_t c,
                                 uint8_t grid_h, uint32_t ph, uint8_t base_index) const
{
    // Height as the fills think of it: 255 at the top row, 0 at the bottom.
    uint8_t denom = grid_h > 1 ? (uint8_t)(grid_h - 1) : 1;
    uint8_t yy = (uint8_t)(255 - ((uint16_t)r * 255) / denom);
    switch (text_fill_)
    {
    case 1: // fire - panel_fire's rising field sampled at the glyph cell
    {
        uint16_t rise = (uint16_t)ph * 6;
        uint8_t n = inoise8((uint16_t)c * 44, (uint16_t)((uint16_t)yy * 4 - rise), (uint16_t)(ph >> 3));
        n = qadd8(qsub8(n, 24), scale8(n, 60));
        uint8_t heat = qsub8(n, scale8(yy, 150));
        return ColorFromPalette(pal, scale8(heat, 250));
    }
    case 2: // rain streaks falling through the letters
    {
        uint16_t fall = (uint16_t)ph;
        uint16_t prng = (uint16_t)((c + 1) * 40503);
        prng = (uint16_t)((prng >> 8) | (prng << 8));
        uint8_t salt = (uint8_t)prng;
        uint16_t lane = (uint16_t)((fall * (192 + (salt & 63))) >> 8) + salt;
        uint8_t head = (uint8_t)(255 - (lane & 0xFF));
        uint8_t d = (uint8_t)(yy - head);
        uint8_t v = 0;
        if (d < 112)
        {
            v = (uint8_t)(255 - d * 2);
            v = scale8(v, v);
        }
        // The letters stay legible between drops: a dim base under the
        // streaks rather than black gaps in the glyphs.
        if (v < 40) v = 40;
        CRGB col = ColorFromPalette(pal, (uint8_t)(salt + (uint8_t)(ph >> 5)), v);
        if (d < 16) col += CRGB(60, 60, 60);
        return col;
    }
    case 3: // plasma
    {
        uint8_t n = inoise8((uint16_t)c * 28, (uint16_t)r * 52, (uint16_t)(ph >> 1));
        return ColorFromPalette(pal, (uint8_t)(n + (uint8_t)(ph >> 4)));
    }
    default:
        return ColorFromPalette(pal, base_index);
    }
}

void LightShow::setStripGroupSize(size_t strip_index, uint8_t leds_per_pixel)
{
    if (strip_index >= strip_motion_pct_.size())
    {
        return;
    }
    if (leds_per_pixel < 1) leds_per_pixel = 1;
    // 6-LED glow strip -> 16%: a pattern crosses the same metres per second
    // it would on a dense strip.
    strip_motion_pct_[strip_index] = (uint8_t)(100 / leds_per_pixel);
}

void LightShow::setStripMaxBrightness(size_t strip_index, uint8_t max_brightness)
{
    if (strip_index >= strip_max_brightness_.size())
    {
        return;
    }
    // 0 would black the strand out entirely; the floor keeps a bad write
    // visible instead of indistinguishable from a dead strip.
    strip_max_brightness_[strip_index] = max_brightness < 1 ? 1 : max_brightness;
}

uint8_t LightShow::device_motion_pct_() const
{
    uint8_t slowest = (uint8_t)BM_MOTION_PERCENT;
    for (uint8_t pct : strip_motion_pct_)
    {
        if (pct < slowest) slowest = pct;
    }
    return slowest;
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
    size_t strip_index = strip_index_of_(controller);
    // While the matrix display overlay is on, it owns the grid strip: the
    // effect rendered the strip as usual, and the display frame replaces the
    // pixels on the way out. Painting here - the one funnel every show goes
    // through - means every effect, transition tick and repaint shows the
    // content without knowing it exists. The effect's buffer is clobbered on
    // this strip only, which is invisible (the overlay repaints every frame)
    // and self-heals the moment the display turns off.
    if (matrix_overlay_active_() && strip_index < pixel_maps_.size() &&
        pixel_maps_[strip_index].grid != nullptr)
    {
        matrix_overlay_paint_(pixel_maps_[strip_index], controller->leds(),
                              controller->size());
        matrix_last_show_ = clock_.now();
    }
    // The per-strip ceiling scales first, so the power maths below sees the
    // level actually going out.
    brightness = strip_brightness_(strip_index, brightness);
    if (power_budget_ma_ > 0)
    {
        brightness = calculate_max_brightness_for_power_vmA(
            controller->leds(), controller->size(), brightness, 5, power_budget_ma_);
    }
    const uint16_t *order =
        strip_index < pixel_maps_.size() ? pixel_maps_[strip_index].order : nullptr;
    if (order && order_scratch_)
    {
        // Everything upstream (effects, transitions, snapshots) lives in
        // logical panel order; the wiring order exists only for the instant
        // the pixels go out, then the logical frame is put back.
        size_t n = controller->size();
        CRGB *leds = controller->leds();
        memcpy(order_scratch_, leds, n * sizeof(CRGB));
        for (size_t k = 0; k < n; k++)
        {
            leds[order[k]] = order_scratch_[k];
        }
        controller->showLeds(brightness);
        memcpy(leds, order_scratch_, n * sizeof(CRGB));
        return;
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

// --------------------------------------------------------------------------
// The matrix display overlay: marquee / word zoom / bitmap / animation on the
// grid strip(s) only, on its own clock. The rest of the rig keeps playing the
// active effect - the display is content, not a mode.

void LightShow::setMatrixDisplay(uint8_t mode)
{
    if (mode > MATRIX_DISPLAY_MAX)
    {
        mode = MATRIX_DISPLAY_OFF;
    }
    if (mode == matrix_display_mode_)
    {
        return;
    }
    matrix_display_mode_ = mode;
    // Content starts from the top: text enters the edge, animations at
    // frame 0.
    matrix_acc_ = 0;
    matrix_last_tick_ = clock_.now();
    // Turning the display off hands the strip back to the effect; a static
    // scene would otherwise leave the last frame stuck for its whole refresh
    // interval. A repaint also shows new content immediately on turn-on.
    requestRepaint();
}

void LightShow::setMatrixSpeed(uint16_t ms)
{
    if (ms < MATRIX_SPEED_MIN_MS) ms = MATRIX_SPEED_MIN_MS;
    if (ms > MATRIX_SPEED_MAX_MS) ms = MATRIX_SPEED_MAX_MS;
    matrix_speed_ms_ = ms;
}

void LightShow::setMatrixStyle(AvailablePalettes palette, bool forward)
{
    matrix_palette_ = palette;
    matrix_dir_ = forward;
}

bool LightShow::matrix_overlay_content_() const
{
    switch (matrix_display_mode_)
    {
    case MATRIX_DISPLAY_TEXT:
    case MATRIX_DISPLAY_WORDS:
        return marquee_text_[0] != '\0';
    case MATRIX_DISPLAY_BITMAP:
        return bitmap_set_;
    case MATRIX_DISPLAY_ANIM:
        return animFrameCount() > 0;
    default:
        return false;
    }
}

bool LightShow::matrix_overlay_active_() const
{
    // No mode, no content behind it, no grid to draw on, or the show is off:
    // the overlay simply doesn't engage, and the effect owns every strip.
    return matrix_display_mode_ != MATRIX_DISPLAY_OFF &&
           active_scene_.scene_id != LightSceneID::off &&
           matrix_overlay_content_() && hasMatrixGrid();
}

void LightShow::matrix_overlay_tick_(unsigned long now)
{
    if (!matrix_overlay_active_())
    {
        // Pinning the tick while idle means re-activation doesn't lurch the
        // phase by however long the display sat dark.
        matrix_last_tick_ = now;
        return;
    }

    unsigned long since = now - matrix_last_tick_;
    if (since == 0)
    {
        return;
    }
    if (since > 250) since = 250; // a stall must not fast-forward the content
    matrix_last_tick_ = now;

    // Advance the overlay's own Q8.8 phase against matrix_speed_ms_ - the
    // same maths as advance_phase_, but on the display's clock, never the
    // effect duration. Rates match what the old scene renderers used with
    // the knob at ~100, except the animation's 256: that makes the frame
    // period exactly matrix_speed_ms_, so a GIF's native pace survives.
    uint16_t rate;
    switch (matrix_display_mode_)
    {
    case MATRIX_DISPLAY_TEXT: rate = 24; break;
    case MATRIX_DISPLAY_WORDS: rate = 32; break;
    case MATRIX_DISPLAY_ANIM: rate = 256; break;
    default: rate = 0; break; // the bitmap doesn't move
    }
    uint16_t ms = matrix_speed_ms_ ? matrix_speed_ms_ : 1;
    matrix_acc_ += (uint32_t)since * rate * 256 / ms;

    // Push a frame to the panel only when the running scene isn't already
    // repainting it fast enough - a field effect at its 16-33 ms cadence
    // funnels through show_now_, which paints the overlay and stamps
    // matrix_last_show_, so this show only fires under slow and static
    // scenes. While a transition runs its ticker repaints every strip on
    // its own fast tick; stay out of its way.
    if (transitioning_)
    {
        return;
    }
    unsigned long need = matrix_display_mode_ == MATRIX_DISPLAY_BITMAP ? 500 : 33;
    if (now - matrix_last_show_ < need)
    {
        return;
    }
    for (size_t s = 0; s < led_controllers_.size(); s++)
    {
        if (s < pixel_maps_.size() && pixel_maps_[s].grid != nullptr)
        {
            show_now_(led_controllers_[s], active_scene_.brightness);
        }
    }
}

void LightShow::matrix_overlay_paint_(const StripMap &m, CRGB *leds, size_t n)
{
    switch (matrix_display_mode_)
    {
    case MATRIX_DISPLAY_TEXT:
        paint_marquee_(m, leds, n);
        break;
    case MATRIX_DISPLAY_WORDS:
        paint_word_zoom_(m, leds, n);
        break;
    case MATRIX_DISPLAY_BITMAP:
        paint_grid_bitmap_(m, leds, n, bitmap_w_, bitmap_h_, bitmap_palette_,
                           bitmap_pixels_);
        break;
    case MATRIX_DISPLAY_ANIM:
    {
        uint8_t count = animFrameCount();
        if (count == 0)
        {
            return;
        }
        // One frame per 256 phase units; rate 256 above makes that exactly
        // matrix_speed_ms_ per frame.
        const AnimFrame &f = anim_frames_[(uint8_t)((matrix_phase_() >> 8) % count)];
        paint_grid_bitmap_(m, leds, n, f.w, f.h, f.palette, f.pixels);
        break;
    }
    default:
        break;
    }
}

void LightShow::paint_marquee_(const StripMap &m, CRGB *leds, size_t n)
{
    size_t len = strlen(marquee_text_);
    if (len == 0 || m.grid_w == 0)
    {
        return;
    }
    CRGBPalette16 current_palette = getPalette(matrix_palette_);
    uint32_t ph = matrix_phase_();
    int32_t text_cols = (int32_t)len * MATRIX_FONT_ADVANCE;

    int32_t cycle = text_cols + m.grid_w;
    int32_t pos = (int32_t)((ph >> 3) % (uint32_t)cycle);
    // Forward: text enters the right edge and marches left. Reverse: the
    // window walks the text backwards, so it enters left and marches right,
    // still readable.
    int32_t w0 = matrix_dir_ ? pos - m.grid_w : text_cols - pos;
    fill_solid(leds, n, CRGB::Black);
    for (uint8_t c = 0; c < m.grid_w; c++)
    {
        int32_t tc = w0 + c;
        if (tc < 0 || tc >= text_cols)
        {
            continue;
        }
        uint8_t bits = text_col_bits_(marquee_text_, len, tc);
        if (!bits)
        {
            continue;
        }
        // Fill 0: the palette gradient slides along the text with it. Other
        // fills paint each glyph pixel from a live field (fire/rain/plasma)
        // - letters made of it.
        uint8_t base_index = (uint8_t)((uint8_t)(tc * 3) + (uint8_t)(ph >> 4));
        CRGB col = ColorFromPalette(current_palette, base_index);
        for (uint8_t r = 0; r < MATRIX_FONT_HEIGHT && r < m.grid_h; r++)
        {
            if (bits & (1 << r))
            {
                uint16_t p = m.grid[r * m.grid_w + c];
                if (p < n)
                {
                    leds[p] = text_fill_ == 0
                                  ? col
                                  : text_fill_color_(current_palette, r, c,
                                                     m.grid_h, ph, base_index);
                }
            }
        }
    }
}

void LightShow::paint_word_zoom_(const StripMap &m, CRGB *leds, size_t n)
{
    CRGBPalette16 current_palette = getPalette(matrix_palette_);
    bool fly_in = matrix_dir_;
    uint32_t ph = matrix_phase_();
    uint8_t hue_drift = (uint8_t)(ph >> 6);

    // The text is tiny; count its words every frame.
    uint8_t word_count = 0;
    {
        bool in_word = false;
        for (const char *p = marquee_text_; *p; p++)
        {
            if (*p != ' ' && !in_word)
            {
                word_count++;
                in_word = true;
            }
            else if (*p == ' ')
            {
                in_word = false;
            }
        }
    }
    if (word_count == 0)
    {
        return;
    }

    // One word per 512 phase units: it grows out of the centre toward the
    // viewer (accelerating, like an approach), then a short dark beat
    // separates it from the next word.
    const uint32_t word_cycle = 512;
    uint8_t widx = (uint8_t)((ph / word_cycle) % word_count);
    uint16_t t = (uint16_t)(ph % word_cycle);

    // Find word widx.
    const char *wstart = nullptr;
    size_t wlen = 0;
    {
        uint8_t seen = 0;
        bool in_word = false;
        for (const char *p = marquee_text_;; p++)
        {
            if (*p && *p != ' ')
            {
                if (!in_word)
                {
                    in_word = true;
                    if (seen == widx) wstart = p;
                }
            }
            else
            {
                if (in_word)
                {
                    if (seen == widx && wstart)
                    {
                        wlen = (size_t)(p - wstart);
                        break;
                    }
                    seen++;
                    in_word = false;
                }
                if (!*p) break;
            }
        }
    }

    fill_solid(leds, n, CRGB::Black);
    if (wstart && wlen > 0 && t < 472)
    {
        if (!fly_in)
        {
            t = (uint16_t)(471 - t); // reversed: the word recedes
        }
        int32_t word_cols = (int32_t)wlen * MATRIX_FONT_ADVANCE - 1;
        // Quadratic ease on the scale reads as acceleration; brightness
        // rides along so near = bright.
        uint32_t e = ((uint32_t)t * t) / 472; // 0..471
        uint16_t s_min = 64;                  // 0.25x, Q8
        uint16_t s_max = (uint16_t)std::min<int32_t>(
            320, (30 * 256) / (word_cols ? word_cols : 1));
        if (s_max < s_min) s_max = s_min;
        uint16_t scale = (uint16_t)(s_min + (uint32_t)(s_max - s_min) * e / 471);
        int32_t inv = (int32_t)(65536 / scale); // Q8 inverse
        uint8_t bri = (uint8_t)(70 + (uint32_t)185 * e / 471);
        CRGB col = ColorFromPalette(current_palette,
                                    (uint8_t)(widx * 37) + hue_drift);
        col.nscale8(bri);

        int32_t cx = (int32_t)m.grid_w * 128; // panel centre, Q8
        int32_t cy = (int32_t)m.grid_h * 128;
        int32_t gx0 = word_cols * 128;        // word centre, Q8
        int32_t gy0 = 7 * 128;                // glyph rows 0..6

        for (uint8_t r = 0; r < m.grid_h; r++)
        {
            int32_t gy_q8 = ((((int32_t)r * 256 + 128 - cy) * inv) >> 8) + gy0;
            if (gy_q8 < 0) continue;
            int32_t gy = gy_q8 >> 8;
            if (gy > 6) continue;
            for (uint8_t c = 0; c < m.grid_w; c++)
            {
                int32_t gx_q8 = ((((int32_t)c * 256 + 128 - cx) * inv) >> 8) + gx0;
                if (gx_q8 < 0) continue;
                uint8_t bits = text_col_bits_(wstart, wlen, gx_q8 >> 8);
                if (bits & (1 << gy))
                {
                    uint16_t p = m.grid[r * m.grid_w + c];
                    if (p < n)
                    {
                        if (text_fill_ == 0)
                        {
                            leds[p] = col;
                        }
                        else
                        {
                            // The fill field paints the word; the approach
                            // still brightens it.
                            CRGB fc = text_fill_color_(
                                current_palette, r, c, m.grid_h, ph,
                                (uint8_t)((uint8_t)(widx * 37) + hue_drift));
                            fc.nscale8(bri);
                            leds[p] = fc;
                        }
                    }
                }
            }
        }
    }
}

void LightShow::paint_grid_bitmap_(const StripMap &m, CRGB *leds, size_t n,
                                   uint8_t w, uint8_t h, const CRGB *palette,
                                   const uint8_t *pixels)
{
    if (w == 0 || h == 0)
    {
        return;
    }
    fill_solid(leds, n, CRGB::Black);
    uint8_t rows = h < m.grid_h ? h : m.grid_h;
    uint8_t cols = w < m.grid_w ? w : m.grid_w;
    for (uint8_t r = 0; r < rows; r++)
    {
        for (uint8_t c = 0; c < cols; c++)
        {
            size_t pi = (size_t)r * w + c;
            uint8_t packed = pixels[pi >> 1];
            uint8_t idx = (pi & 1) ? (packed & 0x0F) : (packed >> 4);
            uint16_t p = m.grid[r * m.grid_w + c];
            if (p < n)
            {
                leds[p] = palette[idx];
            }
        }
    }
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
    case LightSceneID::chevron_wave:
    case LightSceneID::chevron_chase:
    case LightSceneID::chevron_burst:
    case LightSceneID::chevron_glow:
    case LightSceneID::chevron_eq:
    case LightSceneID::confetti:
    case LightSceneID::juggle:
    case LightSceneID::sinelon:
    case LightSceneID::bpm:
    case LightSceneID::pacifica:
    case LightSceneID::panel_waves:
    case LightSceneID::panel_fire:
    case LightSceneID::panel_rain:
    case LightSceneID::panel_spin:
    case LightSceneID::starfield:
    case LightSceneID::orbit_comet:
    case LightSceneID::panel_puddle:
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

// Cheap 8-bit atan2 for panel_spin: linear inside each octant (a few degrees
// off at worst, invisible when painting spokes). 0 = +x, a full turn = 256.
static uint8_t angle8_(int16_t dy, int16_t dx)
{
    if (dx == 0 && dy == 0)
    {
        return 0;
    }
    uint16_t ax = (uint16_t)(dx < 0 ? -dx : dx);
    uint16_t ay = (uint16_t)(dy < 0 ? -dy : dy);
    uint8_t a = (ax >= ay) ? (uint8_t)((ay * 32) / ax)
                           : (uint8_t)(64 - (ax * 32) / ay);
    if (dx < 0) a = (uint8_t)(128 - a);
    if (dy < 0) a = (uint8_t)(0 - a);
    return a;
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

// Pacifica (after Mark Kriegsman's December 2019 "Pacifica" for FastLED).
// The classic runs off GET_MILLIS(); these beat helpers take the show's own
// speed-scaled milliseconds instead, so the speed slider stretches the whole
// ocean and grouped-pixel strands slow it (one shared clock, paced to the
// slowest strip).
static uint16_t pacBeat88(uint16_t bpm88, uint32_t ms)
{
    return ((uint64_t)ms * bpm88 * 280) >> 16;
}
static uint16_t pacBeat16(uint16_t bpm, uint32_t ms)
{
    return pacBeat88(bpm << 8, ms);
}
static uint8_t pacBeat8(uint16_t bpm, uint32_t ms)
{
    return pacBeat16(bpm, ms) >> 8;
}
static uint16_t pacBeatsin16(uint16_t bpm, uint16_t lo, uint16_t hi, uint32_t ms)
{
    uint16_t wave = (uint16_t)(sin16(pacBeat16(bpm, ms)) + 32768);
    return lo + scale16(wave, hi - lo);
}
static uint16_t pacBeatsin88(uint16_t bpm88, uint16_t lo, uint16_t hi, uint32_t ms)
{
    uint16_t wave = (uint16_t)(sin16(pacBeat88(bpm88, ms)) + 32768);
    return lo + scale16(wave, hi - lo);
}
static uint8_t pacBeatsin8(uint16_t bpm, uint8_t lo, uint8_t hi, uint32_t ms)
{
    return lo + scale8(sin8(pacBeat8(bpm, ms)), hi - lo);
}

// The ocean's own colours - deliberately not the user palette. Layer 1 and 2
// crest into green-cyan, layer 3 is the bright blue body of the water.
static const CRGBPalette16 pacifica_palette_1 =
    {0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
     0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x14554B, 0x28AA50};
static const CRGBPalette16 pacifica_palette_2 =
    {0x000507, 0x000409, 0x00030B, 0x00030D, 0x000210, 0x000212, 0x000114, 0x000117,
     0x000019, 0x00001C, 0x000026, 0x000031, 0x00003B, 0x000046, 0x0C5F52, 0x19BE5F};
static const CRGBPalette16 pacifica_palette_3 =
    {0x000208, 0x00030E, 0x000514, 0x00061A, 0x000820, 0x000927, 0x000B2D, 0x000C33,
     0x000E39, 0x001040, 0x001450, 0x001860, 0x001C70, 0x002080, 0x1040BF, 0x2060FF};

/// One layer of moving water: a sine-warped walk along the palette, added into
/// what the previous layers left.
static void pacificaLayer(CRGB *leds, size_t num_leds, const CRGBPalette16 &palette,
                          uint16_t cistart, uint16_t wavescale, uint8_t bri, uint16_t ioff)
{
    uint16_t ci = cistart;
    uint16_t waveangle = ioff;
    uint16_t wavescale_half = (wavescale / 2) + 20;
    for (size_t i = 0; i < num_leds; i++)
    {
        waveangle += 250;
        uint16_t s16 = sin16(waveangle) + 32768;
        uint16_t cs = scale16(s16, wavescale_half) + wavescale_half;
        ci += cs;
        uint16_t sindex16 = sin16(ci) + 32768;
        uint8_t sindex8 = scale16(sindex16, 240);
        leds[i] += ColorFromPalette(palette, sindex8, bri, LINEARBLEND);
    }
}

// ─────────────────────────────────────────────────────────────────────────────

void LightShow::render()
{
    unsigned long now = clock_.now();

    // The matrix display overlay runs on its own clock, independent of
    // whatever scene the switch below is pacing.
    matrix_overlay_tick_(now);

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

            for (size_t si = 0; si < led_controllers_.size(); si++)
            {
                CLEDController *controller = led_controllers_[si];
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                // Per strip: a grouped strand reads a slowed phase, and the
                // centre walks its own strip rather than strip 0's.
                uint32_t sph = strip_phase_(si);
                uint8_t p = (uint8_t)sph;
                size_t center = (sph / 4) % num_leds;

                for (size_t i = 0; i < num_leds; i++)
                {
                    // Create expanding pulse waves from center
                    uint16_t distance = abs((int)i - (int)center);
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
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.kaleidoscope.palette);

            for (size_t si = 0; si < led_controllers_.size(); si++)
            {
                CLEDController *controller = led_controllers_[si];
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint8_t p = (uint8_t)strip_phase_(si);

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
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.plasma_clouds.palette);

            for (size_t si = 0; si < led_controllers_.size(); si++)
            {
                CLEDController *controller = led_controllers_[si];
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint8_t p = (uint8_t)strip_phase_(si);

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
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.aurora_borealis.palette);

            for (size_t si = 0; si < led_controllers_.size(); si++)
            {
                CLEDController *controller = led_controllers_[si];
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint32_t p = strip_phase_(si);

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
            // Grouped-pixel strands scale it down further; the front is one
            // shared position, so it paces to the slowest strip.
            uint32_t wave_v_fp = 25600 / (250 + (uint32_t)active_scene_.scenes.color_explosion.duration * 8);
            wave_v_fp = wave_v_fp * device_motion_pct_() / 100;
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
            uint8_t hue_drift = (uint8_t)(phase_() / 2);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.spiral_galaxy.palette);

            for (size_t si = 0; si < led_controllers_.size(); si++)
            {
                CLEDController *controller = led_controllers_[si];
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint8_t angle = (uint8_t)strip_phase_(si);

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
            // A slow palette drift keeps a calm noise field from looking parked.
            uint8_t hue_drift = (uint8_t)(phase_() >> 4);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.noise_flow.palette);
            uint8_t scale = active_scene_.scenes.noise_flow.scale;
            if (scale == 0) scale = 1;

            for (size_t si = 0; si < led_controllers_.size(); si++)
            {
                CLEDController *controller = led_controllers_[si];
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint16_t flow = (uint16_t)strip_phase_(si);

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
            advance_phase_(dt, 8, active_scene_.scenes.twinkle.duration);
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
            hue_ += 1 + (dt >> 5);
            CRGB eye = ColorFromPalette(current_palette, hue_);

            for (size_t si = 0; si < led_controllers_.size(); si++)
            {
                CLEDController *controller = led_controllers_[si];
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                // Longer trail = gentler fade behind the eye
                fadeToBlackBy(leds, num_leds, 255 / trail);

                // Time-based sweep with sub-pixel position, per strip: one
                // end-to-end pass takes 100 ms at full speed up to ~4 s at the
                // slowest setting - stretched by the strip's grouping, and
                // each strip turns around at its own ends.
                uint32_t sweep_ms = (100 + (uint32_t)active_scene_.scenes.cylon.duration * 20) * 100 / strip_motion_pct_of_(si);
                uint32_t top = ((uint32_t)(num_leds - 1)) << 8;
                uint32_t step = (uint64_t)top * dt / sweep_ms;
                if (cylon_dir_[si] > 0)
                {
                    cylon_pos_[si] += step;
                    if (cylon_pos_[si] >= top) { cylon_pos_[si] = top; cylon_dir_[si] = -1; }
                }
                else
                {
                    if (step >= cylon_pos_[si]) { cylon_pos_[si] = 0; cylon_dir_[si] = 1; }
                    else cylon_pos_[si] -= step;
                }

                size_t i0 = std::min((size_t)(cylon_pos_[si] >> 8), num_leds - 1);
                uint8_t frac = cylon_pos_[si] & 0xFF;
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
                uint32_t rocket_v = ((((uint32_t)span << 8) / (200 + (uint32_t)duration * 12)) * device_motion_pct_() / 100) + 1;
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
                    spark_pos_[s] += (int32_t)spark_vel_[s] * dt * device_motion_pct_() / 100;
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

    case LightSceneID::chevron_wave:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.chevron_wave.duration))
        {
            advance_phase_(dt, 6, active_scene_.scenes.chevron_wave.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.chevron_wave.palette);
            uint8_t width = active_scene_.scenes.chevron_wave.width;
            if (width == 0) width = 1;
            bool fwd = active_scene_.scenes.chevron_wave.direction;

            for (size_t s = 0; s < led_controllers_.size(); s++)
            {
                CLEDController *controller = led_controllers_[s];
                uint8_t ph = (uint8_t)map_phase_(s);
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint8_t segs;
                (void)segs;

                for (size_t i = 0; i < num_leds; i++)
                {
                    PanelPixel px = panel_pixel_(s, i, num_leds, segs);
                    // The palette is painted by panel position, so each
                    // wavefront collapses through a chevron into its point.
                    uint8_t pos = fwd ? px.x : (uint8_t)(255 - px.x);
                    // width sets how much palette is stretched across the
                    // panel: ~1 cycle at width 1 up to ~7 at width 50.
                    uint8_t idx = (uint8_t)(((uint16_t)pos * (width + 7)) / 8) - ph;
                    leds[i] = ColorFromPalette(current_palette, idx);
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::chevron_chase:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.chevron_chase.duration))
        {
            advance_phase_(dt, 40, active_scene_.scenes.chevron_chase.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.chevron_chase.palette);
            uint8_t trail = active_scene_.scenes.chevron_chase.trail;
            if (trail == 0) trail = 1;
            bool fwd = active_scene_.scenes.chevron_chase.direction;

            for (size_t s = 0; s < led_controllers_.size(); s++)
            {
                CLEDController *controller = led_controllers_[s];
                uint32_t ph = map_phase_(s);
                uint8_t fill = (uint8_t)(ph & 0xFF); // sweep progress inside a step
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint8_t segs;
                // Spent chevrons linger by the trail length before going dark.
                fadeToBlackBy(leds, num_leds, 255 / trail);

                for (size_t i = 0; i < num_leds; i++)
                {
                    PanelPixel px = panel_pixel_(s, i, num_leds, segs);
                    // One chevron per 256 phase units, then two dark steps so
                    // the restart of the sweep reads as a loop.
                    uint8_t step = (uint8_t)((ph >> 8) % (segs + 2));
                    uint8_t active = fwd ? px.seg : (uint8_t)(segs - 1 - px.seg);
                    if (active != step) continue;
                    // Leg tips light first; the fill sweeps into the vertex.
                    if (px.vdist >= (uint8_t)(255 - fill))
                    {
                        leds[i] = ColorFromPalette(current_palette, px.seg * (255 / segs) + (uint8_t)(ph >> 6));
                    }
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::chevron_burst:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.chevron_burst.duration))
        {
            advance_phase_(dt, 8, active_scene_.scenes.chevron_burst.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.chevron_burst.palette);
            uint8_t width = active_scene_.scenes.chevron_burst.width;
            if (width == 0) width = 1;
            bool fwd = active_scene_.scenes.chevron_burst.direction;

            for (size_t s = 0; s < led_controllers_.size(); s++)
            {
                CLEDController *controller = led_controllers_[s];
                uint8_t ph = (uint8_t)map_phase_(s);
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint8_t segs;

                for (size_t i = 0; i < num_leds; i++)
                {
                    PanelPixel px = panel_pixel_(s, i, num_leds, segs);
                    // Pulses radiate from each vertex out along both legs
                    // (inward when reversed), neighbouring chevrons offset so
                    // the energy rolls across the panel.
                    uint8_t leg = fwd ? px.vdist : (uint8_t)(255 - px.vdist);
                    uint8_t stagger = px.seg * (128 / segs);
                    uint8_t v = sin8((uint8_t)((uint16_t)leg * (width + 7) / 16) - ph + stagger);
                    v = scale8(v, v); // sharpen: dark water between pulses
                    leds[i] = ColorFromPalette(current_palette, (uint8_t)(px.vdist >> 1) + (uint8_t)(phase_() >> 3), v);
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::chevron_glow:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.chevron_glow.duration))
        {
            // Temporal: breathing pace shouldn't slow on grouped-pixel rigs.
            advance_phase_(dt, 3, active_scene_.scenes.chevron_glow.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.chevron_glow.palette);
            uint8_t t = (uint8_t)phase_();
            uint8_t hue_drift = (uint8_t)(phase_() >> 5);

            for (size_t s = 0; s < led_controllers_.size(); s++)
            {
                CLEDController *controller = led_controllers_[s];
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint8_t segs;

                for (size_t i = 0; i < num_leds; i++)
                {
                    PanelPixel px = panel_pixel_(s, i, num_leds, segs);
                    // Each chevron holds its own palette colour and breathes
                    // against its neighbours; never fully dark, and the leg
                    // tips sit dimmer so the shapes read as lit from their
                    // points.
                    uint8_t seg_phase = px.seg * (255 / segs);
                    uint8_t breath = 48 + scale8(sin8((uint8_t)(t + seg_phase)), 207);
                    CRGB c = ColorFromPalette(current_palette, seg_phase + hue_drift, breath);
                    c.nscale8(255 - (px.vdist >> 2));
                    leds[i] = c;
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::chevron_eq:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.chevron_eq.duration))
        {
            advance_phase_(dt, 10, active_scene_.scenes.chevron_eq.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.chevron_eq.palette);
            uint16_t t = (uint16_t)phase_();

            // Each chevron is a VU bar filling from its vertex out to the leg
            // tips, chasing a noise-driven "audio" level: fast attack, slower
            // fall. State is shared across strips like every other effect.
            for (uint8_t g = 0; g < max_panel_segments_; g++)
            {
                uint8_t tgt = inoise8((uint16_t)(g * 977), t * 3);
                // inoise8 clusters mid-range - stretch it so bars actually
                // slam full and drop empty.
                tgt = qsub8(tgt, 40);
                tgt = qadd8(tgt, scale8(tgt, 90));
                if (tgt > eq_level_[g])
                {
                    eq_level_[g] = qadd8(eq_level_[g], (uint8_t)std::min<uint16_t>(dt, (uint16_t)(tgt - eq_level_[g])));
                }
                else
                {
                    eq_level_[g] = std::max<uint8_t>(tgt, qsub8(eq_level_[g], dt / 3));
                }
            }

            for (size_t s = 0; s < led_controllers_.size(); s++)
            {
                CLEDController *controller = led_controllers_[s];
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint8_t segs;

                for (size_t i = 0; i < num_leds; i++)
                {
                    PanelPixel px = panel_pixel_(s, i, num_leds, segs);
                    uint8_t level = eq_level_[px.seg % max_panel_segments_];
                    if (px.vdist <= level)
                    {
                        // Bar body runs the palette vertex-to-tip; the
                        // outermost lit pixels get a hot white cap.
                        leds[i] = ColorFromPalette(current_palette, px.vdist);
                        if ((uint16_t)px.vdist + 48 > level)
                        {
                            leds[i] += CRGB(70, 70, 70);
                        }
                    }
                    else
                    {
                        leds[i] = CRGB::Black;
                    }
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::confetti:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.confetti.duration))
        {
            // Temporal: the pop rate is a rhythm, not a distance, so it does
            // not slow on grouped-pixel rigs.
            advance_phase_(dt, 4, active_scene_.scenes.confetti.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.confetti.palette);
            hue_ += 1;
            uint8_t density = active_scene_.scenes.confetti.density;
            if (density == 0) density = 1;

            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                fadeToBlackBy(leds, num_leds, 5 + dt / 3);

                // Spawns scale with strip length, density and elapsed time so
                // a 450-LED strip pops as busily as a 50 - the fractional
                // remainder spawns probabilistically rather than being lost.
                uint32_t spawns256 = (uint32_t)num_leds * density * dt * 256 / 400000;
                uint16_t spawns = spawns256 >> 8;
                if (random8() < (spawns256 & 0xFF)) spawns++;
                for (uint16_t sp = 0; sp < spawns; sp++)
                {
                    leds[random16(num_leds)] += ColorFromPalette(current_palette, hue_ + random8(64), 200 + random8(55));
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::juggle:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.juggle.duration))
        {
            advance_phase_(dt, 5, active_scene_.scenes.juggle.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.juggle.palette);
            uint8_t balls = active_scene_.scenes.juggle.ball_count;
            if (balls == 0) balls = 1;
            if (balls > 8) balls = 8;

            for (size_t si = 0; si < led_controllers_.size(); si++)
            {
                CLEDController *controller = led_controllers_[si];
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                fadeToBlackBy(leds, num_leds, 10 + dt / 2);
                uint32_t sph = strip_phase_(si);

                for (uint8_t b = 0; b < balls; b++)
                {
                    // Each ball swings at its own metre (the classic's i+7
                    // beat spread) and wears its own slice of the palette.
                    uint16_t bph = (uint16_t)(sph * (7 + b) * 40);
                    uint16_t sv = (uint16_t)(sin16(bph) + 32768);
                    size_t pos = ((uint32_t)sv * (num_leds - 1)) >> 16;
                    leds[pos] |= ColorFromPalette(current_palette, b * (255 / balls));
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::sinelon:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.sinelon.duration))
        {
            advance_phase_(dt, 5, active_scene_.scenes.sinelon.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.sinelon.palette);
            uint8_t trail = active_scene_.scenes.sinelon.trail;
            if (trail == 0) trail = 1;
            hue_ += 1 + (dt >> 5);
            CRGB c = ColorFromPalette(current_palette, hue_);

            for (size_t si = 0; si < led_controllers_.size(); si++)
            {
                CLEDController *controller = led_controllers_[si];
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                // Longer trail = gentler fade behind the dot, like cylon -
                // but the sine sweep lingers at the ends instead of bouncing.
                fadeToBlackBy(leds, num_leds, 255 / trail);

                uint16_t ph = (uint16_t)(strip_phase_(si) * 70);
                uint16_t sv = (uint16_t)(sin16(ph) + 32768);
                uint32_t pos_q8 = ((uint32_t)sv * ((num_leds - 1) << 8)) >> 16;
                size_t i0 = std::min((size_t)(pos_q8 >> 8), num_leds - 1);
                uint8_t frac = pos_q8 & 0xFF;
                leds[i0] += c.scale8(255 - frac);
                if (i0 + 1 < num_leds) leds[i0 + 1] += c.scale8(frac ? frac : 1);

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::bpm:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.bpm.duration))
        {
            // Temporal: the speed slider is the tempo (~90 bpm at the middle).
            advance_phase_(dt, 40, active_scene_.scenes.bpm.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.bpm.palette);
            hue_ += 1;
            uint8_t beat = 64 + scale8(sin8((uint8_t)phase_()), 191);

            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                for (size_t i = 0; i < num_leds; i++)
                {
                    // The classic's wrapping brightness sum - it reads as a
                    // shimmer racing through the throb, not a plain pulse.
                    uint8_t bri = (uint8_t)(beat - hue_ + (uint8_t)(i * 10));
                    leds[i] = ColorFromPalette(current_palette, hue_ + (uint8_t)(i * 2), bri);
                }
                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::pacifica:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.pacifica.duration))
        {
            // The classic runs in real time; 33 (the field-effect frame cap)
            // maps to 1x, so the speed slider swings the ocean either way.
            uint16_t duration = active_scene_.scenes.pacifica.duration;
            if (duration == 0) duration = 33;
            uint32_t deltams = (uint32_t)dt * 33 * device_motion_pct_() / duration / 100;
            if (deltams == 0) deltams = 1;
            pacifica_ms_ += deltams;
            uint32_t ms = pacifica_ms_;

            // Each layer's colour-index start drifts at its own wobbling pace
            uint16_t speedfactor1 = pacBeatsin16(3, 179, 269, ms);
            uint16_t speedfactor2 = pacBeatsin16(4, 179, 269, ms);
            uint32_t d1 = (deltams * speedfactor1) / 256;
            uint32_t d2 = (deltams * speedfactor2) / 256;
            uint32_t d21 = (d1 + d2) / 2;
            pacifica_ci_[0] += (d1 * pacBeatsin88(1011, 10, 13, ms));
            pacifica_ci_[1] -= (d21 * pacBeatsin88(777, 8, 11, ms));
            pacifica_ci_[2] -= (d1 * pacBeatsin88(501, 5, 7, ms));
            pacifica_ci_[3] -= (d2 * pacBeatsin88(257, 4, 6, ms));

            for (auto &controller : led_controllers_)
            {
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();

                fill_solid(leds, num_leds, CRGB(2, 6, 10));
                pacificaLayer(leds, num_leds, pacifica_palette_1, pacifica_ci_[0],
                              pacBeatsin16(3, 11 * 256, 14 * 256, ms), pacBeatsin8(10, 70, 130, ms), 0 - pacBeat16(301, ms));
                pacificaLayer(leds, num_leds, pacifica_palette_2, pacifica_ci_[1],
                              pacBeatsin16(4, 6 * 256, 9 * 256, ms), pacBeatsin8(17, 40, 80, ms), pacBeat16(401, ms));
                pacificaLayer(leds, num_leds, pacifica_palette_3, pacifica_ci_[2],
                              6 * 256, pacBeatsin8(9, 10, 38, ms), 0 - pacBeat16(503, ms));
                pacificaLayer(leds, num_leds, pacifica_palette_3, pacifica_ci_[3],
                              5 * 256, pacBeatsin8(8, 10, 28, ms), pacBeat16(601, ms));

                // Whitecaps where the layers constructively pile up
                uint8_t basethreshold = pacBeatsin8(9, 55, 65, ms);
                uint8_t wave = pacBeat8(7, ms);
                for (size_t i = 0; i < num_leds; i++)
                {
                    uint8_t threshold = scale8(sin8(wave), 20) + basethreshold;
                    wave += 7;
                    uint8_t l = leds[i].getAverageLight();
                    if (l > threshold)
                    {
                        uint8_t overage = l - threshold;
                        uint8_t overage2 = qadd8(overage, overage);
                        leds[i] += CRGB(overage, overage2, qadd8(overage2, overage2));
                    }
                }
                // Deepen the blues and greens
                for (size_t i = 0; i < num_leds; i++)
                {
                    leds[i].blue = scale8(leds[i].blue, 145);
                    leds[i].green = scale8(leds[i].green, 200);
                    leds[i] |= CRGB(2, 5, 7);
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::panel_waves:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.panel_waves.duration))
        {
            advance_phase_(dt, 5, active_scene_.scenes.panel_waves.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.panel_waves.palette);
            uint8_t scale = active_scene_.scenes.panel_waves.scale;
            if (scale == 0) scale = 1;
            // scale 1-50 -> how many wavefronts fit on the panel at once
            uint8_t f = 2 + scale / 4;
            uint8_t hue_drift = (uint8_t)(phase_() >> 4);

            for (size_t s = 0; s < led_controllers_.size(); s++)
            {
                CLEDController *controller = led_controllers_[s];
                uint8_t ph = (uint8_t)map_phase_(s);
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint8_t segs;
                (void)segs;

                for (size_t i = 0; i < num_leds; i++)
                {
                    PanelPixel px = panel_pixel_(s, i, num_leds, segs);
                    // Three wavefronts crossing the panel on different axes;
                    // where they pile up the palette runs to its far end.
                    uint8_t a = sin8((uint8_t)((px.x * f) / 4) + ph);
                    uint8_t b = sin8((uint8_t)((px.y * f) / 4) - (uint8_t)(ph * 2));
                    uint8_t c = sin8((uint8_t)(((px.x + px.y) * f) / 8) + (uint8_t)(ph + 64));
                    uint8_t idx = (uint8_t)(((uint16_t)a + b + c) / 3);
                    leds[i] = ColorFromPalette(current_palette, idx + hue_drift);
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::panel_fire:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.panel_fire.duration))
        {
            advance_phase_(dt, 10, active_scene_.scenes.panel_fire.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.panel_fire.palette);
            uint8_t variance = active_scene_.scenes.panel_fire.variance;
            // Cooling with height: more variance = less cooling = taller,
            // wilder flames licking over the top edge.
            uint8_t cooling = 220 - variance;

            for (size_t s = 0; s < led_controllers_.size(); s++)
            {
                CLEDController *controller = led_controllers_[s];
                // The noise field scrolls downward through panel space, so the
                // flame shapes rise.
                uint16_t rise = (uint16_t)map_phase_(s) * 6;
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint8_t segs;
                (void)segs;

                for (size_t i = 0; i < num_leds; i++)
                {
                    PanelPixel px = panel_pixel_(s, i, num_leds, segs);
                    uint8_t n = inoise8((uint16_t)px.x * 5,
                                        (uint16_t)((uint16_t)px.y * 4 - rise),
                                        (uint16_t)(phase_() >> 3));
                    // inoise8 clusters mid-range - stretch it so the flames
                    // actually roar and actually die.
                    n = qadd8(qsub8(n, 24), scale8(n, 60));
                    uint8_t heat = qsub8(n, scale8(px.y, cooling));
                    leds[i] = ColorFromPalette(current_palette, scale8(heat, 250));
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::panel_rain:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.panel_rain.duration))
        {
            advance_phase_(dt, 12, active_scene_.scenes.panel_rain.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.panel_rain.palette);
            uint8_t hue_drift = (uint8_t)(phase_() >> 5);
            // density 1-100 -> what fraction of the lanes rain at once
            uint8_t threshold = (uint8_t)(((uint16_t)active_scene_.scenes.panel_rain.density * 255) / 100);

            for (size_t s = 0; s < led_controllers_.size(); s++)
            {
                CLEDController *controller = led_controllers_[s];
                uint16_t fall = (uint16_t)map_phase_(s);
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint8_t segs;
                (void)segs;

                for (size_t i = 0; i < num_leds; i++)
                {
                    PanelPixel px = panel_pixel_(s, i, num_leds, segs);
                    // Each column of the panel is its own drop lane: a stable
                    // hash of x gives it a phase offset, its own pace, and
                    // which cycles it sits out.
                    uint16_t prng = (uint16_t)((px.x + 1) * 40503);
                    prng = (prng >> 8) | (prng << 8);
                    uint8_t salt = prng & 0xFF;
                    uint16_t lane = (uint16_t)((fall * (192 + (salt & 63))) >> 8) + salt;
                    uint8_t cycle = (uint8_t)(lane >> 8);
                    if ((uint8_t)(salt + cycle * 89) > threshold)
                    {
                        leds[i] = CRGB::Black;
                        continue;
                    }
                    uint8_t head = (uint8_t)(255 - (lane & 0xFF)); // top -> bottom
                    uint8_t d = (uint8_t)(px.y - head);            // tail above the head
                    if (d < 112)
                    {
                        uint8_t v = 255 - d * 2;
                        v = scale8(v, v);
                        CRGB c = ColorFromPalette(current_palette, salt + hue_drift, v);
                        if (d < 20)
                        {
                            c += CRGB(60, 60, 60); // bright leading drop
                        }
                        leds[i] = c;
                    }
                    else
                    {
                        leds[i] = CRGB::Black;
                    }
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::panel_spin:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.panel_spin.duration))
        {
            advance_phase_(dt, 7, active_scene_.scenes.panel_spin.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.panel_spin.palette);
            uint8_t arms = active_scene_.scenes.panel_spin.arms;
            if (arms == 0) arms = 1;
            if (arms > 8) arms = 8;
            uint8_t hue_drift = (uint8_t)(phase_() >> 4);

            for (size_t s = 0; s < led_controllers_.size(); s++)
            {
                CLEDController *controller = led_controllers_[s];
                uint8_t ph = (uint8_t)map_phase_(s);
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint8_t segs;
                (void)segs;

                for (size_t i = 0; i < num_leds; i++)
                {
                    PanelPixel px = panel_pixel_(s, i, num_leds, segs);
                    int16_t dx = (int16_t)px.x - 128;
                    int16_t dy = (int16_t)px.y - 128;
                    uint8_t ang = angle8_(dy, dx);
                    uint8_t r = (uint8_t)sqrt16((uint16_t)(dx * dx) + (uint16_t)(dy * dy));
                    // Spokes rotate about the panel centre; the slight radial
                    // twist curls them into a galaxy rather than a fan.
                    uint8_t v = sin8((uint8_t)(ang * arms) + (uint8_t)(ph * 2) - (r >> 1));
                    v = scale8(v, v); // dark water between the spokes
                    leds[i] = ColorFromPalette(current_palette, (uint8_t)(r + hue_drift), v);
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::starfield:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.starfield.duration))
        {
            advance_phase_(dt, 20, active_scene_.scenes.starfield.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.starfield.palette);
            bool outward = active_scene_.scenes.starfield.direction;
            uint8_t density = active_scene_.scenes.starfield.density;
            if (density == 0) density = 1;
            if (density > 100) density = 100;
            uint32_t ph = phase_();
            uint8_t hue_drift = (uint8_t)(ph >> 6);
            // density 1-100 -> 6..38 stars in flight on the grid
            uint8_t stars = 6 + (uint8_t)(((uint16_t)density * 32) / 100);

            for (size_t s = 0; s < led_controllers_.size(); s++)
            {
                CLEDController *controller = led_controllers_[s];
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                const StripMap *m = s < pixel_maps_.size() ? &pixel_maps_[s] : nullptr;

                if (m && m->grid)
                {
                    // Warp field: each star gets a stable heading, pace and
                    // phase offset from its hash, and accelerates out of the
                    // centre (reversed: falls inward).
                    fill_solid(leds, num_leds, CRGB::Black);
                    int32_t cx = (int32_t)m->grid_w * 128; // centre, Q8 cells
                    int32_t cy = (int32_t)m->grid_h * 128;
                    for (uint8_t k = 0; k < stars; k++)
                    {
                        uint16_t hk = (uint16_t)((k + 1) * 40503u + 9887u);
                        uint8_t salt = (uint8_t)(hk >> 8);
                        uint8_t ang = (uint8_t)(hk ^ (hk >> 3));
                        uint8_t t = (uint8_t)((ph * (160 + (salt & 63)) >> 7) + salt * 5);
                        if (!outward) t = (uint8_t)(255 - t);
                        uint8_t e = scale8(t, t); // quadratic ease = acceleration
                        int16_t dxu = (int16_t)cos8(ang) - 128;
                        int16_t dyu = (int16_t)sin8(ang) - 128;
                        int32_t x_q8 = cx + (((int32_t)dxu * e) * m->grid_w) / 255;
                        int32_t y_q8 = cy + (((int32_t)dyu * e) * m->grid_h) / 255;
                        int16_t col = (int16_t)(x_q8 >> 8);
                        int16_t row = (int16_t)(y_q8 >> 8);
                        CRGB c = ColorFromPalette(current_palette, (uint8_t)(salt + hue_drift),
                                                  (uint8_t)(40 + scale8(e, 215)));
                        if (row >= 0 && row < m->grid_h && col >= 0 && col < m->grid_w)
                        {
                            uint16_t p = m->grid[row * m->grid_w + col];
                            if (p < num_leds) leds[p] += c;
                        }
                        // A one-cell tail toward the centre keeps fast stars
                        // readable as motion instead of flicker.
                        int16_t tcol = (int16_t)((cx + ((x_q8 - cx) * 200) / 256) >> 8);
                        int16_t trow = (int16_t)((cy + ((y_q8 - cy) * 200) / 256) >> 8);
                        if (trow >= 0 && trow < m->grid_h && tcol >= 0 && tcol < m->grid_w &&
                            (trow != row || tcol != col))
                        {
                            uint16_t p = m->grid[trow * m->grid_w + tcol];
                            CRGB tc = c;
                            tc.nscale8(90);
                            if (p < num_leds) leds[p] += tc;
                        }
                    }
                }
                else
                {
                    // No grid: star streaks racing along x - on the bike's
                    // frame wrap they fly rear -> front, as if riding through
                    // the field the matrix is warping into.
                    uint16_t flow = (uint16_t)map_phase_(s);
                    uint8_t segs;
                    (void)segs;
                    uint8_t threshold = (uint8_t)(40 + ((uint16_t)density * 200) / 100);
                    for (size_t i = 0; i < num_leds; i++)
                    {
                        PanelPixel px = panel_pixel_(s, i, num_leds, segs);
                        // Each height band is its own star lane.
                        uint16_t prng = (uint16_t)((px.y + 1) * 31337);
                        prng = (uint16_t)((prng >> 8) | (prng << 8));
                        uint8_t salt = (uint8_t)prng;
                        uint16_t lane = (uint16_t)((flow * (192 + (salt & 63))) >> 8) + salt;
                        uint8_t cycle = (uint8_t)(lane >> 8);
                        if ((uint8_t)(salt + cycle * 89) > threshold)
                        {
                            leds[i] = CRGB::Black;
                            continue;
                        }
                        uint8_t head = (uint8_t)(lane & 0xFF);
                        if (!outward) head = (uint8_t)(255 - head);
                        uint8_t d = outward ? (uint8_t)(head - px.x) : (uint8_t)(px.x - head);
                        if (d < 96)
                        {
                            uint8_t v = (uint8_t)(255 - d * 2);
                            v = scale8(v, v);
                            CRGB c = ColorFromPalette(current_palette, (uint8_t)(salt + hue_drift), v);
                            if (d < 12) c += CRGB(70, 70, 70);
                            leds[i] = c;
                        }
                        else
                        {
                            leds[i] = CRGB::Black;
                        }
                    }
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::orbit_comet:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.orbit_comet.duration))
        {
            advance_phase_(dt, 18, active_scene_.scenes.orbit_comet.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.orbit_comet.palette);
            uint8_t trail = active_scene_.scenes.orbit_comet.trail;
            if (trail == 0) trail = 1;
            bool fwd = active_scene_.scenes.orbit_comet.direction;
            uint8_t hue_drift = (uint8_t)(phase_() >> 4);

            for (size_t s = 0; s < led_controllers_.size(); s++)
            {
                CLEDController *controller = led_controllers_[s];
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                if (num_leds == 0)
                {
                    continue;
                }
                // The lap follows the PHYSICAL wiring chain - on the frame
                // wrap the chain is laid along the frame, so the comet orbits
                // the bike's silhouette. Effects write logical positions, so
                // aim each logical slot at where its LED physically sits.
                const uint16_t *order = s < pixel_maps_.size() ? pixel_maps_[s].order : nullptr;
                // One lap per 4096 phase units regardless of strip length,
                // so a synced rig's comets lap together.
                uint32_t sp = strip_phase_(s);
                uint16_t head = (uint16_t)(((sp & 0x0FFF) * (uint32_t)num_leds) >> 12);
                if (!fwd) head = (uint16_t)(num_leds - 1 - head);
                // trail 1-30 -> up to ~60% of the strip glowing behind
                uint32_t tail = 2 + ((uint32_t)trail * num_leds) / 50;

                for (size_t k = 0; k < num_leds; k++)
                {
                    uint16_t phys = order ? order[k] : (uint16_t)k;
                    int32_t d = fwd ? (int32_t)head - (int32_t)phys
                                    : (int32_t)phys - (int32_t)head;
                    if (d < 0) d += (int32_t)num_leds;
                    if ((uint32_t)d <= tail)
                    {
                        uint8_t v = (uint8_t)(255 - ((uint32_t)d * 255) / (tail + 1));
                        v = scale8(v, v);
                        CRGB c = ColorFromPalette(
                            current_palette,
                            (uint8_t)(((uint32_t)phys * 255) / num_leds + hue_drift), v);
                        if (d < 2) c += CRGB(80, 80, 80); // white-hot head
                        leds[k] = c;
                    }
                    else if ((uint32_t)d <= tail * 2 && random8() < 4)
                    {
                        // Sparks shed behind the tail.
                        leds[k] = ColorFromPalette(current_palette, (uint8_t)(hue_drift + 128), 120);
                    }
                    else
                    {
                        leds[k] = CRGB::Black;
                    }
                }

                show_(controller, active_scene_.brightness);
            }
        }
        break;

    case LightSceneID::panel_puddle:
        if (uint16_t dt = field_frame_elapsed_(now, active_scene_.scenes.panel_puddle.duration))
        {
            advance_phase_(dt, 12, active_scene_.scenes.panel_puddle.duration);
            CRGBPalette16 current_palette = getPalette(active_scene_.scenes.panel_puddle.palette);
            uint8_t hue_drift = (uint8_t)(phase_() >> 5);
            uint8_t threshold = (uint8_t)(((uint16_t)active_scene_.scenes.panel_puddle.density * 255) / 100);
            // The flood cycle: rain fills the low ground for ~7/8 of the
            // cycle (to just under half the rig's height), then drains fast.
            // y is true height, so the water pools where the bike is low -
            // the frame's step-through rail floods first.
            uint16_t wt = (uint16_t)(phase_() & 0x0FFF);
            uint8_t level = wt < 3584 ? (uint8_t)(((uint32_t)wt * 112) / 3584)
                                      : (uint8_t)(112 - ((uint32_t)(wt - 3584) * 112) / 512);

            for (size_t s = 0; s < led_controllers_.size(); s++)
            {
                CLEDController *controller = led_controllers_[s];
                uint16_t fall = (uint16_t)map_phase_(s);
                size_t num_leds = controller->size();
                CRGB *leds = controller->leds();
                uint8_t segs;
                (void)segs;

                for (size_t i = 0; i < num_leds; i++)
                {
                    PanelPixel px = panel_pixel_(s, i, num_leds, segs);
                    if (px.y < level)
                    {
                        // Underwater: the palette's low span, slow shimmer.
                        uint8_t v = (uint8_t)(150 + (sin8((uint8_t)(px.x * 3) + (uint8_t)(fall * 2)) >> 2));
                        leds[i] = ColorFromPalette(current_palette,
                                                   (uint8_t)((px.y >> 1) + hue_drift), v);
                    }
                    else if (px.y < (uint8_t)(level + 10) && level > 4)
                    {
                        // The waterline glints.
                        leds[i] = ColorFromPalette(current_palette, (uint8_t)(64 + hue_drift), 255) +
                                  CRGB(50, 50, 50);
                    }
                    else
                    {
                        // Rain above the water - panel_rain's lane maths.
                        uint16_t prng = (uint16_t)((px.x + 1) * 40503);
                        prng = (uint16_t)((prng >> 8) | (prng << 8));
                        uint8_t salt = (uint8_t)(prng & 0xFF);
                        uint16_t lane = (uint16_t)((fall * (192 + (salt & 63))) >> 8) + salt;
                        uint8_t cycle = (uint8_t)(lane >> 8);
                        if ((uint8_t)(salt + cycle * 89) > threshold)
                        {
                            leds[i] = CRGB::Black;
                            continue;
                        }
                        uint8_t head = (uint8_t)(255 - (lane & 0xFF));
                        uint8_t d = (uint8_t)(px.y - head);
                        if (d < 112)
                        {
                            uint8_t v = (uint8_t)(255 - d * 2);
                            v = scale8(v, v);
                            CRGB c = ColorFromPalette(current_palette, (uint8_t)(salt + hue_drift), v);
                            if (d < 20) c += CRGB(60, 60, 60);
                            leds[i] = c;
                        }
                        else
                        {
                            leds[i] = CRGB::Black;
                        }
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
        std::fill(cylon_pos_.begin(), cylon_pos_.end(), 0);
        std::fill(cylon_dir_.begin(), cylon_dir_.end(), 1);
        break;
    case LightSceneID::fireworks:
        fw_stage_ = 2;
        fw_next_launch_ = 0;
        memset(spark_heat_, 0, sizeof(spark_heat_));
        break;
    case LightSceneID::chevron_wave:
    case LightSceneID::chevron_chase:
    case LightSceneID::chevron_burst:
    case LightSceneID::chevron_glow:
    // The panel effects are stateless fields off the shared phase.
    case LightSceneID::panel_waves:
    case LightSceneID::panel_fire:
    case LightSceneID::panel_rain:
    case LightSceneID::panel_spin:
    case LightSceneID::starfield:
    case LightSceneID::orbit_comet:
    case LightSceneID::panel_puddle:
        break;
    case LightSceneID::chevron_eq:
        memset(eq_level_, 0, sizeof(eq_level_));
        break;
    case LightSceneID::pacifica:
        memset(pacifica_ci_, 0, sizeof(pacifica_ci_));
        pacifica_ms_ = 0;
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

    std::fill(cylon_pos_.begin(), cylon_pos_.end(), 0);
    std::fill(cylon_dir_.begin(), cylon_dir_.end(), 1);
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

void LightShow::chevron_wave(uint16_t duration, uint8_t width, AvailablePalettes palette, bool direction)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::chevron_wave;
    new_scene.scenes.chevron_wave.duration = duration;
    new_scene.scenes.chevron_wave.width = width;
    new_scene.scenes.chevron_wave.palette = palette;
    new_scene.scenes.chevron_wave.direction = direction;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::chevron_chase(uint16_t duration, uint8_t trail, AvailablePalettes palette, bool direction)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::chevron_chase;
    new_scene.scenes.chevron_chase.duration = duration;
    new_scene.scenes.chevron_chase.trail = trail;
    new_scene.scenes.chevron_chase.palette = palette;
    new_scene.scenes.chevron_chase.direction = direction;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::chevron_burst(uint16_t duration, uint8_t width, AvailablePalettes palette, bool direction)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::chevron_burst;
    new_scene.scenes.chevron_burst.duration = duration;
    new_scene.scenes.chevron_burst.width = width;
    new_scene.scenes.chevron_burst.palette = palette;
    new_scene.scenes.chevron_burst.direction = direction;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::chevron_glow(uint16_t duration, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::chevron_glow;
    new_scene.scenes.chevron_glow.duration = duration;
    new_scene.scenes.chevron_glow.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::chevron_eq(uint16_t duration, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::chevron_eq;
    new_scene.scenes.chevron_eq.duration = duration;
    new_scene.scenes.chevron_eq.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    memset(eq_level_, 0, sizeof(eq_level_));
    last_render_time_ = 0;
}

void LightShow::confetti(uint16_t duration, uint8_t density, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::confetti;
    new_scene.scenes.confetti.duration = duration;
    new_scene.scenes.confetti.density = density;
    new_scene.scenes.confetti.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::juggle(uint16_t duration, uint8_t ball_count, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::juggle;
    new_scene.scenes.juggle.duration = duration;
    new_scene.scenes.juggle.ball_count = ball_count;
    new_scene.scenes.juggle.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::sinelon(uint16_t duration, uint8_t trail, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::sinelon;
    new_scene.scenes.sinelon.duration = duration;
    new_scene.scenes.sinelon.trail = trail;
    new_scene.scenes.sinelon.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::bpm(uint16_t duration, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::bpm;
    new_scene.scenes.bpm.duration = duration;
    new_scene.scenes.bpm.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::pacifica(uint16_t duration)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::pacifica;
    new_scene.scenes.pacifica.duration = duration;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    memset(pacifica_ci_, 0, sizeof(pacifica_ci_));
    pacifica_ms_ = 0;
    last_render_time_ = 0;
}

void LightShow::panel_waves(uint16_t duration, uint8_t scale, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::panel_waves;
    new_scene.scenes.panel_waves.duration = duration;
    new_scene.scenes.panel_waves.scale = scale;
    new_scene.scenes.panel_waves.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::panel_fire(uint16_t duration, uint8_t variance, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::panel_fire;
    new_scene.scenes.panel_fire.duration = duration;
    new_scene.scenes.panel_fire.variance = variance;
    new_scene.scenes.panel_fire.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::panel_rain(uint16_t duration, uint8_t density, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::panel_rain;
    new_scene.scenes.panel_rain.duration = duration;
    new_scene.scenes.panel_rain.density = density;
    new_scene.scenes.panel_rain.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::panel_spin(uint16_t duration, uint8_t arms, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::panel_spin;
    new_scene.scenes.panel_spin.duration = duration;
    new_scene.scenes.panel_spin.arms = arms;
    new_scene.scenes.panel_spin.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::starfield(uint16_t duration, uint8_t density, AvailablePalettes palette, bool direction)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::starfield;
    new_scene.scenes.starfield.duration = duration;
    new_scene.scenes.starfield.density = density;
    new_scene.scenes.starfield.palette = palette;
    new_scene.scenes.starfield.direction = direction;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::orbit_comet(uint16_t duration, uint8_t trail, AvailablePalettes palette, bool direction)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::orbit_comet;
    new_scene.scenes.orbit_comet.duration = duration;
    new_scene.scenes.orbit_comet.trail = trail;
    new_scene.scenes.orbit_comet.palette = palette;
    new_scene.scenes.orbit_comet.direction = direction;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

    last_render_time_ = 0;
}

void LightShow::panel_puddle(uint16_t duration, uint8_t density, AvailablePalettes palette)
{
    LightScene new_scene = {};
    new_scene.scene_id = LightSceneID::panel_puddle;
    new_scene.scenes.panel_puddle.duration = duration;
    new_scene.scenes.panel_puddle.density = density;
    new_scene.scenes.panel_puddle.palette = palette;
    apply_scene_updates(new_scene);

    if (!scene_changed_)
    {
        return;
    }

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
    {"chevron_wave", LightSceneID::chevron_wave},
    {"chevron_chase", LightSceneID::chevron_chase},
    {"chevron_burst", LightSceneID::chevron_burst},
    {"chevron_glow", LightSceneID::chevron_glow},
    {"chevron_eq", LightSceneID::chevron_eq},
    {"confetti", LightSceneID::confetti},
    {"juggle", LightSceneID::juggle},
    {"sinelon", LightSceneID::sinelon},
    {"bpm", LightSceneID::bpm},
    {"pacifica", LightSceneID::pacifica},
    {"panel_waves", LightSceneID::panel_waves},
    {"panel_fire", LightSceneID::panel_fire},
    {"panel_rain", LightSceneID::panel_rain},
    {"panel_spin", LightSceneID::panel_spin},
    {"panel_text", LightSceneID::panel_text},
    {"panel_bitmap", LightSceneID::panel_bitmap},
    {"panel_words", LightSceneID::panel_words},
    {"starfield", LightSceneID::starfield},
    {"orbit_comet", LightSceneID::orbit_comet},
    {"panel_puddle", LightSceneID::panel_puddle},
    {"panel_anim", LightSceneID::panel_anim},
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