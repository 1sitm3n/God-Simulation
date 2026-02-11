#include "core/util/Log.h"
#include "simulation/Simulation.h"

// Layer stubs
#include "layers/cosmological/CosmologicalLayer.h"
#include "layers/planetary/PlanetaryLayer.h"
#include "layers/biological/BiologicalLayer.h"
#include "layers/civilisation/CivilisationLayer.h"
#include "layers/divine/DivineLayer.h"
#include <filesystem>

int main() {
    godsim::Log::init();

    LOG_INFO("╔════════════════════════════════════╗");
    LOG_INFO("║        GOD SIMULATION v0.1         ║");
    LOG_INFO("║      Phase 0: Foundation           ║");
    LOG_INFO("╚════════════════════════════════════╝");

    // Create simulation with a seed
    godsim::Simulation sim(12345);

    // Register all layers
    sim.add_layer<godsim::CosmologicalLayer>();
    sim.add_layer<godsim::PlanetaryLayer>();
    sim.add_layer<godsim::BiologicalLayer>();
    sim.add_layer<godsim::CivilisationLayer>();
    sim.add_layer<godsim::DivineLayer>();

    // Initialise
    sim.initialise();

    // Run a demo: 10 ticks at the "history" level (1 year per tick)
    LOG_INFO("");
    LOG_INFO("─── Running 10 history ticks (10 years) ───");
    sim.set_tick_level(1); // history level
    sim.run(10);
    LOG_INFO("Time after 10 history ticks: {}", sim.current_time().to_string());

    // Switch to evolution level and run 5 ticks (500 years)
    LOG_INFO("");
    LOG_INFO("─── Running 5 evolution ticks (500 years) ───");
    sim.set_tick_level(2); // evolution level
    sim.run(5);
    LOG_INFO("Time after evolution ticks: {}", sim.current_time().to_string());

    // Save a snapshot
auto snap_path = (std::filesystem::temp_directory_path() / "godsim_test.snap").string();
    sim.save_snapshot(snap_path);    // Run 5 more ticks
    LOG_INFO("");
    LOG_INFO("─── Running 5 more evolution ticks ───");
    sim.run(5);
    LOG_INFO("Time after more ticks: {}", sim.current_time().to_string());

    // Load the snapshot back
sim.load_snapshot(snap_path);
LOG_INFO("Time after loading snapshot: {}", sim.current_time().to_string());

    // Event log stats
    LOG_INFO("");
    LOG_INFO("─── Event Log ───");
    LOG_INFO("Total events: {}", sim.event_bus().log().size());

    // Shutdown
    sim.shutdown();

    return 0;
}
