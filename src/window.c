#include "window.h"
#include "math_utils.h"
#include "geometry.h"
#include "shader.h"
#include "const.h"
// #include <GL/freeglut.h>
#include <stdlib.h>

// 窗口尺寸
int winWidth = 800, winHeight = 1000;

// 化学键半径
const float STICK_RADIUS = 0.1f;

// 分子
Molecule *mol = NULL;

// gl* 这些函数报错通常不返回错误码，而是设定全局错误标志：
// GLenum err = glGetError();
// 每次调用只返回一个错误，如果多个错误，要多次调用取完。
// GL_NO_ERROR也就是0代表没有错误。
// 常见错误有：
// GL_INVALID_ENUM 传了非法的枚举值（如 glDrawElements 传错图元类型）
// GL_INVALID_VALUE    传了非法数值（如负的 size）
// GL_INVALID_OPERATION    非法操作（如没绑 VAO 就 glDrawElements）
// GL_OUT_OF_MEMORY    内存不足
// GL_INVALID_FRAMEBUFFER_OPERATION    帧缓冲不完整
// 注意 glGetError 只能检测到"上一次调用之后"的错误。如果错误是异步的（GPU 延迟执行），可能 glGetError 查不到。
// OpenGL 4.3+ 提供调试回调，错误会自动触发一个回调函数，不用手动查。
// 注册回调
// glDebugMessageCallback(error_callback, NULL);
// glEnable(GL_DEBUG_OUTPUT);
// 回调函数
// void error_callback(GLenum source, GLenum type, GLuint id,
//               GLenum severity, GLsizei length,
//               const char *message, const void *userParam) {
//     fprintf(stderr, "GL: %s\n", message);
// }
// 环境性错误主要发生在"初始化"阶段，而且通常以"函数返回失败/返回 NULL"的方式暴露，而不是靠 glGetError。


// 全局变量：VAO、索引数量、着色器程序
// initScene函数设置了它们的值，用来给display函数使用
GLuint sphereVAO, cylinderVAO; // 球和圆柱VAO的handle编号
int sphereIdxCount, cylinderIdxCount; // 球和圆柱的顶点个数，geometry.c里面的函数计算他们的值
GLuint shaderProgram; // 着色器handle编号

// 创建 VAO（包含 VBO 和 EBO）
GLuint createVAO(float *vertices, int vertexCount,
                       GLuint *indices, int indexCount) {
    GLuint VAO, VBO, EBO;
    // 申请一个 OpenGL 资源编号
    // VBO 存放顶点信息（位置、法线...），EBO 存放片元对应的顶点序号，VAO 告诉 OpenGL 如何解释 VBO 的数据
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    // 必须先绑定VAO，然后绑定VBO，传顶点数据，再绑定EBO，传索引数据，设置顶点属性指针，最后解绑VAO

    // 绑定 VAO
    // 这里绑定是为了写入配置
    glBindVertexArray(VAO);

    // STATIC：数据一次性上传，之后基本不改。
    // DRAW：数据用于绘制（GPU 读取来渲染）
    // GL_DYNAMIC_DRAW：  内容会被频繁修改（每帧更新顶点）
    // GL_STREAM_DRAW：几乎每次绘制都变（每帧完全重写流式数据）

    // 上传顶点数据到 VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * 6 * sizeof(float),
                 vertices, GL_STATIC_DRAW);

    // 上传索引数据到 EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(GLuint),
                 indices, GL_STATIC_DRAW);

    // GL_FALSE：这里指不归一化，原样传给着色器

    // 设置顶点属性指针
    // 位置 (location = 0)：3个float，步长6个float，偏移0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                          6 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);
    // 法线 (location = 1)：3个float，步长6个float，偏移3个float
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE,
                          6 * sizeof(float), (void *)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0); // 绑定到空默认0，也就是解绑，防止后续误操作，也就是后续操作不会再碰这个VAO。
    return VAO;
}

