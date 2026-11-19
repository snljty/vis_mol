#include "window.h"
#include "math_utils.h"
#include "geometry.h"
#include "shader.h"
#include "const.h"
#include "align_alloc.h"
// #include <GL/freeglut.h>
#include <stdlib.h>

// 窗口尺寸
int win_width = 1000, win_height = 800;

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

// 交互手感有两种约定

// 约定 A:抓取式 / 场景跟随光标(grab / scene-follow / drag-the-scene)
// 把鼠标"按在分子上",拖到哪里,分子就跟到哪里。
// 本质:固定相机语义,移动的是"世界"的观感(实际是反着动相机)。
// 这是 VMD、PyMOL、Blender 的默认平移/旋转手感(大家习惯的"抓住物体拽")。

// 约定 B:驱动式 / 相机跟随光标(camera-drive / camera-follow / push-the-camera)
// 是在"推相机":往右拖,相机往右走,于是场景往左走。
// 典型例子:Unity/Unreal 编辑器里某些"轨道相机"、第一人称"鼠标移动镜头"。

// 还有一组更数学的区分,常被问"哪种定义变换":

// (1) 世界坐标对齐 vs 屏幕坐标对齐
// 这里使用屏幕/相机坐标系(沿"屏幕右、屏幕上、屏幕法向"平移)。
// 也有些平移用的是世界坐标轴。
// 两种都对,但手感不同,术语上叫 "screen-space controls" vs "world-space controls"。

// (2) 矩阵组合顺序:左乘(前乘) vs 右乘(后乘)
// 相机矩阵是 view,累计增量变换是 view_new = D · view(左乘/前乘,世界帧)还是 view_new = view · D(右乘/后乘,相机帧)。
// 现在的 orbit/pan 改的是 eye/center,本质是重算 view,不直接碰矩阵组合,所以这一层暂时没踩到。

// Trackball / Arcball(轨迹球):一种具体算法,把屏幕二维拖动映射成三维旋转。它通常默认"抓取式"。你要的左右旋转本质就是简化的 trackball。
// "固定目标旋转" / "绕目标轨道":orbit 的另一个叫法。
// "相对相机" vs "相对世界":就是上面世界坐标对齐 vs 屏幕坐标对齐。

// 注意要用"固定世界轴"做增量旋转,而不是每次从当前姿态推导转轴。 要保存一个累积旋转矩阵。
// 否则会遇到万向节锁问题，旋转到某一个角度后视角不稳定，鼠标轻微移动，分子就会乱转。
// 累积旋转 = 一个矩阵 + 固定轴增量 + 相机固定。既避开了万向节锁,又天然屏幕对齐。用四元数保存累积旋转状态也是可以的，是等价的。

// 分子坐标永远不变，变的只是视角/视觉效果。
// 分子载入后会计算分子的质心和包围分子的最小球半径，然后初始视图让质心出现在屏幕正中心，并且让整个分子都能被屏幕显示下。
// 然后所有的旋转都是基于屏幕坐标系来的，通过保存累积旋转矩阵来实现。
// 光线始终是跟着视角走，保证分子从什么角度观察都不会改变光照效果。
// 平移速度是相对于屏幕像素单位的，这样保证不管分子是显得很大还是很小，视角移动速度都是合理的。

// 目前裁剪平面是动态设置的，保证分子不会被裁剪。但是如果要手动裁剪分子，可以用键盘[]{}分别把近/远裁剪平面相对自动设置的动态值，
// 做额外的拉近/推远偏移。
// 键盘 v 切换正交和透视视角。
// 鼠标滚轮负责通过修改 view_size 来实现让图像放大/缩小，透视视角的 fov 由 view_size 确定，相当于改变了焦距。分子各部分相对大小比例不变。
// 滚轮上半部分向前是推远的感觉（缩小），向后是拉近的感觉（放大），但是实际只是改了 view_size。
// 滚轮方向可以理解成就是手指往哪边移动，就是把分子看起来往哪个方向移动的“伪透视”感觉。
// 鼠标右键按住上下移动是把分子沿屏幕 z 轴（视线）方向平移（dolly），这对正交视角看上去无效，因为连裁剪平面都是动态移动的。
// 对透视视角，改变的是相机到分子的距离 Dcam，fov不变，越远越接近正交，越近近大远小越偏明显，透视关系会改变，远近处大小比例不再固定。
// 鼠标右键按住左右移动是让分子沿着屏幕 z 轴（视线方向）旋转，而且是代表让分子上半部分向左/向右转，就好像分子是躺在一个平面上，推着分子最上面往左右转的感觉。
// 鼠标左键按住左右移动是让分子绕着屏幕 y 轴（高度方向）旋转，上下移动是让分子绕着屏幕 x 轴（宽度方向）旋转。而且都是让分子靠近用户这个方向的部分往哪个方向旋转，
// 就好像用户推地球仪旋转那种感觉。
// 鼠标中键按住上下左右移动是把分子在平面内沿着对应方向平移。
// 双击鼠标中键，回到初始视角。
// 按 ESC 键退出程序。

// 全局变量：VAO、索引数量、着色器程序
// init_scene函数设置了它们的值，用来给display函数使用
static GLuint sphere_VAO, cylinder_VAO; // 球和圆柱VAO的handle编号
static int sphere_idx_count, cylinder_idx_count; // 球和圆柱的顶点个数，geometry.c里面的函数计算他们的值
static GLuint shader_program; // 着色器handle编号

// 获取着色器中全局变量编号
static GLint light_color_location;
static GLint light_dir_location;
static GLint light_dir_comp_location;
static GLint ambient_color_location;
static GLint projection_view_location;
static GLint model3_inverse_transpose_location;
static GLint model_location;
static GLint object_color_location;

