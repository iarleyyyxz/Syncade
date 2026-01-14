#pragma once
#include <SDL2/SDL.h>

class FrameTimer {
public:
    void init(double fps) {
        frame_ms = 1000.0 / fps;
        freq = SDL_GetPerformanceFrequency();
        last = SDL_GetPerformanceCounter();
    }

    void sync() {
        while (true) {
            Uint64 now = SDL_GetPerformanceCounter();
            double elapsed = (now - last) * 1000.0 / freq;
            if (elapsed >= frame_ms) {
                last = now;
                break;
            }
            SDL_Delay(1);
        }
    }

private:
    double frame_ms = 16.67;
    Uint64 freq = 0;
    Uint64 last = 0;
};
