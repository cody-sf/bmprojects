// Helpers.cpp
#include "Helpers.h"

AvailablePalettes stringToPalette(const char *str)
{
    Serial.println("Running to enum");
    if (strcmp(str, "coolpalette") == 0)
    {
        return AvailablePalettes::cool;
    }
    else if (strcmp(str, "earthpalette") == 0)
    {
        return AvailablePalettes::earth;
    }
    else if (strcmp(str, "everglowpalette") == 0)
    {
        return AvailablePalettes::everglow;
    }
    else if (strcmp(str, "fatboypalette") == 0)
    {
        return AvailablePalettes::fatboy;
    }
    else if (strcmp(str, "fireicepalette") == 0)
    {
        return AvailablePalettes::fireice;
    }
    else if (strcmp(str, "flamepalette") == 0)
    {
        return AvailablePalettes::flame;
    }
    else if (strcmp(str, "heartpalette") == 0)
    {
        return AvailablePalettes::heart;
    }
    else if (strcmp(str, "lavapalette") == 0)
    {
        return AvailablePalettes::lava;
    }
    else if (strcmp(str, "melonballpalette") == 0)
    {
        return AvailablePalettes::melonball;
    }
    else if (strcmp(str, "pinksplashpalette") == 0)
    {
        return AvailablePalettes::pinksplash;
    }
    else if (strcmp(str, "rpalette") == 0)
    {
        return AvailablePalettes::r;
    }
    else if (strcmp(str, "sofiapalette") == 0)
    {
        return AvailablePalettes::sofia;
    }
    else if (strcmp(str, "sunsetpalette") == 0)
    {
        return AvailablePalettes::sunset;
    }
    else if (strcmp(str, "trovepalette") == 0)
    {
        return AvailablePalettes::trove;
    }
    else if (strcmp(str, "velvetpalette") == 0)
    {
        return AvailablePalettes::velvet;
    }
    else if (strcmp(str, "vgapalette") == 0)
    {
        return AvailablePalettes::vga;
    }
    else
    {
        // Handle default or unknown case. Depending on your use case, you might want to throw an error, return a default palette, or handle this case differently.
        return AvailablePalettes::vga; // Default to VGA for this example. Adjust as needed.
    }
}