#pragma once
#include <SDL2/SDL.h>

class VideoSystem {
public:
    bool init();
    void render(const void* data, unsigned w, unsigned h, size_t pitch);
    void shutdown();

private:
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    unsigned width = 0, height = 0;
};
