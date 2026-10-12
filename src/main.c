#include "const.h"
#include "math_utils.h"
#include "geometry.h"
#include "shader.h"
#include "window.h"
#include "molecule.h"

#include <glad/glad.h>
// #include <GL/freeglut.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    // 必须先调用这个函数，而且其内部调用的函数顺序也有关系，见window.c
    if (initWindow(argc, argv) != 0) {
        fprintf(stderr, "%s\n", "Failed to initialize OpenGL functions.");
        exit(EXIT_FAILURE);
    }

    // 创建几何体、着色器
    initScene();

    // 回调函数
    glutDisplayFunc(&display);
    glutReshapeFunc(&reshape);
    glutKeyboardFunc(&keyboard);

    // 进入事件循环（永不返回）
    glutMainLoop();

    destroy_molecule(mol);
    mol = NULL;

    return 0;
}
