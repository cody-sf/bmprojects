// ESPNowSync.cpp

#include "SyncController.h"
#include "LightShow.h"
#include <DeviceRoles.h>
#include <map>
#include <string>

void (*SyncController::userCallback)(const uint8_t *mac, const uint8_t *data, int len) = nullptr;
SyncController *SyncController::instance_ = nullptr;
bool initialized = false;
#define ESPNOW_WIFI_CHANNEL 6

SyncController::SyncController(LightShow &light_show, const std::string &userIdentifier, Device &deviceType) : light_show_(light_show),
                                                                                                               brightness_(DEFAULT_BRIGHTNESS),
                                                                                                               shouldSync_(true),
                                                                                                               currentSetting_(PALETTE),
                                                                                                               origin_(Position(DEFAULT_ORIGIN_LATITUDE, DEFAULT_ORIGIN_LONGITUDE)),
                                                                                                               radius_inner_(DEFAULT_PLAYA_INNER_RADIUS),
                                                                                                               radius_outer_(DEFAULT_PLAYA_OUTER_RADIUS)
{
    instance_ = this;
    std::string deviceName = getDeviceName(deviceType);
    auto key = std::make_pair(userIdentifier, deviceName);
    // Set the MAC address based on the provided user identifier and device type
    if (deviceMap.find(key) != deviceMap.end())
    {
        Serial.println("Device found. Using MAC address.");
        memcpy(broadcastAddress, deviceMap[key].mac, 6);
    }
    else
    {
        Serial.println("Device not found. Using broadcast address.");
        memset(broadcastAddress, 0xFF, 6); // Broadcasting to all devices as a fallback
    }
}

void SyncController::readMacAddress()
{
    uint8_t baseMac[6];
    esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
    if (ret == ESP_OK)
    {
        Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
                      baseMac[0], baseMac[1], baseMac[2],
                      baseMac[3], baseMac[4], baseMac[5]);
    }
    else
    {
        Serial.println("Failed to read MAC address");
    }
}

void SyncController::begin(const std::string &userIdentifier = "")
{
    WiFi.mode(WIFI_STA);
    WiFi.setChannel(ESPNOW_WIFI_CHANNEL);
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    esp_now_register_recv_cb(SyncController::onDataReceivedStatic);
    addPeers(userIdentifier);
    Serial.print("[DEFAULT] ESP32 Board MAC Address: ");
    readMacAddress();
}

void SyncController::addPeers(const std::string &userIdentifier)
{
    for (const auto &pair : deviceMap)
    {
        // Check if the user identifier matches the given filter
        if (userIdentifier.empty() || pair.second.userIdentifier == userIdentifier)
        {
            esp_now_peer_info_t peerInfo = {};
            memcpy(peerInfo.peer_addr, pair.second.mac, 6);
            peerInfo.channel = ESPNOW_WIFI_CHANNEL;
            peerInfo.encrypt = false;

            if (!esp_now_is_peer_exist(pair.second.mac))
            {
                if (esp_now_add_peer(&peerInfo) == ESP_OK)
                {
                    Serial.print("Added peer: ");
                    Serial.print(pair.second.userIdentifier.c_str());
                    Serial.print(" - ");
                    Serial.println(pair.second.deviceType.c_str());
                }
                else
                {
                    Serial.println("Failed to add peer");
                }
            }
        }
    }
}

void SyncController::sendUpdate(const SyncData *data)
{
    if (shouldSync_)
    {
        for (const auto &pair : deviceMap)
        {
            esp_err_t result = esp_now_send(pair.second.mac, (uint8_t *)data, sizeof(SyncData));
            if (result == ESP_OK)
            {
                // Serial.println("Data sent successfully to peer");
            }
            else
            {
                // Serial.println("Error sending data to peer");
            }
        }
    }
}

void SyncController::onReceive(void (*callback)(const uint8_t *mac, const uint8_t *data, int len))
{
    userCallback = callback;
}
void SyncController::onDataReceivedStatic(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (instance_)
    {
        instance_->onDataReceived(info, data, len);
    }
}

void SyncController::onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{

    if (len == sizeof(SyncData) && shouldSync_)
    {
        // LightScene currentScene = light_show_.getCurrentScene();
        SyncData receivedData;
        memcpy(&receivedData, data, len);
        if (strncmp(receivedData.identifier, CURRENT_USER, 2) == 0)
        {
            switch (receivedData.messageType)
            {
            case SET_LIGHT_SCENE:
                Serial.print("Received Brightness: ");
                Serial.println(receivedData.scene.brightness);

                setCurrentDeviceScene(receivedData.scene);
                break;
            }
        }
    }
}

void SyncController::setCurrentDeviceScene(LightScene scene)
{
    light_show_.apply_sync_updates(scene);
}

void SyncController::setBrightness(uint8_t brightness)
{
    SyncData syncData;
    syncData.scene = light_show_.getCurrentScene();
    if (sync)
        strncpy(syncData.identifier, CURRENT_USER, 2);
    syncData.messageType = SET_LIGHT_SCENE;
    syncData.scene.brightness = brightness;
    Serial.print("Local Command - Set Brightness: ");
    Serial.println(syncData.scene.brightness);
    this->sendUpdate(&syncData);
    setCurrentDeviceScene(syncData.scene);
}