// 初始化场景（创建几何体、着色器）
void initScene() {
    #ifdef USE_HIGH_RESOLUTION
    int sectors = 32; // 经纬球和圆柱的经向块数
    int stacks = 32; // 只有经纬球用的到，纬向块数
    int numSubdivide = 3; // 正二十面体细分球（棱角球）表面细分次数。 
    #else
    int sectors = 16;
    int stacks = 16;
    int numSubdivide = 2;
    #endif
    (void)stacks; // 这个变量其实用不到，我们已经不用经纬球了，但是还是留在这里，这行避免编译器 Warning

    // numSubdivide = 2 的时候三角形比上面经纬球的 sectors = 16; stacks = 16; 略少，但是更均匀，效果应该差不多。
    // 如果 sectors 和 stacks 都设高质量的 32，可以 numSubdivide 增加到 3。总之只考虑三角形个数，sectors 和 stacks 翻倍，
    // 三角形近似变成四倍；numSubdivide 增加 1，三角形个数也变成四倍。

    float *sphereVerts, *cylinderVerts;
    int sphereVertCount, cylinderVertCount;
    GLuint *sphereIdx, *cylinderIdx;

    // 最初版本的，经纬球
    // generateGeoSphere(sectors, stacks,
    //                &sphereVerts, &sphereVertCount,
    //                &sphereIdx, &sphereIdxCount);
    // 现在版本的，只用棱角球，一定做中点去重，看有没有 Hash 库决定是用散列函数还是排序+二分查找
    generateIcoSphere(numSubdivide, 
                   &sphereVerts, &sphereVertCount,
                   &sphereIdx, &sphereIdxCount);
    generateCylinder(sectors,
                     &cylinderVerts, &cylinderVertCount,
                     &cylinderIdx, &cylinderIdxCount);

    // 赋值给.h声明的两个全局变量。
    sphereVAO = createVAO(sphereVerts, sphereVertCount, sphereIdx, sphereIdxCount);
    cylinderVAO = createVAO(cylinderVerts, cylinderVertCount, cylinderIdx, cylinderIdxCount);

    // 数据已经上传到GPU，可以释放内存了
    free(sphereVerts); free(sphereIdx);
    sphereVerts = NULL; sphereIdx = NULL;
    free(cylinderVerts); free(cylinderIdx);
    cylinderVerts = NULL; cylinderIdx = NULL;

    // 编译并链接得到GPU的着色器程序
    shaderProgram = createShaderProgram(vertexShaderSource, fragmentShaderSource);

    // 开启深度测试，正确处理遮挡关系
    glEnable(GL_DEPTH_TEST);
    // 开启表面剔除
    glEnable(GL_CULL_FACE); // glDisable来去掉表面剔除
    // glCullFace(GL_BACK); // 可以没有这行，默认剔除背面，即三角形顶点逆时针顺序时应有的样子。也可以用GL_FRONT剔除正面。
}

