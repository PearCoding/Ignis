#include "serialization/MemorySerializer.h"

#include <array>

#include <catch2/catch_test_macros.hpp>

using namespace IG;

// Regression: read(int16/int32/int64) used to truncate the value to 8 bits.
TEST_CASE("Serializer round-trips signed integers wider than 8 bits", "[Serializer]")
{
    std::array<uint8, 256> buffer{};

    const int8 i8   = -100;
    const int16 i16 = -1234;
    const int32 i32 = 100000;
    const int64 i64 = -5000000000LL;

    {
        MemorySerializer writer(buffer.data(), buffer.size(), false);
        writer.write(i8);
        writer.write(i16);
        writer.write(i32);
        writer.write(i64);
    }

    MemorySerializer reader(buffer.data(), buffer.size(), true);
    int8 r8;
    int16 r16;
    int32 r32;
    int64 r64;
    reader.read(r8);
    reader.read(r16);
    reader.read(r32);
    reader.read(r64);

    CHECK(r8 == i8);
    CHECK(r16 == i16);
    CHECK(r32 == i32);
    CHECK(r64 == i64);
}

TEST_CASE("Serializer round-trips unsigned integers and floats", "[Serializer]")
{
    std::array<uint8, 256> buffer{};

    const uint16 u16 = 60000;
    const uint32 u32 = 4000000000u;
    const uint64 u64 = 12000000000ULL;
    const float f    = 3.14159f;
    const double d   = 2.718281828;

    {
        MemorySerializer writer(buffer.data(), buffer.size(), false);
        writer.write(u16);
        writer.write(u32);
        writer.write(u64);
        writer.write(f);
        writer.write(d);
    }

    MemorySerializer reader(buffer.data(), buffer.size(), true);
    uint16 ru16;
    uint32 ru32;
    uint64 ru64;
    float rf;
    double rd;
    reader.read(ru16);
    reader.read(ru32);
    reader.read(ru64);
    reader.read(rf);
    reader.read(rd);

    CHECK(ru16 == u16);
    CHECK(ru32 == u32);
    CHECK(ru64 == u64);
    CHECK(rf == f);
    CHECK(rd == d);
}

TEST_CASE("Serializer round-trips strings", "[Serializer]")
{
    std::array<uint8, 64> buffer{};

    const std::string in = "hello world";
    {
        MemorySerializer writer(buffer.data(), buffer.size(), false);
        writer.write(in);
    }

    MemorySerializer reader(buffer.data(), buffer.size(), true);
    std::string out;
    reader.read(out);
    CHECK(out == in);
}

// Regression: read(std::string) on a stream with no terminating zero must stop at end of
// buffer instead of looping forever.
TEST_CASE("Serializer string read stops at end of buffer", "[Serializer]")
{
    std::array<uint8, 4> buffer{ 'a', 'b', 'c', 'd' }; // no null terminator

    MemorySerializer reader(buffer.data(), buffer.size(), true);
    std::string out;
    reader.read(out); // must terminate at EOF, not hang
    CHECK(out == "abcd");
}
