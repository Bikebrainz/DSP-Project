// End-to-end property test: file bytes -> modulate -> (noisy) channel ->
// demodulate -> decode must reproduce the payload bit-exactly.
#include <catch2/catch_test_macros.hpp>

#include <random>

#include "emcast/emcast.hpp"

using emcast::Bytes;
using emcast::DecodeStatus;
using emcast::FrameInfo;

namespace {
Bytes random_bytes(std::mt19937& rng, std::size_t n) {
    Bytes b(n);
    std::uniform_int_distribution<int> d(0, 255);
    for (auto& x : b) x = static_cast<std::uint8_t>(d(rng));
    return b;
}

bool roundtrip(const Bytes& payload, double snr_db, std::uint32_t seed) {
    emcast::Transmitter tx;
    emcast::Samples wave = tx.encode_bytes(payload, "data.bin");
    emcast::Samples channel = (snr_db < 1e8) ? emcast::add_awgn(wave, snr_db, seed) : wave;

    emcast::Receiver rx;
    FrameInfo got;
    DecodeStatus st = rx.decode_samples(channel, got);
    return st == DecodeStatus::Ok && got.payload == payload;
}
}  // namespace

TEST_CASE("Noiseless round-trip is bit-exact", "[roundtrip]") {
    std::mt19937 rng(11);
    for (std::size_t n : {std::size_t{1}, std::size_t{32}, std::size_t{223}, std::size_t{600}}) {
        Bytes payload = random_bytes(rng, n);
        REQUIRE(roundtrip(payload, 1e9, 0));
    }
}

TEST_CASE("Round-trip survives a noisy channel", "[roundtrip]") {
    std::mt19937 rng(12);
    Bytes payload = random_bytes(rng, 256);
    // High-ish SNR: FEC + BFSK should recover exactly.
    REQUIRE(roundtrip(payload, 20.0, 1));
    REQUIRE(roundtrip(payload, 15.0, 2));
}
