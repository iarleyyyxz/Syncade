#include "AudioSystem.h"
#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>

static inline float s16_to_f32_norm(int16_t v) {
    return static_cast<float>(v) / 32768.0f;
}
static inline int16_t f32_to_s16_clamp(float v) {
    v *= 32767.0f;
    if (v > 32767.0f) return 32767;
    if (v < -32768.0f) return -32768;
    return static_cast<int16_t>(v);
}

// ---- ring helpers ----
bool AudioSystem::ring_write(const int16_t* src, size_t samples) {
    if (samples == 0) return true;
    // ensure capacity (single-writer, callback-reader); protect with SDL_LockAudioDevice when used
    size_t free_space = (ring_tail_ + ring_capacity_ - ring_head_ - 1) % ring_capacity_;
    if (samples > free_space) {
        // not enough space; drop oldest (advance tail) to make room (keep newest data)
        size_t need = samples - free_space;
        size_t drop_frames = (need + 1) / 2; // samples are interleaved, but ring stores samples not frames; drop samples directly
        ring_tail_ = (ring_tail_ + drop_frames) % ring_capacity_;
    }

    size_t first = std::min(samples, ring_capacity_ - ring_head_);
    std::copy(src, src + first, ring_.begin() + ring_head_);
    if (samples > first) {
        std::copy(src + first, src + samples, ring_.begin());
    }
    ring_head_ = (ring_head_ + samples) % ring_capacity_;
    return true;
}

size_t AudioSystem::ring_read(int16_t* dst, size_t samples) {
    size_t available = (ring_head_ + ring_capacity_ - ring_tail_) % ring_capacity_;
    size_t to_read = std::min(samples, available);
    size_t first = std::min(to_read, ring_capacity_ - ring_tail_);
    std::copy(ring_.begin() + ring_tail_, ring_.begin() + ring_tail_ + first, dst);
    if (to_read > first) {
        std::copy(ring_.begin(), ring_.begin() + (to_read - first), dst + first);
    }
    ring_tail_ = (ring_tail_ + to_read) % ring_capacity_;
    return to_read;
}

// ---- resampler linear ----
std::vector<int16_t> AudioSystem::resample_linear(const int16_t* in, size_t in_frames, int in_rate, int out_rate) {
    if (in_rate == out_rate || in_frames == 0) {
        return std::vector<int16_t>(in, in + (in_frames * 2));
    }
    double ratio = static_cast<double>(out_rate) / static_cast<double>(in_rate);
    size_t out_frames = static_cast<size_t>(std::ceil(in_frames * ratio));
    std::vector<int16_t> out(out_frames * 2);
    for (size_t i = 0; i < out_frames; ++i) {
        double src_pos = static_cast<double>(i) / ratio;
        size_t idx = static_cast<size_t>(std::floor(src_pos));
        double frac = src_pos - idx;
        // clamp indices
        size_t idx1 = std::min(idx, in_frames - 1);
        size_t idx2 = std::min(idx + 1, in_frames - 1);
        int16_t l1 = in[2 * idx1 + 0];
        int16_t r1 = in[2 * idx1 + 1];
        int16_t l2 = in[2 * idx2 + 0];
        int16_t r2 = in[2 * idx2 + 1];
        float lf = static_cast<float>(l1) * (1.0f - static_cast<float>(frac)) + static_cast<float>(l2) * static_cast<float>(frac);
        float rf = static_cast<float>(r1) * (1.0f - static_cast<float>(frac)) + static_cast<float>(r2) * static_cast<float>(frac);
        out[2 * i + 0] = static_cast<int16_t>(lf);
        out[2 * i + 1] = static_cast<int16_t>(rf);
    }
    return out;
}

// ---- audio callback ----
void AudioSystem::audio_callback(void* userdata, Uint8* stream, int len) {
    AudioSystem* self = reinterpret_cast<AudioSystem*>(userdata);
    if (!self) {
        SDL_memset(stream, 0, len);
        return;
    }

    // expecting device channels; we store int16 interleaved in ring
    int16_t* out = reinterpret_cast<int16_t*>(stream);
    size_t samples_needed = static_cast<size_t>(len) / sizeof(int16_t);

    // read available
    size_t read = self->ring_read(out, samples_needed);
    if (read < samples_needed) {
        // zero the rest (silence)
        size_t bytes = (samples_needed - read) * sizeof(int16_t);
        std::memset(out + read, 0, bytes);
    }

    // apply gain in-place (simple)
    if (self->gain_ != 1.0f && self->gain_ > 0.0f) {
        for (size_t i = 0; i < samples_needed; ++i) {
            float f = s16_to_f32_norm(out[i]) * self->gain_;
            out[i] = f32_to_s16_clamp(f);
        }
    }
}

// ---- public API ----
bool AudioSystem::init(int rate) {
    // already opened with same rate?
    if (dev && sample_rate_ == rate) return true;

    shutdown(); // ensure closed

    SDL_AudioSpec want{};
    SDL_AudioSpec have{};
    want.freq = rate;
    want.format = AUDIO_S16SYS; // we'll keep ring as int16
    want.channels = 2;
    // choose small buffer for lower latency; increase if underruns occur
    want.samples = 512;
    want.callback = AudioSystem::audio_callback;
    want.userdata = this;

    dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (dev == 0) {
        std::cerr << "[audio] SDL_OpenAudioDevice failed: " << SDL_GetError() << "\n";
        return false;
    }

    sample_rate_ = have.freq;
    device_format_ = have.format;
    device_channels_ = have.channels;

    // allocate ring buffer: 2 seconds of audio (frames * channels)
    const double seconds = 2.0;
    ring_capacity_ = static_cast<size_t>(std::max(512u, static_cast<unsigned>(sample_rate_ * device_channels_ * seconds)));
    ring_.assign(ring_capacity_, 0);
    ring_head_ = ring_tail_ = 0;

    std::cerr << "[audio] opened device dev=" << dev << " rate=" << sample_rate_
        << " fmt=0x" << std::hex << device_format_ << std::dec
        << " chans=" << device_channels_ << " ring_capacity(samples)=" << ring_capacity_ << "\n";

    SDL_PauseAudioDevice(dev, 0); // start callback
    return true;
}

void AudioSystem::push(const int16_t* data, size_t frames, int core_sample_rate) {
    if (!dev || frames == 0) return;

    // prepare buffer in device sample rate
    std::vector<int16_t> out;
    if (core_sample_rate > 0 && core_sample_rate != sample_rate_) {
        out = resample_linear(data, frames, core_sample_rate, sample_rate_);
    }
    else if (sample_rate_ != 0 && core_sample_rate == 0) {
        // no core rate provided — assume same (no resample)
        out.assign(data, data + frames * 2);
    }
    else {
        out.assign(data, data + frames * 2);
    }

    // write to ring with lock
    SDL_LockAudioDevice(dev);
    ring_write(out.data(), out.size());
    SDL_UnlockAudioDevice(dev);
}

void AudioSystem::shutdown() {
    if (!dev) return;
    SDL_PauseAudioDevice(dev, 1);
    SDL_CloseAudioDevice(dev);
    dev = 0;
    sample_rate_ = 0;
    device_format_ = 0;
    device_channels_ = 0;
    ring_.clear();
    ring_capacity_ = ring_head_ = ring_tail_ = 0;
}