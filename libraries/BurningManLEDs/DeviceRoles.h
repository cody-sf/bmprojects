#ifndef DEVICEROLES_H
#define DEVICEROLES_H
#include <string>

#define DEVICE_ROLE_NONE 0x0
#define DEVICE_ROLE_BIKE 0x1
#define DEVICE_ROLE_COAT 0x2
#define DEVICE_ROLE_UMBRELLA 0x3
#define DEVICE_ROLE_FANNYPACK 0x4
#define DEVICE_ROLE_COWBOYHAT 0x5
#define DEVICE_ROLE_PATIO 0x6
#define DEVICE_ROLE_ALL 0xff

enum class Device
{
    bike,
    coat,
    backpack,
    umbrella,
    fannypack,
    cowboyhat,
    patio
};

std::string getDeviceName(Device device);

#endif // DEVICEROLES_H