// Helpers.cpp
#include "Helpers.h"

AvailablePalettes stringToPalette(const char *str)
{
    Serial.println("Running to enum");
    if (strcmp(str, "candypalette") == 0)
    {
        return AvailablePalettes::candy;
    }
    if (strcmp(str, "coolpalette") == 0)
    {
        return AvailablePalettes::cool;
    }
    if (strcmp(str, "cosmicwavespalette") == 0)
    {
        return AvailablePalettes::cosmicwaves;
    }
    else if (strcmp(str, "earthpalette") == 0)
    {
        return AvailablePalettes::earth;
    }
    else if (strcmp(str, "eblossompalette") == 0)
    {
        return AvailablePalettes::eblossom;
    }
    else if (strcmp(str, "emeraldpalette") == 0)
    {
        return AvailablePalettes::emerald;
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
    else if (strcmp(str, "fireynightpalette") == 0)
    {
        return AvailablePalettes::fireynight;
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
    else if (strcmp(str, "meadowpalette") == 0)
    {
        return AvailablePalettes::meadow;
    }
    else if (strcmp(str, "melonballpalette") == 0)
    {
        return AvailablePalettes::melonball;
    }
    else if (strcmp(str, "nebulapalette") == 0)
    {
        return AvailablePalettes::nebula;
    }
    else if (strcmp(str, "oasispalette") == 0)
    {
        return AvailablePalettes::oasis;
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
    else if (strcmp(str, "sunsetfusionpalette") == 0)
    {
        return AvailablePalettes::sunsetfusion;
    }
    else if (strcmp(str, "trovepalette") == 0)
    {
        return AvailablePalettes::trove;
    }
    else if (strcmp(str, "vividpalette") == 0)
    {
        return AvailablePalettes::vivid;
    }
    else if (strcmp(str, "velvetpalette") == 0)
    {
        return AvailablePalettes::velvet;
    }
    else if (strcmp(str, "vgapalette") == 0)
    {
        return AvailablePalettes::vga;
    }
    else if (strcmp(str, "wavepalette") == 0)
    {
        return AvailablePalettes::wave;
    }
    else
    {
        // Handle default or unknown case. Depending on your use case, you might want to throw an error, return a default palette, or handle this case differently.
        return AvailablePalettes::vga; // Default to VGA for this example. Adjust as needed.
    }
}

const char *paletteToString(AvailablePalettes palette)
{
    switch (palette)
    {
    case AvailablePalettes::candy:
        return "candy";
    case AvailablePalettes::cool:
        return "cool";
    case AvailablePalettes::cosmicwaves:
        return "cosmicwaves";
    case AvailablePalettes::earth:
        return "earth";
    case AvailablePalettes::eblossom:
        return "eblossom";
    case AvailablePalettes::emerald:
        return "emerald";
    case AvailablePalettes::everglow:
        return "everglow";
    case AvailablePalettes::fatboy:

        return "fatboy";
    case AvailablePalettes::fireice:
        return "fireice";
    case AvailablePalettes::fireynight:
        return "fireynight";
    case AvailablePalettes::flame:
        return "flame";

    case AvailablePalettes::heart:
        return "heart";
    case AvailablePalettes::lava:
        return "lava";
    case AvailablePalettes::meadow:
        return "meadow";
    case AvailablePalettes::melonball:
        return "melonball";
    case AvailablePalettes::nebula:
        return "nebula";
    case AvailablePalettes::oasis:
        return "oasis";
    case AvailablePalettes::pinksplash:
        return "pinksplash";
    case AvailablePalettes::r:
        return "r";
    case AvailablePalettes::sofia:
        return "sofia";
    case AvailablePalettes::sunset:
        return "sunset";
    case AvailablePalettes::sunsetfusion:
        return "sunsetfusion";
    case AvailablePalettes::trove:
        return "trove";
    case AvailablePalettes::vivid:
        return "vivid";
    case AvailablePalettes::velvet:
        return "velvet";
    case AvailablePalettes::vga:
        return "vga";
    case AvailablePalettes::wave:
        return "wave";
    default:
        return "vga";
    }
}

CRGB hexToCRGB(const String &hexString)
{
    // Ensure the string starts with '#' and is 7 characters long
    if (hexString.charAt(0) == '#' && hexString.length() == 7)
    {
        const char *hexCStr = hexString.c_str();        // Convert String to C-string
        long number = strtol(hexCStr + 1, nullptr, 16); // Convert hex string to a long number
        uint8_t r = (number >> 16) & 0xFF;              // Extract the red component
        uint8_t g = (number >> 8) & 0xFF;               // Extract the green component
        uint8_t b = number & 0xFF;                      // Extract the blue component
        return CRGB(r, g, b);
    }
    // Return a default color if the string is invalid
    return CRGB::Black;
}