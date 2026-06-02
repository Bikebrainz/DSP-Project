#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

#include "emcast/channel.hpp"
#include "miniaudio.h"

namespace emcast {
namespace {

struct PlaybackState {
    const Samples* data = nullptr;
    std::atomic<std::size_t> pos{0};
};

void playback_callback(ma_device* device, void* output, const void* /*input*/,
                       ma_uint32 frame_count) {
    auto* state = static_cast<PlaybackState*>(device->pUserData);
    auto* out = static_cast<float*>(output);
    const Samples& data = *state->data;
    std::size_t pos = state->pos.load();
    for (ma_uint32 i = 0; i < frame_count; ++i) {
        out[i] = (pos < data.size()) ? data[pos++] : 0.0f;
    }
    state->pos.store(pos);
}

struct CaptureState {
    Samples* data = nullptr;
    std::size_t max_frames = 0;
};

void capture_callback(ma_device* device, void* /*output*/, const void* input,
                      ma_uint32 frame_count) {
    auto* state = static_cast<CaptureState*>(device->pUserData);
    const auto* in = static_cast<const float*>(input);
    Samples& data = *state->data;
    for (ma_uint32 i = 0; i < frame_count && data.size() < state->max_frames; ++i)
        data.push_back(in[i]);
}

}  // namespace

void play_audio(const Samples& samples, std::uint32_t sample_rate) {
    PlaybackState state;
    state.data = &samples;

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format = ma_format_f32;
    config.playback.channels = 1;
    config.sampleRate = sample_rate;
    config.dataCallback = playback_callback;
    config.pUserData = &state;

    ma_device device;
    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
        throw Error("failed to open audio output device");
    if (ma_device_start(&device) != MA_SUCCESS) {
        ma_device_uninit(&device);
        throw Error("failed to start audio output device");
    }
    while (state.pos.load() < samples.size())
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    // Brief tail so the final buffer drains before we stop.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    ma_device_uninit(&device);
}

Samples record_audio(double seconds, std::uint32_t sample_rate) {
    Samples data;
    CaptureState state;
    state.data = &data;
    state.max_frames = static_cast<std::size_t>(seconds * sample_rate);
    data.reserve(state.max_frames);

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate = sample_rate;
    config.dataCallback = capture_callback;
    config.pUserData = &state;

    ma_device device;
    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS)
        throw Error("failed to open audio input device");
    if (ma_device_start(&device) != MA_SUCCESS) {
        ma_device_uninit(&device);
        throw Error("failed to start audio input device");
    }
    while (data.size() < state.max_frames)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ma_device_uninit(&device);
    return data;
}

}  // namespace emcast
