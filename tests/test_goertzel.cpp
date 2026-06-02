#include <catch2/catch_test_macros.hpp>

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <vector>

#include "emcast/modem.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using emcast::goertzel_power;

namespace {
std::vector<float> tone(double freq, double fs, std::size_t n) {
    std::vector<float> s(n);
    for (std::size_t i = 0; i < n; ++i)
        s[i] = static_cast<float>(std::sin(2.0 * M_PI * freq * i / fs));
    return s;
}
}  // namespace

TEST_CASE("Goertzel concentrates power at the present tone", "[goertzel]") {
    const double fs = 48000.0;
    const std::size_t n = 48;
    auto s = tone(3000.0, fs, n);
    double at_mark = goertzel_power(s.data(), n, 3000.0, fs);
    double at_space = goertzel_power(s.data(), n, 2000.0, fs);
    REQUIRE(at_mark > at_space * 100.0);
}

TEST_CASE("Goertzel of silence is ~zero", "[goertzel]") {
    std::vector<float> s(48, 0.0f);
    REQUIRE(goertzel_power(s.data(), s.size(), 3000.0, 48000.0) < 1e-9);
}
