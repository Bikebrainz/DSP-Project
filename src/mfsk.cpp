// 4-ary FSK: two bits per symbol mapped to one of four tones, recovered by
// non-coherent energy detection (the strongest tone wins). Doubles throughput
// versus BFSK at the cost of occupied bandwidth.
#include "emcast/modem.hpp"

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <array>
#include <cmath>
#include <vector>

#include "modem_detail.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace emcast {
namespace {
// Four tones: freq_space + i * (freq_mark - freq_space), i = 0..3.
std::array<double, 4> tones(const ModemConfig& cfg) {
    const double spacing = cfg.freq_mark - cfg.freq_space;
    return {cfg.freq_space, cfg.freq_space + spacing, cfg.freq_space + 2 * spacing,
            cfg.freq_space + 3 * spacing};
}
}  // namespace

Samples Mfsk::modulate(const Bytes& frame_bytes) const {
    std::vector<std::uint8_t> bits = detail::build_bitstream(cfg_, kSyncWord, frame_bytes);
    if (bits.size() % 2) bits.push_back(0);  // pad to whole 2-bit symbols
    const std::array<double, 4> f = tones(cfg_);

    Samples out;
    out.reserve((bits.size() / 2) * cfg_.samples_per_symbol);
    const double two_pi = 2.0 * M_PI;
    double phase = 0.0;
    for (std::size_t i = 0; i + 1 < bits.size(); i += 2) {
        const int sym = (bits[i] << 1) | bits[i + 1];
        const double dphi = two_pi * f[sym] / cfg_.sample_rate;
        for (std::uint32_t s = 0; s < cfg_.samples_per_symbol; ++s) {
            out.push_back(static_cast<Sample>(cfg_.amplitude * std::sin(phase)));
            phase += dphi;
            if (phase >= two_pi) phase -= two_pi;
        }
    }
    return out;
}

Bytes Mfsk::demodulate(const Samples& samples) const {
    const std::size_t sps = cfg_.samples_per_symbol;
    const std::array<double, 4> f = tones(cfg_);
    auto decode = [&](const std::vector<Sample>& norm, std::size_t start) {
        std::vector<std::uint8_t> bits;
        for (std::size_t pos = start; pos + sps <= norm.size(); pos += sps) {
            int best = 0;
            double best_p = -1.0;
            for (int t = 0; t < 4; ++t) {
                double p = goertzel_power(&norm[pos], sps, f[t], cfg_.sample_rate);
                if (p > best_p) {
                    best_p = p;
                    best = t;
                }
            }
            bits.push_back(static_cast<std::uint8_t>((best >> 1) & 1));
            bits.push_back(static_cast<std::uint8_t>(best & 1));
        }
        return bits;
    };
    return detail::search_and_pack(samples, cfg_, kSyncWord, decode);
}

}  // namespace emcast
