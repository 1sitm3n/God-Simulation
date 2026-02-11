#include <catch2/catch_test_macros.hpp>
#include "core/noise/Noise.h"
#include "layers/planetary/Heightmap.h"
#include "layers/planetary/Biome.h"
#include "layers/planetary/TerrainGenerator.h"
#include "layers/planetary/ClimateGenerator.h"
#include "layers/planetary/PlanetData.h"
#include "core/rng/RNG.h"

using namespace godsim;

// ═══ Perlin Noise Tests ═══

TEST_CASE("Perlin noise is deterministic", "[noise]") {
    PerlinNoise n1(42);
    PerlinNoise n2(42);

    for (int i = 0; i < 100; i++) {
        f64 x = i * 0.1;
        f64 y = i * 0.13;
        REQUIRE(n1.noise(x, y) == n2.noise(x, y));
    }
}

TEST_CASE("Perlin noise output range is bounded", "[noise]") {
    PerlinNoise n(123);

    for (int i = 0; i < 10000; i++) {
        f64 x = (i % 100) * 0.07;
        f64 y = (i / 100) * 0.07;
        f64 v = n.noise(x, y);
        REQUIRE(v >= -2.0); // Theoretical max is sqrt(2) ~ 1.414
        REQUIRE(v <= 2.0);
    }
}

TEST_CASE("Perlin fbm returns normalised range", "[noise]") {
    PerlinNoise n(456);

    for (int i = 0; i < 1000; i++) {
        f64 x = (i % 50) * 0.1;
        f64 y = (i / 50) * 0.1;
        f64 v = n.fbm(x, y, 6, 1.0, 0.5, 2.0);
        REQUIRE(v >= -1.5);
        REQUIRE(v <= 1.5);
    }
}

TEST_CASE("Different seeds produce different noise", "[noise]") {
    PerlinNoise n1(42);
    PerlinNoise n2(99);

    int same = 0;
    for (int i = 0; i < 100; i++) {
        if (n1.noise(i * 0.1, i * 0.2) == n2.noise(i * 0.1, i * 0.2)) same++;
    }
    REQUIRE(same < 25);
}

// ═══ Heightmap Tests ═══

TEST_CASE("Heightmap basic operations", "[heightmap]") {
    Heightmap hm(64, 64, 0.5f);

    REQUIRE(hm.width() == 64);
    REQUIRE(hm.height() == 64);
    REQUIRE(hm.get(0, 0) == 0.5f);

    hm.set(10, 20, 0.8f);
    REQUIRE(hm.get(10, 20) == 0.8f);
}

TEST_CASE("Heightmap normalise", "[heightmap]") {
    Heightmap hm(4, 4);
    hm.set(0, 0, -10.0f);
    hm.set(1, 0, 50.0f);
    hm.set(2, 0, 20.0f);

    hm.normalise();

    REQUIRE(hm.min_value() >= 0.0f);
    REQUIRE(hm.max_value() <= 1.0f);
    // The max original (50) should map to 1.0
    REQUIRE(hm.get(1, 0) == 1.0f);
    // The min original (-10) should map to 0.0
    REQUIRE(hm.get(0, 0) == 0.0f);
}

TEST_CASE("Heightmap bilinear sampling", "[heightmap]") {
    Heightmap hm(2, 2);
    hm.set(0, 0, 0.0f);
    hm.set(1, 0, 1.0f);
    hm.set(0, 1, 0.0f);
    hm.set(1, 1, 1.0f);

    // Midpoint should interpolate to 0.5
    f32 mid = hm.sample(0.5f, 0.5f);
    REQUIRE(mid > 0.4f);
    REQUIRE(mid < 0.6f);
}

TEST_CASE("Heightmap serialisation round-trip", "[heightmap]") {
    Heightmap original(32, 32);
    for (u32 y = 0; y < 32; y++)
        for (u32 x = 0; x < 32; x++)
            original.set(x, y, static_cast<f32>(x + y) / 64.0f);

    BinaryWriter writer;
    original.serialise(writer);

    BinaryReader reader(writer.buffer());
    Heightmap loaded;
    loaded.deserialise(reader);

    REQUIRE(loaded.width() == 32);
    REQUIRE(loaded.height() == 32);
    REQUIRE(loaded.get(10, 10) == original.get(10, 10));
    REQUIRE(loaded.get(31, 31) == original.get(31, 31));
}

// ═══ Biome Classification Tests ═══

TEST_CASE("Biome: deep ocean classification", "[biome]") {
    auto b = classify_biome(0.2f, 15.0f, 0.5f, 0.4f);
    REQUIRE(b == BiomeType::DeepOcean);
}

TEST_CASE("Biome: desert at high temp low moisture", "[biome]") {
    auto b = classify_biome(0.6f, 35.0f, 0.1f, 0.4f);
    REQUIRE(b == BiomeType::Desert);
}

TEST_CASE("Biome: tropical rainforest at high temp high moisture", "[biome]") {
    auto b = classify_biome(0.55f, 28.0f, 0.8f, 0.4f);
    REQUIRE(b == BiomeType::TropicalRainforest);
}

