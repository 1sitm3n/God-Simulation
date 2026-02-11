#pragma once

#include "Heightmap.h"
#include "Biome.h"
#include "PlanetData.h"
#include "core/util/Types.h"
#include "core/util/Log.h"

#include <fstream>
#include <string>
#include <cmath>
#include <algorithm>

namespace godsim {

/// Exports planetary data as PPM image files.
/// PPM is a dead-simple format: no external libraries needed.
/// Open with GIMP, IrfanView, VS Code, or convert with ImageMagick.
class ImageExporter {
public:
    /// Export elevation as a grayscale heightmap.
    static void export_heightmap(const Heightmap& map, const std::string& path) {
        u32 w = map.width();
        u32 h = map.height();

        std::ofstream file(path, std::ios::binary);
        file << "P6\n" << w << " " << h << "\n255\n";

        for (u32 y = 0; y < h; y++) {
            for (u32 x = 0; x < w; x++) {
                f32 v = std::clamp(map.get(x, y), 0.0f, 1.0f);
                u8 c = static_cast<u8>(v * 255);
                file.put(c); file.put(c); file.put(c);
            }
        }

        LOG_INFO("Exported heightmap: {}", path);
    }

    /// Export elevation with ocean colouring (blue below sea level).
    static void export_terrain(const Heightmap& elevation, f32 sea_level,
                                const std::string& path) {
        u32 w = elevation.width();
        u32 h = elevation.height();

        std::ofstream file(path, std::ios::binary);
        file << "P6\n" << w << " " << h << "\n255\n";

        for (u32 y = 0; y < h; y++) {
            for (u32 x = 0; x < w; x++) {
                f32 e = elevation.get(x, y);

                u8 r, g, b;
                if (e < sea_level - 0.05f) {
                    // Deep ocean
                    f32 depth = (sea_level - e) / sea_level;
                    r = static_cast<u8>(15 + (1 - depth) * 30);
                    g = static_cast<u8>(50 + (1 - depth) * 60);
                    b = static_cast<u8>(100 + (1 - depth) * 60);
                } else if (e < sea_level) {
                    // Shallow ocean
                    r = 40; g = 100; b = 150;
                } else if (e < sea_level + 0.02f) {
                    // Beach
                    r = 210; g = 200; b = 160;
                } else {
                    // Land — green to brown to white based on height
                    f32 land_h = (e - sea_level) / (1.0f - sea_level);

                    if (land_h < 0.3f) {
                        // Lowlands — green
                        f32 t = land_h / 0.3f;
                        r = static_cast<u8>(50 + t * 60);
                        g = static_cast<u8>(130 - t * 20);
                        b = static_cast<u8>(40 + t * 20);
                    } else if (land_h < 0.6f) {
                        // Hills — brown
                        f32 t = (land_h - 0.3f) / 0.3f;
                        r = static_cast<u8>(110 + t * 40);
                        g = static_cast<u8>(110 - t * 20);
                        b = static_cast<u8>(60 + t * 20);
                    } else {
                        // Mountains — grey to white
                        f32 t = (land_h - 0.6f) / 0.4f;
                        r = static_cast<u8>(150 + t * 105);
                        g = static_cast<u8>(150 + t * 105);
                        b = static_cast<u8>(150 + t * 105);
                    }
                }

                file.put(static_cast<char>(r));
                file.put(static_cast<char>(g));
                file.put(static_cast<char>(b));
            }
        }

        LOG_INFO("Exported terrain map: {}", path);
    }

    /// Export biome map using the standard biome colour palette.
    static void export_biomes(const PlanetData& planet, const std::string& path) {
        u32 w = planet.width;
        u32 h = planet.height;

        std::ofstream file(path, std::ios::binary);
        file << "P6\n" << w << " " << h << "\n255\n";

        for (u32 y = 0; y < h; y++) {
            for (u32 x = 0; x < w; x++) {
                BiomeType biome = planet.biome_at(x, y);
                const auto& info = BIOME_INFO[static_cast<size_t>(biome)];

                file.put(static_cast<char>(info.r));
                file.put(static_cast<char>(info.g));
                file.put(static_cast<char>(info.b));
            }
        }

        LOG_INFO("Exported biome map: {}", path);
    }

    /// Export temperature as a heatmap (blue = cold, red = hot).
    static void export_temperature(const Heightmap& temperature,
                                    const std::string& path) {
        u32 w = temperature.width();
        u32 h = temperature.height();

        // Get range for normalisation
        f32 min_t = temperature.min_value();
        f32 max_t = temperature.max_value();
        f32 range = max_t - min_t;
        if (range < 1e-6f) range = 1.0f;

        std::ofstream file(path, std::ios::binary);
        file << "P6\n" << w << " " << h << "\n255\n";

        for (u32 y = 0; y < h; y++) {
            for (u32 x = 0; x < w; x++) {
                f32 t = (temperature.get(x, y) - min_t) / range; // [0, 1]

                // Blue → Cyan → Green → Yellow → Red
                u8 r, g, b;
                if (t < 0.25f) {
                    f32 s = t / 0.25f;
                    r = 0; g = static_cast<u8>(s * 180); b = static_cast<u8>(200 - s * 50);
                } else if (t < 0.5f) {
                    f32 s = (t - 0.25f) / 0.25f;
                    r = 0; g = static_cast<u8>(180 + s * 50); b = static_cast<u8>(150 * (1 - s));
                } else if (t < 0.75f) {
                    f32 s = (t - 0.5f) / 0.25f;
                    r = static_cast<u8>(s * 230); g = static_cast<u8>(230 - s * 50); b = 0;
                } else {
                    f32 s = (t - 0.75f) / 0.25f;
                    r = static_cast<u8>(230 + s * 25); g = static_cast<u8>(180 * (1 - s)); b = 0;
                }

                file.put(static_cast<char>(r));
                file.put(static_cast<char>(g));
                file.put(static_cast<char>(b));
            }
        }

        LOG_INFO("Exported temperature map: {}", path);
    }

    /// Export moisture map (brown = dry, green = wet, blue = ocean-level wet).
    static void export_moisture(const Heightmap& moisture, const std::string& path) {
        u32 w = moisture.width();
        u32 h = moisture.height();

        std::ofstream file(path, std::ios::binary);
        file << "P6\n" << w << " " << h << "\n255\n";

        for (u32 y = 0; y < h; y++) {
            for (u32 x = 0; x < w; x++) {
                f32 m = std::clamp(moisture.get(x, y), 0.0f, 1.0f);

                u8 r, g, b;
                if (m < 0.3f) {
                    f32 s = m / 0.3f;
                    r = static_cast<u8>(180 - s * 80);
                    g = static_cast<u8>(150 - s * 30);
                    b = static_cast<u8>(80 + s * 20);
                } else if (m < 0.6f) {
                    f32 s = (m - 0.3f) / 0.3f;
                    r = static_cast<u8>(100 * (1 - s));
                    g = static_cast<u8>(120 + s * 60);
                    b = static_cast<u8>(100 * (1 - s) + s * 50);
                } else {
                    f32 s = (m - 0.6f) / 0.4f;
                    r = static_cast<u8>(30 * (1 - s));
                    g = static_cast<u8>(180 - s * 60);
                    b = static_cast<u8>(50 + s * 130);
                }

                file.put(static_cast<char>(r));
                file.put(static_cast<char>(g));
                file.put(static_cast<char>(b));
            }
        }

        LOG_INFO("Exported moisture map: {}", path);
    }
};

} // namespace godsim
