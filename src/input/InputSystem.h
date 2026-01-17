#pragma once
#include <map>
#include <GLFW/glfw3.h>
#include <libretro/libretro.h>

class InputSystem {
public:
    InputSystem();
    // Atualiza o estado interno perguntando diretamente ao GLFW
    void update(GLFWwindow* window);
    // Retorna o estado salvo
    int16_t state(unsigned id) const;

private:
    std::map<unsigned, int> binds_;
    std::map<unsigned, int16_t> states_;
};