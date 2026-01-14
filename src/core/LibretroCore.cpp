#include "LibretroCore.h"
#include <iostream>
#include <cstring>
#include <vector>

// permitir acesso das callbacks à instância
LibretroCore* LibretroCore::s_instance = nullptr;

// Simple no-op / minimal callbacks to satisfy cores that assume callbacks exist.
// core_video_refresh foi alterado para apresentar via SDL quando houver instância.

static bool core_environment(unsigned cmd, void* data)
{
    switch (cmd)
    {
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
    {
        if (!data) return false;
        auto* fmt = reinterpret_cast<enum retro_pixel_format*>(data);
        *fmt = RETRO_PIXEL_FORMAT_RGB565;
        return true;
    }

    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        if (!data) return false;
        *(const char**)data = "roms";
        return true;

    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        if (!data) return false;
        *(const char**)data = "saves";
        return true;
    }

    return false;
}

static void core_video_refresh(const void* data, unsigned width, unsigned height, size_t pitch)
{
    // se não houver instância do host, não fazemos nada
    if (!LibretroCore::s_instance) return;
    LibretroCore* inst = LibretroCore::s_instance;

    if (!data) return;

    // criar/atualizar textura se a resolução mudou
    if (width != inst->width_ || height != inst->height_)
    {
        inst->width_ = static_cast<int>(width);
        inst->height_ = static_cast<int>(height);

        if (inst->texture_)
        {
            SDL_DestroyTexture(inst->texture_);
            inst->texture_ = nullptr;
        }

        if (inst->renderer_)
        {
            inst->texture_ = SDL_CreateTexture(
                inst->renderer_,
                SDL_PIXELFORMAT_RGB565,
                SDL_TEXTUREACCESS_STREAMING,
                inst->width_,
                inst->height_
            );

            if (!inst->texture_)
            {
                std::cerr << "Failed to create texture: " << SDL_GetError() << "\n";
                return;
            }
        }
    }

    if (!inst->texture_ || !inst->renderer_) return;

    // atualizar textura e apresentar
    SDL_UpdateTexture(inst->texture_, nullptr, data, static_cast<int>(pitch));
    SDL_RenderClear(inst->renderer_);
    SDL_RenderCopy(inst->renderer_, inst->texture_, nullptr, nullptr);
    SDL_RenderPresent(inst->renderer_);
}

static void core_input_poll(void)
{
    // atualiza estados SDL (keyboard) para que SDL_GetKeyboardState retorne valores atuais
    if (LibretroCore::s_instance) {
        LibretroCore::s_instance->poll_input();
    }
    else {
        SDL_PumpEvents();
    }
}

static int16_t core_input_state(unsigned /*port*/, unsigned device, unsigned /*index*/, unsigned id)
{
    if (device != RETRO_DEVICE_JOYPAD) return 0;
    if (!LibretroCore::s_instance) return 0;
    return LibretroCore::s_instance->input_state(id);
}

static void core_audio_sample(int16_t left, int16_t right)
{
    if (!LibretroCore::s_instance) return;
    LibretroCore::s_instance->push_audio_sample(left, right);
}

static size_t core_audio_sample_batch(const int16_t* data, size_t frames)
{
    if (!LibretroCore::s_instance) return frames;
    // loga apenas a primeira chamada para confirmar fluxo de áudio sem floodar o log
    static bool logged_once = false;
    if (!logged_once) {
        logged_once = true;
        std::cerr << "[audio] core_audio_sample_batch called, frames=" << frames << "\n";
    }
    LibretroCore::s_instance->push_audio_batch(data, frames);
    return frames;
}

// ---------------- LibretroCore ----------------

LibretroCore::LibretroCore()
    : core_handle_(nullptr),
    retro_init_(nullptr),
    retro_run_(nullptr),
    retro_load_game_(nullptr),
    retro_set_environment_(nullptr),
    retro_set_video_refresh_(nullptr),
    retro_set_input_poll_(nullptr),
    retro_set_input_state_(nullptr),
    retro_set_audio_sample_(nullptr),
    retro_set_audio_sample_batch_(nullptr),
    retro_get_system_av_info_(nullptr),
    window_(nullptr),
    renderer_(nullptr),
    texture_(nullptr),
    width_(0),
    height_(0),
    input_(),
    audio_()
{
    s_instance = this;
}

LibretroCore::~LibretroCore()
{
    unload();
    if (s_instance == this) s_instance = nullptr;
}

FARPROC LibretroCore::resolve(const char* name)
{
    if (!core_handle_) return nullptr;
    return GetProcAddress(core_handle_, name);
}

void LibretroCore::init_video()
{
    // cria janela e renderer SDL se ainda não existem
    if (window_ && renderer_) return;

    window_ = SDL_CreateWindow(
        "Syncade - Core",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        640,
        480,
        SDL_WINDOW_SHOWN
    );

    if (!window_) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        return;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        return;
    }
}

void LibretroCore::destroy_video()
{
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    width_ = height_ = 0;
}

int LibretroCore::fps() const
{
    return 60; // default; adjust if you query the core for actual rate
}