// ==================== 相机控制状态 ====================
// 是否用两种颜色显示化学键
static const bool bonds_two_colors = true;
// 用 eye/center/up 维护相机位姿；分子坐标与 model 矩阵完全不动，只改视角。
static bool use_perspective = false;   // 当前是透视还是正交

// 保存的累积旋转矩阵，为了避免万向节锁问题。和最初始的旋转矩阵。
static float rot[16];                // 累积的分子旋转矩阵（单位阵初始）
static float saved_rot[16];          // 双击恢复用

// 规范缩放 = 正交半高（世界单位）。透视的 fov 由它推导，保证两种模式切换连续。
// 这里只是随便设一个初值，实际上第一个有意义的值是 init_scene 函数中，通过分子尺寸计算出来的。
static float view_size = 6.0f;

// 可调裁剪平面相对动态基准的裁剪面偏移（0 = 正好包住分子）（近/远裁剪面默认值）。
static float clip_near_offset = 0.0f;
static float clip_far_offset  = 0.0f;

// 也有人喜欢一个初始的经典"俯视 3/4 视角"，让分子看起来更立体、更有"球感"。这时候可以把eye的y设成1.5f左右。
// 相机，目标中心位置、视角的“上”
static float eye[3]    = {0.0f, 0.0f, 8.0f};
static float center[3] = {0.0f, 0.0f, 0.0f};
static float up[3]     = {0.0f, 1.0f, 0.0f};

// 分子质心（加载后只算一次，坐标不变）
static float mol_center[3] = {0.f, 0.f, 0.f};

// 分子包围球半径，加载后算一次
static float mol_radius = 1.0f;

// 初始视角存档（双击中键恢复
static float saved_eye[3], saved_center[3], saved_up[3];
static float saved_view_size;
static bool saved_use_perspective;

// 裁剪平面距离偏移存档 (near/far 相对动态基准)
static float saved_clip_near_offset;
static float saved_clip_far_offset;

// 拖动状态
enum { DRAG_NONE = -1 };
static int drag_button = DRAG_NONE; // 所有按键都是自然数，这里用负数代表没有拖拽。
static int last_mouse_x = 0, last_mouse_y = 0;

// 双击检测（毫秒）
static const long DOUBLE_CLICK_MS = 250;
// 其实设为 -DOUBLE_CLICK_MS - 1 即可，但是 const 不是 C++ 的 constexpr，
// C 标准文件作用域的 static 变量初始化必须是常量表达式，而常量表达式不包括 const 限定变量的。
// GCC 拓展没有问题，但是严格来说这是不可移植的。
static long last_middle_click_ms = -250 - 1;

// 灵敏度
static const float ROT_SPEED   =  0.01f;   // 每像素弧度（x/y旋转）
static const float ROLL_SPEED  =  0.01f;   // 每像素弧度（绕视线方向滚转）
static const float ZOOM_FACTOR =  0.9f;    // 每次滚轮 view_size 乘的系数
static const float PAN_SPEED   =  2.0f;    // 每像素世界单位（平移）

// 显示的原子球半径是原子共价半径多少倍
static const float ATOMIC_RADIUS_DISPLAY_SCALE = 0.5f;

// 创建 VAO（包含 VBO 和 EBO）
GLuint create_VAO(float *vertices, int vertex_count,
                       GLuint *indices, int index_count) {
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
    // GL_DYNAMIC_DRAW： 内容会被频繁修改（每帧更新顶点）
    // GL_STREAM_DRAW：几乎每次绘制都变（每帧完全重写流式数据）

    // 上传顶点数据到 VBO
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertex_count * 6 * sizeof(float),
                 vertices, GL_STATIC_DRAW);

    // 上传索引数据到 EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_count * sizeof(GLuint),
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

// 计算分子质心（只调用一次，分子坐标加载后不变）
static void compute_mol_center() {
    memset(mol_center, 0, 3 * sizeof(float));
    if (mol->natoms == 0) return;
    float mass_weighted_coord[3];
    float tot_mass = 0.f;
    float current_mass;
    for (unsigned int i = 0u; i < mol->natoms; ++ i) {
        current_mass = mol->atomic_weights[i];
        vec3_copy(mass_weighted_coord, atom_coord(mol, i));
        vec3_scale(mass_weighted_coord, current_mass);
        vec3_add_inplace(mol_center, mass_weighted_coord);
        tot_mass += current_mass;
    }
    vec3_scale_inverse(mol_center, tot_mass);
}

// 计算分子的包围球半径（到质心的最大距离，要考虑到原子球也有半径；只算一次）
static float compute_mol_radius() {
    if (mol->natoms == 0) return 1.0f;
    float max_r = 0.f;
    float delta[3];
    float r;
    for (unsigned int i = 0u; i < mol->natoms; ++ i) {
        vec3_sub(delta, atom_coord(mol, i), mol_center);
        r = vec3_length(delta) + mol->atomic_covalence_radius[i] * ATOMIC_RADIUS_DISPLAY_SCALE;
        if (r > max_r) max_r = r;
    }
    if (max_r < 1E-6f) max_r = 1.0f; // 退化时的兜底
    return max_r;
}

// 之前有static void orbit_camera(const float *axis, float angle);和static void roll_camera(float angle);函数，
// 用来让相机绕着分子转。前者是把 eye 绕 center 旋转：轴是世界坐标单位向量 axis，angle 弧度；后者是绕视线方向滚动相机：保持 eye/center 不动，只旋转 up 向量。
// 但是现在实现是让累计旋转矩阵作用于分子，显示的是让 view 矩阵直接整体转动，所以不需要了。

// 把 eye 和 center 一起平移（整个视图平移，不改变眼距 D）
static void pan_camera(const float *pan) {
    vec3_add_inplace(eye, pan);
    vec3_add_inplace(center, pan);
}

