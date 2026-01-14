#include "AudioSystem.h"
#include <iostream>
#include <cmath>
#include <vector>
#include <stdio.h>

static inline float s16_to_f32(int16_t v) {
    // converte int16 [-32768..32767] -> float [-1.0..~1.0]
    return static_cast<float>(v) / 32768.0f;
}

bool AudioSystem::init(int rate) {
    // se já estiver inicializado com mesma taxa, nada a fazer
    if (dev && sample_rate_ == rate) return true;

    // se estiver inicializado com taxa diferente, fechar e reabrir
    if (dev) {
        SDL_ClearQueuedAudio(dev);
        SDL_CloseAudioDevice(dev);
        dev = 0;
        sample_rate_ = 0;
        device_format_ = 0;
        device_channels_ = 0;
    }

    // listar dispositivos disponíveis para diagnóstico
    int numDevices = SDL_GetNumAudioDevices(0);
    std::cerr << "[audio] SDL reported " << numDevices << " output device(s)\n";
    for (int i = 0; i < numDevices; ++i) {
        const char* name = SDL_GetAudioDeviceName(i, 0);
        std::cerr << "[audio]   device[" << i << "] = " << (name ? name : "(null)") << "\n";
    }

    SDL_AudioSpec want{};
    SDL_AudioSpec have{};
    want.freq = rate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    want.callback = nullptr;

    dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE);
    if (dev == 0) {
        std::cerr << "SDL_OpenAudioDevice failed (wanted " << rate << " Hz): " << SDL_GetError() << "\n";
        sample_rate_ = 0;
        return false;
    }

    sample_rate_ = have.freq;
    device_format_ = have.format;
    device_channels_ = have.channels;

    // start playback
    SDL_PauseAudioDevice(dev, 0);

    std::cout << "Audio opened: device=" << dev << " rate=" << have.freq
        << " fmt=0x" << std::hex << have.format << std::dec
        << " chans=" << static_cast<int>(have.channels)
        << " samples=" << have.samples << "\n";

    // Push a short test tone (100 ms) to verify audio path immediately
    {
        const double freq = 440.0;
        const double duration_s = 0.1;
        const int sr = sample_rate_;
        const size_t frames = static_cast<size_t>(duration_s * sr);

        if (device_format_ == AUDIO_F32SYS) {
            // gerar float32 interleaved stereo
            std::vector<float> buf(frames * device_channels_);
            for (size_t i = 0; i < frames; ++i) {
                double t = static_cast<double>(i) / sr;
                double s = std::sin(2.0 * M_PI * freq * t) * 0.25;
                float v = static_cast<float>(s);
                if (device_channels_ == 1) {
                    buf[i] = v;
                }
                else {
                    buf[2 * i + 0] = v;
                    buf[2 * i + 1] = v;
                }
            }
            if (SDL_QueueAudio(dev, buf.data(), static_cast<Uint32>(buf.size() * sizeof(float))) != 0) {
                std::cerr << "[audio] SDL_QueueAudio (test tone float) failed: " << SDL_GetError() << "\n";
            }
            else {
                std::cerr << "[audio] test tone queued (" << frames << " frames, float32)\n";
            }
        }
        else {
            // gerar int16 interleaved stereo (compatível com AUDIO_S16SYS)
            std::vector<int16_t> buf(frames * device_channels_);
            for (size_t i = 0; i < frames; ++i) {
                double t = static_cast<double>(i) / sr;
                double s = std::sin(2.0 * M_PI * freq * t) * 0.25;
                int16_t v = static_cast<int16_t>(s * 32767.0);
                if (device_channels_ == 1) {
                    buf[i] = v;
                }
                else {
                    buf[2 * i + 0] = v;
                    buf[2 * i + 1] = v;
                }
            }
            if (SDL_QueueAudio(dev, buf.data(), static_cast<Uint32>(buf.size() * sizeof(int16_t))) != 0) {
                std::cerr << "[audio] SDL_QueueAudio (test tone s16) failed: " << SDL_GetError() << "\n";
            }
            else {
                std::cerr << "[audio] test tone queued (" << frames << " frames, s16)\n";
            }
        }
    }

    return true;
}

void AudioSystem::push(const int16_t* data, size_t frames) {
    if (!dev) return;

    // if device expects s16 stereo and we have same channels -> direct queue
    if (device_format_ == AUDIO_S16SYS && device_channels_ == 2) {
        const size_t bytes = frames * 2 * sizeof(int16_t);
        if (SDL_QueueAudio(dev, data, static_cast<Uint32>(bytes)) != 0) {
            std::cerr << "SDL_QueueAudio failed: " << SDL_GetError() << "\n";
        }
        else {
            static int call_count = 0;
            ++call_count;
            if (call_count <= 3) {
                Uint32 queued = SDL_GetQueuedAudioSize(dev);
                std::cerr << "[audio] pushed " << frames << " frames (" << bytes << " bytes). queued=" << queued << " bytes\n";
            }
        }
        return;
    }

    // otherwise, convert from int16_stereo -> device_format_/device_channels_
    if (device_format_ == AUDIO_F32SYS) {
        // convert to float32
        if (device_channels_ == 2) {
            std::vector<float> buf(frames * 2);
            for (size_t i = 0; i < frames; ++i) {
                buf[2 * i + 0] = s16_to_f32(data[2 * i + 0]);
                buf[2 * i + 1] = s16_to_f32(data[2 * i + 1]);
            }
            if (SDL_QueueAudio(dev, buf.data(), static_cast<Uint32>(buf.size() * sizeof(float))) != 0) {
                std::cerr << "SDL_QueueAudio (f32) failed: " << SDL_GetError() << "\n";
            }
        }
        else if (device_channels_ == 1) {
            std::vector<float> buf(frames);
            for (size_t i = 0; i < frames; ++i) {
                // downmix stereo -> mono by average
                float l = s16_to_f32(data[2 * i + 0]);
                float r = s16_to_f32(data[2 * i + 1]);
                buf[i] = (l + r) * 0.5f;
            }
            if (SDL_QueueAudio(dev, buf.data(), static_cast<Uint32>(buf.size() * sizeof(float))) != 0) {
                std::cerr << "SDL_QueueAudio (f32 mono) failed: " << SDL_GetError() << "\n";
            }
        }
        else {
            std::cerr << "[audio] unsupported device channel count: " << device_channels_ << "\n";
        }
        return;
    }

    // fallback: device wants s16 but mono/stereo mismatch
    if (device_format_ == AUDIO_S16SYS) {
        if (device_channels_ == 1) {
            std::vector<int16_t> buf(frames);
            for (size_t i = 0; i < frames; ++i) {
                int32_t sum = static_cast<int32_t>(data[2 * i + 0]) + static_cast<int32_t>(data[2 * i + 1]);
                buf[i] = static_cast<int16_t>(sum / 2);
            }
            if (SDL_QueueAudio(dev, buf.data(), static_cast<Uint32>(buf.size() * sizeof(int16_t))) != 0) {
                std::cerr << "SDL_QueueAudio (s16 mono) failed: " << SDL_GetError() << "\n";
            }
            return;
        }
        else {
            std::cerr << "[audio] unsupported device channel count for s16: " << device_channels_ << "\n";
            return;
        }
    }

    std::cerr << "[audio] unsupported device format 0x" << std::hex << device_format_ << std::dec << "\n";
}

void AudioSystem::shutdown() {
    if (!dev) return;
    SDL_ClearQueuedAudio(dev);
    SDL_CloseAudioDevice(dev);
    dev = 0;
    sample_rate_ = 0;
    device_format_ = 0;
    device_channels_ = 0;
}