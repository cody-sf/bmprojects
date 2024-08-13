#include "DeviceRoles.h"

std::string getDeviceName(Device device)
{
    switch (device)
    {
    case Device::bike:
        return "Bike";
    case Device::coat:
        return "Coat";
    case Device::backpack:
        return "Backpack";
    case Device::umbrella:
        return "Umbrella";
    case Device::fannypack:
        return "Fannypack";
    case Device::cowboyhat:
        return "Cowboyhat";
    case Device::patio:
        return "Patio";
    default:
        return "Unknown";
    }
}
