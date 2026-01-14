#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include "../src/core/LibretroCore.h"
#include "Timing.h"

int main() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
        return -1;
    }

    LibretroCore core;
    core.init_video();
    if (!core.load("roms/sf2ce.zip")) {
        core.unload();
        SDL_Quit();
        return -1;
    }

    FrameTimer timer;
    timer.init(core.fps());

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e))
            if (e.type == SDL_QUIT) running = false;

        core.run();
        timer.sync();
    }

    core.unload();
    SDL_Quit();
    return 0;
}
