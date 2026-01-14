#pragma once
#include <SDL2/SDL.h>
#include <cstdint>

class AudioSystem {
public:
    bool init(int sampleRate);
    void push(const int16_t* data, size_t frames);
    void shutdown();
    int sample_rate() const { return sample_rate_; }

private:
    SDL_AudioDeviceID dev = 0;
    int sample_rate_ = 0;

    // formato e canais reportados pelo dispositivo aberto
    SDL_AudioFormat device_format_ = 0;
    int device_channels_ = 0;
};