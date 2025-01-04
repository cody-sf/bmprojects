#ifndef GLOBALDEFAULTS_H
#define GLOBALDEFAULTS_H
#include <map>
#include <string>

#define MULTICAST_LISTENING_PORT 8888
#define DEFAULT_BRIGHTNESS 80

#define CURRENT_USER "CL"

#define DEFAULT_BT_REFRESH_INTERVAL 5000

struct DeviceInfo
{
    std::string userIdentifier;
    std::string deviceType;
    uint8_t mac[6];
};

// Global device map
extern std::map<std::pair<std::string, std::string>, DeviceInfo> deviceMap;

#endif // GLOBALDEFAULTS_H