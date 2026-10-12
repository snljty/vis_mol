#pragma once
#ifndef WINDOW_H
#define WINDOW_H

#include <glad/glad.h>
#include <GL/freeglut.h>

#include "molecule.h"

// 窗口尺寸（全局变量，在 main.c 中定义，其他文件通过 extern 使用，比如window.c）
extern int winWidth;
extern int winHeight;

// 着色器代码，在shader.h中直接引用
extern const char vertexShaderSource[];
extern const char fragmentShaderSource[];

// 全局变量：VAO、索引数量、着色器程序，在 window.c 中定义，其他文件通过 extern 使用
extern GLuint sphereVAO, cylinderVAO; // handle
extern int sphereIdxCount, cylinderIdxCount;
extern GLuint shaderProgram; // handle

// 化学键半径
extern const float STICK_RADIUS;

// 分子本身
extern Molecule *mol;

// 创建 VAO（包含 VBO 和 EBO），返回 VAO 编号
GLuint createVAO(float *vertices, int vertexCount,
                       GLuint *indices, int indexCount);

// 初始化场景（创建几何体、着色器）
void initScene();

// 初始化窗口并返回是否成功（0成功，-1失败）
int initWindow(int argc, char **argv);

// 显示回调（每帧渲染）
void display();

// 窗口大小变化回调
void reshape(int w, int h);

// 键盘回调（ESC 退出）
void keyboard(unsigned char key, int x, int y);

// 注册回调（由 main 调用）
void registerCallbacks(void);

#endif
