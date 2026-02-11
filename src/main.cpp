#include "core/util/Log.h"
#include "simulation/Simulation.h"

// Layers
#include "layers/cosmological/CosmologicalLayer.h"
#include "layers/planetary/PlanetaryLayer.h"
#include "layers/biological/BiologicalLayer.h"
#include "layers/civilisation/CivilisationLayer.h"
#include "layers/divine/DivineLayer.h"

// Renderer
#include "renderer/PlanetRenderer.h"

#include <filesystem>
#include <string>
#include <cstring>

int main(int argc, char* argv[]) {
    godsim::Log::init();

    LOG_INFO("========================================");
    LOG_INFO("       GOD SIMULATION v0.3");
    LOG_INFO("   Phase 1B: Planet Renderer");
    LOG_INFO("========================================");

    // Parse arguments
    godsim::u64 seed = 12345;
    std::string output_dir = "maps";
    bool headless = false;

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--headless") == 0) {
            headless = true;
        } else if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_dir = argv[++i];
        } else {
            // Assume it's a seed
            try { seed = std::stoull(argv[i]); }
            catch (...) { LOG_WARN("Unknown argument: {}", argv[i]); }
        }
    }

    LOG_INFO("Seed: {}", seed);
    if (headless) LOG_INFO("Mode: headless (no renderer)");

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
    std::filesystem::create_directories(output_dir);
    planetary->export_maps(output_dir);
    LOG_INFO("Maps exported to: {}", std::filesystem::absolute(output_dir).string());

    // ─── Render ───
    if (!headless) {
        try {
            godsim::PlanetRenderer renderer;
            renderer.init(planetary->planet());
            renderer.run();
        } catch (const std::exception& e) {
            LOG_ERROR("Renderer failed: {}", e.what());
            LOG_INFO("Run with --headless to skip rendering");
        }
    }

    // ─── Run a few ticks to prove simulation works ───
    LOG_INFO("");
    LOG_INFO("--- Running 10 history ticks ---");
    sim.set_tick_level(1);
    sim.run(10);
    LOG_INFO("Time: {}", sim.current_time().to_string());

    // ─── Snapshot round-trip ───
    auto snap_path = (std::filesystem::temp_directory_path() / "godsim_phase1.snap").string();
    sim.save_snapshot(snap_path);
    sim.load_snapshot(snap_path);
    LOG_INFO("Snapshot round-trip OK");

    sim.shutdown();

    return 0;
}
