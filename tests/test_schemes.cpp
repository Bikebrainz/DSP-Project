// Round-trip tests across every modulation scheme.
#include <catch2/catch_test_macros.hpp>

#include <random>

#include "emcast/emcast.hpp"

using emcast::Bytes;
using emcast::DecodeStatus;
using emcast::Scheme;

namespace {
Bytes random_bytes(std::mt19937& rng, std::size_t n) {
    Bytes b(n);
    std::uniform_int_distribution<int> d(0, 255);
    for (auto& x : b) x = static_cast<std::uint8_t>(d(rng));
    return b;
}

bool roundtrip(Scheme scheme, const Bytes& payload, double snr_db, std::uint32_t seed) {
    emcast::Transmitter tx({}, scheme);
    emcast::Samples wave = tx.encode_bytes(payload, "data.bin");
    emcast::Samples channel = (snr_db < 1e8) ? emcast::add_awgn(wave, snr_db, seed) : wave;

    emcast::Receiver rx({}, scheme);
    emcast::FrameInfo got;
    return rx.decode_samples(channel, got) == DecodeStatus::Ok && got.payload == payload;
}
}  // namespace

TEST_CASE("Scheme name round-trips through string conversion", "[scheme]") {
    for (Scheme s : {Scheme::Bfsk, Scheme::Ook}) {
        Scheme parsed;
        REQUIRE(emcast::scheme_from_string(emcast::to_string(s), parsed));
        REQUIRE(parsed == s);
    }
    Scheme dummy;
    REQUIRE_FALSE(emcast::scheme_from_string("nope", dummy));
}

TEST_CASE("BFSK round-trips noiselessly and in noise", "[scheme][bfsk]") {
    std::mt19937 rng(21);
    Bytes payload = random_bytes(rng, 300);
    REQUIRE(roundtrip(Scheme::Bfsk, payload, 1e9, 0));
    REQUIRE(roundtrip(Scheme::Bfsk, payload, 18.0, 1));
}

TEST_CASE("OOK round-trips noiselessly and at high SNR", "[scheme][ook]") {
    std::mt19937 rng(22);
    Bytes payload = random_bytes(rng, 300);
    REQUIRE(roundtrip(Scheme::Ook, payload, 1e9, 0));
    REQUIRE(roundtrip(Scheme::Ook, payload, 25.0, 1));
}