// 保存初始视角（供双击恢复）
static void save_initial_view() {
    vec3_copy(saved_eye, eye);
    vec3_copy(saved_center, center);
    vec3_copy(saved_up, up);
    saved_view_size = view_size;
    saved_use_perspective = use_perspective;
    saved_clip_near_offset = clip_near_offset;
    saved_clip_far_offset  = clip_far_offset;
    mat4_copy(saved_rot, rot);
}

// 恢复初始视角（双击恢复）
static void restore_initial_view() {
    vec3_copy(eye, saved_eye);
    vec3_copy(center, saved_center);
    vec3_copy(up, saved_up);
    view_size = saved_view_size;
    use_perspective = saved_use_perspective;
    clip_near_offset = saved_clip_near_offset;
    clip_far_offset  = saved_clip_far_offset;
    mat4_copy(rot, saved_rot);
}

// 滚轮：统一改规范 view_size（正交、透视都由此推导投影）
static void mouse_wheel_func(int wheel, int direction, int x, int y) {
    (void)wheel; // 滚轮编号，大多数鼠标只有1个滚轮，则这个参数一定是0。
    // x 和 y 是此时鼠标指针坐标
    (void)x;
    (void)y;
    // direction 是+1代表向上滚（远离用户），-1代表向下滚（靠近用户）。
    if (direction == 0) return;
    if (direction > 0) {
        view_size /= ZOOM_FACTOR;   // 滚上=缩小
    } else {
        view_size *= ZOOM_FACTOR;   // 滚下=放大
    }
    // 通知 GLUT "窗口内容过时了，下次空闲时请重新调用一次显示回调（display）重绘"。
    // 窗口默认不是"每帧都自动重画"的。
    // 只有发生事件（窗口大小改变、被遮挡后重新暴露、你显式请求重绘等）时，GLUT 才会调用 glutDisplayFunc 注册的 display()。
    glutPostRedisplay();
}

// 鼠标按键 + 滚轮回调
static void mouse_func(int button, int state, int x, int y) {
    // 滚轮已由 mouse_wheel_func 单独处理，这里不碰 GLUT_WHEEL_*

    if (state == GLUT_DOWN) {
        // 中键双击检测（左右键双击暂时无视）
        if (button == GLUT_MIDDLE_BUTTON) {
            // GLUT_ELAPSED_TIME 是程序启动到现在时间，单位毫秒。
            long now = (long)glutGet(GLUT_ELAPSED_TIME);
            if (now - last_middle_click_ms < DOUBLE_CLICK_MS) {
                restore_initial_view();
                drag_button = DRAG_NONE;   // 双击不作为拖动起点
                last_middle_click_ms = -DOUBLE_CLICK_MS - 1;
                glutPostRedisplay();
                return;
            }
            last_middle_click_ms = now;
        } else {
            // 任何非中键按下，都打断"中键双击"序列
            last_middle_click_ms = -DOUBLE_CLICK_MS - 1;
        }

        drag_button = button;
        last_mouse_x = x;
        last_mouse_y = y;
    } else {
        drag_button = DRAG_NONE;
    }
}