void SyncController::setSpeed(uint16_t speed)
{
    SyncData syncData;
    syncData.scene = light_show_.getCurrentScene();
    syncData.scene.speed = speed;
    Serial.print("Local Command - Set Speed: ");
    Serial.print(speed);
    Serial.println(syncData.scene.speed);

    setCurrentDeviceScene(syncData.scene);

    // Send the update to other devices
    strncpy(syncData.identifier, CURRENT_USER, 2);
    syncData.messageType = SET_LIGHT_SCENE;
    this->sendUpdate(&syncData);
}

void SyncController::setPalette(AvailablePalettes palette)
{
    SyncData syncData;
    syncData.scene = light_show_.getCurrentScene();
    syncData.scene.primary_palette = palette;
    Serial.print('setting current device palette to: ');
    Serial.println(palette);

    // Send the update to other devices
    strncpy(syncData.identifier, CURRENT_USER, 2);
    syncData.messageType = SET_LIGHT_SCENE;
    Serial.print("Local Command - Set Palette: ");
    Serial.println(syncData.scene.primary_palette);

    this->sendUpdate(&syncData);
    setCurrentDeviceScene(syncData.scene);
}

void SyncController::positionStatus(LocationService &location_service)
{
    Serial.println("Position Status in SyncController");
    Serial.print("Position Available?: ");
    Serial.println(location_service.is_current_position_available());

    SyncData syncData;
    syncData.scene = light_show_.getCurrentScene();
    syncData.scene.scene_id = solid;
    syncData.scene.color = location_service.is_current_position_available() ? CRGB::Green : CRGB::Red;

    // Send Update
    strncpy(syncData.identifier, CURRENT_USER, 2);
    syncData.messageType = SET_LIGHT_SCENE;
    Serial.print("Set Position Status: ");
    Serial.println(location_service.is_current_position_available() ? "Green" : "Red");
    light_show_.solid(syncData.scene.color);
    this->sendUpdate(&syncData);
}

void SyncController::speedometer(LocationService &location_service, CRGB color1, CRGB color2)
{
    Serial.println("Starting Speedometer Show");
    Serial.print("Speed: ");
    float currentSpeed = location_service.current_speed();
    Serial.println(currentSpeed);

    SyncData syncData;
    syncData.scene = light_show_.getCurrentScene();
    syncData.scene.scene_id = solid;
    syncData.scene.color = CRGB::Red;

    if (location_service.is_current_position_available())
    {
        // Ensure speed is within the defined range
        float normalizedSpeed = std::min(currentSpeed, static_cast<float>(MAX_SPEED)) / MAX_SPEED;
        // Interpolate between color1 and color2 based on the normalized speed
        CRGB new_color = blend(color1, color2, static_cast<uint8_t>(normalizedSpeed * 255));
        syncData.scene.color = new_color;
    }

    strncpy(syncData.identifier, CURRENT_USER, 2);
    syncData.messageType = SET_LIGHT_SCENE;
    light_show_.solid(syncData.scene.color);
    this->sendUpdate(&syncData);
}

void SyncController::colorWheel(LocationService &location_service)
{
    Serial.println("Color Wheel in SyncController");
    SyncData syncData;
    syncData.scene = light_show_.getCurrentScene();
    syncData.scene.scene_id = solid;
    syncData.scene.color = this->getColorWheelColor(location_service);

    // Send Update
    strncpy(syncData.identifier, CURRENT_USER, 2);
    syncData.messageType = SET_LIGHT_SCENE;
    Serial.print("Local Command - Set Color Wheel: ");
    this->sendUpdate(&syncData);
    light_show_.solid(syncData.scene.color);
}

void SyncController::colorRadial(LocationService &location_service, CRGB color1, CRGB color2)
{
    Serial.println("Color Radial in SyncController");
    SyncData syncData;
    syncData.scene = light_show_.getCurrentScene();
    syncData.scene.scene_id = solid;
    syncData.scene.color = this->getRadialColor(location_service, color1, color2);

    // Send Update
    strncpy(syncData.identifier, CURRENT_USER, 2);
    syncData.messageType = SET_LIGHT_SCENE;
    Serial.print("Local Command - Set Color Wheel: ");
    this->sendUpdate(&syncData);
    light_show_.solid(syncData.scene.color);
}

void SyncController::jacketDance(CRGB color)
{
    SyncData syncData;
    syncData.scene = light_show_.getCurrentScene();
    syncData.scene.scene_id = solid;
    syncData.scene.color = color;

    // Send Update
    strncpy(syncData.identifier, CURRENT_USER, 2);
    syncData.messageType = SET_LIGHT_SCENE;
    Serial.print("Local Command - Set Jacket Dance: ");
    this->sendUpdate(&syncData);
    light_show_.solid(syncData.scene.color);
}

