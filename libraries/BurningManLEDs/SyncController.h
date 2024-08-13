// ESPNowSync.h

#ifndef SYNC_CONTROLLER_H
#define SYNC_CONTROLLER_H

#include <Arduino.h>
#include <ESP32_NOW.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <LightShow.h>
#include <GlobalDefaults.h>
#include <DeviceRoles.h>
#include <map>

#define DEFAULT_ORIGIN_LATITUDE 40.786331
#define DEFAULT_ORIGIN_LONGITUDE -119.206489
#define DEFAULT_PLAYA_INNER_RADIUS 220
#define DEFAULT_PLAYA_OUTER_RADIUS 2000
#define ORIGIN_COLOR CRGB::White
#define MAX_SPEED 16.0
#define DEFAULT_BRIGHTNESS 25

enum MessageType : uint8_t
{
    MODE_CHANGE,
    PALETTE_CHANGE,
    BRIGHTNESS_CHANGE,
    SET_LIGHT_SCENE
};

enum SettingType
{
    PALETTE,
    MODE,
};

struct SyncData
{
    char identifier[2]; // Identifier for the devices, e.g., "CL"
    LightScene scene;   // The scene to be synchronized
    MessageType messageType;
    SettingType currentSetting;
};
class SyncController
{
public:
    SyncController(LightShow &light_show, const std::string &userIdentifier, Device &deviceType);
    void begin(const std::string &userIdentifier);
    void addPeers(const std::string &userIdentifier);
    void sendUpdate(const SyncData *data);
    void onReceive(void (*callback)(const uint8_t *mac, const uint8_t *data, int len));
    void readMacAddress();
    void handleButtonShortPress();
    void handleButtonLongPress();
    void handleDialChange(int8_t dialDirection);
    void setBrightness(uint8_t brightness);
    void setSpeed(uint16_t speed);
    void setPalette(AvailablePalettes palette);
    void setDirection(bool direction);
    void changeMode(LightSceneID mode);
    void handleDialTurn(int8_t direction);
    void shouldDeviceSync(bool shouldSync);

private:
    static SyncController *instance_;
    static void onDataReceivedStatic(const esp_now_recv_info_t *info, const uint8_t *data, int len);
    void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len);
    static void (*userCallback)(const uint8_t *mac, const uint8_t *data, int len);
    void setCurrentDeviceScene(LightScene scene);
    SettingType currentSetting_;
    uint8_t broadcastAddress[6];
    LightShow &light_show_;
    uint8_t brightness_;
    uint16_t speed_;
    size_t palette_index_;
    bool direction_;
    bool shouldSync_;
};

#endif // SYNC_CONTROLLER_H