// 按住拖动回调：左=绕X/Y，中=沿X/Y平移，右=绕Z + 沿Z平移。一律让分子“跟手”（抓分子约定）
static void motion_func(int x, int y) {
    if (drag_button == DRAG_NONE) return; // 没有拖拽
    int dx = x - last_mouse_x;
    int dy = y - last_mouse_y;
    last_mouse_x = x;
    last_mouse_y = y;

    float r[16];

    // 每像素对应的世界单位：把“整个视野高度 = 2*view_size”摊到 win_height 个像素上。
    // 这样平移速度与分子大小、缩放、窗口尺寸都自洽，永远“跟手”。
    // 正交和透视（中心平面大小恒为 view_size）都适用。
    // pan 这个词来自 panorama，电影行业 pan the camera 代表水平摇镜头。这里指平移。
    float pan_step = PAN_SPEED * view_size / (float)win_height;

    // 相机/屏幕基向量（相机固定故恒定）
    // forward = 屏幕"深处"（eye 看向 center）；right = 屏幕"水平向右"；upv = 屏幕"竖直向上"
    float forward[3], right[3], upv[3];
    vec3_sub(forward, center, eye);
    vec3_normalize(forward);
    vec3_cross(right, forward, up);
    vec3_normalize(right);
    vec3_cross(upv, right, forward);

    // 只给平移用
    float pan[3];
    float pan_x[3], pan_y[3];

    switch (drag_button) {
    case GLUT_LEFT_BUTTON:
        // 视图空间轨道旋转，抓近表面（近侧基准）：前侧跟手
        // 水平拖→绕屏幕"竖直轴" upv：往右拖(dx>0)→前侧向右
        mat4_set_rotation(r, upv, (float)dx * ROT_SPEED);
        mat4_lmul_inplace(rot, r);               // rot = R_upv * rot
        // 垂直拖→绕屏幕"水平轴" right：要跟手必须取 +dy（不是 -dy）
        // 因为绕屏幕+right 正向旋转会让前侧向下，往下拖(dy>0)正好要前侧向下
        mat4_set_rotation(r, right, (float)dy * ROT_SPEED);
        mat4_lmul_inplace(rot, r);               // rot = R_right * rot
        break;
    case GLUT_MIDDLE_BUTTON:
        // 屏幕平面内平移（把分子当物体往屏幕上拖）
        // 视线沿 -right 平移 ⇒ 分子向右移；视线沿 +upv 平移 ⇒ 分子向下移
        // pan = (right * (-dx) + upv * (dy)) * pan_step
        vec3_copy(pan_x, right);
        vec3_copy(pan_y, upv);
        vec3_scale(pan_x, -(float)dx);
        vec3_scale(pan_y,  (float)dy);
        vec3_add(pan, pan_x, pan_y);
        vec3_scale(pan, pan_step);
        pan_camera(pan);
        break;
    case GLUT_RIGHT_BUTTON:
        // 滚转：绕屏幕"深处"轴 forward
        mat4_set_rotation(r, forward, (float)dx * ROLL_SPEED);
        mat4_lmul_inplace(rot, r);               // rot = R_forward * rot
        // 沿屏幕"深处"（forward，正视线的方向）平移近远面。对正交效果没有影响。
        float depth_pan = (float)dy * pan_step;
        // dolly：带安全下限，避免相机推进/穿过分子
        float cand_eye[3];
        vec3_copy(cand_eye, forward);
        vec3_scale(cand_eye, depth_pan);
        vec3_add_inplace(cand_eye, eye);
        float dcm_c[3];
        vec3_sub(dcm_c, cand_eye, mol_center);
        float Dc = vec3_length(dcm_c);
        // 最小允许距离：比动态裁剪的 margin 再留一点余量
        float min_D = 1.2f * (1.5f * mol_radius);
        if (Dc >= min_D) {        
            vec3_copy(pan, forward);
            vec3_scale(pan, depth_pan);
            pan_camera(pan);
        }
        // 否则忽略这次推进（已经太近）
        break;
    default:
        return;
        break;
    }
    glutPostRedisplay();
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
    glUseProgram(shader_program);

    // 光照参数
    // 把光照参数传给着色器这个 uniform
    // 方向光方向
    // 相机空间恒定的光方向（始终从屏幕左上方打来）
    // 之前版本：
    // headlight：光固定在视图/屏幕空间。相机固定 → 世界恒定方向，指向相机（让正视面受光）
    // 由于人的认知习惯，最强光线永远是从上方而非下方来的（日光），如果反过来就会产生物体里外面反转的错觉，所以稍微抬高方向光。
    float lightDir[3];
    vec3_sub(lightDir, eye, center);   // eye - center，从分子指向相机
    vec3_normalize(lightDir);
    lightDir[1] += 0.3f;               // 抬高：让光来自斜上方
    vec3_normalize(lightDir);

    // uniform 是 CPU 传给 GPU 的全局只读变量。着色器对其只读，所有顶点、片元共享，可以每帧更新。
    // glUniform3fv 和 glUniform3f 都是传3个float给uniform，区别是3fv传指针(loc, count, ptr)，3f传值(loc, x, y, z)。
    // 方向光，传的光方向。这部分跟着色器代码绑定，比如如果用点光源，要传光的位置。如果要修改光的类型或者数量（叠加效果），要改着色器代码。
    glUniform3f(light_color_location, 1.0f, 1.0f, 1.0f); // 纯白，这三个数代表光颜色三个分量的强度。

    // 主光
    glUniform3fv(light_dir_location, 1, lightDir);

    // 补光：方向大致与主光相反，照背面
    float lightDirComp[3];
    vec3_opposite(lightDirComp, lightDir);
    glUniform3fv(light_dir_comp_location, 1, lightDirComp);

    glUniform3f(ambient_color_location, 0.2f, 0.2f, 0.2f); // 环境光，很暗，通常只需要一个

    // M V P 矩阵上传顺序无所谓，这只是习惯。

    // 投影矩阵（由规范 view_size 推导，正交/透视二选一）
    float projection[16];
    float aspect = (float)win_width / (float)win_height;

    // 动态裁剪面：始终把分子包在 near/far 之间，Z 平移不会再剪掉分子
    float dcm[3]; // 分子中心指向相机
    vec3_sub(dcm, eye, mol_center);
    float Dcam = vec3_length(dcm);
    float margin = 1.5f * mol_radius;              // 包围余量

    // 考虑键盘[]{}设置的偏移
    float near_ = fmaxf(        0.001f, Dcam - margin + clip_near_offset);
    float far_  = fmaxf(near_ + 0.001f, Dcam + margin + clip_far_offset); // far 是动态调节，只要 clip_far_offset 不太离谱，不太用考虑上溢问题。

    if (use_perspective) {
        // 透视：fov = 2·atan(view_size / D)，D 为眼距。中心平面大小恒等于 view_size
        // 之前版本 VFOV 为 45°对应的弧度，是图形学常用的"默认垂直视场角"，接近人眼/相机的自然视野，不夸张、不狭窄，看起来舒服，是大多数教程和引擎的默认值。
        // 但是现在 fov 基于 view_size 和 D 推导，view_size 由滚轮控制，已经唯一确定了，所以不设定固定值。
        // 想广角就调大 fov，想长焦就调小 fov。
        float d[3];
        vec3_sub(d, eye, center);
        float D = vec3_length(d);
        if (D < 1e-6f) D = 1.0f;   // 防御：避免除以零
        float fov = 2.f * atanf(view_size / D);
        mat4_set_perspective(projection, fov, aspect, near_, far_);
        // mat4_set_perspective 第二参是弧度，见 math_utils.c
    } else {
        // 正交：直接用 view_size 作为半高
        float left   = -view_size * aspect;
        float right  =  view_size * aspect;
        float bottom = -view_size;
        float top    =  view_size;
        mat4_set_orthographic(projection, left, right, bottom, top, near_, far_);
    }

    // 视图矩阵（相机位姿）
    float view[16];

    // 先把质心平移到坐标原点，转完了再平移回去，这样有种让分子绕着自身轴转的感觉。
    // view = look_at @ T(center) @ rot @ T(−center)
    mat4_set_look_at(view, eye, center, up);
    float T1[16], T2[16], M[16];
    mat4_set_translation(T1, -mol_center[0], -mol_center[1], -mol_center[2]);
    mat4_set_translation(T2,  mol_center[0],  mol_center[1],  mol_center[2]);
    mat4_mul(M, T2, rot);        // M = T(center) · rot
    mat4_mul(M, M, T1);          // M = T(center) · rot · T(−center)
    mat4_rmul_inplace(view, M);  // view = look_at · M

    // 合并上传
    float projection_view[16];
    mat4_mul(projection_view, projection, view);
    // 上传4×4矩阵，以指针形式。上传一个指针，倒数第二个参数这里用GL_FALSE和GL_TRUE表示是否转置。
    // projection 和 view 这里总是一起做，可以算好两者乘积一次性传给 GPU。
    glUniformMatrix4fv(projection_view_location, 1, GL_FALSE, projection_view);

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

    // model 矩阵和 model3_inverse_transpose 矩阵，以及 scale 向量，，每个球/圆柱实例用相同的内存。
    float model[16], model3_inverse_transpose[9], scale[3];
    // 加入分子整体旋转 rot 的 3×3 左上角，用来把旋转算进法线
    float rot3[9];
    mat4_topleft_to_mat3(rot3, rot);

    // ========== 绘制化学键 ==========
    // 绑定圆柱的 VAO
    // 这里绑定是为了使用配置。
    glBindVertexArray(cylinder_VAO);

    float bond_vector[3];
    float bond_length;
    float axis[3];
    float translation[3];
    float axis_length;
    float shift[3]; // 圆柱从中心在原点，到一端在原点的平移
    // const float cylinder_height_orientation[3] = {0.f, 1.f, 0.f}; // 只对叉乘算法有用
    if (bonds_two_colors) {
        float bond_vector_i[3], bond_vector_j[3];
        float bond_length_i, bond_length_j;
        float proportion;
        for (unsigned int ibond = 0u; ibond < mol->nbonds; ++ ibond) {
            unsigned int iatom = mol->bonds_indices[ibond * 2];
            unsigned int jatom = mol->bonds_indices[ibond * 2 + 1];
            vec3_sub(bond_vector, atom_coord(mol, jatom), atom_coord(mol, iatom));
            bond_length = vec3_length(bond_vector);
            proportion = mol->atomic_covalence_radius[iatom] / (mol->atomic_covalence_radius[iatom] + mol->atomic_covalence_radius[jatom]);
            vec3_copy(bond_vector_i, bond_vector);
            vec3_scale(bond_vector_i, proportion);
            vec3_copy(bond_vector_j, bond_vector);
            vec3_scale(bond_vector_j, 1.f - proportion);
            bond_length_i = bond_length * proportion;
            bond_length_j = bond_length * (1.f - proportion);
            scale[0] = scale[2] = STICK_RADIUS;

            // 先画 i 那一半
            scale[1] = bond_length_i;
            // 用叉乘计算旋转
            // bond_vector 和 y 几乎共线时候有数值问题，而且有退化情况，必须单独处理。
            // vec3_cross(axis, cylinder_height_orientation, bond_vector_i);
            // axis_length = vec3_length(axis);
            // if (axis_length < 1e-6f) {      // 键∥Y
            //     if (bond_vector_i[1] >= 0.f)
            //         mat4_identity(model);                        // d=+Y：无需旋转
            //     else
            //         mat4_set_rotation_x(model, (float)M_PI);     // d=-Y：绕 X 转 180°
            // } else {
            //     vec3_scale_inverse(axis, axis_length);
            //     mat4_set_rotation_by_cos_angle(model, axis,
            //         vec3_included_angle_cos(cylinder_height_orientation, bond_vector_i));
            // }

            // 另一种旋转计算方法，不用算叉乘和夹角，数值上更友好，即使近退化情况，只要不是严格退化，尽管转轴是错的，结果依然是对的。
            // axis 选取目标方向和 y 轴方向的平均值的一半，转 180° 即可。感觉比叉乘的方法更省事。
            // bond_vector ≈ -y 时候有退化情况，要单独处理。
            vec3_copy(axis, bond_vector_i);
            axis[1] += bond_length_i;               // = bond_length * (d + Y)
            axis_length = vec3_length(axis);
            if (axis_length < 1e-6f) {      // d ≈ -Y 的退化情形
                mat4_set_rotation_x(model, (float)M_PI);   // 绕 X 轴转 180°，把 +Y → -Y
            } else {
                vec3_scale_inverse(axis, axis_length);
                mat4_set_rotation_by_cos_angle(model, axis, -1.f);
            }

            // 现在圆柱顶端应该在 bond_vector 的一半的位置，但是圆柱底端要平移到 iatom 的坐标位置
            vec3_copy(shift, bond_vector_i);
            vec3_scale_inverse(shift, 2.f);
            vec3_add(translation, atom_coord(mol, iatom), shift);
            mat4_prepend_translation(model, translation);

            // 这里model还不包括S
            mat4_topleft_to_mat3(model3_inverse_transpose, model);
            mat3_append_inverse_scale(model3_inverse_transpose, scale);
            mat3_lmul_inplace(model3_inverse_transpose, rot3); // 把分子整体旋转算进法线，光跟视角走
            glUniformMatrix3fv(model3_inverse_transpose_location, 1, GL_FALSE, model3_inverse_transpose);
            // 缩放到正常尺寸，最后再用
            mat4_append_scale(model, scale);
            glUniformMatrix4fv(model_location, 1, GL_FALSE, model);
            // 颜色
            glUniform3fv(object_color_location, 1, element_color(mol->atomic_indices[iatom]));
            // GL_TRIANGLES：每 3 个索引 = 1 个三角形。
            // 第二个参数是索引的个数，即三角形个数的3倍。
            // 第三个参数是索引的数据类型（EBO 里存的是 unsigned int）。
            // 最后一个参数是从EBO的哪个偏移开始读。
            // 这里才是真实绘制，或者说实例化，用当前绑定的着色器（含所有Uniform，比如上传的model矩阵和物体颜色等信息）和当前绑定的VAO绘制一个物体。
            // 我们这里是用glUseProgram(shader_program)来对所有物体用了相同着色器。如果真的要不同物体用不同着色器，这里应该激活新的着色器。
            glDrawElements(GL_TRIANGLES, cylinder_idx_count, GL_UNSIGNED_INT, 0);

            // 再画 j 那一半
            scale[1] = bond_length_j;
            // 用叉乘计算旋转
            // bond_vector 和 y 几乎共线时候有数值问题，而且有退化情况，必须单独处理。
            // vec3_cross(axis, cylinder_height_orientation, bond_vector_i);
            // axis_length = vec3_length(axis);
            // if (axis_length < 1e-6f) {      // 键∥Y
            //     if (bond_vector_j[1] >= 0.f)
            //         mat4_identity(model);                        // d=+Y：无需旋转
            //     else
            //         mat4_set_rotation_x(model, (float)M_PI);     // d=-Y：绕 X 转 180°
            // } else {
            //     vec3_scale_inverse(axis, axis_length);
            //     mat4_set_rotation_by_cos_angle(model, axis,
            //         vec3_included_angle_cos(cylinder_height_orientation, bond_vector_i));
            // }

            // 另一种旋转计算方法，不用算叉乘和夹角，数值上更友好，即使近退化情况，只要不是严格退化，尽管转轴是错的，结果依然是对的。
            // axis 选取目标方向和 y 轴方向的平均值的一半，转 180° 即可。感觉比叉乘的方法更省事。
            // bond_vector ≈ -y 时候有退化情况，要单独处理。
            vec3_copy(axis, bond_vector_j);
            axis[1] += bond_length_j;               // = bond_length * (d + Y)
            axis_length = vec3_length(axis);
            if (axis_length < 1e-6f) {      // d ≈ -Y 的退化情形
                mat4_set_rotation_x(model, (float)M_PI);   // 绕 X 轴转 180°，把 +Y → -Y
            } else {
                vec3_scale_inverse(axis, axis_length);
                mat4_set_rotation_by_cos_angle(model, axis, -1.f);
            }

            // 现在圆柱顶端应该在 bond_vector 的一半的位置，但是圆柱顶端要平移到 jatom 的坐标位置
            vec3_copy(shift, bond_vector_j);
            vec3_scale_inverse(shift, 2.f);
            vec3_sub(translation, atom_coord(mol, jatom), shift);
            mat4_prepend_translation(model, translation);

            // 这里model还不包括S
            mat4_topleft_to_mat3(model3_inverse_transpose, model);
            mat3_append_inverse_scale(model3_inverse_transpose, scale);
            mat3_lmul_inplace(model3_inverse_transpose, rot3); // 把分子整体旋转算进法线，光跟视角走
            glUniformMatrix3fv(model3_inverse_transpose_location, 1, GL_FALSE, model3_inverse_transpose);
            // 缩放到正常尺寸，最后再用
            mat4_append_scale(model, scale);
            glUniformMatrix4fv(model_location, 1, GL_FALSE, model);
            // 颜色
            glUniform3fv(object_color_location, 1, element_color(mol->atomic_indices[jatom]));
            // GL_TRIANGLES：每 3 个索引 = 1 个三角形。
            // 第二个参数是索引的个数，即三角形个数的3倍。
            // 第三个参数是索引的数据类型（EBO 里存的是 unsigned int）。
            // 最后一个参数是从EBO的哪个偏移开始读。
            // 这里才是真实绘制，或者说实例化，用当前绑定的着色器（含所有Uniform，比如上传的model矩阵和物体颜色等信息）和当前绑定的VAO绘制一个物体。
            // 我们这里是用glUseProgram(shader_program)来对所有物体用了相同着色器。如果真的要不同物体用不同着色器，这里应该激活新的着色器。
            glDrawElements(GL_TRIANGLES, cylinder_idx_count, GL_UNSIGNED_INT, 0);
        }
    } else {
        for (unsigned int ibond = 0u; ibond < mol->nbonds; ++ ibond) {
            unsigned int iatom = mol->bonds_indices[ibond * 2];
            unsigned int jatom = mol->bonds_indices[ibond * 2 + 1];
            vec3_sub(bond_vector, atom_coord(mol, jatom), atom_coord(mol, iatom));
            bond_length = vec3_length(bond_vector);
            scale[0] = scale[2] = STICK_RADIUS; scale[1] = bond_length;

            // 用叉乘计算旋转
            // bond_vector 和 y 几乎共线时候有数值问题，而且有退化情况，必须单独处理。
            // vec3_cross(axis, cylinder_height_orientation, bond_vector);
            // axis_length = vec3_length(axis);
            // if (axis_length < 1e-6f) {      // 键∥Y
            //     if (bond_vector[1] >= 0.f)
            //         mat4_identity(model);                        // d=+Y：无需旋转
            //     else
            //         mat4_set_rotation_x(model, (float)M_PI);     // d=-Y：绕 X 转 180°
            // } else {
            //     vec3_scale_inverse(axis, axis_length);
            //     mat4_set_rotation_by_cos_angle(model, axis,
            //         vec3_included_angle_cos(cylinder_height_orientation, bond_vector));
            // }

            // 另一种旋转计算方法，不用算叉乘和夹角，数值上更友好，即使近退化情况，只要不是严格退化，尽管转轴是错的，结果依然是对的。
            // axis 选取目标方向和 y 轴方向的平均值的一半，转 180° 即可。感觉比叉乘的方法更省事。
            // bond_vector ≈ -y 时候有退化情况，要单独处理。
            vec3_copy(axis, bond_vector);
            axis[1] += bond_length;               // = bond_length * (d + Y)
            axis_length = vec3_length(axis);
            if (axis_length < 1e-6f) {      // d ≈ -Y 的退化情形
                mat4_set_rotation_x(model, (float)M_PI);   // 绕 X 轴转 180°，把 +Y → -Y
            } else {
                vec3_scale_inverse(axis, axis_length);
                mat4_set_rotation_by_cos_angle(model, axis, -1.f);
            }

            // 现在圆柱顶端应该在 bond_vector 的一半的位置，但是圆柱底端要平移到 iatom 的坐标位置
            vec3_copy(shift, bond_vector);
            vec3_scale_inverse(shift, 2.f);
            vec3_add(translation, atom_coord(mol, iatom), shift);
            mat4_prepend_translation(model, translation);

            // 这里model还不包括S
            mat4_topleft_to_mat3(model3_inverse_transpose, model);
            mat3_append_inverse_scale(model3_inverse_transpose, scale);
            mat3_lmul_inplace(model3_inverse_transpose, rot3); // 把分子整体旋转算进法线，光跟视角走
            glUniformMatrix3fv(model3_inverse_transpose_location, 1, GL_FALSE, model3_inverse_transpose);
            // 缩放到正常尺寸，最后再用
            mat4_append_scale(model, scale);
            glUniformMatrix4fv(model_location, 1, GL_FALSE, model);
            // 颜色，这里固定灰色
            glUniform3f(object_color_location, 0.6f, 0.6f, 0.6f);
            // GL_TRIANGLES：每 3 个索引 = 1 个三角形。
            // 第二个参数是索引的个数，即三角形个数的3倍。
            // 第三个参数是索引的数据类型（EBO 里存的是 unsigned int）。
            // 最后一个参数是从EBO的哪个偏移开始读。
            // 这里才是真实绘制，或者说实例化，用当前绑定的着色器（含所有Uniform，比如上传的model矩阵和物体颜色等信息）和当前绑定的VAO绘制一个物体。
            // 我们这里是用glUseProgram(shader_program)来对所有物体用了相同着色器。如果真的要不同物体用不同着色器，这里应该激活新的着色器。
            glDrawElements(GL_TRIANGLES, cylinder_idx_count, GL_UNSIGNED_INT, 0);
        }
    }

    // 解绑圆柱的 VAO，这里不做原理上也行，而且通常这里不做解绑
    // glBindVertexArray(0);

    // ========== 绘制原子球 ==========
    // 尽管理想球体，球心在原点时候旋转不变，但是实际法线得跟着旋转，否则光线方向不对，因此还是要乘R矩阵的。
    // 唯一不受法线方向影响的是环境光。其他比如点光源、方向光、漫反射光等都受法线方向影响。
    // 绑定球的 VAO
    glBindVertexArray(sphere_VAO);

    // 绘制每个原子
    for (unsigned int iatom = 0u; iatom < mol->natoms; ++ iatom) {
        mat4_set_translation(model, atom_coord(mol, iatom)[0], atom_coord(mol, iatom)[1], atom_coord(mol, iatom)[2]);
        // 这里model还不包括S
        mat3_identity(model3_inverse_transpose);
        scale[0] = scale[1] = scale[2] = mol->atomic_covalence_radius[iatom] * ATOMIC_RADIUS_DISPLAY_SCALE;
        mat3_append_inverse_scale(model3_inverse_transpose, scale);
        mat3_lmul_inplace(model3_inverse_transpose, rot3); // 把分子整体旋转算进法线，光跟视角走
        glUniformMatrix3fv(model3_inverse_transpose_location, 1, GL_FALSE, model3_inverse_transpose);
        mat4_append_scale(model, scale);
        glUniformMatrix4fv(model_location, 1, GL_FALSE, model);
        glUniform3fv(object_color_location, 1, element_color(mol->atomic_indices[iatom]));
        // 实体化
        // 最后一个参数是 EBO 里的偏移
        glDrawElements(GL_TRIANGLES, sphere_idx_count, GL_UNSIGNED_INT, 0);
    }

    // 解绑球的 VAO
    // glBindVertexArray(0);

    glutSwapBuffers(); // 双缓冲模式把"后缓冲"和"前缓冲"交换
    glutPostRedisplay();  // 持续刷新，刷新率未开启垂直同步取决于GPU计算力，开启了取决于屏幕刷新率（和GPU最大计算力的最小值）
    // glUseProgram(0); // 解除激活着色器，通常没必要而且不这么做，核心模式没有着色器无法绘图。
}

