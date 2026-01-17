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
        ring_tail_ = (ring_tail_ + need) % ring_capacity_;
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

// ---- resampler cubic (Catmull-Rom) ----
// Produz saída interleaved int16 stereo.
// Formula Catmull-Rom (centripetal/catrom) simplificada com tensão 0.5
std::vector<int16_t> AudioSystem::resample_cubic(const int16_t* in, size_t in_frames, int in_rate, int out_rate) {
    if (in_frames == 0) return {};
    if (in_rate == out_rate) return std::vector<int16_t>(in, in + (in_frames * 2));

    const double scale = static_cast<double>(in_rate) / static_cast<double>(out_rate);
    size_t out_frames = static_cast<size_t>(std::ceil(in_frames / scale));

    std::vector<int16_t> out(out_frames * 2);

    auto sample = [&](size_t frame, int ch) -> float {
        // clamp frame index to available frames
        if (frame >= in_frames) frame = in_frames - 1;
        return static_cast<float>(in[2 * frame + ch]);
        };

    for (size_t i = 0; i < out_frames; ++i) {
        double src_pos = static_cast<double>(i) * scale;
        size_t idx = (src_pos >= 1.0) ? static_cast<size_t>(std::floor(src_pos)) : 0;
        double frac = src_pos - static_cast<double>(idx);

        // indices p0..p3
        size_t i0 = (idx == 0) ? 0 : idx - 1;
        size_t i1 = idx;
        size_t i2 = std::min(idx + 1, in_frames - 1);
        size_t i3 = std::min(idx + 2, in_frames - 1);

        // for each channel
        for (int ch = 0; ch < 2; ++ch) {
            float p0 = sample(i0, ch);
            float p1 = sample(i1, ch);
            float p2 = sample(i2, ch);
            float p3 = sample(i3, ch);

            // Catmull-Rom spline interpolation
            // t = frac
            float t = static_cast<float>(frac);
            float t2 = t * t;
            float t3 = t2 * t;

            // Catmull-Rom with tension 0.5:
            float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
            float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
            float c = -0.5f * p0 + 0.5f * p2;
            float d = p1;

            float val = a * t3 + b * t2 + c * t + d;

            // clamp and store
            // val currently in int16 range because pN are int16 values; cast safely
            int32_t iv = static_cast<int32_t>(std::lrintf(val));
            if (iv > 32767) iv = 32767;
            if (iv < -32768) iv = -32768;
            out[2 * i + ch] = static_cast<int16_t>(iv);
        }
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

    int16_t* out = reinterpret_cast<int16_t*>(stream);
    size_t samples_needed = static_cast<size_t>(len) / sizeof(int16_t);

    size_t read = self->ring_read(out, samples_needed);
    if (read < samples_needed) {
        size_t bytes = (samples_needed - read) * sizeof(int16_t);
        std::memset(out + read, 0, bytes);
    }

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

    std::vector<int16_t> out;
    if (core_sample_rate > 0 && core_sample_rate != sample_rate_) {
        out = resample_cubic(data, frames, core_sample_rate, sample_rate_);
    }
    else if (sample_rate_ != 0 && core_sample_rate == 0) {
        out.assign(data, data + frames * 2);
    }
    else {
        out.assign(data, data + frames * 2);
    }

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