#pragma once

#include "Heightmap.h"
#include "core/noise/Noise.h"
#include "core/rng/RNG.h"
#include "core/util/Types.h"
#include "core/util/Log.h"

#include <vector>
#include <cmath>
#include <algorithm>

namespace godsim {

/// Configuration for terrain generation.
struct TerrainConfig {
    u32 width       = 512;
    u32 height      = 512;
    f32 sea_level   = 0.40f;    // Fraction of map that is ocean
    i32 num_plates  = 8;        // Tectonic plates
    i32 fbm_octaves = 7;       // Noise detail
    f32 mountain_scale = 0.3f;  // Strength of mountain ridges
    i32 erosion_iterations = 50; // Hydraulic erosion passes
};

/// Generates terrain heightmaps through a multi-stage pipeline:
///   1. Tectonic plate Voronoi → continent shapes
///   2. fBm noise overlay → natural variation
///   3. Ridged noise at boundaries → mountain ranges
///   4. Hydraulic erosion → rivers, valleys
///   5. Final normalisation
class TerrainGenerator {
public:
    explicit TerrainGenerator(RNG& rng) : rng_(rng) {}

    /// Run the full generation pipeline.
    Heightmap generate(const TerrainConfig& config) {
        LOG_INFO("Generating terrain ({}x{})...", config.width, config.height);

        // Stage 1: Tectonic plates
        LOG_INFO("  Stage 1: Tectonic plates ({} plates)...", config.num_plates);
        auto plate_map = generate_plates(config);
        auto elevation = plates_to_elevation(plate_map, config);

        // Stage 2: Continental noise
        LOG_INFO("  Stage 2: Continental noise ({} octaves)...", config.fbm_octaves);
        apply_continental_noise(elevation, config);

        // Stage 3: Mountain ridges at plate boundaries
        LOG_INFO("  Stage 3: Mountain ridges...");
        apply_mountain_ridges(elevation, plate_map, config);

        // Stage 4: Hydraulic erosion
        LOG_INFO("  Stage 4: Hydraulic erosion ({} iterations)...", config.erosion_iterations);
        apply_erosion(elevation, config);

        // Stage 5: Normalise and adjust so sea_level fraction is underwater
        LOG_INFO("  Stage 5: Normalisation and sea level adjustment...");
        elevation.normalise();

        // Sort values to find the elevation threshold that gives correct ocean coverage
        std::vector<f32> sorted(elevation.data_ptr(), elevation.data_ptr() + elevation.size());
        std::sort(sorted.begin(), sorted.end());

        // The sea_level'th percentile becomes our water threshold
        size_t sea_idx = static_cast<size_t>(config.sea_level * sorted.size());
        f32 threshold = sorted[std::min(sea_idx, sorted.size() - 1)];

        // Remap so that 'threshold' maps to config.sea_level
        // Below threshold: compress to [0, sea_level]
        // Above threshold: compress to [sea_level, 1]
        for (u32 y = 0; y < config.height; y++) {
            for (u32 x = 0; x < config.width; x++) {
                f32 e = elevation.get(x, y);
                if (e <= threshold) {
                    elevation.set(x, y, (e / threshold) * config.sea_level);
                } else {
                    elevation.set(x, y,
                                  config.sea_level + ((e - threshold) / (1.0f - threshold)) *
                                                         (1.0f - config.sea_level));
                }
            }
        }

        LOG_INFO("  Terrain complete. Elevation range: [{:.3f}, {:.3f}]",
                 elevation.min_value(), elevation.max_value());

        return elevation;
    }

private:
    // ─── Stage 1: Tectonic Plates (Voronoi) ───

    struct PlateInfo {
        f32 center_x, center_y;
        f32 drift_x, drift_y;  // Movement direction
        bool is_oceanic;        // Oceanic plates sit lower
    };

