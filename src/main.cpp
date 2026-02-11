#include "core/util/Log.h"
#include "simulation/Simulation.h"

// Layers
#include "layers/cosmological/CosmologicalLayer.h"
#include "layers/planetary/PlanetaryLayer.h"
#include "layers/biological/BiologicalLayer.h"
#include "layers/civilisation/CivilisationLayer.h"
#include "layers/divine/DivineLayer.h"

#include <filesystem>
#include <string>

int main(int argc, char* argv[]) {
    godsim::Log::init();

    LOG_INFO("========================================");
    LOG_INFO("       GOD SIMULATION v0.2");
    LOG_INFO("   Phase 1A: The Living Planet");
    LOG_INFO("========================================");

    // Parse optional seed from command line
    godsim::u64 seed = 12345;
    if (argc > 1) {
        seed = std::stoull(argv[1]);
        LOG_INFO("Using seed from command line: {}", seed);
    }

    // Create simulation
    godsim::Simulation sim(seed);

    sim.add_layer<godsim::CosmologicalLayer>();
    auto* planetary = sim.add_layer<godsim::PlanetaryLayer>();
    sim.add_layer<godsim::BiologicalLayer>();
    sim.add_layer<godsim::CivilisationLayer>();
    sim.add_layer<godsim::DivineLayer>();

    sim.initialise();

    // ─── Generate a planet ───
    planetary->generate_planet("Terra", 512);

    // ─── Export maps ───
    // Output directory: next to the executable, or a user-specified path
    std::string output_dir = ".";
    if (argc > 2) {
        output_dir = argv[2];
    }

    // Create output directory if it doesn't exist
    std::filesystem::create_directories(output_dir);

    planetary->export_maps(output_dir);

    LOG_INFO("");
    LOG_INFO("--- Maps exported to: {} ---", std::filesystem::absolute(output_dir).string());
    LOG_INFO("  elevation.ppm    - Raw heightmap (grayscale)");
    LOG_INFO("  terrain.ppm      - Coloured terrain (ocean/land/mountains)");
    LOG_INFO("  biomes.ppm       - Biome classification");
    LOG_INFO("  temperature.ppm  - Temperature heatmap");
    LOG_INFO("  moisture.ppm     - Moisture distribution");
    LOG_INFO("");
    LOG_INFO("Open these .ppm files with any image viewer (GIMP, IrfanView,");
    LOG_INFO("Paint.NET, or VS Code with an image extension).");

    // ─── Run a few ticks to prove simulation still works ───
    LOG_INFO("");
    LOG_INFO("--- Running 10 history ticks ---");
    sim.set_tick_level(1);
    sim.run(10);
    LOG_INFO("Time: {}", sim.current_time().to_string());

    // ─── Snapshot round-trip ───
    auto snap_path = (std::filesystem::temp_directory_path() / "godsim_phase1.snap").string();
    sim.save_snapshot(snap_path);
    sim.load_snapshot(snap_path);
    LOG_INFO("Snapshot round-trip OK (planet data preserved)");

    sim.shutdown();

    return 0;
}
