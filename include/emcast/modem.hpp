// emcast — physical-layer modems (pluggable modulation schemes).
#ifndef EMCAST_MODEM_HPP
#define EMCAST_MODEM_HPP

#include <memory>
#include <string>

#include "emcast/types.hpp"

namespace emcast {

/// Single-bin Goertzel power: the energy of `samples` at frequency `freq`.
/// Cheaper than a full FFT when only a few tones are of interest.
double goertzel_power(const Sample* samples, std::size_t n, double freq,
                      double sample_rate);

/// Available modulation schemes.
enum class Scheme {
    Bfsk,  ///< Binary frequency-shift keying (robust; the default).
    Ook,   ///< On-off keying (one tone gated on/off; simpler, less robust).
};

const char* to_string(Scheme scheme);
/// Parse a scheme name ("bfsk", "ook"); returns false if unrecognized.
bool scheme_from_string(const std::string& name, Scheme& out);

/// Abstract modem: turns frame bytes into a waveform and back.
///
/// All schemes share the same bit-level framing: an alternating preamble (for
/// gain/timing), a 32-bit CCSDS sync marker, then the frame bytes (MSB first).
class Modem {
public:
    virtual ~Modem() = default;

    /// Modulate frame bytes into an audio waveform (preamble + sync + data).
    virtual Samples modulate(const Bytes& frame_bytes) const = 0;

    /// Recover frame bytes from a waveform. Empty if no sync marker is found.
    virtual Bytes demodulate(const Samples& samples) const = 0;

    const ModemConfig& config() const { return cfg_; }

    /// The 32-bit sync marker (CCSDS attached sync marker, 0x1ACFFC1D).
    static constexpr std::uint32_t kSyncWord = 0x1ACFFC1Du;

protected:
    explicit Modem(const ModemConfig& cfg) : cfg_(cfg) {}
    ModemConfig cfg_;
};

/// Construct a modem for the given scheme.
std::unique_ptr<Modem> make_modem(Scheme scheme, const ModemConfig& cfg = {});

/// Binary frequency-shift keying: a 1 is the "mark" tone, a 0 the "space" tone,
/// sent with continuous phase. Decision compares the two tone energies, so it
/// is insensitive to absolute level (good for acoustic links).
class Bfsk final : public Modem {
public:
    explicit Bfsk(const ModemConfig& cfg = {}) : Modem(cfg) {}
    Samples modulate(const Bytes& frame_bytes) const override;
    Bytes demodulate(const Samples& samples) const override;
};

/// On-off keying: the "mark" tone is gated on for a 1 and off for a 0. The
/// receiver thresholds per-symbol tone energy adaptively. Simpler than BFSK but
/// sensitive to amplitude, so it degrades sooner in noise.
class Ook final : public Modem {
public:
    explicit Ook(const ModemConfig& cfg = {}) : Modem(cfg) {}
    Samples modulate(const Bytes& frame_bytes) const override;
    Bytes demodulate(const Samples& samples) const override;
};

}  // namespace emcast

#endif  // EMCAST_MODEM_HPP
