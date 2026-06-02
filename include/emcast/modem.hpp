// emcast — physical-layer modem (binary FSK).
#ifndef EMCAST_MODEM_HPP
#define EMCAST_MODEM_HPP

#include "emcast/types.hpp"

namespace emcast {

/// Single-bin Goertzel power: the energy of `samples` at frequency `freq`.
/// Cheaper than a full FFT when only a few tones are of interest.
double goertzel_power(const Sample* samples, std::size_t n, double freq,
                      double sample_rate);

/// Binary frequency-shift-keying modem.
///
/// Transmit framing (bit level): an alternating preamble (for AGC and timing
/// recovery), then a 32-bit CCSDS sync marker, then the frame bytes (MSB
/// first). A binary 1 is sent as the "mark" tone, a 0 as the "space" tone,
/// with continuous phase across symbols to avoid spectral splatter.
class Bfsk {
public:
    explicit Bfsk(const ModemConfig& cfg = {}) : cfg_(cfg) {}

    const ModemConfig& config() const { return cfg_; }

    /// Modulate frame bytes into an audio waveform (preamble + sync + data).
    Samples modulate(const Bytes& frame_bytes) const;

    /// Recover frame bytes from a waveform: detect signal start, search symbol
    /// timing offsets, locate the sync marker, and slice symbols into bytes.
    /// Returns an empty vector if no sync marker is found.
    Bytes demodulate(const Samples& samples) const;

    /// The 32-bit sync marker (CCSDS attached sync marker, 0x1ACFFC1D).
    static constexpr std::uint32_t kSyncWord = 0x1ACFFC1Du;

private:
    ModemConfig cfg_;
};

}  // namespace emcast

#endif  // EMCAST_MODEM_HPP
