// emcast — signal analysis (spectrogram) for visualization and inspection.
#ifndef EMCAST_ANALYSIS_HPP
#define EMCAST_ANALYSIS_HPP

#include "emcast/types.hpp"

namespace emcast {

/// A short-time Fourier magnitude spectrogram.
/// `mag` is row-major: mag[frame * bins + bin], linear magnitude.
struct Spectrogram {
    std::size_t frames = 0;        ///< Number of time steps.
    std::size_t bins = 0;          ///< Frequency bins (fft_size / 2).
    std::uint32_t sample_rate = 0;
    std::size_t fft_size = 0;
    std::vector<float> mag;

    /// Frequency resolution per bin, in Hz.
    double bin_hz() const { return fft_size ? static_cast<double>(sample_rate) / fft_size : 0.0; }
};

/// Compute a Hann-windowed magnitude spectrogram. `hop` of 0 means fft_size/2.
Spectrogram compute_spectrogram(const Samples& samples, std::uint32_t sample_rate,
                                std::size_t fft_size = 512, std::size_t hop = 0);

/// Render a spectrogram to an RGB8 image (time → x, frequency → y with low
/// frequencies at the bottom). Sets `width`/`height`; returns width*height*3 bytes.
std::vector<std::uint8_t> spectrogram_to_rgb(const Spectrogram& spec, std::size_t& width,
                                             std::size_t& height);

/// Write a spectrogram of a WAV file to a PNG. Throws emcast::Error on failure.
void write_spectrogram_png(const std::string& wav_path, const std::string& png_path,
                           std::size_t fft_size = 512);

}  // namespace emcast

#endif  // EMCAST_ANALYSIS_HPP
