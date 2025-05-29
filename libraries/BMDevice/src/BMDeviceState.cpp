#include "BMDeviceState.h"

BMDeviceState::BMDeviceState() {
    initializeDefaults();
}

void BMDeviceState::initializeDefaults() {
    // Core device state
    power = true;
    brightness = 50;
    speed = 100;
    reverseStrip = true;
    
    // Effect state
    currentPalette = AvailablePalettes::cool;
    currentEffect = LightSceneID::palette_stream;
    
    // Effect parameters - matching your current defaults
    waveWidth = 10;
    meteorCount = 5;
    trailLength = 10;
    heatVariance = 50;
    mirrorCount = 4;
    cometCount = 3;
    dropRate = 25;
    cloudScale = 20;
    blobCount = 8;
    waveCount = 6;
    flashIntensity = 80;
    flashFrequency = 1000;
    explosionSize = 20;
    spiralArms = 4;
    effectColor = CRGB::Green;
    
    // Location state
    origin = Position(40.7868, -119.2065); // Default BRC location
    currentPosition = Position();
    positionAvailable = false;
    currentSpeed = 0;
}

String BMDeviceState::toJSON() const {
    StaticJsonDocument<512> doc;
    
    doc["pwr"] = power;
    doc["bri"] = brightness;
    doc["spd"] = speed;
    doc["dir"] = reverseStrip;
    doc["fx"] = LightShow::effectIdToName(currentEffect);
    doc["pal"] = LightShow::paletteIdToName(currentPalette);
    
    doc["posAvail"] = positionAvailable;
    doc["spdCur"] = currentSpeed;
    
    if (positionAvailable) {
        Position& currentPos = const_cast<Position&>(currentPosition);
        JsonObject posObj = doc.createNestedObject("pos");
        posObj["lat"] = currentPos.latitude();
        posObj["lon"] = currentPos.longitude();
    }
    
    String output;
    serializeJson(doc, output);
    return output;
}

void BMDeviceState::applyStateUpdate(const String& json) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        Serial.print("[BMDeviceState] JSON parse error: ");
        Serial.println(error.c_str());
        return;
    }
    
    // Update core state if present
    if (doc.containsKey("pwr")) power = doc["pwr"];
    if (doc.containsKey("bri")) {
        brightness = doc["bri"];
        constrainBrightness();
    }
    if (doc.containsKey("spd")) {
        speed = doc["spd"];
        constrainSpeed();
    }
    if (doc.containsKey("dir")) reverseStrip = doc["dir"];
    
    // Update effect state
    if (doc.containsKey("fxId")) {
        uint8_t effectId = doc["fxId"];
        if (effectId <= (uint8_t)LightSceneID::spiral_galaxy) {
            currentEffect = (LightSceneID)effectId;
        }
    }
    if (doc.containsKey("palId")) {
        uint8_t paletteId = doc["palId"];
        if (paletteId <= (uint8_t)AvailablePalettes::moltenmetal) {
            currentPalette = (AvailablePalettes)paletteId;
        }
    }
    
    // Update effect parameters with constraints
    if (doc.containsKey("waveWidth")) {
        waveWidth = doc["waveWidth"];
        constrainEffectParameter(waveWidth, 1, 50);
    }
    if (doc.containsKey("meteorCount")) {
        meteorCount = doc["meteorCount"];
        constrainEffectParameter(meteorCount, 1, 20);
    }
    // Add more parameter updates as needed...
}

void BMDeviceState::reset() {
    initializeDefaults();
}

void BMDeviceState::constrainBrightness(int min, int max) {
    brightness = constrain(brightness, min, max);
}

void BMDeviceState::constrainSpeed(int min, int max) {
    speed = constrain(speed, min, max);
}

void BMDeviceState::constrainEffectParameter(int& param, int min, int max) {
    param = constrain(param, min, max);
} 