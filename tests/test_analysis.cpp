#include <catch2/catch_test_macros.hpp>

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>

#include "emcast/analysis.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

TEST_CASE("Spectrogram peaks at the tone frequency", "[analysis]") {
    const std::uint32_t sr = 48000;
    const std::size_t fft = 512;
    const double freq = 3000.0;
    emcast::Samples s(fft * 8);
    for (std::size_t i = 0; i < s.size(); ++i)
        s[i] = static_cast<float>(std::sin(2.0 * M_PI * freq * i / sr));

    emcast::Spectrogram spec = emcast::compute_spectrogram(s, sr, fft);
    REQUIRE(spec.frames > 0);
    REQUIRE(spec.bins == fft / 2);

    // Find the strongest bin in the first frame.
    std::size_t best = 0;
    float best_m = -1.0f;
    for (std::size_t k = 0; k < spec.bins; ++k) {
        float m = spec.mag[k];
        if (m > best_m) {
            best_m = m;
            best = k;
        }
    }
    const std::size_t expected = static_cast<std::size_t>(std::lround(freq / spec.bin_hz()));
    REQUIRE(best >= expected - 1);
    REQUIRE(best <= expected + 1);
}

TEST_CASE("Spectrogram of a too-short signal is empty", "[analysis]") {
    emcast::Samples s(100, 0.0f);
    emcast::Spectrogram spec = emcast::compute_spectrogram(s, 48000, 512);
    REQUIRE(spec.frames == 0);
    std::size_t w = 0, h = 0;
    auto img = emcast::spectrogram_to_rgb(spec, w, h);
    REQUIRE(w == 0);
}
