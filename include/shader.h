#pragma once
#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>

GLuint compile_shader(GLenum type, const char *source);
GLuint create_shader_program(const char *vertex_source, const char *fragment_source);

#endif
