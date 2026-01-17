#include "GameRenderPass.h"
#include <iostream>

static GLuint compile_shader(GLenum type, const char* src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

bool GameRenderPass::init(int, int)
{
    const char* vs_src = R"(
        #version 330 core
        layout(location=0) in vec2 pos;
        layout(location=1) in vec2 uv;
        out vec2 v_uv;
        void main() {
            v_uv = vec2(uv.x, 1.0 - uv.y);
            gl_Position = vec4(pos,0,1);
        }
    )";

    const char* fs_src = R"(
        #version 330 core
        in vec2 v_uv;
        out vec4 color;
        uniform sampler2D tex;
        void main() {
            color = texture(tex, v_uv);
        }
    )";

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);

    shader_ = glCreateProgram();
    glAttachShader(shader_, vs);
    glAttachShader(shader_, fs);
    glLinkProgram(shader_);

    glDeleteShader(vs);
    glDeleteShader(fs);

    float quad[] = {
        -1,-1, 0,0,
         1,-1, 1,0,
        -1, 1, 0,1,
         1, 1, 1,1
    };

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

    glVertexAttribPointer(0,2,GL_FLOAT,false,4*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1,2,GL_FLOAT,false,4*sizeof(float),(void*)(2*sizeof(float)));
    glEnableVertexAttribArray(1);

    return true;
}

void GameRenderPass::shutdown()
{
    glDeleteBuffers(1, &vbo_);
    glDeleteVertexArrays(1, &vao_);
    glDeleteProgram(shader_);
}

void GameRenderPass::set_input_texture(GLuint tex)
{
    input_tex_ = tex;
}

void GameRenderPass::render()
{
    glUseProgram(shader_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, input_tex_);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}
