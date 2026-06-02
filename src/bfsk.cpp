#include "emcast/modem.hpp"

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace emcast {
namespace {

// Expand a byte stream to a bit vector, MSB first.
std::vector<std::uint8_t> bytes_to_bits(const Bytes& bytes) {
    std::vector<std::uint8_t> bits;
    bits.reserve(bytes.size() * 8);
    for (std::uint8_t b : bytes)
        for (int i = 7; i >= 0; --i) bits.push_back((b >> i) & 1u);
    return bits;
}

// Pack a bit vector (MSB first) back into bytes, dropping a trailing partial.
Bytes bits_to_bytes(const std::vector<std::uint8_t>& bits, std::size_t start) {
    Bytes out;
    out.reserve((bits.size() - start) / 8);
    std::size_t i = start;
    while (i + 8 <= bits.size()) {
        std::uint8_t b = 0;
        for (int k = 0; k < 8; ++k) b = static_cast<std::uint8_t>((b << 1) | bits[i + k]);
        out.push_back(b);
        i += 8;
    }
    return out;
}

}  // namespace

Samples Bfsk::modulate(const Bytes& frame_bytes) const {
    // Bit stream: preamble (alternating) + sync marker + frame bytes.
    std::vector<std::uint8_t> bits;
    for (std::uint32_t i = 0; i < cfg_.preamble_bits; ++i) bits.push_back(i & 1u);
    for (int i = 31; i >= 0; --i) bits.push_back((kSyncWord >> i) & 1u);
    const std::vector<std::uint8_t> data_bits = bytes_to_bits(frame_bytes);
    bits.insert(bits.end(), data_bits.begin(), data_bits.end());

    Samples out;
    out.reserve(bits.size() * cfg_.samples_per_symbol);
    double phase = 0.0;
    const double two_pi = 2.0 * M_PI;
    for (std::uint8_t bit : bits) {
        const double freq = bit ? cfg_.freq_mark : cfg_.freq_space;
        const double dphi = two_pi * freq / cfg_.sample_rate;
        for (std::uint32_t s = 0; s < cfg_.samples_per_symbol; ++s) {
            out.push_back(static_cast<Sample>(cfg_.amplitude * std::sin(phase)));
            phase += dphi;
            if (phase >= two_pi) phase -= two_pi;
        }
    }
    return out;
}

Bytes Bfsk::demodulate(const Samples& samples) const {
    const std::size_t sps = cfg_.samples_per_symbol;
    if (samples.size() < sps * 2) return {};

    // Normalize amplitude (defensive; the tone decision is amplitude-agnostic).
    double peak = 1e-9;
    for (Sample v : samples) peak = std::max(peak, static_cast<double>(std::fabs(v)));
    std::vector<Sample> norm(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i)
        norm[i] = static_cast<Sample>(samples[i] / peak);

    // Detect signal start: first sps-window whose energy exceeds a fraction of
    // the strongest window. Handles leading silence on a live recording.
    double peak_energy = 0.0;
    for (std::size_t i = 0; i + sps <= norm.size(); i += sps) {
        double e = 0.0;
        for (std::size_t k = 0; k < sps; ++k) e += static_cast<double>(norm[i + k]) * norm[i + k];
        peak_energy = std::max(peak_energy, e);
    }
    const double thresh = 0.20 * peak_energy;
    std::size_t s0 = 0;
    for (std::size_t i = 0; i + sps <= norm.size(); i += sps) {
        double e = 0.0;
        for (std::size_t k = 0; k < sps; ++k) e += static_cast<double>(norm[i + k]) * norm[i + k];
        if (e >= thresh) {
            s0 = i;
            break;
        }
    }
    // Back off one symbol so a slightly early window doesn't clip the preamble.
    if (s0 >= sps) s0 -= sps;

    // Build the sync bit pattern once.
    std::vector<std::uint8_t> sync_bits;
    sync_bits.reserve(32);
    for (int i = 31; i >= 0; --i) sync_bits.push_back((kSyncWord >> i) & 1u);

    const std::size_t fine_step = std::max<std::size_t>(1, sps / 16);
    for (std::size_t fine = 0; fine < sps; fine += fine_step) {
        const std::size_t start = s0 + fine;
        if (start + sps > norm.size()) break;

        // Demodulate every symbol from `start` into bits.
        std::vector<std::uint8_t> bits;
        bits.reserve((norm.size() - start) / sps);
        for (std::size_t pos = start; pos + sps <= norm.size(); pos += sps) {
            const double p_space = goertzel_power(&norm[pos], sps, cfg_.freq_space, cfg_.sample_rate);
            const double p_mark = goertzel_power(&norm[pos], sps, cfg_.freq_mark, cfg_.sample_rate);
            bits.push_back(p_mark > p_space ? 1u : 0u);
        }

        // Search for the sync marker (tolerate up to 2 bit errors).
        if (bits.size() < sync_bits.size()) continue;
        for (std::size_t i = 0; i + sync_bits.size() <= bits.size(); ++i) {
            int mism = 0;
            for (std::size_t k = 0; k < sync_bits.size() && mism <= 2; ++k)
                if (bits[i + k] != sync_bits[k]) ++mism;
            if (mism <= 2) {
                return bits_to_bytes(bits, i + sync_bits.size());
            }
        }
    }
    return {};
}

}  // namespace emcast