    /// Generate a plate assignment map using Voronoi.
    std::vector<i32> generate_plates(const TerrainConfig& config) {
        std::vector<PlateInfo> plates(config.num_plates);

        // Random plate centres and properties
        for (int i = 0; i < config.num_plates; i++) {
            plates[i].center_x = rng_.next_float(0.0f, static_cast<f32>(config.width));
            plates[i].center_y = rng_.next_float(0.0f, static_cast<f32>(config.height));
            plates[i].drift_x = rng_.next_float(-1.0f, 1.0f);
            plates[i].drift_y = rng_.next_float(-1.0f, 1.0f);
            plates[i].is_oceanic = rng_.next_float() < 0.45f;
        }

        // Assign each cell to nearest plate (Voronoi)
        std::vector<i32> plate_map(config.width * config.height);
        for (u32 y = 0; y < config.height; y++) {
            for (u32 x = 0; x < config.width; x++) {
                f32 min_dist = 1e18f;
                i32 nearest = 0;

                for (int i = 0; i < config.num_plates; i++) {
                    // Toroidal distance (wraps horizontally for planet)
                    f32 dx = static_cast<f32>(x) - plates[i].center_x;
                    f32 dy = static_cast<f32>(y) - plates[i].center_y;

                    // Wrap X for horizontal continuity
                    if (dx > config.width * 0.5f) dx -= config.width;
                    if (dx < -config.width * 0.5f) dx += config.width;

                    f32 dist = dx * dx + dy * dy;
                    if (dist < min_dist) {
                        min_dist = dist;
                        nearest = i;
                    }
                }
                plate_map[y * config.width + x] = nearest;
            }
        }

        plates_ = std::move(plates);
        return plate_map;
    }

    /// Convert plate assignments to initial elevation.
    Heightmap plates_to_elevation(const std::vector<i32>& plate_map,
                                   const TerrainConfig& config) {
        Heightmap elevation(config.width, config.height);

        for (u32 y = 0; y < config.height; y++) {
            for (u32 x = 0; x < config.width; x++) {
                i32 plate = plate_map[y * config.width + x];
                // Continental plates sit higher than oceanic
                f32 base = plates_[plate].is_oceanic ? 0.25f : 0.55f;
                elevation.set(x, y, base);
            }
        }

        return elevation;
    }

    // ─── Stage 2: Continental Noise ───

    void apply_continental_noise(Heightmap& elevation, const TerrainConfig& config) {
        PerlinNoise continents(rng_.next_u64());
        PerlinNoise detail(rng_.next_u64());

        for (u32 y = 0; y < config.height; y++) {
            for (u32 x = 0; x < config.width; x++) {
                f64 nx = static_cast<f64>(x) / config.width;
                f64 ny = static_cast<f64>(y) / config.height;

                // Large-scale continental shapes
                f64 continent_noise = continents.fbm(nx * 4.0, ny * 4.0,
                    config.fbm_octaves, 1.0, 0.55, 2.0);

                // Smaller detail
                f64 detail_noise = detail.fbm(nx * 12.0, ny * 12.0,
                    4, 1.0, 0.5, 2.0);

                f32 current = elevation.get(x, y);
                current += static_cast<f32>(continent_noise) * 0.35f;
                current += static_cast<f32>(detail_noise) * 0.08f;

                elevation.set(x, y, current);
            }
        }
    }

    // ─── Stage 3: Mountain Ridges at Plate Boundaries ───

    void apply_mountain_ridges(Heightmap& elevation,
                                const std::vector<i32>& plate_map,
                                const TerrainConfig& config) {
        PerlinNoise ridge_noise(rng_.next_u64());

        // Detect plate boundaries and add ridged noise there
        for (u32 y = 1; y < config.height - 1; y++) {
            for (u32 x = 1; x < config.width - 1; x++) {
                i32 center = plate_map[y * config.width + x];
                bool is_boundary = false;

                // Check 4 neighbours
                if (plate_map[y * config.width + (x - 1)] != center) is_boundary = true;
                if (plate_map[y * config.width + (x + 1)] != center) is_boundary = true;
                if (plate_map[(y - 1) * config.width + x] != center) is_boundary = true;
                if (plate_map[(y + 1) * config.width + x] != center) is_boundary = true;

                if (is_boundary) {
                    f64 nx = static_cast<f64>(x) / config.width;
                    f64 ny = static_cast<f64>(y) / config.height;

                    f64 ridge = ridge_noise.ridged(nx * 8.0, ny * 8.0, 5, 1.0, 0.6, 2.0);

                    f32 current = elevation.get(x, y);
                    current += static_cast<f32>(ridge) * config.mountain_scale;
                    elevation.set(x, y, current);
                }
            }
        }

        // Blur slightly to smooth harsh plate edges
        elevation.blur(2);
    }

