#include "shader.h"
#include <stdio.h>
#include <stdlib.h>

// 着色器源码在shaders目录下
#include "vertex.vert.h"
#include "fragment.frag.h"

// ==================== 着色器编译与链接 ====================
// 编译顶点和片元着色器
GLuint compileShader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    // 第一个参数代表是顶点着色器还是片元着色器。
    // 第二个参数代表字符串数组中的元素个数。
    // 第三个参数是指向包含要加载的着色器代码的字符串。
    // 最后一个参数是指向字符串长度的指针，如果是NULL，就代表每个字符串都是以'\0'结尾。
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    int success;
    // 第二个参数：GL_SHADER_TYPE返回类型；GL_DELETE_STATUS删除状态
    // GL_INFO_LOG_LEHGTH日志长度，GL_SHADER_SOURCE_LENGTH着色器源码长度
    // 应该还有其他很多可用的参数
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        int logLen;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen) {
            char *infoLog = (char *)malloc(logLen);
            glGetShaderInfoLog(shader, logLen, NULL, infoLog);
            fprintf(stderr, "ERROR::SHADER::COMPILATION_FAILED\n%s\n", infoLog);
            free(infoLog); infoLog = NULL;
        } else {
            fprintf(stderr, "ERROR::SHADER::COMPILATION_FAILED\n");
        }
        exit(EXIT_FAILURE);
    }
    return shader;
}

// 调用上面的函数编译每段着色器代码，然后链接到一起
GLuint createShaderProgram(const char *vertexSource, const char *fragmentSource) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success;
    // 参数类似glGetShaderiv
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        int logLen;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen) {
            char *infoLog = (char *)malloc(logLen);
            glGetProgramInfoLog(program, logLen, NULL, infoLog);
            fprintf(stderr, "ERROR::PROGRAM::LINKING_FAILED\n%s\n", infoLog);
            free(infoLog); infoLog = NULL;
        } else {
            fprintf(stderr, "ERROR::PROGRAM::LINKING_FAILED\n");
        }
        exit(EXIT_FAILURE);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

