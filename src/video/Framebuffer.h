#pragma once
#include <glad/glad.h>

class Framebuffer {
public:
    bool init(int w, int h);
    void bind();
    void unbind();

    GLuint texture() const { return color_tex_; }

    void shutdown();

private:
    GLuint fbo_ = 0;
    GLuint color_tex_ = 0;
};
