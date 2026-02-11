#pragma once

#include "layers/Layer.h"
#include "PlanetData.h"
#include "TerrainGenerator.h"
#include "ClimateGenerator.h"
#include "ImageExporter.h"

namespace godsim {

/// The Planetary layer simulates individual worlds — terrain, climate, and biomes.
/// Phase 1A: Full generation pipeline, no rendering yet.
class PlanetaryLayer : public Layer {
public:
    LayerID     id()   const override { return LayerID::Planetary; }
    std::string name() const override { return "Planetary"; }

    void initialise(Registry& registry, EventBus& bus, RNG& rng) override {
        registry_ = &registry;
        bus_ = &bus;
        rng_ = &rng;
        LOG_INFO("PlanetaryLayer initialised");
    }

    void shutdown() override {
        LOG_INFO("PlanetaryLayer shutdown (ticked {} times)", tick_count_);
    }

    /// Generate a planet from scratch using the full pipeline.
    void generate_planet(const std::string& planet_name = "Terra",
                          u32 size = 512) {
        LOG_INFO("=== Generating Planet: {} ({}x{}) ===", planet_name, size, size);

        planet_.name = planet_name;
        planet_.width = size;
        planet_.height = size;

        // ─── Terrain Generation ───
        TerrainConfig terrain_config;
        terrain_config.width = size;
        terrain_config.height = size;
        terrain_config.sea_level = 0.40f;
        terrain_config.num_plates = 7 + rng_->next_int(0, 5);
        terrain_config.erosion_iterations = static_cast<i32>(size) * 100;

        TerrainGenerator terrain_gen(*rng_);
        planet_.elevation = terrain_gen.generate(terrain_config);
        planet_.sea_level = terrain_config.sea_level;

        // ─── Climate Generation ───
        ClimateConfig climate_config;
        climate_config.sea_level = terrain_config.sea_level;

        ClimateGenerator climate_gen(*rng_);
        planet_.temperature = climate_gen.generate_temperature(
            planet_.elevation, climate_config);
        planet_.moisture = climate_gen.generate_moisture(
            planet_.elevation, planet_.temperature, climate_config);

        // ─── Biome Classification ───
        LOG_INFO("  Classifying biomes...");
        planet_.classify_biomes();

        LOG_INFO("=== Planet Generated ===");
        LOG_INFO("  Land: {:.1f}%", planet_.land_fraction * 100.0f);
        LOG_INFO("  Avg temp: {:.1f} C", planet_.avg_temperature);
        LOG_INFO("  Avg moisture: {:.2f}", planet_.avg_moisture);

        generated_ = true;
    }

    /// Export all maps as PPM images to the given directory.
    void export_maps(const std::string& output_dir) const {
        if (!generated_) {
            LOG_WARN("No planet generated yet — nothing to export");
            return;
        }

        ImageExporter::export_heightmap(planet_.elevation,
            output_dir + "/elevation.ppm");
        ImageExporter::export_terrain(planet_.elevation, planet_.sea_level,
            output_dir + "/terrain.ppm");
        ImageExporter::export_biomes(planet_,
            output_dir + "/biomes.ppm");
        ImageExporter::export_temperature(planet_.temperature,
            output_dir + "/temperature.ppm");
        ImageExporter::export_moisture(planet_.moisture,
            output_dir + "/moisture.ppm");
    }

    void tick(SimTime current_time, SimTime delta_time) override {
        increment_tick();
        // Future: slow geological processes (erosion, drift, climate shift)
        bus_->emit(
            LayerTickedEvent{LayerID::Planetary, current_time, delta_time},
            current_time, ALL_LAYERS
        );
    }

    void serialise(BinaryWriter& writer) const override {
        writer.write_u64(tick_count_);
        writer.write_u8(generated_ ? 1 : 0);
        if (generated_) {
            planet_.serialise(writer);
        }
    }

    void deserialise(BinaryReader& reader) override {
        tick_count_ = reader.read_u64();
        generated_ = reader.read_u8() != 0;
        if (generated_) {
            planet_.deserialise(reader);
        }
    }

    // ─── Access ───
    const PlanetData& planet() const { return planet_; }
    PlanetData& planet() { return planet_; }
    bool is_generated() const { return generated_; }

private:
    Registry* registry_ = nullptr;
    EventBus* bus_ = nullptr;
    RNG* rng_ = nullptr;

    PlanetData planet_;
    bool generated_ = false;
};

} // namespace godsim
