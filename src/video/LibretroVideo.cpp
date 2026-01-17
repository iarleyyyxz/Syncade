#include "LibretroVideo.h"

bool LibretroVideo::init(int w, int h) {
    width = w;
    height = h;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(
        GL_TEXTURE_2D, 0,
        GL_RGB565,
        width, height,
        0,
        GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
        nullptr
    );

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return true;
}

void LibretroVideo::upload(const void* data, int pitch) {
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / 2);
    glTexSubImage2D(
        GL_TEXTURE_2D, 0,
        0, 0, width, height,
        GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
        data
    );

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}