int init_window(int argc, char **argv) {
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
    if (!mol) {
        fprintf(stderr, "Error! Cannot read molecule.\n");
        exit(EXIT_FAILURE);
    }

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
    glutInitWindowSize(win_width, win_height);

    glutCreateWindow(ifilename);

    // 加载 OpenGL 函数（必须在创建窗口之后，需要OpenGL上下文）
    if (!gladLoadGL()) {
        fprintf(stderr, "Failed to initialize GLAD\n");
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

// 初始化场景（创建几何体、着色器）
void init_scene() {
    #ifdef USE_HIGH_RESOLUTION
    const int sectors = 32; // 经纬球和圆柱的经向块数
    const int stacks = 32; // 只有经纬球用的到，纬向块数
    const int num_subdivide = 3; // 正二十面体细分球（棱角球）表面细分次数。 
    #else
    const int sectors = 16;
    const int stacks = 16;
    const int num_subdivide = 2;
    #endif
    (void)stacks; // 这个变量其实用不到，我们已经不用经纬球了，但是还是留在这里，这行避免编译器 Warning

    // num_subdivide = 2 的时候三角形比上面经纬球的 sectors = 16; stacks = 16; 略少，但是更均匀，效果应该差不多。
    // 如果 sectors 和 stacks 都设高质量的 32，可以 num_subdivide 增加到 3。总之只考虑三角形个数，sectors 和 stacks 翻倍，
    // 三角形近似变成四倍；num_subdivide 增加 1，三角形个数也变成四倍。

    float *sphere_verts, *cylinder_verts;
    int sphere_vert_count, cylinder_vert_count;
    GLuint *sphere_idx, *cylinder_idx;

    // 最初版本的，经纬球
    // generate_geosphere(sectors, stacks,
    //                &sphere_verts, &sphere_vert_count,
    //                &sphere_idx, &sphere_idx_count);
    // 现在版本的，只用棱角球，一定做中点去重，看有没有 Hash 库决定是用散列函数还是排序+二分查找
    generate_icosphere(num_subdivide, 
                   &sphere_verts, &sphere_vert_count,
                   &sphere_idx, &sphere_idx_count);
    generate_cylinder(sectors,
                     &cylinder_verts, &cylinder_vert_count,
                     &cylinder_idx, &cylinder_idx_count);

    // 赋值给.h声明的两个全局变量。
    sphere_VAO = create_VAO(sphere_verts, sphere_vert_count, sphere_idx, sphere_idx_count);
    cylinder_VAO = create_VAO(cylinder_verts, cylinder_vert_count, cylinder_idx, cylinder_idx_count);

    // 数据已经上传到GPU，可以释放内存了
    my_free(sphere_verts); my_free(sphere_idx);
    sphere_verts = NULL; sphere_idx = NULL;
    my_free(cylinder_verts); my_free(cylinder_idx);
    cylinder_verts = NULL; cylinder_idx = NULL;

    // 编译并链接得到GPU的着色器程序
    shader_program = create_shader_program(vertex_shader_source, fragment_shader_source);

    // 着色器变量位置编号
    light_color_location = glGetUniformLocation(shader_program, "light_color");
    light_dir_location = glGetUniformLocation(shader_program, "light_dir");
    light_dir_comp_location = glGetUniformLocation(shader_program, "light_dir_comp");
    ambient_color_location = glGetUniformLocation(shader_program, "ambient_color");
    projection_view_location = glGetUniformLocation(shader_program, "projection_view");
    model3_inverse_transpose_location = glGetUniformLocation(shader_program, "model3_inverse_transpose");
    model_location = glGetUniformLocation(shader_program, "model");
    object_color_location = glGetUniformLocation(shader_program, "object_color");

    // 开启深度测试，正确处理遮挡关系
    glEnable(GL_DEPTH_TEST);
    // 开启表面剔除
    glEnable(GL_CULL_FACE); // glDisable来去掉表面剔除
    // glCullFace(GL_BACK); // 可以没有这行，默认剔除背面，即三角形顶点逆时针顺序时应有的样子。也可以用GL_FRONT剔除正面。

    // 让相机一开始就对准分子质心（而非世界原点），
    // 这样质心偏离原点的分子打开时也居中显示。坐标本身不动，只改视角。
    mat4_identity(rot);       // 初始的累积旋转矩阵为单位阵
    compute_mol_center();     // 分子中心只算一次

    // 依据分子尺寸初始化取景 / 相机 / 裁剪面
    mol_radius = compute_mol_radius();

    // 取景：让分子约占视野 80%（同时考虑竖直和水平方向）
    float aspect = (float)win_width / (float)win_height;
    float fit = fminf(1.0f, aspect);
    view_size = mol_radius / (fit * 0.8f);

    // 眼距：由 45° 垂直视场角反推（D 只是"透视强度"旋钮，不影响中心平面大小）
    float fov0 = 45.0f * (float)M_PI / 180.0f;
    float D0 = view_size / tanf(fov0 * 0.5f);

    // 相机对准分子质心，沿 +Z 后退 D0（坐标本身不动，只改视角）
    vec3_copy(center, mol_center);
    vec3_copy(eye, center);
    eye[2] += D0;

    // 偏移清零：默认裁剪面正好包住分子（display 里按 Dcam ± margin 动态计算）
    clip_near_offset = 0.0f;
    clip_far_offset  = 0.0f;

    save_initial_view();
}

// 窗口大小变化回调
void reshape(int w, int h) {
    // 防止窗口被最小化时候渲染异常和aspect的除以0风险
    if (w < 1) w = 1;
    if (h < 1) h = 1;

    win_width  = w;
    win_height = h;
    glViewport(0, 0, w, h); // 左下角x y坐标、宽度和高度
}

// 键盘回调（ESC 退出）
void keyboard(unsigned char key, int x, int y) {
    (void)x;
    (void)y;
    float clip_step = 0.1f * mol_radius;   // 每次调整 0.1 个分子半径
    float max_clip_far_offset; // 避免把远裁平面推的太远
    switch (key) {
    case 0x1B: exit(EXIT_SUCCESS); break;  // ESC
    case 'v': // v 切换正交/透视
        use_perspective = !use_perspective;
        glutPostRedisplay();
        break;
    case '[': // 近裁剪面偏移朝相机靠近（切得更少）
        clip_near_offset -= clip_step;
        glutPostRedisplay();
        break;
    case ']': // 近裁剪面偏移推远（切入分子看内部）
        clip_near_offset += clip_step; 
        glutPostRedisplay();
        break;
    case '{': // 远裁剪面偏移拉近
        clip_far_offset -= clip_step;
        glutPostRedisplay();
        break;
    case '}': // 远裁剪面偏移推远
        clip_far_offset += clip_step;
        max_clip_far_offset = clip_near_offset + 10.0f * mol_radius;
        if (clip_far_offset > max_clip_far_offset) clip_far_offset = max_clip_far_offset;
        glutPostRedisplay();
        break;
    default:
        break;
    }
}

// 注册回调
void register_callbacks() {
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutMouseFunc(mouse_func);
    glutMotionFunc(motion_func);
    glutMouseWheelFunc(mouse_wheel_func);
}
