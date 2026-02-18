#include "BMDeviceState.h"

BMDeviceState::BMDeviceState() {
    initializeDefaults();
}

void BMDeviceState::initializeDefaults() {
    // Core device state
    power = true;
    brightness = 10;
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
    
    // GPS Speed control defaults
    gpsLowSpeed = 5.0;   // 5 km/h
    gpsTopSpeed = 25.0;  // 25 km/h  
    gpsLightshowSpeedEnabled = false;
    
    // GPS Speedometer color defaults
    gpsSlowColor = CRGB::Red;     // Red for slow speeds
    gpsFastColor = CRGB::Blue;    // Blue for fast speeds
}

String BMDeviceState::toJSON() const {
    StaticJsonDocument<1024> doc; // Increased size for all effect parameters
    
    // Core device state
    doc["pwr"] = power;
    doc["bri"] = brightness;
    doc["spd"] = speed;
    doc["dir"] = reverseStrip;
    doc["fx"] = LightShow::effectIdToName(currentEffect);
    doc["pal"] = LightShow::paletteIdToName(currentPalette);
    
    // All effect parameters
    doc["waveWidth"] = waveWidth;
    doc["meteorCount"] = meteorCount;
    doc["trailLength"] = trailLength;
    doc["heatVariance"] = heatVariance;
    doc["mirrorCount"] = mirrorCount;
    doc["cometCount"] = cometCount;
    doc["dropRate"] = dropRate;
    doc["cloudScale"] = cloudScale;
    doc["blobCount"] = blobCount;
    doc["waveCount"] = waveCount;
    doc["flashIntensity"] = flashIntensity;
    doc["flashFrequency"] = flashFrequency;
    doc["explosionSize"] = explosionSize;
    doc["spiralArms"] = spiralArms;
    
    // Effect color
    JsonObject effectColorObj = doc.createNestedObject("effectColor");
    effectColorObj["r"] = effectColor.r;
    effectColorObj["g"] = effectColor.g;
    effectColorObj["b"] = effectColor.b;
    
    // GPS/Position data
    doc["posAvail"] = positionAvailable;
    doc["spdCur"] = currentSpeed;
    
    // GPS Speed control settings
    doc["gpsLowSpd"] = gpsLowSpeed;
    doc["gpsTopSpd"] = gpsTopSpeed;
    doc["gpsLightSpdEn"] = gpsLightshowSpeedEnabled;
    
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
    if (doc.containsKey("trailLength")) {
        trailLength = doc["trailLength"];
        constrainEffectParameter(trailLength, 1, 30);
    }
    if (doc.containsKey("heatVariance")) {
        heatVariance = doc["heatVariance"];
        constrainEffectParameter(heatVariance, 1, 100);
    }
    if (doc.containsKey("mirrorCount")) {
        mirrorCount = doc["mirrorCount"];
        constrainEffectParameter(mirrorCount, 1, 10);
    }
    if (doc.containsKey("cometCount")) {
        cometCount = doc["cometCount"];
        constrainEffectParameter(cometCount, 1, 10);
    }
    if (doc.containsKey("dropRate")) {
        dropRate = doc["dropRate"];
        constrainEffectParameter(dropRate, 1, 100);
    }
    if (doc.containsKey("cloudScale")) {
        cloudScale = doc["cloudScale"];
        constrainEffectParameter(cloudScale, 1, 50);
    }
    if (doc.containsKey("blobCount")) {
        blobCount = doc["blobCount"];
        constrainEffectParameter(blobCount, 1, 20);
    }
    if (doc.containsKey("waveCount")) {
        waveCount = doc["waveCount"];
        constrainEffectParameter(waveCount, 1, 15);
    }
    if (doc.containsKey("flashIntensity")) {
        flashIntensity = doc["flashIntensity"];
        constrainEffectParameter(flashIntensity, 1, 100);
    }
    if (doc.containsKey("flashFrequency")) {
        flashFrequency = doc["flashFrequency"];
        constrainEffectParameter(flashFrequency, 100, 5000);
    }
    if (doc.containsKey("explosionSize")) {
        explosionSize = doc["explosionSize"];
        constrainEffectParameter(explosionSize, 1, 50);
    }
    if (doc.containsKey("spiralArms")) {
        spiralArms = doc["spiralArms"];
        constrainEffectParameter(spiralArms, 1, 10);
    }
    
    // Update effect color
    if (doc.containsKey("effectColor")) {
        JsonObject colorObj = doc["effectColor"];
        if (colorObj.containsKey("r") && colorObj.containsKey("g") && colorObj.containsKey("b")) {
            effectColor.r = constrain(colorObj["r"], 0, 255);
            effectColor.g = constrain(colorObj["g"], 0, 255);
            effectColor.b = constrain(colorObj["b"], 0, 255);
        }
    }
    
    // Update GPS/position data
    if (doc.containsKey("posAvail")) positionAvailable = doc["posAvail"];
    if (doc.containsKey("spdCur")) currentSpeed = doc["spdCur"];
    if (doc.containsKey("pos")) {
        JsonObject posObj = doc["pos"];
        if (posObj.containsKey("lat") && posObj.containsKey("lon")) {
            currentPosition = Position(posObj["lat"], posObj["lon"]);
        }
    }
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