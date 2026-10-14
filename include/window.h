#pragma once
#ifndef WINDOW_H
#define WINDOW_H

#include <glad/glad.h>
#include <GL/freeglut.h>

#include "molecule.h"

// 窗口尺寸是全局变量，在 window.c 中定义

// 着色器代码，在shader.h中直接引用
extern const char vertexShaderSource[];
extern const char fragmentShaderSource[];

// 全局变量：VAO、索引数量、着色器程序，在 window.c 中定义

// 化学键半径在 window.c 中定义

// 分子本身
extern Molecule *mol;

// 是否用两种颜色画化学键，当前是透视还是正交，视角相关全局变量，都在 window.c 中定义

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
