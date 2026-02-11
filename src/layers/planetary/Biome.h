#pragma once

#include "core/util/Types.h"
#include <string>
#include <array>

namespace godsim {

/// Biome types derived from temperature and moisture.
/// Uses a Whittaker-diagram approach.
enum class BiomeType : u8 {
    Ocean = 0,
    DeepOcean,
    Ice,
    Tundra,
    BorealForest,       // Taiga
    TemperateGrassland,
    TemperateForest,
    TemperateRainforest,
    Shrubland,          // Mediterranean / chaparral
    Desert,
    Savanna,
    TropicalForest,
    TropicalRainforest,
    Wetland,
    Mountain,           // High altitude — above treeline
    Beach,
    COUNT
};

struct BiomeInfo {
    const char* name;
    u8 r, g, b; // Display colour
};

/// Colour palette and names for each biome.
inline constexpr std::array<BiomeInfo, static_cast<size_t>(BiomeType::COUNT)> BIOME_INFO = {{
    {"Ocean",               28,  107, 160},
    {"Deep Ocean",          15,  60,  110},
    {"Ice",                 220, 235, 245},
    {"Tundra",              180, 200, 190},
    {"Boreal Forest",       40,  100, 60 },
    {"Temperate Grassland", 140, 175, 80 },
    {"Temperate Forest",    50,  130, 50 },
    {"Temperate Rainforest",30,  100, 80 },
    {"Shrubland",           165, 155, 95 },
    {"Desert",              210, 190, 140},
    {"Savanna",             185, 175, 95 },
    {"Tropical Forest",     35,  120, 45 },
    {"Tropical Rainforest", 20,  90,  35 },
    {"Wetland",             80,  130, 110},
    {"Mountain",            140, 140, 145},
    {"Beach",               220, 210, 165},
}};

/// Classify a cell into a biome based on its properties.
///   elevation:    [0, 1] where sea_level determines land/water
///   temperature:  [-50, 50] degrees C approximately
///   moisture:     [0, 1] relative moisture availability
///   sea_level:    elevation threshold for ocean
inline BiomeType classify_biome(f32 elevation, f32 temperature, f32 moisture,
                                 f32 sea_level = 0.4f) {
    // ─── Water ───
    if (elevation < sea_level - 0.05f) return BiomeType::DeepOcean;
    if (elevation < sea_level)         return BiomeType::Ocean;

    // Normalised land height (0 = shore, 1 = peak)
    f32 land_height = (elevation - sea_level) / (1.0f - sea_level);

    // ─── Beach (very close to shore) ───
    if (land_height < 0.02f) return BiomeType::Beach;

    // ─── Mountain (high altitude) ───
    if (land_height > 0.7f) return BiomeType::Mountain;

    // ─── Ice / Frozen ───
    if (temperature < -10.0f) return BiomeType::Ice;

    // ─── Whittaker-style classification ───
    // Cold
    if (temperature < 0.0f) {
        return BiomeType::Tundra;
    }

    // Cool (0-10°C)
    if (temperature < 10.0f) {
        if (moisture > 0.5f) return BiomeType::BorealForest;
        return BiomeType::Tundra;
    }

    // Temperate (10-20°C)
    if (temperature < 20.0f) {
        if (moisture > 0.7f)  return BiomeType::TemperateRainforest;
        if (moisture > 0.4f)  return BiomeType::TemperateForest;
        if (moisture > 0.2f)  return BiomeType::Shrubland;
        return BiomeType::TemperateGrassland;
    }

    // Warm (20-30°C)
    if (temperature < 30.0f) {
        if (moisture > 0.65f) return BiomeType::TropicalRainforest;
        if (moisture > 0.35f) return BiomeType::TropicalForest;
        if (moisture > 0.15f) return BiomeType::Savanna;
        return BiomeType::Desert;
    }

    // Hot (>30°C)
    if (moisture > 0.6f)  return BiomeType::TropicalRainforest;
    if (moisture > 0.3f)  return BiomeType::Savanna;
    return BiomeType::Desert;
}

} // namespace godsim
