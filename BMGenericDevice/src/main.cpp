#include <BMDevice.h>
#include "OTAConfig.h"
#if OTA_ENABLED
#include "BMOTA.h"
static BMOTA ota;
#endif
// Home Assistant bridge - a real class only on keep-WiFi-alive sign builds
// with a broker configured in WiFiCreds.h, an empty stub everywhere else.
#include "BMMQTT.h"
static BMMQTT mqttBridge;

#ifdef TARGET_HOTEL_SIGN
#include "HotelSign.h"
static HotelSign hotelSign;
#endif

// Default UUIDs for generic device
#define SERVICE_UUID "4746abe4-2135-4a84-8f2f-f47f3a73e73b"
#define FEATURES_UUID "3927e9db-012b-4db9-8890-984fe28faf83"
#define STATUS_UUID "c054c450-cf93-4e6f-848e-2c521e739f4b"

// Board capability flags. Cody's bike runs the SLUT board minus the GPS
// module - move it into the GPS group when one gets soldered on.
#if defined(TARGET_SLUT) || defined(TARGET_HOTEL_SIGN)
    #define BM_HAS_GPS
    #define BM_HAS_ENCODER
    #define BM_HAS_FADEUP
#elif defined(TARGET_CODYS_BIKE)
    #define BM_HAS_ENCODER
    #define BM_HAS_FADEUP
#endif

// Chip-specific LED Configuration
#ifdef TARGET_ESP32_C6
    // ESP32-C6 configuration (single strip for now)
    #define NUM_STRIPS 1
    #define LEDS_PER_STRIP 450
    #define COLOR_ORDER GRB

    CRGB leds0[LEDS_PER_STRIP];

#elif defined(TARGET_SLUT) || defined(TARGET_HOTEL_SIGN)
    #define NUM_STRIPS 8
    #define LEDS_PER_STRIP 350
    #define COLOR_ORDER GRB

    // GPS Pin Configuration (only used if GPS is enabled)
    #define GPS_RX_PIN 21
    #define GPS_TX_PIN 22
    #define GPS_BAUD_RATE 9600  // Confirmed working baud rate

    CRGB leds0[LEDS_PER_STRIP], leds1[LEDS_PER_STRIP], leds2[LEDS_PER_STRIP], leds3[LEDS_PER_STRIP];
    CRGB leds4[LEDS_PER_STRIP], leds5[LEDS_PER_STRIP], leds6[LEDS_PER_STRIP], leds7[LEDS_PER_STRIP];

#elif defined(TARGET_CODYS_BIKE)
    // Cody's bike: the chevron panel on the third 12 V output (GPIO 32), the
    // frame wrap glow strip on the second (GPIO 33), and the front-rack 8x32
    // matrix on the first 5 V output (GPIO 5). CodysBikeMap.h is generated
    // from the Fusion panel design and the strip measurements by
    // scripts/generate_bike_map.py; FrontRackMatrixMap.h (the radial
    // continuation of bike space) by scripts/generate_matrix_map.py.
    #include "CodysBikeMap.h"
    #include "FrontRackMatrixMap.h"
    #define NUM_STRIPS 3
    // 12 mm button/bullet pixels (12 V WS2811) are usually RGB on the wire,
    // unlike the GRB WS2812B strips; BRG is the common alternate batch. To
    // verify: play solid red - green means the string is really GRB, blue
    // means BRG. Override without editing source: -DCHEVRON_COLOR_ORDER=BRG.
    #ifndef CHEVRON_COLOR_ORDER
    #define CHEVRON_COLOR_ORDER RGB
    #endif
    // The 12 V glow flex is usually RGB on the wire too - same red test,
    // same kind of override: -DFRAME_COLOR_ORDER=BRG.
    #ifndef FRAME_COLOR_ORDER
    #define FRAME_COLOR_ORDER RGB
    #endif
    // The 8x32 flex sheet is ordinary WS2812B - GRB unless the batch says
    // otherwise: -DMATRIX_COLOR_ORDER=RGB.
    #ifndef MATRIX_COLOR_ORDER
    #define MATRIX_COLOR_ORDER GRB
    #endif

    CRGB leds0[CODYS_BIKE_LED_COUNT];
    CRGB leds1[CODYS_BIKE_FRAME_LED_COUNT];
    CRGB leds2[FRONT_RACK_MATRIX_LED_COUNT];

#elif TARGET_TAKOYAKI_SIGN
    // Takoyaki sign configuration (same pins/count as classic target)
    #define NUM_STRIPS 8
    #define LEDS_PER_STRIP 450

    // Strips 1 & 5 & 6 (GPIO 33, 13, 18): RGB on wire — GRB in software gave R/G swap, blue OK
    #define STRIP0_COLOR_ORDER GRB 
    #define STRIP1_COLOR_ORDER GRB // Board 2
    #define STRIP2_COLOR_ORDER RGB // Board 1
    #define STRIP3_COLOR_ORDER GRB 
    #define STRIP4_COLOR_ORDER GRB // Board 3
    #define STRIP5_COLOR_ORDER RGB // Board 4
    #define STRIP6_COLOR_ORDER RGB // Board 5
    #define STRIP7_COLOR_ORDER GRB
    
    // Individual LED strip arrays for all 8 strips
    CRGB leds0[LEDS_PER_STRIP], leds1[LEDS_PER_STRIP], leds2[LEDS_PER_STRIP], leds3[LEDS_PER_STRIP];
    CRGB leds4[LEDS_PER_STRIP], leds5[LEDS_PER_STRIP], leds6[LEDS_PER_STRIP], leds7[LEDS_PER_STRIP];

