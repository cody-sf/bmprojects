#include "GlobalDefaults.h"

// Initialize the device map with MAC addresses
std::map<std::pair<std::string, std::string>, DeviceInfo> deviceMap = {
    {{"CL", "Bike"}, {"CL", "Bike", {0xCC, 0xDB, 0xA7, 0x5A, 0x23, 0xF4}}},
    {{"CL", "Fannypack"}, {"CL", "Fannypack", {0x0C, 0xB8, 0x15, 0x77, 0x89, 0xE8}}},
    {{"AS", "Bike"}, {"AS", "Bike", {0xB4, 0xCF, 0x33, 0xC1, 0xD4, 0xE5}}},
    // Add more devices here
};