// 显示回调（每帧渲染）
void display() {
    // 清屏
    // 背景色 RGBA
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    // 因为深度测试用"上一帧的深度值"判断遮挡就错了，每帧必须重置深度，保证这一帧重新判断谁在前谁在后。
    // 把颜色缓冲和深度缓冲都清空
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 激活着色器，接下来用着色器来画所有东西
    glUseProgram(shaderProgram);

    // 光照参数
    // 把光照参数传给着色器这个 uniform
    // 方向光方向，下一行归一化
    float lightDir[3] = {0.5f, 1.0f, 0.3f};
    vec3_normalize(lightDir);
    // uniform 是 CPU 传给 GPU 的全局只读变量。着色器对其只读，所有顶点、片元共享，可以每帧更新。
    // glUniform3fv 和 glUniform3f 都是传3个float给uniform，区别是3fv传指针(loc, count, ptr)，3f传值(loc, x, y, z)。
    // 方向光，传的光方向。这部分跟着色器代码绑定，比如如果用点光源，要传光的位置。如果要修改光的类型或者数量（叠加效果），要改着色器代码。
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightDir"), 1, lightDir);
    glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"), 1.0f, 1.0f, 1.0f); // 纯白，这三个数代表光颜色三个分量的强度。
    glUniform3f(glGetUniformLocation(shaderProgram, "ambientColor"), 0.2f, 0.2f, 0.2f); // 环境光，很暗，通常只需要一个

    // M V P矩阵上传顺序无所谓，这只是习惯。

    // 投影矩阵
    float projection[16];
    float aspect = (float)winWidth / (float)winHeight;
    float viewSize = 6.0f;  // 控制可见区域的大小（高度的一半）
    // viewSize 只跟正交视角有关，越大能看到的范围越大，物体就显得越小
    float left   = -viewSize * aspect;
    float right  =  viewSize * aspect;
    float bottom = -viewSize;
    float top    =  viewSize;
    float vfov_degree = 45.0f;
    float near_ = 0.1f;
    float far_ = 100.f;

    // 如果用正交视角
    mat4_set_orthographic(projection, left, right, bottom, top, near_, far_);
    // 如果用透视视角
    // VFOV 为 45°对应的弧度，是图形学常用的"默认垂直视场角"，接近人眼/相机的自然视野，不夸张、不狭窄，看起来舒服，是大多数教程和引擎的默认值。
    // 想广角就调大，想长焦就调小。
    // mat4_set_perspective(projection, vfov_degree * (float)M_PI / 180.0f, aspect, near_, far_);

    // 上传4×4矩阵，以指针形式。上传一个指针，倒数第二个参数这里用GL_FALSE和GL_TRUE表示是否转置。
    // glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, projection);

    // 视图矩阵
    // 眼睛（相机）位置、目标中心位置、“上”方向、视角矩阵
    float eye[3] = {0.0f, 1.5f, 8.0f};
    float center[3] = {0.0f, 1.0f, 0.0f};
    // up 不能跟视线方向共线。第一人称视角里，up 通常跟随人物的"头顶方向"。
    float up[3] = {0.0f, 1.0f, 0.0f};
    float view[16];
    mat4_set_look_at(view, eye, center, up);
    // glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, view);

    float projection_view[16];
    mat4_mul(projection_view, projection, view);
    // projection 和 view 这里总是一起做，可以算好两者乘积一次性传给 GPU。
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection_view"), 1, GL_FALSE, projection_view);

    // 棍子长度、半径、球长度在前面定义的全局变量

    // 下面要做实例化和局部→世界的变换。

    // 变换顺序建议先分别scale，接着分别rotate到相对方向正确，然后分别translate到相对位置正确，（这之后可以整体scale），
    // 接下来整体rotate，最后整体translate。

    // 注意法线和坐标变换方式不同。我们假设用 3×3 矩阵 M 来变换坐标，N 来变换法线。那么点 p 变成 M @ p，点 p 附近表面如果有一个非常近的点 q，会被变换成 M @ q。
    // 两者作差也是线性的，只要让两者足够近，我们就可以推出每个点附近的切线 t 应该被变换到 M @ t。
    // 假设 t 处法线是 n，那么有 n.T @ t = 0，(N @ n).T @ (M @ t) = 0。后者即 n.T @ (N.T @ M) @ t = 0。为了对任意点 t 和对应的 n 都成立，应该让
    // N.T @ M = I，解得 N = M^(-1).T。
    // 如果用 4×4 矩阵去变换，由于model不涉及非仿射投影，最后一行一定是(0,0,0,1)，可以不要。法线平移不影响方向，所以平移部分可以不要。
    // 因此我们只要一个 4×4 矩阵的左上角。model 只涉及 T R S 操作。我们假设所有平移的总效果是 t，所有旋转总效果是 R，所有缩放总效果是 S。这是可以的，因为缩放不改变平移，
    // 而 R @ t 只是把平移矢量从 t 变成 R @ t。我们只要保证最后当做 T @ R @ S顺序操作即可。此时矩阵最后一行是 (0, 0, 0, 1)，左上角是 R @ S，右上角是 t。
    // 我们用 O 代表 R @ S，注意 S 是对角矩阵，那么矩阵为
    // O t
    // 0 1
    // 设其逆为
    // A B
    // C D
    // ，则乘法结果为 I，即
    // O @ A + t @ C & O @ B + t D
    // C             & D
    // 从而 C = (0, 0, 0)，D=1，代入得到
    // O @ A = I
    // O @ B + t = (0, 0, 0)^\top
    // 因此 我们想要的矩阵是A B C D 构成矩阵的转置的左上角 A^\top 就是 O^(-1)^\top，而
    // O^(-1)^\top = (R @ S)^(-1)^\top = (S^(-1) @ R^(-1))^\top = R^(-\top) @ S^(-1) = R @ S^(-1)
    // 这里利用了 R 是酉矩阵，而 S 只是对角矩阵，求逆只需要把每个对角元求倒数即可，R @ S^(-1) 相当于把 R 的每列除以 S 对应位置的对角元。
    // 这就是 mat3_append_inverse_scale 干的事。

    // ========== 整体变换（棍子和球共享） ==========
    // 从物体的局部空间坐标开始变换
    // 按照 model = T @ R @ S 顺序乘法。
    // 为了方便算 model 矩阵左上角3 * 3的逆，我们先把scale以外的部分乘起来，最后乘scale。

    // model 矩阵和 model3_inv_transpose 矩阵，每个球/圆柱实例用相同的内存。
    float model[16], model3_inv_transpose[9], scale[3];
    float T_half_cylinder[16];

    // ========== 绘制化学键 ==========
    // 绑定圆柱的 VAO
    // 这里绑定是为了使用配置。
    glBindVertexArray(cylinderVAO);

    float bond_vector[3];
    float bond_length;
    float axis[3];
    float translation[3];
    const float cylinder_height_orientation[3] = {0.f, 1.f, 0.f};
    for (unsigned int ibond = 0u; ibond < mol->nbonds; ++ ibond) {
        unsigned int iatom = mol->bonds_indices[ibond * 2];
        unsigned int jatom = mol->bonds_indices[ibond * 2 + 1];
        vec3_sub(bond_vector, atom_coord(mol, jatom), atom_coord(mol, iatom));
        bond_length = vec3_length(bond_vector);
        scale[0] = scale[2] = STICK_RADIUS; scale[1] = bond_length;

        // 用叉乘计算旋转
        // bond_vector 和 y 几乎共线时候有数值问题，而且有退化情况，必须单独处理。
        // vec3_cross(axis, cylinder_height_orientation, bond_vector);
        // if (vec3_length(axis) < 1e-6f) {      // 键∥Y
        //     if (bond_vector[1] > 0.f)
        //         mat4_identity(model);                        // d=+Y：无需旋转
        //     else
        //         mat4_set_rotation_x(model, (float)M_PI);     // d=-Y：绕 X 转 180°
        // } else {
        //     vec3_normalize(axis);
        //     mat4_set_rotation_by_cos_angle(model, axis,
        //         vec3_included_angle_cos(cylinder_height_orientation, bond_vector));
        // }

        // 另一种旋转计算方法，不用算叉乘和夹角，数值上更友好，即使近退化情况，只要不是严格退化，尽管转轴是错的，结果依然是对的。
        // axis 选取目标方向和 y 轴方向的平均值的一半，转 180° 即可。感觉比叉乘的方法更省事。
        // bond_vector ≈ -y 时候有退化情况，要单独处理。
        vec3_copy(axis, bond_vector);
        axis[1] += bond_length;               // = bond_length * (d + Y)
        if (vec3_length(axis) < 1e-6f) {      // d ≈ -Y 的退化情形
            mat4_set_rotation_x(model, (float)M_PI);   // 绕 X 轴转 180°，把 +Y → -Y
        } else {
            vec3_normalize(axis);
            mat4_set_rotation_by_cos_angle(model, axis, -1.f);
        }

        // 现在圆柱顶端应该在 bond_vector 的一半的位置，但是圆柱中心要平移到 iatom 的坐标位置
        vec3_scale_inverse(bond_vector, 2.f);
        vec3_add(translation, atom_coord(mol, iatom), bond_vector);
        mat4_prepend_translation(model, translation);

        // 这里model还不包括S
        mat4_topleft_to_mat3(model3_inv_transpose, model);
        mat3_append_inverse_scale(model3_inv_transpose, scale);
        glUniformMatrix3fv(glGetUniformLocation(shaderProgram, "model3_inverse_transpose"), 1, GL_FALSE, model3_inv_transpose);
        // 缩放到正常尺寸，最后再用
        mat4_append_scale(model, scale);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, model);
        // 颜色
        glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), 0.6f, 0.6f, 0.6f);
        // GL_TRIANGLES：每 3 个索引 = 1 个三角形。
        // 第二个参数是索引的个数，即三角形个数的3倍。
        // 第三个参数是索引的数据类型（EBO 里存的是 unsigned int）。
        // 最后一个参数是从EBO的哪个偏移开始读。
        // 这里才是真实绘制，或者说实例化，用当前绑定的着色器（含所有Uniform，比如上传的model矩阵和物体颜色等信息）和当前绑定的VAO绘制一个物体。
        // 我们这里是用glUseProgram(shaderProgram)来对所有物体用了相同着色器。如果真的要不同物体用不同着色器，这里应该激活新的着色器。
        glDrawElements(GL_TRIANGLES, cylinderIdxCount, GL_UNSIGNED_INT, 0);
    }

    // 解绑圆柱的 VAO，这里不做原理上也行，而且通常这里不做解绑
    // glBindVertexArray(0);

    // ========== 绘制原子球 ==========
    // 尽管理想球体，球心在原点时候旋转不变，但是实际法线得跟着旋转，否则光线方向不对，因此还是要乘R矩阵的。
    // 唯一不受法线方向影响的是环境光。其他比如点光源、方向光、漫反射光等都受法线方向影响。
    // 绑定球的 VAO
    glBindVertexArray(sphereVAO);

    // 绘制每个原子
    for (unsigned int iatom = 0u; iatom < mol->natoms; ++ iatom) {
        mat4_set_translation(model, atom_coord(mol, iatom)[0], atom_coord(mol, iatom)[1], atom_coord(mol, iatom)[2]);
        // 这里model还不包括S
        mat3_identity(model3_inv_transpose);
        scale[0] = mol->atomic_covalence_radius[iatom] / 2.f;
        scale[1] = mol->atomic_covalence_radius[iatom] / 2.f;
        scale[2] = mol->atomic_covalence_radius[iatom] / 2.f;
        mat3_append_inverse_scale(model3_inv_transpose, scale);
        glUniformMatrix3fv(glGetUniformLocation(shaderProgram, "model3_inverse_transpose"), 1, GL_FALSE, model3_inv_transpose);
        mat4_append_scale(model, scale);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, model);
        glUniform3fv(glGetUniformLocation(shaderProgram, "objectColor"), 1, element_color(mol->atomic_indices[iatom]));
        // 实体化
        // 最后一个参数是 EBO 里的偏移
        glDrawElements(GL_TRIANGLES, sphereIdxCount, GL_UNSIGNED_INT, 0);
    }

    // 解绑球的 VAO
    // glBindVertexArray(0);

    glutSwapBuffers(); // 双缓冲模式把"后缓冲"和"前缓冲"交换
    glutPostRedisplay();  // 持续刷新，刷新率未开启垂直同步取决于GPU计算力，开启了取决于屏幕刷新率（和GPU最大计算力的最小值）
    // glUseProgram(0); // 解除激活着色器，通常没必要而且不这么做，核心模式没有着色器无法绘图。
}

