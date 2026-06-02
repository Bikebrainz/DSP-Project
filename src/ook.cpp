#include "emcast/modem.hpp"

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <algorithm>
#include <cmath>
#include <vector>

#include "modem_detail.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace emcast {

Samples Ook::modulate(const Bytes& frame_bytes) const {
    std::vector<std::uint8_t> bits = detail::build_bitstream(cfg_, kSyncWord, frame_bytes);

    Samples out;
    out.reserve(bits.size() * cfg_.samples_per_symbol);
    double phase = 0.0;
    const double two_pi = 2.0 * M_PI;
    const double dphi = two_pi * cfg_.freq_mark / cfg_.sample_rate;
    for (std::uint8_t bit : bits) {
        for (std::uint32_t s = 0; s < cfg_.samples_per_symbol; ++s) {
            // Gate the carrier: full amplitude for a 1, silence for a 0.
            out.push_back(bit ? static_cast<Sample>(cfg_.amplitude * std::sin(phase)) : 0.0f);
            phase += dphi;
            if (phase >= two_pi) phase -= two_pi;
        }
    }
    return out;
}

Bytes Ook::demodulate(const Samples& samples) const {
    const std::size_t sps = cfg_.samples_per_symbol;
    if (samples.size() < sps * 2) return {};

    std::vector<Sample> norm = detail::normalize(samples, sps);
    const std::size_t s0 = detail::detect_start(norm, sps);
    const std::size_t fine_step = std::max<std::size_t>(1, sps / 16);

    for (std::size_t fine = 0; fine < sps; fine += fine_step) {
        const std::size_t start = s0 + fine;
        if (start + sps > norm.size()) break;

        // Per-symbol energy at the mark tone.
        std::vector<double> power;
        power.reserve((norm.size() - start) / sps);
        for (std::size_t pos = start; pos + sps <= norm.size(); pos += sps)
            power.push_back(goertzel_power(&norm[pos], sps, cfg_.freq_mark, cfg_.sample_rate));

        // Adaptive threshold at a fraction of the strongest "on" symbol.
        double peak = 0.0;
        for (double p : power) peak = std::max(peak, p);
        const double thresh = 0.45 * peak;

        std::vector<std::uint8_t> bits(power.size());
        for (std::size_t i = 0; i < power.size(); ++i) bits[i] = power[i] > thresh ? 1u : 0u;

        Bytes frame = detail::pack_after_sync(bits, kSyncWord);
        if (!frame.empty()) return frame;
    }
    return {};
}

}  // namespace emcast
