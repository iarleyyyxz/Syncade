#include "VideoSystem.h"
#include <iostream>

bool VideoSystem::init() {
    window = SDL_CreateWindow("Arcade Engine",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720, SDL_WINDOW_SHOWN);

    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED);

    return window && renderer;
}

void VideoSystem::render(const void* data, unsigned w, unsigned h, size_t pitch) {
    if (!data) return;

    if (!texture || w != width || h != height) {
        if (texture) SDL_DestroyTexture(texture);
        width = w; height = h;
        texture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_RGB565,
            SDL_TEXTUREACCESS_STREAMING,
            w, h);
    }

    SDL_UpdateTexture(texture, nullptr, data, pitch);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
}

void VideoSystem::shutdown() {
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
}
