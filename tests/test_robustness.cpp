// Robustness: random/garbage input must never crash and must be rejected
// cleanly; every profile must round-trip.
#include <catch2/catch_test_macros.hpp>

#include <random>

#include "emcast/emcast.hpp"

using emcast::Bytes;
using emcast::DecodeStatus;

namespace {
Bytes random_bytes(std::mt19937& rng, std::size_t n) {
    Bytes b(n);
    std::uniform_int_distribution<int> d(0, 255);
    for (auto& x : b) x = static_cast<std::uint8_t>(d(rng));
    return b;
}
}  // namespace

TEST_CASE("Random noise never crashes the decoder", "[robust]") {
    std::mt19937 rng(101);
    std::uniform_real_distribution<float> amp(-1.0f, 1.0f);
    emcast::Receiver rx;
    for (std::size_t len : {std::size_t{0}, std::size_t{10}, std::size_t{5000},
                            std::size_t{100000}}) {
        emcast::Samples noise(len);
        for (auto& s : noise) s = amp(rng);
        emcast::FrameInfo info;
        DecodeStatus st = DecodeStatus::Ok;
        REQUIRE_NOTHROW(st = rx.decode_samples(noise, info));
        REQUIRE(st != DecodeStatus::Ok);  // garbage must not masquerade as valid
    }
}

TEST_CASE("Random bytes never crash decode_frame", "[robust]") {
    std::mt19937 rng(102);
    for (int t = 0; t < 50; ++t) {
        Bytes junk = random_bytes(rng, 255 * (1 + (t % 4)));
        emcast::FrameInfo info;
        REQUIRE_NOTHROW((void)emcast::decode_frame(junk, info));
    }
}

TEST_CASE("All profiles round-trip", "[robust][profile]") {
    std::mt19937 rng(103);
    Bytes payload = random_bytes(rng, 400);
    for (const char* name : {"balanced", "robust", "fast"}) {
        emcast::ModemConfig cfg;
        REQUIRE(emcast::profile_from_string(name, cfg));
        emcast::Transmitter tx(cfg);
        emcast::Receiver rx(cfg);
        emcast::Samples wave = tx.encode_bytes(payload, "p.bin");
        emcast::FrameInfo got;
        REQUIRE(rx.decode_samples(wave, got) == DecodeStatus::Ok);
        REQUIRE(got.payload == payload);
    }
}

TEST_CASE("Robust profile survives a noisy channel", "[robust][profile]") {
    std::mt19937 rng(104);
    Bytes payload = random_bytes(rng, 256);
    emcast::ModemConfig cfg = emcast::ModemConfig::robust();
    emcast::Transmitter tx(cfg);
    emcast::Receiver rx(cfg);
    emcast::Samples wave = tx.encode_bytes(payload, "p.bin");
    emcast::Samples noisy = emcast::add_awgn(wave, 6.0, 7);
    emcast::FrameInfo got;
    REQUIRE(rx.decode_samples(noisy, got) == DecodeStatus::Ok);
    REQUIRE(got.payload == payload);
}
