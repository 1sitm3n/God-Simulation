#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include "core/serialise/BinaryStream.h"
#include <cmath>

using namespace godsim;

TEST_CASE("BinaryStream round-trip integers", "[serialise]") {
    BinaryWriter writer;
    writer.write_u8(42);
    writer.write_u32(123456);
    writer.write_u64(9876543210ULL);
    writer.write_i64(-42);

    BinaryReader reader(writer.buffer());
    REQUIRE(reader.read_u8() == 42);
    REQUIRE(reader.read_u32() == 123456);
    REQUIRE(reader.read_u64() == 9876543210ULL);
    REQUIRE(reader.read_i64() == -42);
    REQUIRE(reader.at_end());
}

TEST_CASE("BinaryStream round-trip floats", "[serialise]") {
    BinaryWriter writer;
    writer.write_f32(3.14f);
    writer.write_f64(2.718281828459045);

    BinaryReader reader(writer.buffer());
    REQUIRE(reader.read_f32() == 3.14f);
    REQUIRE(std::abs(reader.read_f64() - 2.718281828459045) < 1e-15);
    REQUIRE(reader.at_end());
}

TEST_CASE("BinaryStream round-trip strings", "[serialise]") {
    BinaryWriter writer;
    writer.write_string("Hello, World!");
    writer.write_string("");
    writer.write_string("Unicode: test string with special chars");

    BinaryReader reader(writer.buffer());
    REQUIRE(reader.read_string() == "Hello, World!");
    REQUIRE(reader.read_string() == "");
    REQUIRE(reader.read_string() == "Unicode: test string with special chars");
    REQUIRE(reader.at_end());
}

TEST_CASE("BinaryStream file round-trip", "[serialise]") {
    BinaryWriter writer;
    writer.write_u32(42);
    writer.write_string("test");
    writer.write_f64(3.14);

    std::string path = (std::filesystem::temp_directory_path() / "godsim_test_binary.bin").string();
    writer.save_to_file(path);

    auto reader = BinaryReader::from_file(path);
    REQUIRE(reader.read_u32() == 42);
    REQUIRE(reader.read_string() == "test");
    REQUIRE(std::abs(reader.read_f64() - 3.14) < 1e-10);
    REQUIRE(reader.at_end());
}

TEST_CASE("BinaryReader throws on read past end", "[serialise]") {
    BinaryWriter writer;
    writer.write_u8(1);

    BinaryReader reader(writer.buffer());
    reader.read_u8(); // consume the one byte
    REQUIRE(reader.at_end());
    REQUIRE_THROWS(reader.read_u8());
}

TEST_CASE("BinaryStream raw bytes round-trip", "[serialise]") {
    u8 data[] = {0xDE, 0xAD, 0xBE, 0xEF};

    BinaryWriter writer;
    writer.write_bytes(data, 4);

    BinaryReader reader(writer.buffer());
    u8 out[4];
    reader.read_bytes(out, 4);

    REQUIRE(out[0] == 0xDE);
    REQUIRE(out[1] == 0xAD);
    REQUIRE(out[2] == 0xBE);
    REQUIRE(out[3] == 0xEF);
}
