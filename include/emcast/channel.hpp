// emcast — transmission channels: where a modulated waveform goes.
//
//  * AWGN   : add white Gaussian noise at a target SNR (deterministic; for
//             tests and benchmarks).
//  * WAV    : persist / load a waveform as a 16-bit PCM mono WAV file.
//  * Audio  : play through the speakers / capture from the microphone (real
//             over-the-air transmission). Requires an audio device.
#ifndef EMCAST_CHANNEL_HPP
#define EMCAST_CHANNEL_HPP

#include <string>

#include "emcast/types.hpp"

namespace emcast {

/// Return a copy of `in` with additive white Gaussian noise at `snr_db`
/// (relative to the signal's mean power). `seed` makes the result repeatable.
Samples add_awgn(const Samples& in, double snr_db, std::uint32_t seed = 0x5EED);

/// Write `samples` to `path` as a mono 16-bit PCM WAV at `sample_rate`.
void write_wav(const std::string& path, const Samples& samples, std::uint32_t sample_rate);

/// Read a WAV file as mono float samples; reports the file's sample rate.
/// Multi-channel files are down-mixed to mono.
Samples read_wav(const std::string& path, std::uint32_t& sample_rate_out);

/// Play `samples` through the default output device (blocking until done).
/// Throws emcast::Error if no audio backend/device is available.
void play_audio(const Samples& samples, std::uint32_t sample_rate);

/// Capture `seconds` of mono audio from the default input device.
/// Throws emcast::Error if no audio backend/device is available.
Samples record_audio(double seconds, std::uint32_t sample_rate);

}  // namespace emcast

#endif  // EMCAST_CHANNEL_HPP
