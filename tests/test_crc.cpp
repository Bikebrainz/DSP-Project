#include <catch2/catch_test_macros.hpp>

#include "emcast/fec.hpp"

using emcast::crc32;

TEST_CASE("CRC-32 matches the known IEEE check value", "[crc]") {
    const std::string s = "123456789";
    emcast::Bytes b(s.begin(), s.end());
    REQUIRE(crc32(b) == 0xCBF43926u);
}

TEST_CASE("CRC-32 of empty input is zero", "[crc]") {
    emcast::Bytes empty;
    REQUIRE(crc32(empty) == 0u);
}

TEST_CASE("CRC-32 changes when a byte flips", "[crc]") {
    emcast::Bytes a{0x10, 0x20, 0x30, 0x40};
    emcast::Bytes b = a;
    b[2] ^= 0x01;
    REQUIRE(crc32(a) != crc32(b));
}