TEST_CASE("Biome: tundra at cold temperatures", "[biome]") {
    auto b = classify_biome(0.5f, -5.0f, 0.3f, 0.4f);
    REQUIRE(b == BiomeType::Tundra);
}

TEST_CASE("Biome: mountain at high elevation", "[biome]") {
    auto b = classify_biome(0.9f, 5.0f, 0.3f, 0.4f);
    REQUIRE(b == BiomeType::Mountain);
}

TEST_CASE("Biome: beach near sea level", "[biome]") {
    auto b = classify_biome(0.405f, 20.0f, 0.5f, 0.4f);
    REQUIRE(b == BiomeType::Beach);
}

// ═══ Terrain Generator Tests ═══

TEST_CASE("Terrain generator produces valid heightmap", "[terrain]") {
    RNG rng(42);
    TerrainGenerator gen(rng);

    TerrainConfig config;
    config.width = 64;
    config.height = 64;
    config.erosion_iterations = 100;

    auto elevation = gen.generate(config);

    REQUIRE(elevation.width() == 64);
    REQUIRE(elevation.height() == 64);
    REQUIRE(elevation.min_value() >= 0.0f);
    REQUIRE(elevation.max_value() <= 1.0f);
}

TEST_CASE("Terrain generator is deterministic", "[terrain]") {
    TerrainConfig config;
    config.width = 32;
    config.height = 32;
    config.erosion_iterations = 50;

    RNG rng1(42);
    TerrainGenerator gen1(rng1);
    auto map1 = gen1.generate(config);

    RNG rng2(42);
    TerrainGenerator gen2(rng2);
    auto map2 = gen2.generate(config);

    for (u32 y = 0; y < 32; y++) {
        for (u32 x = 0; x < 32; x++) {
            REQUIRE(map1.get(x, y) == map2.get(x, y));
        }
    }
}

TEST_CASE("Terrain has both land and ocean", "[terrain]") {
    RNG rng(42);
    TerrainGenerator gen(rng);

    TerrainConfig config;
    config.width = 64;
    config.height = 64;
    config.erosion_iterations = 100;

    auto elevation = gen.generate(config);

    int above = 0, below = 0;
    for (u32 y = 0; y < 64; y++) {
        for (u32 x = 0; x < 64; x++) {
            if (elevation.get(x, y) > config.sea_level) above++;
            else below++;
        }
    }

    // Both land and ocean should exist
    REQUIRE(above > 0);
    REQUIRE(below > 0);
}

// ═══ Climate Tests ═══

TEST_CASE("Temperature is cold at poles, warm at equator", "[climate]") {
    // Create a flat elevation map at land level
    Heightmap elevation(64, 64, 0.5f);

    RNG rng(42);
    ClimateGenerator climate(rng);
    ClimateConfig config;
    auto temp = climate.generate_temperature(elevation, config);

    // Equator (center row) should be warmer than poles (top/bottom)
    f32 equator_temp = temp.get(32, 32); // Center
    f32 pole_temp = temp.get(32, 0);     // Top

    REQUIRE(equator_temp > pole_temp);
}

TEST_CASE("Moisture decreases inland", "[climate]") {
    // Create elevation: ocean on left, land on right
    Heightmap elevation(64, 64);
    for (u32 y = 0; y < 64; y++) {
        for (u32 x = 0; x < 64; x++) {
            elevation.set(x, y, x < 16 ? 0.2f : 0.6f); // Left=ocean, right=land
        }
    }

    RNG rng(42);
    ClimateGenerator climate(rng);
    ClimateConfig config;
    auto temp = climate.generate_temperature(elevation, config);
    auto moisture = climate.generate_moisture(elevation, temp, config);

    // Near-coast land should be wetter than far inland
    f32 coastal = moisture.get(18, 32); // Just past shore
    f32 inland = moisture.get(60, 32);  // Far from ocean

    REQUIRE(coastal > inland);
}

// ═══ PlanetData Tests ═══

TEST_CASE("PlanetData serialisation round-trip", "[planet]") {
    RNG rng(42);
    TerrainGenerator terrain_gen(rng);
    ClimateGenerator climate_gen(rng);

    TerrainConfig tc;
    tc.width = 32;
    tc.height = 32;
    tc.erosion_iterations = 50;

    PlanetData planet;
    planet.name = "TestWorld";
    planet.width = 32;
    planet.height = 32;
    planet.sea_level = 0.4f;
    planet.elevation = terrain_gen.generate(tc);

    ClimateConfig cc;
    cc.sea_level = 0.4f;
    planet.temperature = climate_gen.generate_temperature(planet.elevation, cc);
    planet.moisture = climate_gen.generate_moisture(planet.elevation, planet.temperature, cc);
    planet.classify_biomes();

    // Serialise
    BinaryWriter writer;
    planet.serialise(writer);

    // Deserialise
    BinaryReader reader(writer.buffer());
    PlanetData loaded;
    loaded.deserialise(reader);

    REQUIRE(loaded.name == "TestWorld");
    REQUIRE(loaded.width == 32);
    REQUIRE(loaded.height == 32);
    REQUIRE(loaded.elevation.get(10, 10) == planet.elevation.get(10, 10));
    REQUIRE(loaded.biome_at(5, 5) == planet.biome_at(5, 5));
}
