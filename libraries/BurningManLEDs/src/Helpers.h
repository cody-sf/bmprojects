// Helpers.h
#ifndef HELPERS_H
#define HELPERS_H

#include "LightShow.h"

AvailablePalettes stringToPalette(const char *str);
const char *paletteToString(AvailablePalettes palette);
CRGB hexToCRGB(const String &hexString);

#endif // HELPERS_H