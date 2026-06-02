#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <random>
#include <vector>

#include "emcast/fec.hpp"

using emcast::Bytes;
using emcast::ReedSolomon;

namespace {
Bytes random_block(std::mt19937& rng, std::size_t n) {
    Bytes b(n);
    std::uniform_int_distribution<int> d(0, 255);
    for (auto& x : b) x = static_cast<std::uint8_t>(d(rng));
    return b;
}
}  // namespace

TEST_CASE("RS encodes to the right size and decodes cleanly", "[rs]") {
    ReedSolomon rs(32);  // RS(255, 223)
    REQUIRE(rs.max_data_len() == 223);

    std::mt19937 rng(1);
    Bytes data = random_block(rng, 223);
    Bytes code = rs.encode(data);
    REQUIRE(code.size() == 255);

    Bytes out;
    REQUIRE(rs.decode(code, out));
    REQUIRE(out == data);
}

TEST_CASE("RS corrects up to t byte errors", "[rs]") {
    ReedSolomon rs(32);  // t = 16
    std::mt19937 rng(2);

    for (int trial = 0; trial < 20; ++trial) {
        Bytes data = random_block(rng, 223);
        Bytes code = rs.encode(data);

        // Flip 16 distinct byte positions.
        std::vector<std::size_t> idx(code.size());
        for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = i;
        std::shuffle(idx.begin(), idx.end(), rng);
        for (int e = 0; e < 16; ++e) code[idx[e]] ^= 0xFF;

        Bytes out;
        REQUIRE(rs.decode(code, out));
        REQUIRE(out == data);
    }
}

TEST_CASE("RS does not falsely 'correct' beyond capacity", "[rs]") {
    ReedSolomon rs(32);
    std::mt19937 rng(3);
    Bytes data = random_block(rng, 223);
    Bytes code = rs.encode(data);

    // 40 errors >> t=16: decoder must NOT report a successful, correct recovery.
    std::vector<std::size_t> idx(code.size());
    for (std::size_t i = 0; i < idx.size(); ++i) idx[i] = i;
    std::shuffle(idx.begin(), idx.end(), rng);
    for (int e = 0; e < 40; ++e) code[idx[e]] ^= 0xFF;

    Bytes out;
    bool ok = rs.decode(code, out);
    REQUIRE_FALSE((ok && out == data));
}

TEST_CASE("RS works with shorter parity settings", "[rs]") {
    ReedSolomon rs(10);  // t = 5
    std::mt19937 rng(4);
    Bytes data = random_block(rng, 100);
    Bytes code = rs.encode(data);
    REQUIRE(code.size() == 110);
    code[5] ^= 0xAA;
    code[50] ^= 0x11;
    Bytes out;
    REQUIRE(rs.decode(code, out));
    REQUIRE(out == data);
}
