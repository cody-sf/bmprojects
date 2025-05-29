# 🔥 Burning Man LED Effects Library 🔥

**Light up the playa with spectacular LED effects perfect for Burning Man!**

This library provides an extensive collection of modern, eye-catching LED effects designed specifically for the desert environment and artistic expression of Burning Man.

## ✨ New Effects

### 🌊 Pulse Wave
Expanding waves of color that radiate from a moving center point, creating mesmerizing ripple effects.
```cpp
lightShow.pulse_wave(duration, wave_width, palette);
```

### ☄️ Meteor Shower  
Bright meteors with fading trails that streak across your LED strips, simulating a cosmic light show.
```cpp
lightShow.meteor_shower(duration, meteor_count, trail_length, palette);
```

### 🔥 Fire Plasma
Realistic fire simulation with heat diffusion, random sparks, and organic flame movement.
```cpp
lightShow.fire_plasma(duration, heat_variance, palette);
```

### 🌀 Kaleidoscope
Mesmerizing mirrored patterns that shift and flow, creating complex geometric designs.
```cpp
lightShow.kaleidoscope(duration, mirror_count, palette);
```

### 🌈 Rainbow Comet
Fast-moving rainbow comets with colorful trails that cycle through the spectrum.
```cpp
lightShow.rainbow_comet(duration, comet_count, trail_length);
```

### 💻 Matrix Rain
Digital rain effect inspired by The Matrix, with customizable colors and drop rates.
```cpp
lightShow.matrix_rain(duration, drop_rate, color);
```

### ☁️ Plasma Clouds
Smooth flowing plasma clouds with organic movement and seamless color transitions.
```cpp
lightShow.plasma_clouds(duration, cloud_scale, palette);
```

### 🌋 Lava Lamp
Floating color blobs that move slowly like a lava lamp, perfect for chill vibes.
```cpp
lightShow.lava_lamp(duration, blob_count, palette);
```

### 🌌 Aurora Borealis
Northern lights effect with flowing waves and ethereal movement patterns.
```cpp
lightShow.aurora_borealis(duration, wave_count, palette);
```

### ⚡ Lightning Storm
Dramatic lightning flashes with storm clouds and realistic electrical effects.
```cpp
lightShow.lightning_storm(duration, flash_intensity, flash_frequency);
```

### 💥 Color Explosion
Explosive waves of color that radiate outward from random centers.
```cpp
lightShow.color_explosion(duration, explosion_size, palette);
```

### 🌌 Spiral Galaxy
Rotating spiral arms of light that create cosmic galaxy patterns.
```cpp
lightShow.spiral_galaxy(duration, spiral_arms, palette);
```

## 🎨 New Palettes

### Electric Desert
Inspired by the playa at night - bright oranges, electric blues, and hot pinks.

### Psychedelic Playa  
Wild, trippy color combinations perfect for the surreal desert experience.

### Burning Rainbow
Intense saturated rainbow colors that pop in the desert environment.

### Neon Nights
Cyberpunk-inspired neon colors - perfect for futuristic art installations.

### Desert Storm
Dust browns, lightning golds, and storm grays capturing the power of desert weather.

### Cosmic Fire
Space and fire combined - deep blues transitioning to bright flames.

### Alien Glow
Otherworldly alien colors in greens, teals, and purples.

### Molten Metal
Hot metal colors showing the progression from cold to molten white-hot.

## 🚀 Quick Start

```cpp
#include <FastLED.h>
#include <LightShow.h>

#define LED_PIN 6
#define NUM_LEDS 60

CRGB leds[NUM_LEDS];
CLEDController* controller;
LightShow lightShow;

void setup() {
    controller = &FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
    lightShow.add_led_controller(controller);
    lightShow.brightness(100);
}

void loop() {
    // Show off those new effects!
    lightShow.fire_plasma(25, 100, AvailablePalettes::cosmic_fire);
    
    while(true) {
        lightShow.render();
        FastLED.show();
        delay(10);
    }
}
```

## 🏜️ Perfect For

- **Backpacks** - Stand out in the crowd
- **Bicycles** - Be seen and be awesome  
- **Art Cars** - Create moving light sculptures
- **Camps** - Set the perfect ambiance
- **Costumes** - Become a living light show
- **Art Installations** - Interactive desert experiences

## 🔧 Hardware Requirements

- ESP32 or Arduino-compatible microcontroller
- WS2812B/NeoPixel LED strips or matrices
- Adequate power supply for your LED count
- FastLED library

## 💡 Pro Tips

1. **Power Management**: Calculate your power needs - these effects can be bright!
2. **Heat Management**: Use heat sinks in the desert sun
3. **Dust Protection**: Seal your electronics from playa dust
4. **Battery Life**: Test your setup thoroughly before heading to the playa
5. **Brightness**: Start at 50% brightness and adjust - these effects are vivid!

## 🌟 Effect Parameters Guide

- **Duration**: Lower = faster animation (try 20-50ms for fast, 100ms+ for slow)
- **Wave Width**: Controls the spread of pulse effects (4-12 works well)
- **Meteor Count**: 2-5 meteors look great, more can be overwhelming
- **Heat Variance**: 50-150 for realistic fire, 200+ for wild flames
- **Trail Length**: 4-10 pixels for good comet/meteor trails

## 🎭 Burning Man Integration

These effects are designed with Burning Man's principles in mind:

- **Radical Self-Expression**: Stand out with unique, personalized light shows
- **Gifting**: Share the joy of spectacular lights with fellow burners
- **Participation**: Interactive effects that respond to the environment
- **Leave No Trace**: Efficient code that respects battery life

## 🚨 Safety Notes

- Test all effects thoroughly before the event
- Bring backup power and spare components
- Use appropriate gauge wiring for your LED count
- Consider eye safety with very bright effects
- Respect others' space with intense strobing effects

---

**Have an absolutely spectacular burn! May your LEDs shine bright and your art inspire wonder! 🌟🔥✨**

*Built with love for the Burning Man community* 