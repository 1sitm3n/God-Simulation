#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include "core/util/Log.h"
#include "simulation/Simulation.h"
#include "layers/cosmological/CosmologicalLayer.h"
#include "layers/planetary/PlanetaryLayer.h"
#include "layers/biological/BiologicalLayer.h"
#include "layers/civilisation/CivilisationLayer.h"
#include "layers/divine/DivineLayer.h"

using namespace godsim;

static void setup_sim(Simulation& sim) {
    sim.add_layer<CosmologicalLayer>();
    sim.add_layer<PlanetaryLayer>();
    sim.add_layer<BiologicalLayer>();
    sim.add_layer<CivilisationLayer>();
    sim.add_layer<DivineLayer>();
    sim.initialise();
}

TEST_CASE("Full simulation lifecycle", "[integration]") {
    Log::init();

    Simulation sim(42);
    setup_sim(sim);

    // Run at history level (index 1)
    sim.set_tick_level(1);
    sim.run(100);

    // Time should have advanced by ~100 years
    auto time = sim.current_time();
    REQUIRE(time.years() > 90.0);
    REQUIRE(time.years() < 110.0);

    // Events should have been logged
    REQUIRE(sim.event_bus().log().size() > 0);

    sim.shutdown();
}

TEST_CASE("Snapshot save and load", "[integration]") {
    Simulation sim(42);
    setup_sim(sim);

    sim.set_tick_level(2); // evolution level
    sim.run(10);

    SimTime time_at_snapshot = sim.current_time();
    std::string path = (std::filesystem::temp_directory_path() / "godsim_integration_test.snap").string();
    sim.save_snapshot(path);

    // Run more ticks
    sim.run(10);
    REQUIRE(sim.current_time() > time_at_snapshot);

    // Load snapshot — time should revert
    sim.load_snapshot(path);
    REQUIRE(sim.current_time() == time_at_snapshot);

    sim.shutdown();
}

TEST_CASE("Cross-layer events are delivered", "[integration]") {
    Simulation sim(42);
    setup_sim(sim);

    // Subscribe to layer tick events on Planetary
    int cosmo_ticks_seen_by_planetary = 0;
    sim.event_bus().subscribe<LayerTickedEvent>(LayerID::Planetary,
        [&](const Event&, const LayerTickedEvent& e) {
            if (e.layer == LayerID::Cosmological) {
                cosmo_ticks_seen_by_planetary++;
            }
        });

    sim.set_tick_level(1); // history level — all layers tick
    sim.run(5);

    // Cosmological layer emits LayerTickedEvent each tick,
    // Planetary should have received them
    REQUIRE(cosmo_ticks_seen_by_planetary == 5);

    sim.shutdown();
}

TEST_CASE("Different tick levels activate correct layers", "[integration]") {
    Simulation sim(42);
    setup_sim(sim);

    // At cosmic level (index 4), only cosmological should tick
    sim.set_tick_level(4);
    sim.run(3);

    // We can check via event log — should have LayerTickedEvents
    // only from Cosmological layer (plus the scheduler's own tick event)
    auto& log = sim.event_bus().log();
    int cosmo_tick_events = 0;
    int other_tick_events = 0;

    for (const auto& event : log.events()) {
        if (auto* lte = std::get_if<LayerTickedEvent>(&event.payload)) {
            if (lte->layer == LayerID::Cosmological) cosmo_tick_events++;
            else if (lte->layer != LayerID::COUNT) other_tick_events++;
        }
    }

    REQUIRE(cosmo_tick_events == 3);
    REQUIRE(other_tick_events == 0);

    sim.shutdown();
}
