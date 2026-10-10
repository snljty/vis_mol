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
    if (argc - 1 != 1) {
        fprintf(stdout, "Usage: \"%s\" xxx.gjf|xxx.xyz\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    char ifilename[PATH_MAX + 1];
    ifilename[PATH_MAX] = '\0';
    strncpy(ifilename, argv[1], PATH_MAX);

    Molecule *mol = read_molecule(ifilename);

    get_intramolecular_connectivity(mol);

    // print_molecule(mol);
    print_intramolecular_connectivity(mol);
    // print_box(mol);

    destroy_molecule(mol);
    mol = NULL;

    #ifdef HAVE_UTHASH
    destroy_elements_hash_table();
    #endif

    // 必须先调用这个函数，而且其内部调用的函数顺序也有关系，见window.c
    if (initWindow(argc, argv, "Lollipop - freeGLUT") != 0) {
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

    return 0;
}
