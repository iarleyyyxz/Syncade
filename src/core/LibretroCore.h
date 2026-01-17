#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <libretro/libretro.h>

#include "../input/InputSystem.h"
#include "../audio/AudioSystem.h"
#include "../video/GameRenderPass.h"

class LibretroCore {
public:
    LibretroCore();
    ~LibretroCore();

    bool load(const char* rom_path);
    void unload();
    void run();
    void render();

    // Setters
    void setInput(InputSystem* input) { input_ = input; }
    void setWindow(GLFWwindow* window) { window_ = window; }

    // Callbacks de processamento
    void on_video_frame(const void* data, unsigned w, unsigned h, size_t pitch);
    void push_audio_sample(int16_t l, int16_t r);
    void push_audio_batch(const int16_t* data, size_t frames);
    int16_t input_state(unsigned id);

    int fps() { return fps_; }

    static LibretroCore* s_instance;

private:
    FARPROC resolve(const char* name);

    // libretro pointers
    HMODULE core_handle_ = nullptr;
    void (*retro_init_)(void) = nullptr;
    void (*retro_run_)(void) = nullptr;
    bool (*retro_load_game_)(const struct retro_game_info*) = nullptr;
    void (*retro_set_environment_)(retro_environment_t) = nullptr;
    void (*retro_set_video_refresh_)(retro_video_refresh_t) = nullptr;
    void (*retro_set_audio_sample_)(retro_audio_sample_t) = nullptr;
    void (*retro_set_audio_sample_batch_)(retro_audio_sample_batch_t) = nullptr;
    void (*retro_set_input_poll_)(retro_input_poll_t) = nullptr;
    void (*retro_set_input_state_)(retro_input_state_t) = nullptr;
    void (*retro_get_system_av_info_)(struct retro_system_av_info*) = nullptr;
    void (*retro_deinit_)(void) = nullptr;
    void (*retro_unload_game_)(void) = nullptr;

    // Callbacks estáticos para a DLL
    static void RETRO_CALLCONV input_poll_cb();
    static int16_t RETRO_CALLCONV input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id);

    // Video State
    GLuint gl_texture_ = 0;
    const void* frame_data_ = nullptr;
    int frame_w_ = 0, frame_h_ = 0, frame_pitch_ = 0;
    bool frame_dirty_ = false;
    GameRenderPass* render_pass_ = nullptr;

    // Systems
    InputSystem* input_ = nullptr;
    AudioSystem audio_;
    GLFWwindow* window_ = nullptr;

    int fps_ = 60;

    int sample_rate_core_ = 0;
};