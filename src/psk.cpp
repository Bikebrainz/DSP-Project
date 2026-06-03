// Differential PSK schemes (DBPSK, DQPSK) on a single carrier (config.freq_mark).
//
// Differential encoding means each symbol's phase *change* (not its absolute
// phase) carries the data, so the receiver needs no carrier/phase recovery:
// it correlates each symbol against the previous one. Because the carrier sits
// on an integer number of cycles per symbol, the per-window carrier phase is
// constant and cancels in the symbol-to-symbol product.
#include "emcast/modem.hpp"

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <complex>
#include <vector>

#include "modem_detail.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace emcast {

// ---- DBPSK ------------------------------------------------------------------

Samples Dbpsk::modulate(const Bytes& frame_bytes) const {
    std::vector<std::uint8_t> bits = detail::build_bitstream(cfg_, kSyncWord, frame_bytes);
    Samples out;
    out.reserve(bits.size() * cfg_.samples_per_symbol);
    const double two_pi = 2.0 * M_PI;
    const double dphi = two_pi * cfg_.freq_mark / cfg_.sample_rate;
    double carrier = 0.0;
    double data_phase = 0.0;
    for (std::uint8_t bit : bits) {
        if (bit) data_phase += M_PI;  // a 1 flips the phase relative to the last symbol
        for (std::uint32_t s = 0; s < cfg_.samples_per_symbol; ++s) {
            out.push_back(static_cast<Sample>(cfg_.amplitude * std::sin(carrier + data_phase)));
            carrier += dphi;
            if (carrier >= two_pi) carrier -= two_pi;
        }
    }
    return out;
}

Bytes Dbpsk::demodulate(const Samples& samples) const {
    const std::size_t sps = cfg_.samples_per_symbol;
    auto decode = [&](const std::vector<Sample>& norm, std::size_t start) {
        std::vector<std::uint8_t> bits;
        std::complex<double> prev;
        bool have_prev = false;
        for (std::size_t pos = start; pos + sps <= norm.size(); pos += sps) {
            std::complex<double> z = goertzel_iq(&norm[pos], sps, cfg_.freq_mark, cfg_.sample_rate);
            if (have_prev) {
                std::complex<double> d = z * std::conj(prev);
                bits.push_back(d.real() < 0.0 ? 1u : 0u);
            }
            prev = z;
            have_prev = true;
        }
        return bits;
    };
    return detail::search_and_pack(samples, cfg_, kSyncWord, decode);
}

// ---- DQPSK ------------------------------------------------------------------

namespace {
// Gray-coded phase steps indexed by dibit value (b1<<1 | b0).
const double kQpskStep[4] = {0.0, M_PI / 2, 3 * M_PI / 2, M_PI};  // 00,01,10,11
// Inverse: quadrant index (round(angle / (pi/2))) -> dibit value.
const int kQpskInv[4] = {0, 1, 3, 2};
}  // namespace

Samples Dqpsk::modulate(const Bytes& frame_bytes) const {
    std::vector<std::uint8_t> bits = detail::build_bitstream(cfg_, kSyncWord, frame_bytes);
    if (bits.size() % 2) bits.push_back(0);  // pad to whole dibits

    Samples out;
    out.reserve((bits.size() / 2) * cfg_.samples_per_symbol);
    const double two_pi = 2.0 * M_PI;
    const double dphi = two_pi * cfg_.freq_mark / cfg_.sample_rate;
    double carrier = 0.0;
    double data_phase = 0.0;
    for (std::size_t i = 0; i + 1 < bits.size(); i += 2) {
        const int dibit = (bits[i] << 1) | bits[i + 1];
        data_phase += kQpskStep[dibit];
        if (data_phase >= two_pi) data_phase -= two_pi;
        for (std::uint32_t s = 0; s < cfg_.samples_per_symbol; ++s) {
            out.push_back(static_cast<Sample>(cfg_.amplitude * std::sin(carrier + data_phase)));
            carrier += dphi;
            if (carrier >= two_pi) carrier -= two_pi;
        }
    }
    return out;
}

Bytes Dqpsk::demodulate(const Samples& samples) const {
    const std::size_t sps = cfg_.samples_per_symbol;
    const double two_pi = 2.0 * M_PI;
    auto decode = [&](const std::vector<Sample>& norm, std::size_t start) {
        std::vector<std::uint8_t> bits;
        std::complex<double> prev;
        bool have_prev = false;
        for (std::size_t pos = start; pos + sps <= norm.size(); pos += sps) {
            std::complex<double> z = goertzel_iq(&norm[pos], sps, cfg_.freq_mark, cfg_.sample_rate);
            if (have_prev) {
                std::complex<double> d = z * std::conj(prev);
                double ang = std::atan2(d.imag(), d.real());
                if (ang < 0) ang += two_pi;
                int q = static_cast<int>(std::lround(ang / (M_PI / 2))) % 4;
                int dibit = kQpskInv[q];
                bits.push_back(static_cast<std::uint8_t>((dibit >> 1) & 1));
                bits.push_back(static_cast<std::uint8_t>(dibit & 1));
            }
            prev = z;
            have_prev = true;
        }
        return bits;
    };
    return detail::search_and_pack(samples, cfg_, kSyncWord, decode);
}

}  // namespace emcast
