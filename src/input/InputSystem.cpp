#include "InputSystem.h"

InputSystem::InputSystem() {
    binds_[RETRO_DEVICE_ID_JOYPAD_UP] = GLFW_KEY_UP;
    binds_[RETRO_DEVICE_ID_JOYPAD_DOWN] = GLFW_KEY_DOWN;
    binds_[RETRO_DEVICE_ID_JOYPAD_LEFT] = GLFW_KEY_LEFT;
    binds_[RETRO_DEVICE_ID_JOYPAD_RIGHT] = GLFW_KEY_RIGHT;
    binds_[RETRO_DEVICE_ID_JOYPAD_A] = GLFW_KEY_X;
    binds_[RETRO_DEVICE_ID_JOYPAD_B] = GLFW_KEY_Z;
    binds_[RETRO_DEVICE_ID_JOYPAD_X] = GLFW_KEY_S;
    binds_[RETRO_DEVICE_ID_JOYPAD_Y] = GLFW_KEY_A;
    binds_[RETRO_DEVICE_ID_JOYPAD_L] = GLFW_KEY_Q;
    binds_[RETRO_DEVICE_ID_JOYPAD_R] = GLFW_KEY_W;
    binds_[RETRO_DEVICE_ID_JOYPAD_START] = GLFW_KEY_ENTER;
    binds_[RETRO_DEVICE_ID_JOYPAD_SELECT] = GLFW_KEY_RIGHT_SHIFT;
}

void InputSystem::update(GLFWwindow* window) {
    for (auto const& [retro_id, glfw_key] : binds_) {
        // Leitura direta do hardware via GLFW
        states_[retro_id] = (glfwGetKey(window, glfw_key) == GLFW_PRESS) ? 1 : 0;
    }
}

int16_t InputSystem::state(unsigned id) const {
    auto it = states_.find(id);
    return (it != states_.end()) ? it->second : 0;
}