void SyncController::handleButtonShortPress()
{
    LightScene currentScene = light_show_.getCurrentScene();
    SyncData syncData;
    AvailablePalettes newPalette;

    palette_index_ = (palette_index_ + 1) % light_show_.getPaletteCount();
    newPalette = static_cast<AvailablePalettes>(palette_index_);
    syncData.scene = currentScene;
    syncData.scene.primary_palette = newPalette;

    setCurrentDeviceScene(syncData.scene);
    // Send the update to other devices
    strncpy(syncData.identifier, CURRENT_USER, 2);
    syncData.currentSetting = currentSetting_;
    syncData.messageType = SET_LIGHT_SCENE;
    this->sendUpdate(&syncData);
}

void SyncController::handleButtonLongPress()
{
    // Cycle through settings
    switch (currentSetting_)
    {
    case PALETTE:
        currentSetting_ = PALETTE;
        break;
    }

    SyncData syncData;
    strncpy(syncData.identifier, CURRENT_USER, 2);
    syncData.messageType = SET_LIGHT_SCENE;
    syncData.currentSetting = currentSetting_;
    this->sendUpdate(&syncData);
}

void SyncController::handleDialChange(int8_t dialDirection)
{
    LightScene currentScene = light_show_.getCurrentScene();
    SyncData syncData;
    strncpy(syncData.identifier, CURRENT_USER, 2);
    syncData.scene = currentScene;

    uint8_t currentBrightness = currentScene.brightness;
    currentBrightness = constrain(currentBrightness + (dialDirection * 5), 0, 150); // Scale from 1 to 255

    Serial.print("Setting current brightness to: ");
    Serial.println(currentBrightness);

    syncData.scene.brightness = currentBrightness;
    syncData.messageType = SET_LIGHT_SCENE;
    setCurrentDeviceScene(syncData.scene);
    this->sendUpdate(&syncData);
}

void SyncController::changeMode(LightSceneID mode)
{
    LightScene currentScene = light_show_.getCurrentScene();
    SyncData syncData;
    syncData.scene = currentScene;
    syncData.scene.scene_id = mode;
    strncpy(syncData.identifier, CURRENT_USER, 2);
    syncData.messageType = SET_LIGHT_SCENE;
    if (mode == color_wheel || mode == position_status)
    {
        Serial.println("Changing to solid because detected either color wheel or position status");
        syncData.scene.scene_id = solid;
    }
    else
    {
        setCurrentDeviceScene(syncData.scene);
    }

    this->sendUpdate(&syncData);
}

void SyncController::setDirection(bool direction)
{
    direction_ = direction;
    SyncData syncData;
    syncData.scene = light_show_.getCurrentScene();
    syncData.scene.direction = direction;
    Serial.print('setting current device direction to: ');
    Serial.println(direction);

    // Send the update to other devices
    strncpy(syncData.identifier, CURRENT_USER, 2);
    syncData.messageType = SET_LIGHT_SCENE;

    this->sendUpdate(&syncData);
    setCurrentDeviceScene(syncData.scene);
}

void SyncController::shouldDeviceSync(bool shouldSync)
{
    shouldSync_ = shouldSync;
}

CRGB SyncController::getColorWheelColor(LocationService &location_service)
{
    if (location_service.is_current_position_available())
    {
        Position current_position = location_service.current_position();
        Position offset = current_position - origin_;
        uint8_t hue = 255 * offset.polar_angle() / (2 * PI);
        CRGB new_color = CHSV(hue, 255, 255);
        float distance = current_position.distance_from(origin_);
        unsigned int half_radius_inner = radius_inner_ >> 1;

        if (distance < half_radius_inner)
        {
            new_color = ORIGIN_COLOR;
        }
        else if (distance >= half_radius_inner && distance <= radius_inner_)
        {
            uint8_t scale_factor = 255 * (distance - half_radius_inner) / half_radius_inner;
            new_color = CRGB(ORIGIN_COLOR).lerp8(new_color, scale_factor);
        }
        return new_color;
    }
    else
    {
        // If the position is not available, return red
        Serial.println("Position not available");
        return CRGB::Red;
    }
}

CRGB SyncController::getRadialColor(LocationService &location_service, CRGB color1, CRGB color2)
{
    if (location_service.is_current_position_available())
    {
        Position current_position = location_service.current_position();
        float distance = current_position.distance_from(origin_);
        CRGB new_color = color1;

        if (distance <= radius_inner_)
        {
            // Blend from color1 to color2 within the inner radius
            uint8_t blend_factor = 255 * (distance / radius_inner_);
            new_color = color1.lerp8(color2, blend_factor);
        }
        else if (distance > radius_inner_ && distance <= radius_outer_)
        {
            // Blend from color2 to black (or any other color if needed) in the outer ring
            uint8_t blend_factor = 255 * ((distance - radius_inner_) / (radius_outer_ - radius_inner_));
            new_color = color2.lerp8(CRGB::Black, blend_factor);
        }

        return new_color;
    }

    // Default color if the position is not available
    return color1;
}

void SyncController::setOrigin(const Position &origin)
{
    origin_ = origin;
}

void SyncController::setRadiusInner(unsigned int radius_inner)
{
    radius_inner_ = radius_inner;
}

void SyncController::setRadiusOuter(unsigned int radius_outer)
{
    radius_outer_ = radius_outer;
}
