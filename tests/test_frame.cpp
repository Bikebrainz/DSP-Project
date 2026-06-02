#include <catch2/catch_test_macros.hpp>

#include <random>

#include "emcast/frame.hpp"

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
}  // namespace

TEST_CASE("Frame round-trips across block boundaries", "[frame]") {
    std::mt19937 rng(7);
    for (std::size_t n : {std::size_t{0}, std::size_t{1}, std::size_t{222}, std::size_t{223},
                          std::size_t{224}, std::size_t{500}, std::size_t{2048}}) {
        Bytes payload = random_bytes(rng, n);
        Bytes frame = emcast::encode_frame(payload, "photo.jpg");
        REQUIRE(frame.size() % emcast::kBlockTotal == 0);

        FrameInfo info;
        REQUIRE(emcast::decode_frame(frame, info) == DecodeStatus::Ok);
        REQUIRE(info.payload == payload);
        REQUIRE(info.filename == "photo.jpg");
    }
}

TEST_CASE("Frame strips directory components from the name", "[frame]") {
    Bytes payload{1, 2, 3, 4, 5};
    Bytes frame = emcast::encode_frame(payload, "C:\\photos\\sub/dir\\cat.png");
    FrameInfo info;
    REQUIRE(emcast::decode_frame(frame, info) == DecodeStatus::Ok);
    REQUIRE(info.filename == "cat.png");
}

TEST_CASE("Frame tolerates byte errors within RS capacity", "[frame]") {
    std::mt19937 rng(8);
    Bytes payload = random_bytes(rng, 400);  // spans 2 blocks
    Bytes frame = emcast::encode_frame(payload, "x");

    // Corrupt up to 16 bytes in each 255-byte block — within RS(255,223).
    for (std::size_t base = 0; base < frame.size(); base += emcast::kBlockTotal) {
        for (int e = 0; e < 16; ++e) frame[base + e] ^= 0xFF;
    }
    FrameInfo info;
    REQUIRE(emcast::decode_frame(frame, info) == DecodeStatus::Ok);
    REQUIRE(info.payload == payload);
}

TEST_CASE("Frame reports truncation", "[frame]") {
    Bytes payload{9, 8, 7};
    Bytes frame = emcast::encode_frame(payload, "x");
    frame.resize(frame.size() / 2);  // chop it in half
    FrameInfo info;
    DecodeStatus st = emcast::decode_frame(frame, info);
    REQUIRE((st == DecodeStatus::Truncated || st == DecodeStatus::BadHeader));
}
