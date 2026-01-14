#include "InputSystem.h"

int16_t InputSystem::state(unsigned id) {
    const Uint8* k = SDL_GetKeyboardState(nullptr);
    if (binds.count(id)) return k[binds[id]];
    return 0;
}

void InputSystem::setKey(unsigned id, SDL_Scancode scancode) {
    binds[id] = scancode;
}

void InputSystem::reset() {
    binds = {
        { RETRO_DEVICE_ID_JOYPAD_UP,     SDL_SCANCODE_UP },
        { RETRO_DEVICE_ID_JOYPAD_DOWN,   SDL_SCANCODE_DOWN },
        { RETRO_DEVICE_ID_JOYPAD_LEFT,   SDL_SCANCODE_LEFT },
        { RETRO_DEVICE_ID_JOYPAD_RIGHT,  SDL_SCANCODE_RIGHT },
        { RETRO_DEVICE_ID_JOYPAD_A,      SDL_SCANCODE_Z },       // A
        { RETRO_DEVICE_ID_JOYPAD_B,      SDL_SCANCODE_X },       // B
        { RETRO_DEVICE_ID_JOYPAD_X,      SDL_SCANCODE_A },       // X
        { RETRO_DEVICE_ID_JOYPAD_Y,      SDL_SCANCODE_S },       // Y
        { RETRO_DEVICE_ID_JOYPAD_L,      SDL_SCANCODE_Q },       // L
        { RETRO_DEVICE_ID_JOYPAD_R,      SDL_SCANCODE_W },       // R
        { RETRO_DEVICE_ID_JOYPAD_START,  SDL_SCANCODE_RETURN },  // Start
        { RETRO_DEVICE_ID_JOYPAD_SELECT, SDL_SCANCODE_RSHIFT }   // Select
	};
}