#include "emcast/analysis.hpp"

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <algorithm>
#include <cmath>

#include "emcast/channel.hpp"
#include "stb_image_write.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace emcast {
namespace {

// Simple perceptual colormap: dark blue -> cyan -> yellow -> red as t: 0..1.
void colormap(float t, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) {
    t = std::min(1.0f, std::max(0.0f, t));
    // Four-stop gradient.
    const float stops[5][3] = {{0, 0, 0.2f}, {0, 0.4f, 0.8f}, {0, 0.9f, 0.7f},
                               {0.95f, 0.95f, 0.1f}, {0.9f, 0.1f, 0.05f}};
    float x = t * 4.0f;
    int i = std::min(3, static_cast<int>(x));
    float f = x - i;
    auto lerp = [&](int c) { return stops[i][c] + f * (stops[i + 1][c] - stops[i][c]); };
    r = static_cast<std::uint8_t>(lerp(0) * 255);
    g = static_cast<std::uint8_t>(lerp(1) * 255);
    b = static_cast<std::uint8_t>(lerp(2) * 255);
}

}  // namespace

Spectrogram compute_spectrogram(const Samples& samples, std::uint32_t sample_rate,
                                std::size_t fft_size, std::size_t hop) {
    if (hop == 0) hop = fft_size / 2;
    Spectrogram spec;
    spec.sample_rate = sample_rate;
    spec.fft_size = fft_size;
    spec.bins = fft_size / 2;
    if (samples.size() < fft_size || fft_size == 0) return spec;
    spec.frames = 1 + (samples.size() - fft_size) / hop;

    // Precompute Hann window and the DFT twiddle factors.
    std::vector<double> win(fft_size);
    for (std::size_t n = 0; n < fft_size; ++n)
        win[n] = 0.5 * (1.0 - std::cos(2.0 * M_PI * n / (fft_size - 1)));

    std::vector<double> cosw(fft_size * spec.bins), sinw(fft_size * spec.bins);
    for (std::size_t k = 0; k < spec.bins; ++k) {
        for (std::size_t n = 0; n < fft_size; ++n) {
            double a = 2.0 * M_PI * k * n / fft_size;
            cosw[k * fft_size + n] = std::cos(a);
            sinw[k * fft_size + n] = std::sin(a);
        }
    }

    spec.mag.resize(spec.frames * spec.bins);
    for (std::size_t f = 0; f < spec.frames; ++f) {
        const std::size_t start = f * hop;
        for (std::size_t k = 0; k < spec.bins; ++k) {
            double re = 0.0, im = 0.0;
            const double* ck = &cosw[k * fft_size];
            const double* sk = &sinw[k * fft_size];
            for (std::size_t n = 0; n < fft_size; ++n) {
                const double x = static_cast<double>(samples[start + n]) * win[n];
                re += x * ck[n];
                im -= x * sk[n];
            }
            spec.mag[f * spec.bins + k] = static_cast<float>(std::sqrt(re * re + im * im));
        }
    }
    return spec;
}

std::vector<std::uint8_t> spectrogram_to_rgb(const Spectrogram& spec, std::size_t& width,
                                             std::size_t& height) {
    width = spec.frames;
    height = spec.bins;
    std::vector<std::uint8_t> img(width * height * 3, 0);
    if (spec.mag.empty()) return img;

    // Normalize on a dB scale for visibility.
    float peak = 1e-12f;
    for (float m : spec.mag) peak = std::max(peak, m);
    const float floor_db = -60.0f;
    for (std::size_t f = 0; f < spec.frames; ++f) {
        for (std::size_t k = 0; k < spec.bins; ++k) {
            float m = spec.mag[f * spec.bins + k] / peak;
            float db = 20.0f * std::log10(std::max(m, 1e-6f));
            float t = (db - floor_db) / (0.0f - floor_db);  // map [floor,0] -> [0,1]
            std::uint8_t r, g, b;
            colormap(t, r, g, b);
            // Low frequencies at the bottom of the image.
            std::size_t y = height - 1 - k;
            std::size_t idx = (y * width + f) * 3;
            img[idx] = r;
            img[idx + 1] = g;
            img[idx + 2] = b;
        }
    }
    return img;
}

void write_spectrogram_png(const std::string& wav_path, const std::string& png_path,
                           std::size_t fft_size) {
    std::uint32_t sr = 0;
    Samples samples = read_wav(wav_path, sr);
    Spectrogram spec = compute_spectrogram(samples, sr, fft_size);
    std::size_t w = 0, h = 0;
    std::vector<std::uint8_t> img = spectrogram_to_rgb(spec, w, h);
    if (w == 0 || h == 0) throw Error("signal too short for a spectrogram: " + wav_path);
    if (!stbi_write_png(png_path.c_str(), static_cast<int>(w), static_cast<int>(h), 3, img.data(),
                        static_cast<int>(w * 3)))
        throw Error("could not write PNG: " + png_path);
}

}  // namespace emcast