int initWindow(int argc, char **argv) {
    // 先读入分子
    if (argc - 1 != 1) {
        fprintf(stdout, "Usage: \"%s\" xxx.gjf|xyz|out|fch\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    char ifilename[PATH_MAX + 1];
    ifilename[PATH_MAX] = '\0';
    strncpy(ifilename, argv[1], PATH_MAX);

    // 全局变量 mol
    mol = read_molecule(ifilename);

    get_intramolecular_connectivity(mol);

    // print_molecule(mol);
    // print_intramolecular_connectivity(mol);
    // print_box(mol);

    #ifdef HAVE_UTHASH
    destroy_elements_hash_table();
    #endif

    // 在 OpenGL 部分必须第一个被整个程序调用，初始化GLUT库本身
    glutInit(&argc, argv);

    // 配置，顺序可换，要在建立窗口前
    glutInitContextVersion(3, 3); // 请求 OpenGL 3.3
    // OpenGL 3.0 开始标记固定管线弃用，3.1 彻底移除固定管线，引发大量开发者不满
    // 3.2 妥协引入了核心模式，无固定管线，但是兼容模式仍然支持固定管线。
    // 3.3 延续双模式，GLSL 330 成为了教程标准。
    // OpenGL 3.3 是核心模式可编程管线的起点，核心模式完全移除了固定管线。
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    // GLUT_DOUBLE 是双缓冲，后缓冲一帧绘制完后和前缓冲交换，一帧完整显示
    // GLUT_RGB 是用RGB/RGBA 色彩模式，唯一合理的选择。如果要支持透明度通道，用RGBA
    // 相比于老旧早已淘汰的索引颜色模式（调色板模式，每个像素存一个调色板索引）
    glutInitContextProfile(GLUT_CORE_PROFILE);   // 核心模式，只保留可编程管线，放弃对旧的固定管线兼容
    glutInitWindowSize(winWidth, winHeight);

    glutCreateWindow(ifilename);

    // 加载 OpenGL 函数（必须在创建窗口之后，需要OpenGL上下文）
    if (!gladLoadGL()) {
        printf("Failed to initialize GLAD\n");
        return -1;
    }

    // 开启垂直同步 VSync，帧率与显示器刷新率同步，避免帧率高于显示器刷新率造成计算浪费。
    glutSwapInterval(1);

    // 后续如果要做抗锯齿，MSAA（多重采样抗锯齿）最简单，大概是创建窗口时请求多重采样缓冲：
    // glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
    // 然后启用：
    // glEnable(GL_MULTISAMPLE);
    // 可能要先设采样数：
    // glutSetOption(GLUT_MULTISAMPLE, 4);
    // 如果要做 FXAA （快速近似抗锯齿），要改着色器，通过那个灰度计算公式判断亮度，判断边缘，进行模糊化处理。
    // 这需要 FBO （Framebuffer Object，帧缓冲对象）离屏渲染和全屏四边形，以及一个专门的 FXAA 着色器。
    // 如果要做计算量最大的 SSAA （超采样抗锯齿），要以更高分辨率渲染到 FBO，再把这个高分辨率纹理缩小显示到屏幕（降采样）。

    // FBO = 一个"渲染目标容器"，它本身不存数据，只是组织多个附件（attachments）。
    // 一个 FBO 可以挂载：
    // 颜色附件（颜色缓冲）：存渲染出的颜色。
    // 深度附件（深度缓冲）：存深度值。
    // 模板附件（模板缓冲）：存模板值。
    // FBO 是"接线板"——把各种缓冲（颜色/深度/模板）接上去，渲染时写到这些缓冲。

    // FBO 的附件有两种来源，一种是 RBO（Renderbuffer Object，渲染缓冲对象）
    // RBO 是一个"只用于渲染的缓冲"，专门给 FBO 当附件用。
    // 通常用于深度缓冲（和模板缓冲），因为它们的值不需要回读（只用于测试）。
    // 不能直接采样纹理，只能渲染用。
    // 另一种是纹理，纹理也可以当 FBO 的颜色附件，这样渲染结果可以采样（用于后处理）。

    return 0;
}

// 窗口大小变化回调
void reshape(int w, int h) {
    // 防止窗口被最小化时候渲染异常和aspect的除以0风险
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    winWidth = w;
    winHeight = h;
    glViewport(0, 0, w, h); // 左下角x y坐标、宽度和高度
}

// 键盘回调（ESC 退出）
void keyboard(unsigned char key, int x, int y) {
    (void)x;
    (void)y;
    if (key == 0x1B) exit(EXIT_SUCCESS); // ESC
}
