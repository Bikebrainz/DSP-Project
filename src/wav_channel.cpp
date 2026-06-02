#include <algorithm>
#include <cmath>

#include "dr_wav.h"
#include "emcast/channel.hpp"

namespace emcast {

void write_wav(const std::string& path, const Samples& samples, std::uint32_t sample_rate) {
    drwav_data_format format{};
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM;
    format.channels = 1;
    format.sampleRate = sample_rate;
    format.bitsPerSample = 16;

    drwav wav;
    if (!drwav_init_file_write(&wav, path.c_str(), &format, nullptr))
        throw Error("could not open WAV for writing: " + path);

    std::vector<std::int16_t> pcm(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        double v = std::max(-1.0, std::min(1.0, static_cast<double>(samples[i])));
        pcm[i] = static_cast<std::int16_t>(std::lround(v * 32767.0));
    }
    drwav_write_pcm_frames(&wav, pcm.size(), pcm.data());
    drwav_uninit(&wav);
}

Samples read_wav(const std::string& path, std::uint32_t& sample_rate_out) {
    unsigned int channels = 0;
    unsigned int sample_rate = 0;
    drwav_uint64 total_frames = 0;
    float* data = drwav_open_file_and_read_pcm_frames_f32(
        path.c_str(), &channels, &sample_rate, &total_frames, nullptr);
    if (!data) throw Error("could not open WAV for reading: " + path);

    sample_rate_out = sample_rate;
    Samples out(static_cast<std::size_t>(total_frames));
    if (channels <= 1) {
        for (std::size_t i = 0; i < out.size(); ++i) out[i] = data[i];
    } else {
        // Down-mix to mono.
        for (std::size_t i = 0; i < out.size(); ++i) {
            double acc = 0.0;
            for (unsigned int c = 0; c < channels; ++c) acc += data[i * channels + c];
            out[i] = static_cast<Sample>(acc / channels);
        }
    }
    drwav_free(data, nullptr);
    return out;
}

}  // namespace emcast
