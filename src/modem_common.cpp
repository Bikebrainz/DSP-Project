#include <algorithm>
#include <cmath>
#include <cstring>

#include "emcast/modem.hpp"
#include "modem_detail.hpp"

namespace emcast {

const char* to_string(Scheme scheme) {
    switch (scheme) {
        case Scheme::Bfsk: return "bfsk";
        case Scheme::Ook: return "ook";
        case Scheme::Dbpsk: return "dbpsk";
        case Scheme::Dqpsk: return "dqpsk";
        case Scheme::Mfsk: return "mfsk";
    }
    return "unknown";
}

bool scheme_from_string(const std::string& name, Scheme& out) {
    if (name == "bfsk") { out = Scheme::Bfsk; return true; }
    if (name == "ook") { out = Scheme::Ook; return true; }
    if (name == "dbpsk") { out = Scheme::Dbpsk; return true; }
    if (name == "dqpsk") { out = Scheme::Dqpsk; return true; }
    if (name == "mfsk") { out = Scheme::Mfsk; return true; }
    return false;
}

std::unique_ptr<Modem> make_modem(Scheme scheme, const ModemConfig& cfg) {
    switch (scheme) {
        case Scheme::Bfsk: return std::make_unique<Bfsk>(cfg);
        case Scheme::Ook: return std::make_unique<Ook>(cfg);
        case Scheme::Dbpsk: return std::make_unique<Dbpsk>(cfg);
        case Scheme::Dqpsk: return std::make_unique<Dqpsk>(cfg);
        case Scheme::Mfsk: return std::make_unique<Mfsk>(cfg);
    }
    return std::make_unique<Bfsk>(cfg);
}

namespace detail {

std::vector<std::uint8_t> bytes_to_bits(const Bytes& bytes) {
    std::vector<std::uint8_t> bits;
    bits.reserve(bytes.size() * 8);
    for (std::uint8_t b : bytes)
        for (int i = 7; i >= 0; --i) bits.push_back((b >> i) & 1u);
    return bits;
}

std::vector<std::uint8_t> sync_bits(std::uint32_t sync_word) {
    std::vector<std::uint8_t> bits;
    bits.reserve(32);
    for (int i = 31; i >= 0; --i) bits.push_back((sync_word >> i) & 1u);
    return bits;
}

std::vector<std::uint8_t> build_bitstream(const ModemConfig& cfg, std::uint32_t sync_word,
                                          const Bytes& frame_bytes) {
    std::vector<std::uint8_t> bits;
    for (std::uint32_t i = 0; i < cfg.preamble_bits; ++i) bits.push_back(i & 1u);
    std::vector<std::uint8_t> sync = sync_bits(sync_word);
    bits.insert(bits.end(), sync.begin(), sync.end());
    std::vector<std::uint8_t> data = bytes_to_bits(frame_bytes);
    bits.insert(bits.end(), data.begin(), data.end());
    return bits;
}

std::vector<Sample> normalize(const Samples& samples, std::size_t /*sps*/) {
    double peak = 1e-9;
    for (Sample v : samples) peak = std::max(peak, static_cast<double>(std::fabs(v)));
    std::vector<Sample> norm(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i)
        norm[i] = static_cast<Sample>(samples[i] / peak);
    return norm;
}

std::size_t detect_start(const std::vector<Sample>& norm, std::size_t sps) {
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
    if (s0 >= sps) s0 -= sps;  // back off so a slightly late window can't clip
    return s0;
}

Bytes search_and_pack(const Samples& samples, const ModemConfig& cfg,
                      std::uint32_t sync_word, const BitDecoder& decode) {
    const std::size_t sps = cfg.samples_per_symbol;
    if (samples.size() < sps * 2) return {};
    std::vector<Sample> norm = normalize(samples, sps);
    const std::size_t s0 = detect_start(norm, sps);
    const std::size_t fine_step = std::max<std::size_t>(1, sps / 16);
    for (std::size_t fine = 0; fine < sps; fine += fine_step) {
        const std::size_t start = s0 + fine;
        if (start + sps > norm.size()) break;
        std::vector<std::uint8_t> bits = decode(norm, start);
        Bytes frame = pack_after_sync(bits, sync_word);
        if (!frame.empty()) return frame;
    }
    return {};
}

Bytes pack_after_sync(const std::vector<std::uint8_t>& bits, std::uint32_t sync_word) {
    std::vector<std::uint8_t> sync = sync_bits(sync_word);
    if (bits.size() < sync.size()) return {};
    for (std::size_t i = 0; i + sync.size() <= bits.size(); ++i) {
        int mism = 0;
        for (std::size_t k = 0; k < sync.size() && mism <= 2; ++k)
            if (bits[i + k] != sync[k]) ++mism;
        if (mism <= 2) {
            // Pack the bits after the marker into bytes (MSB first).
            std::size_t start = i + sync.size();
            Bytes out;
            out.reserve((bits.size() - start) / 8);
            std::size_t p = start;
            while (p + 8 <= bits.size()) {
                std::uint8_t b = 0;
                for (int k = 0; k < 8; ++k) b = static_cast<std::uint8_t>((b << 1) | bits[p + k]);
                out.push_back(b);
                p += 8;
            }
            return out;
        }
    }
    return {};
}

}  // namespace detail
}  // namespace emcast
