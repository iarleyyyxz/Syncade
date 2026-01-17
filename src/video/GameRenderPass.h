#pragma once
#include <glad/glad.h>

class GameRenderPass {
public:
    bool init(int screen_w, int screen_h);
    void shutdown();

    void set_input_texture(GLuint tex);
    void render();

private:
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint shader_ = 0;
    GLuint input_tex_ = 0;
};
