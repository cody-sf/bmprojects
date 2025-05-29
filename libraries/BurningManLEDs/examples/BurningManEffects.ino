#include <FastLED.h>
#include <LightShow.h>

// LED Configuration
#define LED_PIN     6
#define NUM_LEDS    60
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];
CLEDController* controller;
LightShow lightShow;

void setup() {
    Serial.begin(115200);
    Serial.println("🔥 BURNING MAN LED EFFECTS DEMO 🔥");
    
    // Initialize FastLED
    controller = &FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(100);
    
    // Add LED controller to LightShow
    lightShow.add_led_controller(controller);
    lightShow.brightness(100);
    
    Serial.println("Ready to light up the playa! 🌟");
}

void loop() {
    // Cycle through all the new amazing effects!
    
    Serial.println("🌊 Pulse Wave Effect - Electric Desert Palette");
    lightShow.pulse_wave(50, 8, AvailablePalettes::electric_desert);
    runEffect(8000); // Run for 8 seconds
    
    Serial.println("☄️ Meteor Shower - Psychedelic Playa Palette");
    lightShow.meteor_shower(30, 3, 8, AvailablePalettes::psychedelic_playa);
    runEffect(10000);
    
    Serial.println("🔥 Fire Plasma - Cosmic Fire Palette");
    lightShow.fire_plasma(25, 100, AvailablePalettes::cosmic_fire);
    runEffect(12000);
    
    Serial.println("🌀 Kaleidoscope - Neon Nights Palette");
    lightShow.kaleidoscope(40, 4, AvailablePalettes::neon_nights);
    runEffect(8000);
    
    Serial.println("🌈 Rainbow Comet");
    lightShow.rainbow_comet(20, 2, 6);
    runEffect(10000);
    
    Serial.println("💻 Matrix Rain - Classic Green");
    lightShow.matrix_rain(15, 80, CRGB::Green);
    runEffect(8000);
    
    Serial.println("☁️ Plasma Clouds - Alien Glow Palette");
    lightShow.plasma_clouds(30, 6, AvailablePalettes::alien_glow);
    runEffect(10000);
    
    Serial.println("🌋 Lava Lamp - Molten Metal Palette");
    lightShow.lava_lamp(80, 3, AvailablePalettes::molten_metal);
    runEffect(12000);
    
    Serial.println("🌌 Aurora Borealis - Burning Rainbow Palette");
    lightShow.aurora_borealis(45, 5, AvailablePalettes::burning_rainbow);
    runEffect(15000);
    
    Serial.println("⚡ Lightning Storm");
    lightShow.lightning_storm(100, 200, 500);
    runEffect(10000);
    
    Serial.println("💥 Color Explosion - Desert Storm Palette");
    lightShow.color_explosion(35, 12, AvailablePalettes::desert_storm);
    runEffect(8000);
    
    Serial.println("🌌 Spiral Galaxy - Electric Desert Palette");
    lightShow.spiral_galaxy(25, 3, AvailablePalettes::electric_desert);
    runEffect(12000);
    
    Serial.println("🎉 Cycle complete! Starting over...\n");
    delay(2000);
}

void runEffect(unsigned long duration) {
    unsigned long startTime = millis();
    while (millis() - startTime < duration) {
        lightShow.render();
        FastLED.show();
        delay(10); // Small delay for smooth animation
    }
}

/* 
🔥 BURNING MAN EFFECT DESCRIPTIONS 🔥

🌊 PULSE WAVE: Expanding waves of color radiating from a moving center point
☄️ METEOR SHOWER: Bright meteors with fading trails streaking across your LEDs  
🔥 FIRE PLASMA: Realistic fire simulation with heat diffusion and random sparks
🌀 KALEIDOSCOPE: Mesmerizing mirrored patterns that shift and flow
🌈 RAINBOW COMET: Fast-moving rainbow comets with colorful trails
💻 MATRIX RAIN: Digital rain effect like The Matrix
☁️ PLASMA CLOUDS: Smooth flowing plasma clouds with organic movement
🌋 LAVA LAMP: Floating color blobs that move like a lava lamp
🌌 AURORA BOREALIS: Northern lights effect with flowing waves
⚡ LIGHTNING STORM: Dramatic lightning flashes with storm clouds
💥 COLOR EXPLOSION: Explosive waves of color radiating outward
🌌 SPIRAL GALAXY: Rotating spiral arms of light

NEW PALETTES:
- Electric Desert: Inspired by the playa at night
- Psychedelic Playa: Wild, trippy color combinations  
- Burning Rainbow: Intense saturated rainbow colors
- Neon Nights: Cyberpunk neon colors
- Desert Storm: Dust and lightning colors
- Cosmic Fire: Space and fire combined
- Alien Glow: Otherworldly alien colors
- Molten Metal: Hot metal heating up and cooling down

These effects will make you the most visible and awesome person 
at Burning Man! Perfect for backpacks, bikes, art cars, and more!

Have fun lighting up the playa! 🌟🔥✨
*/ 