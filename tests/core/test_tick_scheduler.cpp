#include <catch2/catch_test_macros.hpp>
#include "core/time/SimTime.h"
#include "core/time/TickScheduler.h"
#include "core/events/EventBus.h"
#include "core/ecs/Registry.h"
#include "core/rng/RNG.h"
#include "layers/Layer.h"

using namespace godsim;

// ─── SimTime Tests ───
TEST_CASE("SimTime unit conversions", "[time]") {
    auto t = SimTime::from_years(1.0);
    REQUIRE(t.ticks == 365); // 365.25 truncated

    auto t2 = SimTime::from_megayears(1.0);
    REQUIRE(t2.years() > 999000.0);
    REQUIRE(t2.years() < 1001000.0);
}

TEST_CASE("SimTime arithmetic", "[time]") {
    auto a = SimTime::from_days(100);
    auto b = SimTime::from_days(50);

    REQUIRE((a + b).ticks == 150);
    REQUIRE((a - b).ticks == 50);
}

TEST_CASE("SimTime comparison", "[time]") {
    auto a = SimTime::from_days(100);
    auto b = SimTime::from_days(200);

    REQUIRE(a < b);
    REQUIRE(b > a);
    REQUIRE(a != b);
    REQUIRE(a == SimTime::from_days(100));
}

// ─── Minimal test layer ───
class TestLayer : public Layer {
public:
    TestLayer(LayerID lid, const std::string& n) : lid_(lid), name_(n) {}

    LayerID id() const override { return lid_; }
    std::string name() const override { return name_; }

    void initialise(Registry&, EventBus&, RNG&) override {}
    void shutdown() override {}

    void tick(SimTime current_time, SimTime delta_time) override {
        increment_tick();
        last_time_ = current_time;
        last_delta_ = delta_time;
    }

    void serialise(BinaryWriter&) const override {}
    void deserialise(BinaryReader&) override {}

    SimTime last_time_ = {};
    SimTime last_delta_ = {};

private:
    LayerID lid_;
    std::string name_;
};

// ─── TickScheduler Tests ───
TEST_CASE("TickScheduler ticks layers at correct level", "[time]") {
    EventBus bus;
    TickScheduler scheduler(bus);

    // Two levels: fine (1 day, all layers) and coarse (365 days, layer 0 only)
    scheduler.add_level({"fine", SimTime::from_days(1), ALL_LAYERS});
    scheduler.add_level({"coarse", SimTime::from_years(1), LAYER_BIT(LayerID::Cosmological)});

    TestLayer cosmo(LayerID::Cosmological, "Cosmo");
    TestLayer planet(LayerID::Planetary, "Planet");
    scheduler.register_layer(&cosmo);
    scheduler.register_layer(&planet);

    // Run at fine level — both should tick
    scheduler.set_active_level(0);
    scheduler.run(10);

    REQUIRE(cosmo.tick_count() == 10);
    REQUIRE(planet.tick_count() == 10);

    // Switch to coarse — only cosmo should tick from here
    u64 cosmo_before = cosmo.tick_count();
    u64 planet_before = planet.tick_count();

    scheduler.set_active_level(1);
    scheduler.run(5);

    REQUIRE(cosmo.tick_count() == cosmo_before + 5);
    REQUIRE(planet.tick_count() == planet_before); // unchanged
}

TEST_CASE("TickScheduler advances time correctly", "[time]") {
    EventBus bus;
    TickScheduler scheduler(bus);
    scheduler.add_level({"yearly", SimTime::from_years(1), ALL_LAYERS});

    TestLayer layer(LayerID::Cosmological, "Test");
    scheduler.register_layer(&layer);

    scheduler.set_active_level(0);
    scheduler.run(10);

    // Should have advanced by ~10 years
    auto time = scheduler.current_time();
    REQUIRE(time.years() > 9.0);
    REQUIRE(time.years() < 11.0);
}

TEST_CASE("TickScheduler pause prevents ticking", "[time]") {
    EventBus bus;
    TickScheduler scheduler(bus);
    scheduler.add_level({"daily", SimTime::from_days(1), ALL_LAYERS});

    TestLayer layer(LayerID::Cosmological, "Test");
    scheduler.register_layer(&layer);

    // Paused by default
    REQUIRE(scheduler.is_paused());
    scheduler.step(); // should not tick
    REQUIRE(layer.tick_count() == 0);

    scheduler.resume();
    scheduler.step();
    REQUIRE(layer.tick_count() == 1);

    scheduler.pause();
    scheduler.step();
    REQUIRE(layer.tick_count() == 1); // still 1
}

TEST_CASE("TickScheduler dispatches events after tick", "[time]") {
    EventBus bus;
    TickScheduler scheduler(bus);
    scheduler.configure_defaults();

    TestLayer layer(LayerID::Cosmological, "Test");
    scheduler.register_layer(&layer);

    int events_received = 0;
    bus.subscribe<LayerTickedEvent>(LayerID::Cosmological,
        [&](const Event&, const LayerTickedEvent&) { events_received++; });

    scheduler.set_active_level(0);
    scheduler.run(5);

    // Each step dispatches events (tick event from scheduler itself)
    REQUIRE(events_received >= 5);
}
