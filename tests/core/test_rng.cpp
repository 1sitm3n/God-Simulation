#include <catch2/catch_test_macros.hpp>
#include "core/rng/RNG.h"
#include <set>

using namespace godsim;

TEST_CASE("RNG determinism - same seed same sequence", "[rng]") {
    RNG rng1(12345);
    RNG rng2(12345);

    for (int i = 0; i < 1000; i++) {
        REQUIRE(rng1.next_u32() == rng2.next_u32());
    }
}

TEST_CASE("RNG different seeds produce different sequences", "[rng]") {
    RNG rng1(12345);
    RNG rng2(54321);

    // Very unlikely (but not impossible) that they match
    int matches = 0;
    for (int i = 0; i < 100; i++) {
        if (rng1.next_u32() == rng2.next_u32()) matches++;
    }
    REQUIRE(matches < 5); // statistically near-impossible to match many
}

TEST_CASE("RNG float range", "[rng]") {
    RNG rng(42);
    for (int i = 0; i < 10000; i++) {
        f32 v = rng.next_float();
        REQUIRE(v >= 0.0f);
        REQUIRE(v < 1.0f);
    }
}

TEST_CASE("RNG float range with min/max", "[rng]") {
    RNG rng(42);
    for (int i = 0; i < 10000; i++) {
        f32 v = rng.next_float(-10.0f, 10.0f);
        REQUIRE(v >= -10.0f);
        REQUIRE(v < 10.0f);
    }
}

TEST_CASE("RNG int range", "[rng]") {
    RNG rng(42);
    for (int i = 0; i < 10000; i++) {
        i32 v = rng.next_int(1, 6);
        REQUIRE(v >= 1);
        REQUIRE(v <= 6);
    }
}

TEST_CASE("RNG split produces independent streams", "[rng]") {
    RNG master(42);
    RNG child1 = master.split();
    RNG child2 = master.split();

    // Children should produce different sequences from each other
    std::set<u32> values1, values2;
    for (int i = 0; i < 100; i++) {
        values1.insert(child1.next_u32());
        values2.insert(child2.next_u32());
    }

    // Count overlaps (should be very few)
    int overlaps = 0;
    for (auto v : values1) {
        if (values2.count(v)) overlaps++;
    }
    REQUIRE(overlaps < 5);
}

TEST_CASE("RNG split is deterministic", "[rng]") {
    RNG master1(42);
    RNG master2(42);

    RNG child1 = master1.split();
    RNG child2 = master2.split();

    for (int i = 0; i < 100; i++) {
        REQUIRE(child1.next_u32() == child2.next_u32());
    }
}