#else
    // ESP32 classic configuration (8 strips)
    #define NUM_STRIPS 8
    #define LEDS_PER_STRIP 450
    #define COLOR_ORDER GRB
    
    // Individual LED strip arrays for all 8 strips
    CRGB leds0[LEDS_PER_STRIP], leds1[LEDS_PER_STRIP], leds2[LEDS_PER_STRIP], leds3[LEDS_PER_STRIP];
    CRGB leds4[LEDS_PER_STRIP], leds5[LEDS_PER_STRIP], leds6[LEDS_PER_STRIP], leds7[LEDS_PER_STRIP];
#endif

#ifdef BM_HAS_ENCODER
    #define ENCODER_PIN_A 17
    #define ENCODER_PIN_B 16
    #define ENCODER_BUTTON_PIN 18
    int encoderState = 0;
    volatile long encoderPosition = 0;
    volatile int lastEncoded = 0;
    void IRAM_ATTR handleEncoder()
    {
    int MSB = digitalRead(ENCODER_PIN_A);   // Most significant bit
    int LSB = digitalRead(ENCODER_PIN_B);   // Least significant bit
    int encoded = (MSB << 1) | LSB;         // Convert the 2 pin values to a single number
    int sum = (lastEncoded << 2) | encoded; // Add the previous encoded value
    if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011)
    {
        encoderState++;
    }

    if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000)
    {
        encoderState--;
    }
    // Signs flipped relative to the raw quadrature: the A/B pins are wired
    // swapped on the board, so without this a clockwise detent read as -1 and
    // every menu turned the wrong way.
    if (encoderState == 4)
    {
        encoderPosition = encoderPosition - 1; // Full cycle this way = counter-clockwise
        encoderState = 0;  // Reset state
    }
    else if (encoderState == -4)
    {
        encoderPosition = encoderPosition + 1; // Full cycle this way = clockwise
        encoderState = 0;  // Reset state
    }
    lastEncoded = encoded; // Store this value for next time
    }
    // Button Press Stuff
    int lastState = LOW; // the previous state from the input pin
    int currentState;    // the current reading from the input pin
    unsigned long pressedTime = 0;
    unsigned long releasedTime = 0;
    unsigned long lastButtonEdgeAt = 0;
    const int SHORT_PRESS_TIME = 1000;
    const int OFF_TIME = 5000;
    // Contact bounce shows up as a flurry of edges; anything this close to an
    // accepted edge is the switch settling, not the thumb.
    const unsigned long BUTTON_DEBOUNCE_MS = 30;

    // Menu system for encoder control. Order = presses to reach it: the
    // effect rotation is the knob's marquee control, so it sits one press
    // from brightness; speed, the rarest tweak, comes last.
    enum EncoderMenu {
        MENU_BRIGHTNESS = 0,
        MENU_EFFECT,
        MENU_PALETTE,
        MENU_SPEED,
        MENU_COUNT  // Total number of menu items
    };

    EncoderMenu currentMenu = MENU_BRIGHTNESS;
    bool buttonInitialized = false;
    unsigned long lastMenuChange = 0;
    // With no screen the menu state is invisible, so after a quiet spell it
    // falls back to brightness - the knob always means "brightness" when you
    // reach for it cold.
    const unsigned long MENU_TIMEOUT_MS = 30000;
#endif

