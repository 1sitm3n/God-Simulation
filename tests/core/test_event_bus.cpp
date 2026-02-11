#include <catch2/catch_test_macros.hpp>
#include "core/events/EventBus.h"

using namespace godsim;

TEST_CASE("EventBus emit and dispatch", "[events]") {
    EventBus bus;

    int received = 0;
    bus.subscribe<DebugEvent>(LayerID::Planetary,
        [&](const Event& e, const DebugEvent& payload) {
            received++;
            REQUIRE(payload.message == "hello");
        });

    bus.emit(DebugEvent{"hello"}, SimTime::from_days(1), ALL_LAYERS);
    REQUIRE(bus.pending_count() == 1);

    bus.dispatch();
    REQUIRE(received == 1);
    REQUIRE(bus.pending_count() == 0);
}

TEST_CASE("EventBus layer targeting", "[events]") {
    EventBus bus;

    int planetary_count = 0;
    int biological_count = 0;

    bus.subscribe<DebugEvent>(LayerID::Planetary,
        [&](const Event&, const DebugEvent&) { planetary_count++; });
    bus.subscribe<DebugEvent>(LayerID::Biological,
        [&](const Event&, const DebugEvent&) { biological_count++; });

    // Emit targeting only Planetary
    bus.emit(DebugEvent{"targeted"}, SimTime::from_days(1),
             LAYER_BIT(LayerID::Planetary));
    bus.dispatch();

    REQUIRE(planetary_count == 1);
    REQUIRE(biological_count == 0);
}

TEST_CASE("EventBus broadcast reaches all layers", "[events]") {
    EventBus bus;

    int total = 0;
    for (int i = 0; i < static_cast<int>(LayerID::COUNT); i++) {
        bus.subscribe<DebugEvent>(static_cast<LayerID>(i),
            [&](const Event&, const DebugEvent&) { total++; });
    }

    bus.emit(DebugEvent{"broadcast"}, SimTime::from_days(1), ALL_LAYERS);
    bus.dispatch();

    REQUIRE(total == static_cast<int>(LayerID::COUNT));
}

TEST_CASE("EventBus events are logged", "[events]") {
    EventBus bus;

    bus.emit(DebugEvent{"one"}, SimTime::from_days(1));
    bus.emit(DebugEvent{"two"}, SimTime::from_days(2));
    bus.emit(DebugEvent{"three"}, SimTime::from_days(3));
    bus.dispatch();

    REQUIRE(bus.log().size() == 3);
}

TEST_CASE("EventBus events dispatched in timestamp order", "[events]") {
    EventBus bus;

    std::vector<i64> received_order;
    bus.subscribe<DebugEvent>(LayerID::Cosmological,
        [&](const Event& e, const DebugEvent&) {
            received_order.push_back(e.timestamp.ticks);
        });

    // Emit out of order
    bus.emit(DebugEvent{"third"}, SimTime::from_days(30));
    bus.emit(DebugEvent{"first"}, SimTime::from_days(10));
    bus.emit(DebugEvent{"second"}, SimTime::from_days(20));
    bus.dispatch();

    REQUIRE(received_order.size() == 3);
    REQUIRE(received_order[0] == 10);
    REQUIRE(received_order[1] == 20);
    REQUIRE(received_order[2] == 30);
}

TEST_CASE("EventLog query by time range", "[events]") {
    EventLog log;

    for (int i = 1; i <= 10; i++) {
        Event e;
        e.id = i;
        e.timestamp = SimTime::from_days(i * 100);
        e.payload = DebugEvent{"event"};
        log.append(e);
    }

    auto results = log.query(SimTime::from_days(300), SimTime::from_days(700));
    REQUIRE(results.size() == 5); // days 300, 400, 500, 600, 700
}

TEST_CASE("EventLog truncate_after", "[events]") {
    EventLog log;

    for (int i = 1; i <= 10; i++) {
        Event e;
        e.id = i;
        e.timestamp = SimTime::from_days(i * 100);
        e.payload = DebugEvent{"event"};
        log.append(e);
    }

    log.truncate_after(SimTime::from_days(500));
    REQUIRE(log.size() == 5); // days 100, 200, 300, 400, 500
}

TEST_CASE("EventBus multiple handlers for same type", "[events]") {
    EventBus bus;

    int handler1_count = 0;
    int handler2_count = 0;

    bus.subscribe<DebugEvent>(LayerID::Cosmological,
        [&](const Event&, const DebugEvent&) { handler1_count++; });
    bus.subscribe<DebugEvent>(LayerID::Cosmological,
        [&](const Event&, const DebugEvent&) { handler2_count++; });

    bus.emit(DebugEvent{"test"}, SimTime::from_days(1));
    bus.dispatch();

    REQUIRE(handler1_count == 1);
    REQUIRE(handler2_count == 1);
}
