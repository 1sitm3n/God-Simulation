#pragma once
/// Biome classification and display properties.

#include "core/util/Types.h"
#include <array>

namespace godsim {

enum class BiomeType : u8 {
    Ocean = 0,
    DeepOcean,
    Ice,
    Tundra,
    BorealForest,
    TemperateGrassland,
    TemperateForest,
    TemperateRainforest,
    Shrubland,
    Desert,
    Savanna,
    TropicalForest,
    TropicalRainforest,
    Wetland,
    Mountain,
    Beach,
    COUNT
};

struct BiomeInfo {
    const char* name;
    u8 r, g, b;
};

/// Satellite-photo-inspired palette — muted, natural tones.
inline constexpr std::array<BiomeInfo, static_cast<size_t>(BiomeType::COUNT)> BIOME_INFO = {{
    {"Ocean",               22,  82,  130},
    {"Deep Ocean",          12,  48,  90 },
    {"Ice",                 225, 232, 238},
    {"Tundra",              170, 185, 170},
    {"Boreal Forest",       52,  78,  52 },
    {"Temperate Grassland", 125, 148, 72 },
    {"Temperate Forest",    58,  105, 48 },
    {"Temperate Rainforest",38,  88,  62 },
    {"Shrubland",           148, 138, 90 },
    {"Desert",              195, 175, 130},
    {"Savanna",             168, 155, 85 },
    {"Tropical Forest",     42,  98,  42 },
    {"Tropical Rainforest", 28,  75,  32 },
    {"Wetland",             68,  108, 88 },
    {"Mountain",            128, 125, 130},
    {"Beach",               195, 185, 150},
}};

/// Classify a cell into a biome based on elevation, temperature, and moisture.
inline BiomeType classify_biome(f32 elevation, f32 temperature, f32 moisture, f32 sea_level = 0.4f) {
    // ─── Water ───
    if (elevation < sea_level) {
        if (temperature < -15.0f) return BiomeType::Ice;
        if (elevation < sea_level * 0.5f) return BiomeType::DeepOcean;
        return BiomeType::Ocean;
    }

    // ─── Land ───
    f32 land_elev = (elevation - sea_level) / (1.0f - sea_level); // 0..1 above sea level

    // Beach
    if (land_elev < 0.03f && temperature > -5.0f) return BiomeType::Beach;

    // Mountain
    if (land_elev > 0.65f) return BiomeType::Mountain;
    if (land_elev > 0.55f && temperature < 0.0f) return BiomeType::Mountain;

    // ─── Climate-based biomes ───
    // Ice and tundra
    if (temperature < -20.0f) return BiomeType::Ice;
    if (temperature < -10.0f) return BiomeType::Tundra;

    // Cold biomes
    if (temperature < 0.0f) {
        if (moisture > 0.5f) return BiomeType::BorealForest;
        return BiomeType::Tundra;
    }

    // Temperate
    if (temperature < 12.0f) {
        if (moisture > 0.7f) return BiomeType::TemperateRainforest;
        if (moisture > 0.45f) return BiomeType::TemperateForest;
        if (moisture > 0.25f) return BiomeType::TemperateGrassland;
        return BiomeType::Shrubland;
    }

    // Hot biomes
    if (moisture < 0.2f) return BiomeType::Desert;
    if (moisture < 0.35f) return BiomeType::Savanna;
    if (moisture < 0.55f) return BiomeType::TropicalForest;
    if (moisture > 0.65f) return BiomeType::TropicalRainforest;
    return BiomeType::Wetland;
}

} // namespace godsim
