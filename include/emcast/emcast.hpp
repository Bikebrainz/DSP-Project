// emcast — public umbrella header and high-level SDK facade.
//
// Quick start:
//   emcast::Transmitter tx;
//   emcast::Samples wave = tx.encode_file("photo.jpg");
//   emcast::write_wav("signal.wav", wave, tx.config().sample_rate);
//
//   emcast::Receiver rx;
//   std::uint32_t sr;
//   emcast::Samples in = emcast::read_wav("signal.wav", sr);
//   emcast::DecodeStatus st = rx.decode_to_file(in, "out.jpg");
#ifndef EMCAST_EMCAST_HPP
#define EMCAST_EMCAST_HPP

#include <string>

#include "emcast/analysis.hpp"
#include "emcast/channel.hpp"
#include "emcast/fec.hpp"
#include "emcast/frame.hpp"
#include "emcast/modem.hpp"
#include "emcast/types.hpp"

namespace emcast {

/// Library version string (semantic versioning).
constexpr const char* kVersion = "1.0.0";

/// Read an entire file into a byte buffer. Throws emcast::Error on failure.
Bytes read_file(const std::string& path);

/// Write a byte buffer to a file (truncating). Throws emcast::Error on failure.
void write_file(const std::string& path, const Bytes& data);

/// Encodes payloads/files into modulated waveforms.
class Transmitter {
public:
    explicit Transmitter(const ModemConfig& cfg = {}, Scheme scheme = Scheme::Bfsk)
        : modem_(make_modem(scheme, cfg)) {}
    const ModemConfig& config() const { return modem_->config(); }

    /// Encode raw bytes (with optional file-name metadata) into a waveform.
    Samples encode_bytes(const Bytes& payload, const std::string& filename = "") const {
        return modem_->modulate(encode_frame(payload, filename));
    }

    /// Read a file and encode its contents into a waveform.
    Samples encode_file(const std::string& path) const {
        return encode_bytes(read_file(path), path);
    }

private:
    std::unique_ptr<Modem> modem_;
};

/// Recovers payloads/files from modulated waveforms.
class Receiver {
public:
    explicit Receiver(const ModemConfig& cfg = {}, Scheme scheme = Scheme::Bfsk)
        : modem_(make_modem(scheme, cfg)) {}
    const ModemConfig& config() const { return modem_->config(); }

    /// Demodulate and parse a waveform into frame contents.
    DecodeStatus decode_samples(const Samples& samples, FrameInfo& out) const {
        Bytes frame = modem_->demodulate(samples);
        if (frame.empty()) return DecodeStatus::NoSync;
        return decode_frame(frame, out);
    }

    /// Decode a waveform and write the recovered payload to `out_path`.
    /// If `recovered_name` is non-null it receives the embedded file name.
    DecodeStatus decode_to_file(const Samples& samples, const std::string& out_path,
                                std::string* recovered_name = nullptr) const {
        FrameInfo info;
        DecodeStatus st = decode_samples(samples, info);
        if (st != DecodeStatus::Ok) return st;
        if (recovered_name) *recovered_name = info.filename;
        write_file(out_path, info.payload);
        return st;
    }

private:
    std::unique_ptr<Modem> modem_;
};

}  // namespace emcast

#endif  // EMCAST_EMCAST_HPP
