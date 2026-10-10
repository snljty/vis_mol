#pragma once
#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>

GLuint compileShader(GLenum type, const char *source);
GLuint createShaderProgram(const char *vertexSource, const char *fragmentSource);

#endif
