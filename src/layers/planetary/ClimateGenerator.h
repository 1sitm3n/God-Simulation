#pragma once

#include "Heightmap.h"
#include "core/noise/Noise.h"
#include "core/rng/RNG.h"
#include "core/util/Types.h"
#include "core/util/Log.h"

#include <cmath>
#include <algorithm>
#include <queue>

namespace godsim {

/// Configuration for climate generation.
struct ClimateConfig {
    f32 sea_level       = 0.40f;
    f32 axial_tilt      = 23.5f;   // Degrees — affects seasonal variation
    f32 base_temp       = 15.0f;   // Global average temperature °C
    f32 temp_range      = 70.0f;   // Pole-to-equator temperature range
    f32 altitude_lapse  = 40.0f;   // Temperature drop per unit altitude
    f32 ocean_moisture  = 0.9f;    // Moisture at ocean cells
};

/// Generates temperature and moisture maps from terrain.
class ClimateGenerator {
public:
    explicit ClimateGenerator(RNG& rng) : rng_(rng) {}

    /// Generate temperature map. Output in approximate °C.
    Heightmap generate_temperature(const Heightmap& elevation,
                                    const ClimateConfig& config) {
        LOG_INFO("  Generating temperature map...");
        u32 w = elevation.width();
        u32 h = elevation.height();
        Heightmap temp(w, h);
        PerlinNoise noise(rng_.next_u64());

        for (u32 y = 0; y < h; y++) {
            // Latitude: 0 at equator (center), 1 at poles
            f32 latitude = std::abs(2.0f * static_cast<f32>(y) / h - 1.0f);

            // Base temperature: hot at equator, cold at poles
            // Uses cosine curve for realistic latitude bands
            f32 lat_temp = config.base_temp - config.temp_range * 0.5f *
                           (latitude * latitude); // Quadratic falloff

            for (u32 x = 0; x < w; x++) {
                f32 elev = elevation.get(x, y);
                f32 t = lat_temp;

                // Altitude cooling: higher = colder
                if (elev > config.sea_level) {
                    f32 land_height = (elev - config.sea_level) / (1.0f - config.sea_level);
                    t -= land_height * config.altitude_lapse;
                }

                // Ocean moderates temperature (less extreme)
                if (elev < config.sea_level) {
                    t = t * 0.7f + config.base_temp * 0.3f;
                }

                // Add some noise for local variation
                f64 nx = static_cast<f64>(x) / w;
                f64 ny = static_cast<f64>(y) / h;
                f32 variation = static_cast<f32>(
                    noise.fbm(nx * 6.0, ny * 6.0, 3, 1.0, 0.5, 2.0)) * 5.0f;
                t += variation;

                temp.set(x, y, t);
            }
        }

        LOG_INFO("    Temperature range: {:.1f}°C to {:.1f}°C",
                 temp.min_value(), temp.max_value());
        return temp;
    }

    /// Generate moisture map. Output in [0, 1].
    /// Based on: distance from ocean, rain shadow from mountains,
    /// latitude (tropical convergence zones are wet).
    Heightmap generate_moisture(const Heightmap& elevation,
                                 const Heightmap& temperature,
                                 const ClimateConfig& config) {
        LOG_INFO("  Generating moisture map...");
        u32 w = elevation.width();
        u32 h = elevation.height();

        // Step 1: Distance from ocean (BFS flood fill)
        Heightmap ocean_dist = compute_ocean_distance(elevation, config);

        // Step 2: Combine factors
        PerlinNoise noise(rng_.next_u64());
        Heightmap moisture(w, h);

        for (u32 y = 0; y < h; y++) {
            f32 latitude = std::abs(2.0f * static_cast<f32>(y) / h - 1.0f);

            // ITCZ: tropical convergence zone is wet (near equator)
            f32 tropical_moisture = std::exp(-latitude * latitude * 8.0f) * 0.3f;

            // Temperate storm tracks
            f32 temperate_moisture = std::exp(-(latitude - 0.5f) * (latitude - 0.5f) * 20.0f) * 0.15f;

            for (u32 x = 0; x < w; x++) {
                f32 elev = elevation.get(x, y);

                // Ocean cells
                if (elev < config.sea_level) {
                    moisture.set(x, y, config.ocean_moisture);
                    continue;
                }

                // Distance from ocean (closer = wetter)
                f32 dist = ocean_dist.get(x, y);
                f32 max_dist = std::sqrt(static_cast<f32>(w * w + h * h)) * 0.5f;
                f32 ocean_factor = 1.0f - std::clamp(dist / max_dist, 0.0f, 1.0f);
                ocean_factor = std::pow(ocean_factor, 0.4f); // Slow falloff

                // Altitude: mountains create rain shadow (reduce moisture)
                f32 land_height = (elev - config.sea_level) / (1.0f - config.sea_level);
                f32 altitude_factor = 1.0f - land_height * 0.5f;

                // Combine
                f32 m = ocean_factor * 0.5f + tropical_moisture + temperate_moisture;
                m *= altitude_factor;

                // Noise variation
                f64 nx = static_cast<f64>(x) / w;
                f64 ny = static_cast<f64>(y) / h;
                f32 variation = static_cast<f32>(
                    noise.fbm(nx * 5.0, ny * 5.0, 3, 1.0, 0.5, 2.0)) * 0.15f;
                m += variation;

                moisture.set(x, y, std::clamp(m, 0.0f, 1.0f));
            }
        }

        LOG_INFO("    Moisture range: {:.3f} to {:.3f}",
                 moisture.min_value(), moisture.max_value());
        return moisture;
    }

private:
    /// BFS flood fill from ocean cells to compute distance-to-ocean.
    Heightmap compute_ocean_distance(const Heightmap& elevation,
                                      const ClimateConfig& config) {
        u32 w = elevation.width();
        u32 h = elevation.height();
        Heightmap dist(w, h, -1.0f); // -1 = unvisited

        struct Cell { u32 x, y; };
        std::queue<Cell> queue;

        // Seed with all ocean cells
        for (u32 y = 0; y < h; y++) {
            for (u32 x = 0; x < w; x++) {
                if (elevation.get(x, y) < config.sea_level) {
                    dist.set(x, y, 0.0f);
                    queue.push({x, y});
                }
            }
        }

        // BFS
        const i32 dx[] = {-1, 1, 0, 0};
        const i32 dy[] = {0, 0, -1, 1};

        while (!queue.empty()) {
            auto [cx, cy] = queue.front();
            queue.pop();
            f32 current_dist = dist.get(cx, cy);

            for (int d = 0; d < 4; d++) {
                i32 nx = static_cast<i32>(cx) + dx[d];
                i32 ny = static_cast<i32>(cy) + dy[d];

                if (nx < 0 || nx >= static_cast<i32>(w) ||
                    ny < 0 || ny >= static_cast<i32>(h)) continue;

                u32 ux = static_cast<u32>(nx);
                u32 uy = static_cast<u32>(ny);

                if (dist.get(ux, uy) < 0.0f) { // Unvisited
                    dist.set(ux, uy, current_dist + 1.0f);
                    queue.push({ux, uy});
                }
            }
        }

        return dist;
    }

    RNG& rng_;
};

} // namespace godsim
