#include <catch2/catch_test_macros.hpp>
#include "core/ecs/EntityID.h"
#include "core/ecs/Registry.h"
#include "core/util/Log.h"

using namespace godsim;

// ─── Test Components ───
struct Position {
    f32 x, y, z;
};
struct Velocity {
    f32 dx, dy, dz;
};
struct Name {
    std::string value;
};

// ─── EntityID Tests ───
TEST_CASE("EntityID encodes and decodes layer correctly", "[ecs]") {
    auto eid = EntityID::create(LayerID::Biological, 42);
    REQUIRE(eid.layer() == LayerID::Biological);
    REQUIRE(eid.id() == 42);
}

TEST_CASE("EntityID null is invalid", "[ecs]") {
    auto null = EntityID::null();
    REQUIRE_FALSE(null.is_valid());
}

TEST_CASE("EntityID equality", "[ecs]") {
    auto a = EntityID::create(LayerID::Planetary, 100);
    auto b = EntityID::create(LayerID::Planetary, 100);
    auto c = EntityID::create(LayerID::Planetary, 101);
    auto d = EntityID::create(LayerID::Biological, 100);

    REQUIRE(a == b);
    REQUIRE(a != c);
    REQUIRE(a != d); // same id but different layer
}

TEST_CASE("EntityID preserves full ID range", "[ecs]") {
    u64 big_id = 0x00FFFFFFFFFFF;
    auto eid = EntityID::create(LayerID::Divine, big_id);
    REQUIRE(eid.layer() == LayerID::Divine);
    REQUIRE(eid.id() == big_id);
}

// ─── Registry Tests ───
TEST_CASE("Registry creates entities in correct layers", "[ecs]") {
    Registry reg;

    auto e1 = reg.create_entity(LayerID::Cosmological);
    auto e2 = reg.create_entity(LayerID::Biological);
    auto e3 = reg.create_entity(LayerID::Cosmological);

    REQUIRE(e1.layer() == LayerID::Cosmological);
    REQUIRE(e2.layer() == LayerID::Biological);
    REQUIRE(e3.layer() == LayerID::Cosmological);
    REQUIRE(reg.entity_count() == 3);
}

TEST_CASE("Registry add and get components", "[ecs]") {
    Registry reg;
    auto eid = reg.create_entity(LayerID::Planetary);

    reg.add_component<Position>(eid, 1.0f, 2.0f, 3.0f);
    reg.add_component<Name>(eid, "TestEntity");

    REQUIRE(reg.has_component<Position>(eid));
    REQUIRE(reg.has_component<Name>(eid));
    REQUIRE_FALSE(reg.has_component<Velocity>(eid));

    auto& pos = reg.get_component<Position>(eid);
    REQUIRE(pos.x == 1.0f);
    REQUIRE(pos.y == 2.0f);

    auto& nm = reg.get_component<Name>(eid);
    REQUIRE(nm.value == "TestEntity");
}

TEST_CASE("Registry destroy entity", "[ecs]") {
    Registry reg;
    auto e1 = reg.create_entity(LayerID::Biological);
    auto e2 = reg.create_entity(LayerID::Biological);
    reg.add_component<Position>(e1, 0.f, 0.f, 0.f);
    reg.add_component<Position>(e2, 1.f, 1.f, 1.f);

    REQUIRE(reg.entity_count() == 2);
    reg.destroy_entity(e1);
    REQUIRE(reg.entity_count() == 1);
    REQUIRE_FALSE(reg.is_alive(e1));
    REQUIRE(reg.is_alive(e2));
}

TEST_CASE("Registry iterate all entities with components", "[ecs]") {
    Registry reg;
    auto e1 = reg.create_entity(LayerID::Planetary);
    auto e2 = reg.create_entity(LayerID::Biological);
    auto e3 = reg.create_entity(LayerID::Planetary);

    reg.add_component<Position>(e1, 1.f, 0.f, 0.f);
    reg.add_component<Position>(e2, 2.f, 0.f, 0.f);
    reg.add_component<Position>(e3, 3.f, 0.f, 0.f);
    reg.add_component<Velocity>(e1, 0.f, 1.f, 0.f); // only e1 has both

    int count = 0;
    reg.each<Position, Velocity>([&](EntityID eid, Position& pos, Velocity& vel) {
        count++;
        REQUIRE(eid == e1);
    });
    REQUIRE(count == 1);
}

TEST_CASE("Registry iterate entities in specific layer", "[ecs]") {
    Registry reg;
    auto e1 = reg.create_entity(LayerID::Cosmological);
    auto e2 = reg.create_entity(LayerID::Planetary);
    auto e3 = reg.create_entity(LayerID::Cosmological);

    reg.add_component<Position>(e1, 1.f, 0.f, 0.f);
    reg.add_component<Position>(e2, 2.f, 0.f, 0.f);
    reg.add_component<Position>(e3, 3.f, 0.f, 0.f);

    int cosmo_count = 0;
    reg.each_in_layer<Position>(LayerID::Cosmological,
        [&](EntityID eid, Position& pos) { cosmo_count++; });
    REQUIRE(cosmo_count == 2);

    int planet_count = 0;
    reg.each_in_layer<Position>(LayerID::Planetary,
        [&](EntityID eid, Position& pos) { planet_count++; });
    REQUIRE(planet_count == 1);
}

TEST_CASE("Registry entity count per layer", "[ecs]") {
    Registry reg;
    for (int i = 0; i < 100; i++) reg.create_entity(LayerID::Biological);
    for (int i = 0; i < 50; i++) reg.create_entity(LayerID::Civilisation);

    REQUIRE(reg.entity_count() == 150);
    REQUIRE(reg.entity_count_in_layer(LayerID::Biological) == 100);
    REQUIRE(reg.entity_count_in_layer(LayerID::Civilisation) == 50);
    REQUIRE(reg.entity_count_in_layer(LayerID::Cosmological) == 0);
}

TEST_CASE("Registry remove component", "[ecs]") {
    Registry reg;
    auto eid = reg.create_entity(LayerID::Planetary);
    reg.add_component<Position>(eid, 0.f, 0.f, 0.f);
    REQUIRE(reg.has_component<Position>(eid));

    reg.remove_component<Position>(eid);
    REQUIRE_FALSE(reg.has_component<Position>(eid));
}
