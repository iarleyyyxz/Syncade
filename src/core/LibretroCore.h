#pragma once
#include <windows.h>
#include <string>
#include <SDL2/SDL.h>
#include "libretro/libretro.h"
#include "../input/InputSystem.h"
#include "../audio/AudioSystem.h"

class LibretroCore {
public:
    LibretroCore();
    ~LibretroCore();

    // carrega o core e a ROM; retorna false em erro
    bool load(const char* rom_path);
    void unload();
    void run();
    int fps() const; // retorna inteiro para evitar warning de conversão

    // inicializa o subsistema de vídeo (usa SDL internamente)
    void init_video();
    void destroy_video();

    // input helpers usados pelas callbacks estáticas
    void poll_input();
    int16_t input_state(unsigned id);

    // audio helpers usados pelas callbacks estáticas
    bool init_audio(int sampleRate);
    void shutdown_audio();
    void push_audio_sample(int16_t left, int16_t right);
    void push_audio_batch(const int16_t* data, size_t frames);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    int width_ = 0;
    int height_ = 0;

    static LibretroCore* s_instance;

private:
    HMODULE core_handle_;

    // sistema de input do host
    InputSystem input_;

    // sistema de audio do host
    AudioSystem audio_;

    // typedefs para as funções do core (resolvidas dinamicamente)
    using retro_init_t = void(*)();
    using retro_run_t = void(*)();
    using retro_load_game_t = bool(*)(const retro_game_info*);
    using retro_set_environment_t = void(*)(retro_environment_t);
    using retro_set_video_refresh_t = void(*)(retro_video_refresh_t);
    using retro_set_input_poll_t = void(*)(retro_input_poll_t);
    using retro_set_input_state_t = void(*)(retro_input_state_t);
    using retro_set_audio_sample_t = void(*)(retro_audio_sample_t);
    using retro_set_audio_sample_batch_t = void(*)(retro_audio_sample_batch_t);
    using retro_get_system_av_info_t = void(*)(struct retro_system_av_info*);

    retro_init_t retro_init_;
    retro_run_t retro_run_;
    retro_load_game_t retro_load_game_;

    retro_set_environment_t retro_set_environment_;
    retro_set_video_refresh_t retro_set_video_refresh_;
    retro_set_input_poll_t retro_set_input_poll_;
    retro_set_input_state_t retro_set_input_state_;
    retro_set_audio_sample_t retro_set_audio_sample_;
    retro_set_audio_sample_batch_t retro_set_audio_sample_batch_;

    retro_get_system_av_info_t retro_get_system_av_info_;

    // helpers
    FARPROC resolve(const char* name);
};