#include "LibretroCore.h"
#include <iostream>

LibretroCore* LibretroCore::s_instance = nullptr;

// --- Callbacks globais para a Libretro ---
static bool core_environment(unsigned cmd, void* data) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        *(retro_pixel_format*)data = RETRO_PIXEL_FORMAT_RGB565;
        return true;
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *(const char**)data = "roms";
		return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char**)data = "saves";
        return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE: {
        auto* cb = (struct retro_log_callback*)data;
        cb->log = [](enum retro_log_level level, const char* fmt, ...) {
            va_list args; va_start(args, fmt); vprintf(fmt, args); va_end(args);
            };
        return true;
    }
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        *(int*)data = 1 | 2; // Vídeo e Áudio ativados
        return true;
    }
    return false;
}

static void core_video_refresh(const void* data, unsigned w, unsigned h, size_t pitch) {
    if (LibretroCore::s_instance) LibretroCore::s_instance->on_video_frame(data, w, h, pitch);
}

static void core_audio_sample(int16_t l, int16_t r) {
    if (LibretroCore::s_instance) LibretroCore::s_instance->push_audio_sample(l, r);
}

static size_t core_audio_sample_batch(const int16_t* data, size_t frames) {
    if (LibretroCore::s_instance) LibretroCore::s_instance->push_audio_batch(data, frames);
    return frames;
}

// --- Implementação da Classe ---

LibretroCore::LibretroCore() { s_instance = this; }
LibretroCore::~LibretroCore() { unload(); }

FARPROC LibretroCore::resolve(const char* name) {
    return GetProcAddress(core_handle_, name);
}

bool LibretroCore::load(const char* rom_path) {
    core_handle_ = LoadLibraryA("cores/fbneo_libretro.dll");
    if (!core_handle_) return false;

    // Binding das funções
    retro_init_ = (void(*)())resolve("retro_init");
    retro_run_ = (void(*)())resolve("retro_run");
    retro_load_game_ = (bool(*)(const retro_game_info*))resolve("retro_load_game");
    retro_set_environment_ = (void(*)(retro_environment_t))resolve("retro_set_environment");
    retro_set_video_refresh_ = (void(*)(retro_video_refresh_t))resolve("retro_set_video_refresh");
    retro_set_input_poll_ = (void(*)(retro_input_poll_t))resolve("retro_set_input_poll");
    retro_set_input_state_ = (void(*)(retro_input_state_t))resolve("retro_set_input_state");
    retro_set_audio_sample_ = (void(*)(retro_audio_sample_t))resolve("retro_set_audio_sample");
    retro_set_audio_sample_batch_ = (void(*)(retro_audio_sample_batch_t))resolve("retro_set_audio_sample_batch");
    retro_get_system_av_info_ = (void(*)(retro_system_av_info*))resolve("retro_get_system_av_info");
    retro_deinit_ = (void(*)())resolve("retro_deinit");
    retro_unload_game_ = (void(*)())resolve("retro_unload_game");

    // Configuração inicial
    retro_set_environment_(core_environment);
    retro_set_video_refresh_(core_video_refresh);
    retro_set_audio_sample_(core_audio_sample);
    retro_set_audio_sample_batch_(core_audio_sample_batch);
    retro_set_input_poll_(input_poll_cb);
    retro_set_input_state_(input_state_cb);

    retro_init_();

    // 3. Lógica de Inicialização de Áudio com Fallbacks
    int requestedSampleRate = 0;
    if (retro_get_system_av_info_) {
        retro_system_av_info info;
        std::memset(&info, 0, sizeof(info));
        retro_get_system_av_info_(&info);

        requestedSampleRate = static_cast<int>(info.timing.sample_rate + 0.5);
        this->sample_rate_core_ = requestedSampleRate; // Salva para o push_audio
        this->fps_ = (int)info.timing.fps;

        std::cerr << "[audio] core requested sample_rate = " << requestedSampleRate << "\n";
    }

    std::vector<int> candidates;
    if (requestedSampleRate > 0) candidates.push_back(requestedSampleRate);
    candidates.push_back(48000); // Fallback padrão Windows moderno
    candidates.push_back(44100); // Fallback CD Quality

    bool audio_ok = false;
    for (int r : candidates) {
        if (r <= 0) continue;
        std::cerr << "[audio] trying init at " << r << " Hz\n";

        // audio_ é sua instância de AudioSystem
        if (audio_.init(r)) {
            std::cerr << "[audio] initialized at " << r << " Hz\n";
            audio_ok = true;
            break;
        }
        else {
            std::cerr << "[audio] init failed at " << r << " Hz\n";
        }
    }

    if (!audio_ok) {
        std::cerr << "[audio] WARNING: audio device not initialized; continuing without audio\n";
    }

    // OpenGL Texture
    glGenTextures(1, &gl_texture_);
    glBindTexture(GL_TEXTURE_2D, gl_texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    render_pass_ = new GameRenderPass();
    render_pass_->init(1920, 1080);

    retro_game_info game{ rom_path, nullptr, 0, nullptr };
    return retro_load_game_(&game);
}

void LibretroCore::run() {
    if (retro_run_) retro_run_();
}

void LibretroCore::render() {
    if (!frame_data_ || !render_pass_) return;

    if (frame_dirty_) {
        glBindTexture(GL_TEXTURE_2D, gl_texture_);
        // FBNeo usa RGB565 por padrão no seu setup. 2 bytes por pixel.
        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame_pitch_ / 2);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB565, frame_w_, frame_h_, 0,
            GL_RGB, GL_UNSIGNED_SHORT_5_6_5, frame_data_);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        frame_dirty_ = false;
    }

    render_pass_->set_input_texture(gl_texture_);
    render_pass_->render();
}

void LibretroCore::on_video_frame(const void* data, unsigned w, unsigned h, size_t pitch) {
    frame_data_ = data;
    frame_w_ = w; frame_h_ = h; frame_pitch_ = (int)pitch;
    frame_dirty_ = true;
}

// --- Áudio ---
void LibretroCore::push_audio_sample(int16_t l, int16_t r) {
    int16_t buf[2] = { l, r };
    audio_.push(buf, 1, sample_rate_core_);
}

void LibretroCore::push_audio_batch(const int16_t* data, size_t frames) {
    audio_.push(data, frames, sample_rate_core_);
}

// --- Input ---
void RETRO_CALLCONV LibretroCore::input_poll_cb() {
    if (s_instance && s_instance->input_ && s_instance->window_) {
        s_instance->input_->update(s_instance->window_);
    }
}

int16_t RETRO_CALLCONV LibretroCore::input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port == 0 && device == RETRO_DEVICE_JOYPAD && s_instance) {
        return s_instance->input_state(id);
    }
    return 0;
}

int16_t LibretroCore::input_state(unsigned id) {
    return input_ ? input_->state(id) : 0;
}

void LibretroCore::unload() {
    audio_.shutdown();
    if (retro_unload_game_) retro_unload_game_();
    if (retro_deinit_) retro_deinit_();
    if (core_handle_) FreeLibrary(core_handle_);
}