bool LibretroCore::load(const char* rom_path)
{
    if (core_handle_) unload();

    core_handle_ = LoadLibraryA("cores/fbneo_libretro.dll");
    if (!core_handle_) {
        std::cerr << "Failed to load core DLL\n";
        return false;
    }

    // resolve required functions dynamically to avoid link-time dependencies
    retro_init_ = reinterpret_cast<retro_init_t>(resolve("retro_init"));
    retro_run_ = reinterpret_cast<retro_run_t>(resolve("retro_run"));
    retro_load_game_ = reinterpret_cast<retro_load_game_t>(resolve("retro_load_game"));

    // optional setters
    retro_set_environment_ = reinterpret_cast<retro_set_environment_t>(resolve("retro_set_environment"));
    retro_set_video_refresh_ = reinterpret_cast<retro_set_video_refresh_t>(resolve("retro_set_video_refresh"));
    retro_set_input_poll_ = reinterpret_cast<retro_set_input_poll_t>(resolve("retro_set_input_poll"));
    retro_set_input_state_ = reinterpret_cast<retro_set_input_state_t>(resolve("retro_set_input_state"));
    retro_set_audio_sample_ = reinterpret_cast<retro_set_audio_sample_t>(resolve("retro_set_audio_sample"));
    retro_set_audio_sample_batch_ = reinterpret_cast<retro_set_audio_sample_batch_t>(resolve("retro_set_audio_sample_batch"));

    // function that used to be referenced directly at link-time — resolve dynamically
    retro_get_system_av_info_ = reinterpret_cast<retro_get_system_av_info_t>(resolve("retro_get_system_av_info"));

    if (!retro_init_ || !retro_run_ || !retro_load_game_) {
        std::cerr << "Missing required core exports\n";
        FreeLibrary(core_handle_);
        core_handle_ = nullptr;
        return false;
    }

    // register callbacks if setters are present
    if (retro_set_environment_) retro_set_environment_(core_environment);
    if (retro_set_video_refresh_) retro_set_video_refresh_(core_video_refresh);
    if (retro_set_input_poll_) retro_set_input_poll_(core_input_poll);
    if (retro_set_input_state_) retro_set_input_state_(core_input_state);
    if (retro_set_audio_sample_) retro_set_audio_sample_(core_audio_sample);
    if (retro_set_audio_sample_batch_) retro_set_audio_sample_batch_(core_audio_sample_batch);

    // prepare host subsystems before core init/load where reasonable
    init_video();

    // init core
    retro_init_();

    // if provided, get system av info dynamically
    int requestedSampleRate = 0;
    if (retro_get_system_av_info_) {
        retro_system_av_info info;
        std::memset(&info, 0, sizeof(info));
        retro_get_system_av_info_(&info);
        requestedSampleRate = static_cast<int>(info.timing.sample_rate + 0.5);
        std::cerr << "[audio] core requested sample_rate = " << requestedSampleRate << "\n";
    }
    else {
        std::cerr << "[audio] retro_get_system_av_info not available; will try fallbacks\n";
    }

    // Try to initialize audio with fallback rates
    std::vector<int> candidates;
    if (requestedSampleRate > 0) candidates.push_back(requestedSampleRate);
    // common fallbacks
    candidates.push_back(48000);
    candidates.push_back(44100);

    bool audio_ok = false;
    for (int r : candidates) {
        if (r <= 0) continue;
        std::cerr << "[audio] trying init at " << r << " Hz\n";
        if (init_audio(r)) {
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

    // load game
    retro_game_info game{};
    game.path = rom_path;
    game.data = nullptr;
    game.size = 0;
    game.meta = nullptr;

    if (!retro_load_game_(&game)) {
        std::cerr << "Failed to load game\n";
        FreeLibrary(core_handle_);
        core_handle_ = nullptr;
        destroy_video();
        shutdown_audio();
        return false;
    }

    return true;
}

void LibretroCore::run()
{
    if (retro_run_) retro_run_();
}

void LibretroCore::unload()
{
    // destruir recursos de vídeo antes de liberar a DLL
    destroy_video();

    // desligar audio
    shutdown_audio();

    if (core_handle_) {
        FreeLibrary(core_handle_);
        core_handle_ = nullptr;
    }

    retro_init_ = nullptr;
    retro_run_ = nullptr;
    retro_load_game_ = nullptr;
    retro_set_environment_ = nullptr;
    retro_set_video_refresh_ = nullptr;
    retro_set_input_poll_ = nullptr;
    retro_set_input_state_ = nullptr;
    retro_set_audio_sample_ = nullptr;
    retro_set_audio_sample_batch_ = nullptr;
    retro_get_system_av_info_ = nullptr;
}

// ---------- input helpers ----------

void LibretroCore::poll_input()
{
    // garante que o estado do teclado SDL está atualizado
    SDL_PumpEvents();
}

int16_t LibretroCore::input_state(unsigned id)
{
    return input_.state(id);
}

// ---------- audio helpers ----------

bool LibretroCore::init_audio(int sampleRate)
{
    return audio_.init(sampleRate);
}

void LibretroCore::shutdown_audio()
{
    audio_.shutdown();
}

void LibretroCore::push_audio_sample(int16_t left, int16_t right)
{
    int16_t buf[2] = { left, right };
    audio_.push(buf, 1);
}

void LibretroCore::push_audio_batch(const int16_t* data, size_t frames)
{
    audio_.push(data, frames);
}