#define SDL_MAIN_HANDLED

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Timing.h"

#include "core/LibretroCore.h"

int main(int argc, char** argv)
{
    if (!glfwInit()) {
        std::cerr << "GLFW init failed\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1920, 1080, "Syncade", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "Erro SDL: " << SDL_GetError() << std::endl;
    }

    LibretroCore* core = new LibretroCore();

    InputSystem* input = new InputSystem();
    core->setInput(input);
    core->setWindow(window); // <-- MUITO IMPORTANTE: passa a janela GLFW para o Core

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "GLAD init failed\n";
        return -1;
    }

    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, 1920, 1080);

    if (!core->load("roms/sf2ce.zip")) {
        std::cerr << "Failed to load core\n";
        return -1;
    }

    FrameTimer timer;
    timer.init(core->fps());

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        core->run();
        timer.sync();

        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        core->render();

        glfwSwapBuffers(window);
    }

    core->unload();
    glfwTerminate();
    return 0;
}
