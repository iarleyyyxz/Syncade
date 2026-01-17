#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <vector>

class AudioSystem {
public:
    AudioSystem() = default;
    ~AudioSystem() { shutdown(); }

    // inicializa dispositivo de saída; return true se ok
    bool init(int sampleRate);

    // push frames interleaved int16 stereo (sempre esperado stereo do core)
    void push(const int16_t* data, size_t frames, int core_sample_rate = 0);

    // fecha dispositivo e limpa estado
    void shutdown();

    int sample_rate() const { return sample_rate_; }

    // configura ganho (1.0 = unity)
    void set_gain(float g) { gain_ = g; }

private:
    // audio callback (pull)
    static void audio_callback(void* userdata, Uint8* stream, int len);

    // resample de melhor qualidade (Catmull-Rom / cubic) de int16 stereo
    std::vector<int16_t> resample_cubic(const int16_t* in, size_t in_frames, int in_rate, int out_rate);

    // ring buffer helpers
    bool ring_write(const int16_t* src, size_t samples); // samples = interleaved samples count (frames*channels)
    size_t ring_read(int16_t* dst, size_t samples);

    SDL_AudioDeviceID dev = 0;
    int sample_rate_ = 0;
    SDL_AudioFormat device_format_ = 0;
    int device_channels_ = 0;

    // ring buffer (interleaved samples: int16)
    std::vector<int16_t> ring_;
    size_t ring_head_ = 0; // write pos
    size_t ring_tail_ = 0; // read pos
    size_t ring_capacity_ = 0;

    float gain_ = 1.0f;
};