// BMDevice instance with dynamic naming
BMDevice device(SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

#ifdef BM_HAS_FADEUP
// Fade-up variables to prevent power spikes at startup
int targetBrightness = 50;  // Will be set from loaded defaults
int currentFadeBrightness = 1;  // Start very low
unsigned long fadeStartTime = 0;
const unsigned long FADE_DURATION = 3000;  // 3 seconds fade-up
bool fadeUpComplete = false;


#endif

#ifdef BM_HAS_ENCODER
// Function to cycle through curated light show effects
void cycleEffect(int direction) {
    // Curated list of visually distinct effects
    static const LightSceneID curatedEffects[] = {
#ifdef TARGET_CODYS_BIKE
        // The bike's whole rotation, short on purpose: every detent lands
        // somewhere good. No display modes here - panel_text/panel_words are
        // setMatrixDisplay() aliases that never change currentEffect, so
        // cycling onto one wedged the clockwise cycle forever; the matrix
        // overlay is the remote/app's business now.
        LightSceneID::chevron_wave,      // 🏹 Palette flowing through the chevrons
        LightSceneID::chevron_chase,     // ➡️ Turn-signal arrow sweep
        LightSceneID::chevron_burst,     // 💥 Pulses out of the points
        LightSceneID::chevron_glow,      // 🫁 Per-chevron breathing
        LightSceneID::chevron_eq,        // 🎚️ VU bars on the chevrons
        LightSceneID::color_explosion,   // 💥 Explosive colors
        LightSceneID::aurora_borealis,   // 🌌 Northern lights
#else
        LightSceneID::meteor_shower,     // ☄️ Meteors with trails
        LightSceneID::fire_plasma,       // 🔥 Realistic fire
        LightSceneID::kaleidoscope,      // 🌀 Mirrored patterns
        LightSceneID::rainbow_comet,     // 🌈 Rainbow comets
        LightSceneID::matrix_rain,       // 💻 Matrix-style rain
        LightSceneID::plasma_clouds,     // ☁️ Flowing plasma
        LightSceneID::aurora_borealis,   // 🌌 Northern lights
        LightSceneID::color_explosion,   // 💥 Explosive colors
        LightSceneID::spiral_galaxy,     // 🌌 Rotating spirals
        LightSceneID::noise_flow,        // 🌊 Drifting Perlin noise
        LightSceneID::twinkle,           // ✨ Fairy-light twinkles
        LightSceneID::ripple,            // 💧 Expanding rings
        LightSceneID::cylon,             // 👁️ Bouncing eye with trail
        LightSceneID::fireworks,         // 🎆 Rockets and bursts
        LightSceneID::confetti,          // 🎊 Palette speckles popping
        LightSceneID::juggle,            // 🤹 Weaving palette dots
        LightSceneID::sinelon,           // 〰️ Sine-swinging dot with trail
        LightSceneID::bpm,               // 🥁 Throbbing gradient, speed = tempo
        LightSceneID::pacifica,          // 🌊 Kriegsman's gentle ocean
#endif
    };
    
    static const int totalCuratedEffects = sizeof(curatedEffects) / sizeof(curatedEffects[0]);
    static int currentIndex = 0;
    
    // Find current effect in curated list
    LightSceneID currentEffect = device.getState().currentEffect;
    for (int i = 0; i < totalCuratedEffects; i++) {
        if (curatedEffects[i] == currentEffect) {
            currentIndex = i;
            break;
        }
    }
    
    // Move to next/previous effect
    currentIndex += direction;
    if (currentIndex < 0) currentIndex = totalCuratedEffects - 1;
    if (currentIndex >= totalCuratedEffects) currentIndex = 0;
    
    LightSceneID newEffect = curatedEffects[currentIndex];
    device.setEffect(newEffect);
    
    Serial.print("Effect changed to: ");
    Serial.print(static_cast<int>(newEffect));
    Serial.print(" (");
    Serial.print(device.getLightShow().effectIdToName(newEffect));
    Serial.println(")");
}

// Function to cycle through all available palettes
void cyclePalette(int direction) {
    AvailablePalettes currentPalette = device.getState().currentPalette;
    int paletteIndex = static_cast<int>(currentPalette);
    
    // custom4 is the last id: the four slots the app uploads sit past the
    // built-ins. Empty ones are skipped below.
    const int totalPalettes = static_cast<int>(AvailablePalettes::custom4) + 1;
    
    // Walk until the next palette that is actually playable. The loop is
    // bounded by the palette count, so an encoder turn can never spin forever
    // even with every custom slot empty.
    for (int step = 0; step < totalPalettes; step++) {
        paletteIndex += direction;
        if (paletteIndex < 0) paletteIndex = totalPalettes - 1;
        if (paletteIndex >= totalPalettes) paletteIndex = 0;
        
        if (device.getLightShow().isPaletteAvailable(static_cast<AvailablePalettes>(paletteIndex))) {
            break;
        }
    }
    
    device.setPalette(static_cast<AvailablePalettes>(paletteIndex));
    Serial.print("Palette changed to: ");
    Serial.print(paletteIndex);
    Serial.print(" (");
    Serial.print(device.getLightShow().paletteIdToName(static_cast<AvailablePalettes>(paletteIndex)));
    Serial.println(")");
}

// Blink strip feedback for a menu change: one blink per menu position
// (1 = brightness ... 4 = speed) in the menu's colour, so a press answers
// "which knob am I holding" without a screen.
void announceMenu() {
    static const CRGB menuColors[MENU_COUNT] = {
        CRGB::White,   // brightness: plain light level
        CRGB::Green,   // effect
        CRGB::Magenta, // palette: colour
        CRGB::Blue,    // speed
    };
    static const char* menuNames[MENU_COUNT] = {
        "Brightness", "Effect", "Palette", "Speed"
    };
    device.flashFeedback(menuColors[currentMenu], currentMenu + 1);
    Serial.print("Menu changed to: ");
    Serial.println(menuNames[currentMenu]);
}

// Function to handle encoder changes based on current menu
void handleEncoderChange() {
    static long lastEncoderPosition = 0;

    // Silently fall back to brightness after a quiet spell (MENU_TIMEOUT_MS):
    // the next turn then does the expected thing instead of whatever menu was
    // left armed minutes ago.
    if (currentMenu != MENU_BRIGHTNESS && millis() - lastMenuChange > MENU_TIMEOUT_MS) {
        currentMenu = MENU_BRIGHTNESS;
        Serial.println("Menu timed out - back to Brightness");
    }

    // Apply every detent banked since the last pass, not just its sign - a
    // fast spin during a BLE burst used to collapse into a single step.
    long delta = encoderPosition - lastEncoderPosition;
    if (delta != 0) {
        lastEncoderPosition = encoderPosition;
        // A long stall still shouldn't bank a leap across the whole range.
        delta = constrain(delta, -8, 8);
        int8_t direction = (delta > 0) ? 1 : -1;

        Serial.print("Encoder changed, menu: ");
        Serial.print(currentMenu);
        Serial.print(", steps: ");
        Serial.println(delta);

        switch (currentMenu) {
            case MENU_BRIGHTNESS: {
                int brightness = device.getState().brightness;
                brightness += delta * 10; // Change by 10 each detent
                // Same ceiling the app respects: maxBrightness is stored as a
                // percent, brightness internally as a 1-255 level.
                int maxLevel = brightnessPercentToLevel(
                    device.getDefaults().getCurrentDefaults().maxBrightness);
                brightness = constrain(brightness, 1, maxLevel);
                device.setBrightness(brightness);
                
                // If user manually adjusts brightness, update target and skip fade-up
                #ifdef BM_HAS_FADEUP
                if (!fadeUpComplete) {
                    targetBrightness = brightness;
                    fadeUpComplete = true;
                    Serial.println("=== Manual brightness adjustment - fade-up cancelled ===");
                }
                #endif
                
                Serial.print("Brightness: ");
                Serial.println(brightness);
                break;
            }
            case MENU_SPEED: {
                int speed = device.getState().speed;
                speed -= delta * 5; // Change by 5 each detent
                speed = constrain(speed, 5, 200); // BMDevice constrains speed to 5-200
                device.getState().speed = speed;
                // Force light show update to apply new speed
                device.setEffect(device.getState().currentEffect);
                Serial.print("Speed: ");
                Serial.println(speed);
                break;
            }
            case MENU_PALETTE:
                for (long s = 0; s < abs(delta); s++) cyclePalette(direction);
                break;
            case MENU_EFFECT:
                for (long s = 0; s < abs(delta); s++) cycleEffect(direction);
                break;
        }

        // Push the new settings to a connected app (coalesced by BMDevice, so
        // spinning the wheel produces one update once it stops)
        device.markStatusDirty();

        lastMenuChange = millis();
    }
    
    // Handle button presses for menu navigation
    currentState = digitalRead(ENCODER_BUTTON_PIN);

    // Fix initial button press on boot
    if (!buttonInitialized) {
        lastState = currentState;
        buttonInitialized = true;
        return;
    }

    if (currentState == lastState) {
        return;
    }
    // An edge this soon after the last accepted one is contact bounce; leave
    // lastState alone and a real new level shows up as a fresh edge next pass.
    // (Replaces the blocking delay(100) that used to hitch the render loop.)
    if (millis() - lastButtonEdgeAt < BUTTON_DEBOUNCE_MS) {
        return;
    }
    lastButtonEdgeAt = millis();

    if (lastState == HIGH && currentState == LOW) {
        pressedTime = millis();
    }
    else if (lastState == LOW && currentState == HIGH) {
        releasedTime = millis();
        long pressDuration = releasedTime - pressedTime;
        
        if (pressDuration < SHORT_PRESS_TIME) {
            // Short Press - cycle through menu options, blink where it landed
            currentMenu = static_cast<EncoderMenu>((currentMenu + 1) % MENU_COUNT);
            announceMenu();
            lastMenuChange = millis();
        }
        else if (pressDuration > SHORT_PRESS_TIME && pressDuration < OFF_TIME) {
            // Long Press - toggle power
            bool power = device.getState().power;
            device.getState().power = !power;
            device.markStatusDirty();
            Serial.print("Power: ");
            Serial.println(device.getState().power ? "On" : "Off");
        }
    }
    lastState = currentState;
}


#endif

// ── Hotel sign: neon flicker driver for pin-25 strip ─────────────────────────
// The state machine, presets and BLE parameter handling now live in
// HotelSign.{h,cpp}; main.cpp only owns the controller and hands it the frame.
#ifdef TARGET_HOTEL_SIGN
CLEDController *neonCtrl = nullptr;
#endif
// ─────────────────────────────────────────────────────────────────────────────

// CPU clock. BLE, WiFi and the RMT-driven WS2812 output (APB-clocked) all run
// fine below the 240 MHz default, and every step down is a constant saving on
// battery. 160 is a safe default for 8-strip rigs; single-strip wearables can
// try -DBM_CPU_MHZ=80 in build_flags for more.
#ifndef BM_CPU_MHZ
#define BM_CPU_MHZ 160
#endif

void setup() {
    setCpuFrequencyMhz(BM_CPU_MHZ);

    Serial.begin(115200);
    delay(100);

    Serial.println("=== BMGeneric Device Starting ===");
    Serial.printf("CPU: %d MHz\n", getCpuFrequencyMhz());
    
    // Add LED strips based on chip type
    Serial.println("Adding LED strips...");
    
#ifdef TARGET_ESP32_C6
    // ESP32-C6: Single strip on GPIO 17
    device.addLEDStrip<WS2812B, 17, COLOR_ORDER>(leds0, LEDS_PER_STRIP);
    Serial.println("ESP32-C6 configuration:");
    Serial.printf("  Strip 0: GPIO 17, %d LEDs, GRB color order\n", LEDS_PER_STRIP);

#elif defined(TARGET_SLUT)
    // SLUT: 7 strips on the specified pins
    Serial.println("SLUT configuration:");
    device.addLEDStrip<WS2812B, 5, COLOR_ORDER>(leds0, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 14, COLOR_ORDER>(leds1, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 27, COLOR_ORDER>(leds2, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 26, COLOR_ORDER>(leds3, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 25, RGB>(leds4, LEDS_PER_STRIP);  // 12v 1
    device.addLEDStrip<WS2812B, 33, RGB>(leds5, LEDS_PER_STRIP);  // 12v 2
    device.addLEDStrip<WS2812B, 32, RGB>(leds6, LEDS_PER_STRIP);  // 12v 3

#elif defined(TARGET_CODYS_BIKE)
    // Cody's bike: chevron panel on the SLUT board's 12v 3 output, frame wrap
    // glow strip on 12v 2 (front -> rear down one side, rear -> front back up
    // the other), 8x32 matrix on the front rack off the first 5 V output.
    Serial.println("CODYS_BIKE configuration:");
    device.addLEDStrip<WS2811, 32, CHEVRON_COLOR_ORDER>(leds0, CODYS_BIKE_LED_COUNT);
    device.addLEDStrip<WS2811, 33, FRAME_COLOR_ORDER>(leds1, CODYS_BIKE_FRAME_LED_COUNT);
    device.addLEDStrip<WS2812B, 5, MATRIX_COLOR_ORDER>(leds2, FRONT_RACK_MATRIX_LED_COUNT);
    // Teach the show the bike's geometry - the panel's holes and the frame
    // strip's run alongside it in one shared rear->front coordinate space, and
    // the forward-facing matrix as that space's radial continuation (rings
    // around where the bike's axis pierces the sheet, stepping with the
    // chevrons) - so the chevron effects play the whole bike as one rig...
    device.getLightShow().setPixelMap(0, CODYS_BIKE_MAP, CODYS_BIKE_SEGMENTS);
    device.getLightShow().setPixelMap(1, CODYS_BIKE_FRAME_MAP, CODYS_BIKE_SEGMENTS);
    device.getLightShow().setPixelMap(2, FRONT_RACK_MATRIX_MAP, FRONT_RACK_MATRIX_SEGMENTS);
    // ...and give every 1D effect that same rear->front axis, so streams,
    // meteors and the rest sweep the bike - the frame's two sides in mirror,
    // the matrix as rings blooming out of its centre - instead of snaking the
    // serpentine or looping around the wrap.
    device.getLightShow().setRenderOrder(0, CODYS_BIKE_STREAM_ORDER);
    device.getLightShow().setRenderOrder(1, CODYS_BIKE_FRAME_STREAM_ORDER);
    device.getLightShow().setRenderOrder(2, FRONT_RACK_MATRIX_STREAM_ORDER);
    // The matrix is also a real display: rows and columns for the marquee
    // text and bitmap effects (the radial map above erases columns on purpose).
    device.getLightShow().setMatrixGrid(2, FRONT_RACK_MATRIX_WIDTH,
                                        FRONT_RACK_MATRIX_HEIGHT, FRONT_RACK_MATRIX_GRID);
    Serial.printf("  Strip 0: GPIO 32, %d LEDs, %d-chevron panel map\n",
                  CODYS_BIKE_LED_COUNT, CODYS_BIKE_SEGMENTS);
    Serial.printf("  Strip 1: GPIO 33, %d glow pixels (%d LEDs each), frame wrap map\n",
                  CODYS_BIKE_FRAME_LED_COUNT, CODYS_BIKE_FRAME_GROUP_SIZE);
    Serial.printf("  Strip 2: GPIO 5, %dx%d front-rack matrix, radial map\n",
                  FRONT_RACK_MATRIX_WIDTH, FRONT_RACK_MATRIX_HEIGHT);

#elif defined(TARGET_HOTEL_SIGN)
    // Hotel sign: 6 BLE-controlled strips + pin 25 managed by neon flicker code
    Serial.println("HOTEL_SIGN configuration:");
    device.addLEDStrip<WS2812B, 5, COLOR_ORDER>(leds0, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 14, COLOR_ORDER>(leds1, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 27, COLOR_ORDER>(leds2, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 26, COLOR_ORDER>(leds3, LEDS_PER_STRIP);
    // pin 25 (leds4) registered directly with FastLED — neon flicker, not BMDevice effects
    neonCtrl = &FastLED.addLeds<WS2812B, 25, RGB>(leds4, LEDS_PER_STRIP);
    device.addLEDStrip<WS2812B, 33, RGB>(leds5, LEDS_PER_STRIP);  // 12v 2
    device.addLEDStrip<WS2812B, 32, RGB>(leds6, LEDS_PER_STRIP);  // 12v 3
    hotelSign.begin(leds4, LEDS_PER_STRIP);
    Serial.println("  Pin 25 (leds4): neon hotel sign flicker, adjustable");

#elif TARGET_TAKOYAKI_SIGN
    // Takoyaki sign: same pinout as classic ESP32 with per-strip color order
    device.addLEDStrip<WS2812B, 32, STRIP0_COLOR_ORDER>(leds0, LEDS_PER_STRIP);  // Strip 0: GPIO 32
    device.addLEDStrip<WS2812B, 33, STRIP1_COLOR_ORDER>(leds1, LEDS_PER_STRIP);  // Strip 1: GPIO 33  // Board 2
    device.addLEDStrip<WS2812B, 27, STRIP2_COLOR_ORDER>(leds2, LEDS_PER_STRIP);  // Strip 2: GPIO 27  // Board 1
    device.addLEDStrip<WS2812B, 14, STRIP3_COLOR_ORDER>(leds3, LEDS_PER_STRIP);  // Strip 3: GPIO 1
    device.addLEDStrip<WS2812B, 12, STRIP4_COLOR_ORDER>(leds4, LEDS_PER_STRIP);  // Strip 4: GPIO 1   // Board 3
    device.addLEDStrip<WS2812B, 13, STRIP5_COLOR_ORDER>(leds5, LEDS_PER_STRIP);  // Strip 5: GPIO 13  // Board 4
    device.addLEDStrip<WS2812B, 18, STRIP6_COLOR_ORDER>(leds6, LEDS_PER_STRIP);  // Strip 6: GPIO 18  // Board 5
    device.addLEDStrip<WS2812B, 5, STRIP7_COLOR_ORDER>(leds7, LEDS_PER_STRIP);   // Strip 7: GPIO 5

    Serial.println("TAKOYAKI_SIGN configuration:");
    Serial.println("  Strip 0: GPIO 32, 450 LEDs, STRIP0_COLOR_ORDER");
    Serial.println("  Strip 1: GPIO 33, 450 LEDs, STRIP1_COLOR_ORDER");
    Serial.println("  Strip 2: GPIO 27, 450 LEDs, STRIP2_COLOR_ORDER");
    Serial.println("  Strip 3: GPIO 14, 450 LEDs, STRIP3_COLOR_ORDER");
    Serial.println("  Strip 4: GPIO 12, 450 LEDs, STRIP4_COLOR_ORDER");
    Serial.println("  Strip 5: GPIO 13, 450 LEDs, STRIP5_COLOR_ORDER");
    Serial.println("  Strip 6: GPIO 18, 450 LEDs, STRIP6_COLOR_ORDER");
    Serial.println("  Strip 7: GPIO 5, 450 LEDs, STRIP7_COLOR_ORDER");
    
#else
    // ESP32 classic: 8 strips on the specified pinspio 
    device.addLEDStrip<WS2812B, 32, COLOR_ORDER>(leds0, LEDS_PER_STRIP);  // Strip 0: GPIO 32
    device.addLEDStrip<WS2812B, 33, COLOR_ORDER>(leds1, LEDS_PER_STRIP);  // Strip 1: GPIO 33
    device.addLEDStrip<WS2812B, 27, COLOR_ORDER>(leds2, LEDS_PER_STRIP);  // Strip 2: GPIO 27
    device.addLEDStrip<WS2812B, 14, COLOR_ORDER>(leds3, LEDS_PER_STRIP);  // Strip 3: GPIO 14
    device.addLEDStrip<WS2812B, 12, COLOR_ORDER>(leds4, LEDS_PER_STRIP);  // Strip 4: GPIO 12
    device.addLEDStrip<WS2812B, 13, COLOR_ORDER>(leds5, LEDS_PER_STRIP);  // Strip 5: GPIO 13
    device.addLEDStrip<WS2812B, 18, COLOR_ORDER>(leds6, LEDS_PER_STRIP);  // Strip 6: GPIO 18
    device.addLEDStrip<WS2812B, 5, COLOR_ORDER>(leds7, LEDS_PER_STRIP);   // Strip 7: GPIO 5
    
    Serial.println("ESP32 classic configuration:");
    Serial.println("  Strip 0: GPIO 32, 450 LEDs, GRB color order");
    Serial.println("  Strip 1: GPIO 33, 450 LEDs, GRB color order");
    Serial.println("  Strip 2: GPIO 27, 450 LEDs, GRB color order");
    Serial.println("  Strip 3: GPIO 14, 450 LEDs, GRB color order");
    Serial.println("  Strip 4: GPIO 12, 450 LEDs, GRB color order");
    Serial.println("  Strip 5: GPIO 13, 450 LEDs, GRB color order");
    Serial.println("  Strip 6: GPIO 18, 450 LEDs, GRB color order");
    Serial.println("  Strip 7: GPIO 5, 450 LEDs, GRB color order");
#endif
    
#ifdef BM_HAS_GPS
    Serial.println("ENABLE_GPS_ON_UPLOAD is true - enabling GPS...");
    Serial.printf("About to call enableGPS with pins RX:%d TX:%d @ %d baud\n", GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);
    device.enableGPS(GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);
    Serial.printf("GPS enable call completed - pins RX:%d TX:%d @ %d baud\n", GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);
#else
    Serial.println("ENABLE_GPS_ON_UPLOAD is false - GPS disabled");
#endif

#ifdef BM_HAS_ENCODER
    // Initialize encoder pins
    Serial.println("Initializing encoder...");
    pinMode(ENCODER_PIN_A, INPUT_PULLUP);
    pinMode(ENCODER_PIN_B, INPUT_PULLUP);
    pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);

    // Attach interrupts for encoder
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), handleEncoder, CHANGE);
    attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), handleEncoder, CHANGE);

    // Initialize pressedTime and releasedTime to avoid incorrect long press detection
    pressedTime = millis();
    releasedTime = millis();

    Serial.printf("Encoder initialized - Pins A:%d B:%d Button:%d\n", ENCODER_PIN_A, ENCODER_PIN_B, ENCODER_BUTTON_PIN);
    Serial.println("Encoder Menu Controls:");
    Serial.println("  Rotate: Adjust current parameter");
    Serial.println("  Short Press: Change menu (Brightness -> Effect -> Palette -> Speed)");
    Serial.println("    Strips blink where it landed: 1 white / 2 green / 3 magenta / 4 blue");
    Serial.println("  Long Press: Toggle power on/off");
    Serial.println("  Menu falls back to Brightness after 30s idle");
#endif
    
    // Start the device (this will handle everything automatically)
    if (!device.begin()) {
        Serial.println("Failed to initialize BMDevice!");
        while (1);
    }

#ifdef TARGET_CODYS_BIKE
    // The frame wrap strip gangs 6 LEDs per addressable pixel - that's
    // hardware, not a preference, so assert it after begin() (which re-applies
    // the NVRAM group blob, default 1, over anything set earlier). Written
    // through defaults too so the app's "strips" chunk reports the truth; NVS
    // skips the write when the stored value already matches.
    device.getDefaults().setStripGroupSize(1, CODYS_BIKE_FRAME_GROUP_SIZE);
    device.getLightShow().setStripGroupSize(1, CODYS_BIKE_FRAME_GROUP_SIZE);
#endif

#ifdef BM_MAX_MILLIAMPS_PER_STRIP
    // Per-strip current ceiling at 5 V (strips are power-injected per strip, so
    // per-strip is the budget that means something). A frame that would draw
    // more is dimmed by FastLED's power maths - protects the pack and caps the
    // worst-case battery draw. Enable per env: -DBM_MAX_MILLIAMPS_PER_STRIP=2000
    device.getLightShow().setPowerBudgetPerStrip(BM_MAX_MILLIAMPS_PER_STRIP);
    Serial.printf("Power budget: %d mA per strip @ 5V\n", BM_MAX_MILLIAMPS_PER_STRIP);
#endif

#if OTA_ENABLED
    static char pendingSsid[33] = {0};

    device.setCustomFeatureHandler([&](uint8_t feature, const uint8_t* data, size_t length) {
#ifdef TARGET_HOTEL_SIGN
        if (hotelSign.handleFeature(feature, data, length)) {
            device.markStatusDirty();  // reflect the new sign settings back to the app
            return true;
        }
#endif
        if (feature == BLE_FEATURE_SET_WIFI_SSID && length >= 2) {
            size_t ssidLen = length - 1;
            if (ssidLen > 32) ssidLen = 32;
            memset(pendingSsid, 0, sizeof(pendingSsid));
            memcpy(pendingSsid, data + 1, ssidLen);
            Serial.printf("[OTA] WiFi SSID buffered: %s (waiting for password)\n", pendingSsid);
            return true;
        }
        if (feature == BLE_FEATURE_SET_WIFI_PASSWORD && length >= 2) {
            char passBuf[64] = {0};
            size_t passLen = length - 1;
            if (passLen > 63) passLen = 63;
            memcpy(passBuf, data + 1, passLen);
            if (strlen(pendingSsid) > 0) {
                ota.setWifiCredentials(pendingSsid, passBuf);
                memset(pendingSsid, 0, sizeof(pendingSsid));
            } else {
                Serial.println("[OTA] Password received but no SSID buffered - ignoring");
            }
            return true;
        }
        if (feature == BLE_FEATURE_GET_WIFI_STATUS) {
            device.getBluetoothHandler().sendStatusUpdate(ota.getWifiStatusJson());
            return true;
        }
        if (feature == BLE_FEATURE_OTA_CHECK_NOW) {
            ota.checkNow();
            device.getBluetoothHandler().sendStatusUpdate(ota.getWifiStatusJson());
            return true;
        }
        return false;
    });
    ota.begin();
    Serial.println("[OTA] Update checks enabled - will check after boot delay");
#endif

    // Home Assistant: one MQTT-discovery light entity (on/off + brightness)
    // so the sign follows the room's Hue switches at home. Everything deeper
    // (effects, palettes) stays with the app/watch/encoder. No-op unless the
    // build keeps WiFi alive and WiFiCreds.h names a broker.
    mqttBridge.begin(&device);

#ifdef TARGET_HOTEL_SIGN
#if !OTA_ENABLED
    // No OTA handler to piggy-back on, so the sign owns the custom handler.
    device.setCustomFeatureHandler([&](uint8_t feature, const uint8_t* data, size_t length) {
        if (hotelSign.handleFeature(feature, data, length)) {
            device.markStatusDirty();
            return true;
        }
        return false;
    });
#endif
    // Let the app's sign controls track the hardware: this rides along with the
    // normal status burst on connect and after every change.
    device.registerStatusChunk("sign", []() {
        device.getBluetoothHandler().sendStatusUpdate(hotelSign.statusJson());
    }, "Hotel sign neon flicker parameters");
#endif

#ifdef BM_HAS_FADEUP
    // Store the target brightness that was loaded from defaults
    targetBrightness = device.getState().brightness;

    // Override with very low brightness to start the fade-up
    device.setBrightness(currentFadeBrightness);

    // Start the fade timer
    fadeStartTime = millis();

    Serial.print("=== Starting brightness fade-up ===");
    Serial.print("Target brightness: ");
    Serial.print(targetBrightness);
    Serial.print(", Starting from: ");
    Serial.println(currentFadeBrightness);

#endif
    
    Serial.println("=== BMGeneric Device Ready ===");
    Serial.println("All configuration is now handled by BMDevice library!");
    Serial.println("Use BLE commands to configure the device:");
    Serial.println("  BLE_FEATURE_SET_OWNER (0x30) - Set owner");
    Serial.println("  BLE_FEATURE_SET_DEVICE_TYPE (0x31) - Set device type");
    Serial.println("  BLE_FEATURE_CONFIGURE_LED_STRIP (0x32) - Configure LED strip");
    Serial.println("  BLE_FEATURE_GET_CONFIGURATION (0x33) - Get configuration");
    Serial.println("  BLE_FEATURE_RESET_TO_DEFAULTS (0x34) - Reset to defaults");
    
#ifdef BM_HAS_GPS
    Serial.println("");
    Serial.println("GPS Status:");
    Serial.printf("  GPS Enabled: YES (pins RX:%d TX:%d @ %d baud)\n", GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);
    if (device.getState().positionAvailable) {
        Position pos = device.getState().currentPosition;
        Serial.printf("  Position: %.6f, %.6f\n", pos.latitude(), pos.longitude());
        Serial.printf("  Speed: %.2f mph\n", device.getState().currentSpeed);
        Serial.println("  Status: GPS fix acquired");
    } else {
        Serial.println("  Status: Waiting for GPS fix...");
        Serial.println("  Note: GPS fix can take 30-60 seconds outdoors");
    }
#else
    Serial.println("");
    Serial.println("GPS Status: DISABLED");
    Serial.println("  To enable GPS, set ENABLE_GPS_ON_UPLOAD to true");
#endif
    
    Serial.flush();
}

void loop() {
#if OTA_ENABLED
    ota.loop();
    if (ota.isUpdating()) {
        // Every registered strip solid red while the new image downloads,
        // re-shown at a relaxed pace so the OTA task gets the CPU.
        FastLED.showColor(CRGB::Red);
        delay(100);
        return;
    }
#endif

#ifdef BM_HAS_FADEUP
    // Handle brightness fade-up to prevent power spikes
    if (!fadeUpComplete) {
        unsigned long elapsed = millis() - fadeStartTime;
        
        if (elapsed < FADE_DURATION) {
            // Calculate current brightness using smooth interpolation
            float progress = (float)elapsed / FADE_DURATION;
            // Use ease-out curve for smoother fade
            progress = 1.0f - (1.0f - progress) * (1.0f - progress);
            
            int newBrightness = currentFadeBrightness + (progress * (targetBrightness - currentFadeBrightness));
            newBrightness = constrain(newBrightness, 1, targetBrightness);
            
            // Only update if brightness changed to avoid unnecessary calls
            if (newBrightness != device.getState().brightness) {
                device.setBrightness(newBrightness);
            }
        } else {
            // Fade complete - set final target brightness
            device.setBrightness(targetBrightness);
            fadeUpComplete = true;
            Serial.print("=== Brightness fade-up complete! Final brightness: ");
            Serial.println(targetBrightness);
        }
    }
#endif
    
    device.loop();
    mqttBridge.loop();

#ifdef TARGET_HOTEL_SIGN
    if (hotelSign.update(device.getState().power, device.getState().brightness)) {
        neonCtrl->showLeds(255);  // flush neon data — LightShow only calls per-controller show, never touches leds4
    }
#endif

#ifdef BM_HAS_ENCODER
    handleEncoderChange();
    yield();
#endif
} 