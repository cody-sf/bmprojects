#include <BMDevice.h>

// Default UUIDs for generic device
#define SERVICE_UUID "4746abe4-2135-4a84-8f2f-f47f3a73e73b"
#define FEATURES_UUID "3927e9db-012b-4db9-8890-984fe28faf83"
#define STATUS_UUID "c054c450-cf93-4e6f-848e-2c521e739f4b"




// Chip-specific LED Configuration
#ifdef TARGET_ESP32_C6
    // ESP32-C6 configuration (single strip for now)
    #define NUM_STRIPS 1
    #define LEDS_PER_STRIP 350
    #define COLOR_ORDER GRB

    CRGB leds0[LEDS_PER_STRIP];

#elif TARGET_SLUT
    #define NUM_STRIPS 8
    #define LEDS_PER_STRIP 350
    #define COLOR_ORDER GRB

    // GPS Pin Configuration (only used if GPS is enabled)
    #define GPS_RX_PIN 21
    #define GPS_TX_PIN 22
    #define GPS_BAUD_RATE 9600  // Confirmed working baud rate

    CRGB leds0[LEDS_PER_STRIP], leds1[LEDS_PER_STRIP], leds2[LEDS_PER_STRIP], leds3[LEDS_PER_STRIP];
    CRGB leds4[LEDS_PER_STRIP], leds5[LEDS_PER_STRIP], leds6[LEDS_PER_STRIP], leds7[LEDS_PER_STRIP];

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
    if (encoderState == 4)
    {
        encoderPosition = encoderPosition + 1; // Increment position after a full cycle
        encoderState = 0;  // Reset state
    }
    else if (encoderState == -4)
    {
        encoderPosition = encoderPosition - 1; // Decrement position after a full cycle
        encoderState = 0;  // Reset state
    }
    lastEncoded = encoded; // Store this value for next time
    }
    // Button Press Stuff
    int lastState = LOW; // the previous state from the input pin
    int currentState;    // the current reading from the input pin
    unsigned long pressedTime = 0;
    unsigned long releasedTime = 0;
    const int SHORT_PRESS_TIME = 1000;
    const int OFF_TIME = 5000;
    
    // Menu system for encoder control
    enum EncoderMenu {
        MENU_BRIGHTNESS = 0,
        MENU_SPEED,
        MENU_PALETTE,
        MENU_EFFECT,
        MENU_COUNT  // Total number of menu items
    };
    
    EncoderMenu currentMenu = MENU_BRIGHTNESS;
    bool buttonInitialized = false;
    unsigned long lastMenuChange = 0;
    const unsigned long MENU_DISPLAY_TIME = 2000; // Show menu selection for 2 seconds

#else
    // ESP32 classic configuration (8 strips)
    #define NUM_STRIPS 8
    #define LEDS_PER_STRIP 450
    #define COLOR_ORDER GRB
    
    // Individual LED strip arrays for all 8 strips
    CRGB leds0[LEDS_PER_STRIP], leds1[LEDS_PER_STRIP], leds2[LEDS_PER_STRIP], leds3[LEDS_PER_STRIP];
    CRGB leds4[LEDS_PER_STRIP], leds5[LEDS_PER_STRIP], leds6[LEDS_PER_STRIP], leds7[LEDS_PER_STRIP];
#endif

// BMDevice instance with dynamic naming
BMDevice device(SERVICE_UUID, FEATURES_UUID, STATUS_UUID);