    // ─── Stage 4: Hydraulic Erosion (simplified) ───
    // Simulates water flowing downhill, carving valleys and depositing sediment.

    void apply_erosion(Heightmap& elevation, const TerrainConfig& config) {
        u32 w = config.width;
        u32 h = config.height;

        for (i32 iter = 0; iter < config.erosion_iterations; iter++) {
            // Drop a "raindrop" at a random position
            f32 px = rng_.next_float(1.0f, static_cast<f32>(w - 2));
            f32 py = rng_.next_float(1.0f, static_cast<f32>(h - 2));
            f32 sediment = 0.0f;
            f32 speed = 0.0f;
            f32 water = 1.0f;

            const f32 erosion_rate = 0.3f;
            const f32 deposit_rate = 0.3f;
            const f32 evaporate_rate = 0.01f;
            const f32 gravity = 4.0f;
            const i32 max_lifetime = 50;

            f32 dir_x = 0, dir_y = 0;
            f32 inertia = 0.3f;

            for (i32 step = 0; step < max_lifetime; step++) {
                u32 ix = static_cast<u32>(px);
                u32 iy = static_cast<u32>(py);

                if (ix < 1 || ix >= w - 1 || iy < 1 || iy >= h - 1) break;

                // Compute gradient
                f32 h_l = elevation.get(ix - 1, iy);
                f32 h_r = elevation.get(ix + 1, iy);
                f32 h_u = elevation.get(ix, iy - 1);
                f32 h_d = elevation.get(ix, iy + 1);

                f32 gx = (h_r - h_l) * 0.5f;
                f32 gy = (h_d - h_u) * 0.5f;

                // Update direction with inertia
                dir_x = dir_x * inertia - gx * (1.0f - inertia);
                dir_y = dir_y * inertia - gy * (1.0f - inertia);

                // Normalise direction
                f32 len = std::sqrt(dir_x * dir_x + dir_y * dir_y);
                if (len < 1e-6f) break;
                dir_x /= len;
                dir_y /= len;

                // Move
                f32 new_px = px + dir_x;
                f32 new_py = py + dir_y;

                u32 nix = static_cast<u32>(new_px);
                u32 niy = static_cast<u32>(new_py);
                if (nix < 1 || nix >= w - 1 || niy < 1 || niy >= h - 1) break;

                f32 old_h = elevation.sample(px, py);
                f32 new_h = elevation.sample(new_px, new_py);
                f32 h_diff = new_h - old_h;

                if (h_diff > 0) {
                    // Going uphill — deposit sediment
                    f32 to_deposit = std::min(sediment, h_diff);
                    elevation.at(ix, iy) += to_deposit;
                    sediment -= to_deposit;
                } else {
                    // Going downhill — erode
                    f32 capacity = std::max(-h_diff, 0.01f) * speed * water * 8.0f;
                    if (sediment > capacity) {
                        f32 to_deposit = (sediment - capacity) * deposit_rate;
                        elevation.at(ix, iy) += to_deposit;
                        sediment -= to_deposit;
                    } else {
                        f32 to_erode = std::min((capacity - sediment) * erosion_rate,
                                                 -h_diff);
                        elevation.at(ix, iy) -= to_erode;
                        sediment += to_erode;
                    }
                }

                speed = std::sqrt(std::max(speed * speed + h_diff * gravity, 0.0f));
                water *= (1.0f - evaporate_rate);

                px = new_px;
                py = new_py;
            }
        }
    }

    RNG& rng_;
    std::vector<PlateInfo> plates_;
};

} // namespace godsim
