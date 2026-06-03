// emcast — shared primitive types and configuration.
//
// emcast is a data-over-signal transmission toolkit: it encodes an arbitrary
// file into a modulated audio waveform (with framing, CRC integrity checks and
// Reed-Solomon forward error correction), transmits it over a pluggable channel
// (WAV file, simulated noisy channel, or a real speaker->microphone link), and
// decodes it back to a bit-exact copy of the original file.
#ifndef EMCAST_TYPES_HPP
#define EMCAST_TYPES_HPP

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace emcast {

/// Raw bytes (file contents, frame bytes, codewords).
using Bytes = std::vector<std::uint8_t>;

/// A real-valued audio sample, nominally in the range [-1.0, 1.0].
using Sample = float;

/// A block of audio samples (mono, time-domain).
using Samples = std::vector<Sample>;

/// Physical-layer parameters shared by the modulator and demodulator.
/// Both ends MUST agree on these values for a transfer to succeed.
struct ModemConfig {
    std::uint32_t sample_rate = 48000;  ///< Samples per second.
    std::uint32_t samples_per_symbol = 48;  ///< Determines the baud rate.
    double freq_space = 2000.0;  ///< Tone for a binary 0, in Hz (BFSK).
    double freq_mark = 3000.0;   ///< Tone for a binary 1, in Hz (BFSK).
    std::uint32_t preamble_bits = 64;  ///< Alternating bits for AGC/timing.
    double amplitude = 0.7;  ///< Peak output amplitude in [0, 1].

    /// Baud rate (symbols, i.e. bits for BFSK, per second).
    double baud() const {
        return static_cast<double>(sample_rate) / samples_per_symbol;
    }

    /// Balanced default: 1000 baud at 48 kHz, tones at 2000/3000 Hz.
    static ModemConfig balanced() { return ModemConfig{}; }

    /// Noise-robust: 500 baud, longer preamble. Best for live/over-the-air.
    static ModemConfig robust() {
        ModemConfig c;
        c.samples_per_symbol = 96;  // 500 baud
        c.freq_space = 2000.0;      // bin 4 at 500 Hz resolution
        c.freq_mark = 3000.0;       // bin 6
        c.preamble_bits = 128;
        return c;
    }

    /// High-throughput: 2000 baud, tones at 4000/6000 Hz. Needs a cleaner link.
    static ModemConfig fast() {
        ModemConfig c;
        c.samples_per_symbol = 24;  // 2000 baud
        c.freq_space = 4000.0;      // bin 2 at 2000 Hz resolution
        c.freq_mark = 6000.0;       // bin 3
        c.preamble_bits = 48;
        return c;
    }
};

/// Select a ModemConfig preset by name ("balanced", "robust", "fast").
/// Returns false if the name is unrecognized.
bool profile_from_string(const std::string& name, ModemConfig& out);

/// Reason a decode attempt failed, surfaced through DecodeResult.
enum class DecodeStatus {
    Ok,              ///< Frame recovered and payload CRC verified.
    NoSync,          ///< Sync marker never found in the sample stream.
    BadHeader,       ///< Header magic/CRC invalid after FEC.
    TooManyErrors,   ///< Reed-Solomon could not correct a block.
    PayloadCrcFail,  ///< Frame decoded but payload CRC mismatched.
    Truncated,       ///< Stream ended before the full frame was present.
};

/// Human-readable name for a DecodeStatus (for logs / CLI output).
const char* to_string(DecodeStatus status);

/// Exception thrown for unrecoverable, programmer-facing errors
/// (bad arguments, I/O failures). Decode *quality* failures are reported
/// via DecodeStatus rather than thrown.
class Error : public std::runtime_error {
public:
    explicit Error(const std::string& what) : std::runtime_error(what) {}
};

}  // namespace emcast

#endif  // EMCAST_TYPES_HPP
