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
                                                                                                               currentSetting_(PALETTE)

{
    instance_ = this;
    std::string deviceName = getDeviceName(deviceType);
    auto key = std::make_pair(userIdentifier, deviceName);
    // Set the MAC address based on the provided user identifier and device type
    if (deviceMap.find(key) != deviceMap.end())
    {
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
                Serial.println("Data sent successfully to peer");
            }
            else
            {
                Serial.println("Error sending data to peer");
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
                Serial.println("Received SET_LIGHT_SCENE: ");

                Serial.print("Received Scene ID: ");
                Serial.println(receivedData.scene.scene_id);

                Serial.print("Received Palette: ");
                Serial.println(receivedData.scene.primary_palette);

                Serial.print("Rceived speed: ");
                Serial.println(receivedData.scene.speed);

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
    Serial.println("Setting current device scene");
    light_show_.apply_sync_updates(scene);
}

void SyncController::setBrightness(uint8_t brightness)
{
    light_show_.brightness(brightness);
    SyncData syncData;
    strncpy(syncData.identifier, CURRENT_USER, 2);
    syncData.messageType = SET_LIGHT_SCENE;
    syncData.scene.brightness = map(brightness, 1, 100, 0, 255);
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
    strncpy(syncData.identifier, "CL", 2);
    syncData.messageType = SET_LIGHT_SCENE;
    Serial.print("Local Command - Set Palette: ");
    Serial.println(syncData.scene.primary_palette);

    this->sendUpdate(&syncData);
    setCurrentDeviceScene(syncData.scene);
}

void SyncController::handleButtonShortPress()
{
    LightScene currentScene = light_show_.getCurrentScene();
    SyncData syncData;
    AvailablePalettes newPalette;

    Serial.print("Current Setting: ");
    Serial.println(currentSetting_);

    switch (currentSetting_)
    {
    case PALETTE:
        // Cycle through palettes
        palette_index_ = (palette_index_ + 1) % light_show_.getPaletteCount();
        newPalette = static_cast<AvailablePalettes>(palette_index_);
        syncData.scene = currentScene;
        syncData.scene.primary_palette = newPalette;
        break;

    case MODE:
        // Implement logic to cycle through modes, e.g., spectrum, strobe, etc.
        // Example:
        // light_show_.changeMode(NextMode());  // Assuming a method to cycle modes
        // syncData.messageType = MODE_CHANGE;
        // syncData.scene.scene_id = light_show_.getCurrentMode();
        break;
    }

    setCurrentDeviceScene(syncData.scene);
    // Send the update to other devices
    strncpy(syncData.identifier, "CL", 2);
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
        currentSetting_ = MODE;
        break;
    case MODE:
        currentSetting_ = PALETTE;
        break;
    }

    Serial.print("Current Setting: ");
    switch (currentSetting_)
    {
    case PALETTE:
        Serial.println("Palette");
        break;
    case MODE:
        Serial.println("Mode");
        break;
    }
    // Set the correct MessageType based on currentSetting_
    MessageType messageType;
    switch (currentSetting_)
    {
    case PALETTE:
        messageType = PALETTE_CHANGE;
        break;
    case MODE:
        messageType = MODE_CHANGE;
        break;
    }
    SyncData syncData;
    strncpy(syncData.identifier, "CL", 2);
    syncData.messageType = messageType;
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
    currentBrightness = constrain(currentBrightness + (dialDirection * 5), 0, 254); // Scale from 1 to 255

    Serial.print("Setting current brightness to: ");
    Serial.println(currentBrightness);

    syncData.scene.brightness = currentBrightness;
    syncData.messageType = SET_LIGHT_SCENE;
    setCurrentDeviceScene(syncData.scene);
    this->sendUpdate(&syncData);
}

// void SyncController::setDirection(bool direction)
// {
//     light_show_.palette_stream(speed_, AP_palette, direction);
//     direction_ = direction;
// }

void SyncController::shouldDeviceSync(bool shouldSync)
{
    shouldSync_ = shouldSync;
}