#ifdef TARGET_SLUT
// Function to cycle through curated light show effects
void cycleEffect(int direction) {
    // Curated list of visually distinct effects
    static const LightSceneID curatedEffects[] = {
        LightSceneID::meteor_shower,     // ☄️ Meteors with trails
        LightSceneID::fire_plasma,       // 🔥 Realistic fire
        LightSceneID::kaleidoscope,      // 🌀 Mirrored patterns
        LightSceneID::rainbow_comet,     // 🌈 Rainbow comets
        LightSceneID::matrix_rain,       // 💻 Matrix-style rain
        LightSceneID::plasma_clouds,     // ☁️ Flowing plasma
        LightSceneID::aurora_borealis,   // 🌌 Northern lights
        LightSceneID::color_explosion,   // 💥 Explosive colors
        LightSceneID::spiral_galaxy,     // 🌌 Rotating spirals
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
    
    // Total number of palettes - moltenmetal is the last one (81)
    const int totalPalettes = static_cast<int>(AvailablePalettes::moltenmetal) + 1;
    
    paletteIndex += direction;
    if (paletteIndex < 0) paletteIndex = totalPalettes - 1;
    if (paletteIndex >= totalPalettes) paletteIndex = 0;
    
    device.setPalette(static_cast<AvailablePalettes>(paletteIndex));
    Serial.print("Palette changed to: ");
    Serial.print(paletteIndex);
    Serial.print(" (");
    Serial.print(device.getLightShow().paletteIdToName(static_cast<AvailablePalettes>(paletteIndex)));
    Serial.println(")");
}

// Function to handle encoder changes based on current menu
void handleEncoderChange() {
    static long lastEncoderPosition = 0;
    
    // Check if the encoder position has changed
    if (encoderPosition != lastEncoderPosition) {
        int8_t direction = (encoderPosition > lastEncoderPosition) ? 1 : -1;
        lastEncoderPosition = encoderPosition;
        
        Serial.print("Encoder changed, menu: ");
        Serial.print(currentMenu);
        Serial.print(", direction: ");
        Serial.println(direction);
        
        switch (currentMenu) {
            case MENU_BRIGHTNESS: {
                int brightness = device.getState().brightness;
                brightness += direction * 10; // Change by 10 each step
                brightness = constrain(brightness, 10, 175);
                device.setBrightness(brightness);
                Serial.print("Brightness: ");
                Serial.println(brightness);
                break;
            }
            case MENU_SPEED: {
                int speed = device.getState().speed;
                speed -= direction * 5; // Change by 5 each step
                speed = constrain(speed, 5, 200); // BMDevice constrains speed to 5-200
                device.getState().speed = speed;
                // Force light show update to apply new speed
                device.setEffect(device.getState().currentEffect);
                Serial.print("Speed: ");
                Serial.println(speed);
                break;
            }
            case MENU_PALETTE:
                cyclePalette(direction);
                break;
            case MENU_EFFECT:
                cycleEffect(direction);
                break;
        }
        
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
    
    if (lastState == HIGH && currentState == LOW) {
        pressedTime = millis();
    }
    else if (lastState == LOW && currentState == HIGH) {
        releasedTime = millis();
        long pressDuration = releasedTime - pressedTime;
        
        if (pressDuration < SHORT_PRESS_TIME) {
            // Short Press - cycle through menu options
            currentMenu = static_cast<EncoderMenu>((currentMenu + 1) % MENU_COUNT);
            Serial.print("Menu changed to: ");
            switch (currentMenu) {
                case MENU_BRIGHTNESS: Serial.println("Brightness"); break;
                case MENU_SPEED: Serial.println("Speed"); break;
                case MENU_PALETTE: Serial.println("Palette"); break;
                case MENU_EFFECT: Serial.println("Effect"); break;
            }
            lastMenuChange = millis();
            delay(100);
        }
        else if (pressDuration > SHORT_PRESS_TIME && pressDuration < OFF_TIME) {
            // Long Press - toggle power
            bool power = device.getState().power;
            device.getState().power = !power;
            Serial.print("Power: ");
            Serial.println(device.getState().power ? "On" : "Off");
            delay(100);
        }
    }
    lastState = currentState;
}
#endif

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("=== BMGeneric Device Starting ===");
    
    // Add LED strips based on chip type
    Serial.println("Adding LED strips...");
    
#ifdef TARGET_ESP32_C6
    // ESP32-C6: Single strip on GPIO 17
    device.addLEDStrip<WS2812B, 17, COLOR_ORDER>(leds0, LEDS_PER_STRIP);
    Serial.println("ESP32-C6 configuration:");
    Serial.println("  Strip 0: GPIO 17, 350 LEDs, GRB color order");

#elif TARGET_SLUT
    // SLUT: 8 strips on the specified pins
    Serial.println("SLUT configuration:");
    device.addLEDStrip<WS2812B, 5, COLOR_ORDER>(leds0, LEDS_PER_STRIP);  // Strip 0: GPIO 32
    device.addLEDStrip<WS2812B, 14, COLOR_ORDER>(leds1, LEDS_PER_STRIP);  // Strip 1: GPIO 33
    device.addLEDStrip<WS2812B, 27, COLOR_ORDER>(leds2, LEDS_PER_STRIP);  // Strip 2: GPIO 27
    device.addLEDStrip<WS2812B, 26, COLOR_ORDER>(leds3, LEDS_PER_STRIP);  // Strip 3: GPIO 14
    device.addLEDStrip<WS2812B, 25, RGB>(leds4, LEDS_PER_STRIP);  //12v 1
    device.addLEDStrip<WS2812B, 33, RGB>(leds5, LEDS_PER_STRIP);  // Strip 5: GPIO 13
    device.addLEDStrip<WS2812B, 32, COLOR_ORDER>(leds6, LEDS_PER_STRIP);  // Strip 6: GPIO 18
    
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
    
#ifdef TARGET_SLUT
    Serial.println("ENABLE_GPS_ON_UPLOAD is true - enabling GPS...");
    Serial.printf("About to call enableGPS with pins RX:%d TX:%d @ %d baud\n", GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);
    device.enableGPS(GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);
    Serial.printf("GPS enable call completed - pins RX:%d TX:%d @ %d baud\n", GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);
    
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
    Serial.println("  Short Press: Change menu (Brightness -> Speed -> Palette -> Effect)");
    Serial.println("  Long Press: Toggle power on/off");
#else
    Serial.println("ENABLE_GPS_ON_UPLOAD is false - GPS disabled");
#endif
    
    // Start the device (this will handle everything automatically)
    if (!device.begin()) {
        Serial.println("Failed to initialize BMDevice!");
        while (1);
    }
    
    Serial.println("=== BMGeneric Device Ready ===");
    Serial.println("All configuration is now handled by BMDevice library!");
    Serial.println("Use BLE commands to configure the device:");
    Serial.println("  BLE_FEATURE_SET_OWNER (0x30) - Set owner");
    Serial.println("  BLE_FEATURE_SET_DEVICE_TYPE (0x31) - Set device type");
    Serial.println("  BLE_FEATURE_CONFIGURE_LED_STRIP (0x32) - Configure LED strip");
    Serial.println("  BLE_FEATURE_GET_CONFIGURATION (0x33) - Get configuration");
    Serial.println("  BLE_FEATURE_RESET_TO_DEFAULTS (0x34) - Reset to defaults");
    
#ifdef TARGET_SLUT
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
    device.loop();
    
#ifdef TARGET_SLUT
    // Handle encoder input for SLUT target
    handleEncoderChange();
    yield(); // Allow other tasks to run
#endif
} 