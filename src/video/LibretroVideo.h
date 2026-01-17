#pragma once
#include <glad/glad.h>
#include <cstdint>

class LibretroVideo {
public:
    bool init(int w, int h);
    void upload(const void* data, int pitch);
    GLuint texture() const { return tex; }

private:
    GLuint tex = 0;
    int width = 0;
    int height = 0;
};
