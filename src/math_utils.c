#include "math_utils.h"

// 4×4 矩阵左上角的 3×3 包括了旋转和缩放（包括反射），右上角 3×1 是旋转
// 4×4矩阵左乘于向量(x, y, z, w).T,得到变换后坐标(x', y', z', w')
// (x, y, z, w).T实际在w≠0的时候，表示的是三维点(x/w, y/w, z/w)
// 左上角3×3矩阵写成正交归一且行列式为1的矩阵乘以对角阵的形式时候，这个酉矩阵就是旋转操作。
// 对角阵代表3个方向的缩放（代表反射），但是通常手性应该是不变的。
// 最右边一列前3个元素代表平移。
// 当最后一行为(0, 0, 0, 1)时，这就是一个仿射。
// 最后一行当前3个元素为0时，相当于对最后一个元素做除法，坐标整体除以最后一个元素。
// 当最后一行前3个元素不是0的时候，缩放比例和坐标位置有关，不同位置的点缩放比例不同，这相当于一个一般的射影变换。
// 如果比如最后一行是(0, 0, -1, 0), w'只依赖于-z，就有近大远小的效果。
// 当其为(a, b, c, d)并且a b c都不是0的时候，会产生复杂的透视扭曲，比如把一个正方体投影成任意斜视角的情况。a
// 当最后一行最后一列为0时，原点(0, 0, 0, 1)会被映射到w=0，透视除法会得到无穷大，某个方向的点会消失在无穷远处。
// 比如相机的位置，本来就不该被渲染，这是合理的。
// 假设最初坐标是(x, y, z, 1)，映射到(x', y', z', w')，矩阵最后一行为(a, b, c, d)，那么
// 平面ax+by+cz+1=0上面所有的点(x,y,z)都会被射影变换到无穷远从而消失，叫做消失平面。
// (a, b, c).T就是消失平面的法向量。d是小事平面的偏移，决定消失平面离原点有多远，改变射影中心/消失点的位置。
// a b c如果只有一个非0，就是沿着对应轴的方向透视。

// 我们指出几点：
// 同类型矩阵相乘仍是同类型，即R1 @ R2 = R, T1 @ T2 = T, S1 @ S2 = S。
// T @ R 其实就是 T + R，这也容易证明。
// 然后容易证明 R @ S 依然是左上角矩阵（右下角为1），是把左上角每列依次乘以对角元s1 s2 s3。
// 如果上述R加上了T，是同样的变换方式。
// 据此可以简化一些运算，但是要小心。

// ==================== 矩阵工具函数 ====================

// OpenGL 坐标变换总共有5步,从局部坐标到屏幕坐标
// 1. 局部空间坐标（物体自身的坐标，不是实际大小和位置）到世界空间坐标，用Model矩阵做，通常是 T @ R @ S
// 2. 世界空间坐标（物体放到整个场景中的位置、大小、朝向）到观察空间坐标，用View矩阵做，下文的mat4_set_look_at就是干这个用的
// 3. 观察空间坐标（整个世界以相机为原点重新表示）到裁剪空间坐标，用Projection矩阵做透视或者正交变换（这一步w分量可能不是1）
// 前面三步矩阵有时候合为一步叫MVP = P @ V @ M
// 4. 做裁剪测试，只保留可见范围内 -w <= x, y, z <= w
// 对于一个三角形，如果有且只有一部分顶点通过了裁剪测试，OpenGL会自动找三角形被截断边和边界的交点，构建一个新的三角形或者两个新的三角形拼成四边形
// 5. 裁剪后的裁剪空间坐标（判断顶点是否在可见范围内）到归一化设备坐标(NDC)，用透视除法做，GPU 会自动执行透视除法
// ndc_{x,y,z} = clip_{x,y,z} / clip_w
// NDC（归一化设备坐标）的范围是 [-1, 1]，最终映射到屏幕像素
// OpenGL 约定最近为ndc_z=-1,最远为ndc_z=1
// 5. NDC到屏幕坐标。这步是个很简单的线性变换：
// 屏幕 x = (ndc_x + 1) / 2 * 视口宽 + 视口左
// 屏幕 y = (ndc_y + 1) / 2 * 视图高 + 视口底


// 观察空间坐标到裁剪空间坐标用
// 透视视角
void mat4_set_perspective(float *m, float fov, float aspect, float near_, float far_) {
    // near_ 和 far_ 是近裁剪面距离和远裁剪面距离，注意不要去掉下划线。
    // near 和 far 在 Windows 环境下是保留关键字（16位时代的段地址修饰符），很多 Windows 头文件（如 <windows.h>、<minwindef.h>）会把它们定义为空宏。
    // 它们实际指的 z = -near 平面和 z = -far 平面，near 和 far 都得是正的，否则平面看不见。而且要求 near < far。
    // 只有 z = -far 和 z = -near 之间的内容能被看到。
    // 由于通过深度缓冲判断谁在前面，如果深度范围太大，很近的两个物体可能被判断成相同深度值，分不清谁在前后，产生深度冲突。因此只有near和far之间能被看到。
    // fov 是垂直场视角 field of view，单位弧度，决定能看多宽。
    // fov 越大，视野越广，物体越小（广角镜头）；fov 越小，视野越窄，物体越大（长焦镜头）
    // 从而 y 方向，距离视觉原点距离为d处，视锥半高度为 d*tan(fov/2)
    // NDC的y方向[-1, 1]范围的半高度是1，对应的距离d就应该是1/tan(fov/2)，我们定义这个是焦距f。
    // aspect 是宽高比，窗口宽度➗️窗口高度

    // 要通过透视除法ndc = clip / clip_w实现除以-z，即近大远小。我们让clip_w = -z，从而clip_{x,y,z}=(-z)ndc_{x,y,z}
    // 所以假设矩阵最后一行是(a, b, c, d)，要让其作用在(x, y, z, 1)上得到(clip_x, clip_y, clip_z, clip_w)，其中clip_w=-z
    // a x + b y + c z + d = - z，即 a x + b y + (c + 1) z + d = 0，解得a = b = d = 0，c = - 1。
    // 由于ndc_{x,y,z} = clip_{x,y,z} / clip_w，clip_w=-z，所以我们应该有clip_x = ndc_x * (-z)，clip_y = ndc_y * (-z)。

    // 眼睛（相机）看向 -z 方向，场景中点 (x, y, z) 发出的光线在距离原点 near 的近裁剪面投影点为(proj_x, proj_y, near)。
    // 我们做完投影后，投影在near平面的y坐标叫proj_y，按照上面焦距的定义，near平面半高度应该是near/f，proj_y刚好等于这个值的时候后面NDC应该是最大的1。
    // 从而 ndc_y = ndc_y / 1 = proj_y / (near/f)，从而 ndc_y = proj_y / near * f, 则根据相似三角形
    // proj_x / (-near) = x / z     ->     proj_x = x * (-near) / z
    // proj_y / (-near) = y / z     ->     proj_y = y * (-near) / z
    // 结合三者有
    // clip_y = (-z) * ndc_y = (-z) * proj_y / near * f 
    // = (-z) * y * (-near) / z / near * f = y * f
    // 从而 f 就是 y 方向的缩放因子。x 方向同理，只不过相比y方向有一个比例aspect，相当于near平面投影范围内半宽度和板高度的比值。
    // 因此 x 方向的缩放因子是 f / aspect。
    // ndc_y = y_proj / [near * tan(fov/2)] = y * (-near) / z / [near * tan(fov/2)]
    // 透视视锥（frustum）的 x 和 y 方向是关于视线（z 轴）对称的，所以x和y方向没有平移。
    // z 方向比较复杂，因为 ndc_z = clip_z / clip_w = clip_z / (-z)，不是线性。
    // 我们让 sz * z + tz = clip_z，从而ndc_z = clip_z / (-z) = - sz - tz / z
    // 代入 z = -near 时，让 ndc_z = -1; z = - far 时，让 ndc_z = 1。
    // 解得 tz = (2 * far * near) / (near - far)，sz = (far + near) / (near - far)。
    
    memset(m, 0, 16 * sizeof(float));
    float f = 1.0f / tanf(fov / 2.0f); // 焦距
    mat4_get(m, 0, 0) = f / aspect;
    mat4_get(m, 1, 1) = f;
    mat4_get(m, 2, 2) = (far_ + near_) / (near_ - far_);
    mat4_get(m, 3, 2) = -1.0f;
    mat4_get(m, 2, 3) = (2.0f * far_ * near_) / (near_ - far_);

    /*
    sx
       sy
          sz tz
          -1 0
    */
    // sy = f
    // sx = f / aspect
    // sz = (far + near) / (near - far)
    // tz = (2 * far * near) / (near - far)
}

// 观察空间坐标到裁剪空间坐标用
// 正交视角
void mat4_set_orthographic(float *m, float left, float right, float bottom, float top, float near_, float far_) {
    // 正交投影是射影的，没有仿射，最后一行矩阵是(0, 0, 0, 1)
    // 因此后面透视除法什么都没变，相当于NDC就是clip,
    memset(m, 0, 16 * sizeof(float));
    // scale, 把[left, right] * [bottom, top] * [-near, -far]之间的长方体映射到 [-1, 1]^3
    mat4_get(m, 0, 0) =  2.0f / (right - left);
    mat4_get(m, 1, 1) =  2.0f / (top - bottom);
    mat4_get(m, 2, 2) = -2.0f / (far_ - near_);
    // translation，把( (left+right)/2，(bottom+top)/2, (-near-far)/2 ) 平移到(0, 0, 0)
    mat4_get(m, 0, 3) = -(right + left) / (right - left);
    mat4_get(m, 1, 3) = -(top + bottom) / (top - bottom);
    mat4_get(m, 2, 3) =  (far_ + near_) / (near_ - far_);
    mat4_get(m, 3, 3) = 1.0f;
    /*
    sx       tx
       sy    ty
          sz tz
             1
    */
}

// 世界空间坐标到观察（相机）空间坐标用
// eye是相机位置，center是相机看向的目标点，up是世界“向上”的方向，通常是(0,1,0)
// up 通常大致朝上，如果不朝上相当于歪着头观察。描述相机的翻滚角度。
void mat4_set_look_at(float *m, const float *eye, const float *center, const float *up) {
    // 前方 f(forward)：相机的朝向
    // f = normalize(center - eye)
    float f[3];
    vec3_sub(f, center, eye);
    vec3_normalize(f);

    // 右方 s(side)：与f、up都垂直
    // s = normalize(cross(f, up))
    float s[3];
    vec3_cross(s, f, up);
    vec3_normalize(s);

    // 上方 u(up)，重新正交化，保证与s、f都垂直。之前up可能不垂直于f。
    float u[3];
    vec3_cross(u, s, f);
    // 此时 u 已经归一化
    // 注意叉乘顺序，f = u × s，-f = s × u，所以 s u -f 是右手系。世界的 s u -f 作为相机的 x y z。

    // 向量 a 在向量 b 上的投影的是 (a·b) / (b·b) * b，在 b 为单位向量时候，其分量就是 a·b
    // 世界点 p 在相机坐标系下的坐标，就是它相对于相机在相机的三个轴的投影：
    // cam_x = (p - eye) ·   s  = p·s - eye·s
    // cam_y = (p - eye) ·   u  = p·u - eye·u
    // cam_z = (p - eye) · (-f) = p·(-f) + eye·f
    // 此时相机的 x y z 分别是世界的 s u -f，旋转矩阵作用于(s, u, -f)为单位阵。

    /*
         s.T -s·eye
         u.T -u·eye
        -f.T  f·eye
         0.T   1
    */
    memset(m, 0, 16 * sizeof(float));
    mat4_get(m, 0, 0) =  s[0];
    mat4_get(m, 1, 0) =  u[0];
    mat4_get(m, 2, 0) = -f[0];
    mat4_get(m, 0, 1) =  s[1];
    mat4_get(m, 1, 1) =  u[1];
    mat4_get(m, 2, 1) = -f[1];
    mat4_get(m, 0, 2) =  s[2];
    mat4_get(m, 1, 2) =  u[2];
    mat4_get(m, 2, 2) = -f[2];
    mat4_get(m, 0, 3) = -vec3_dot(eye, s);
    mat4_get(m, 1, 3) = -vec3_dot(eye, u);
    mat4_get(m, 2, 3) =  vec3_dot(eye, f);
    mat4_get(m, 3, 3) = 1.f;
}

void mat4_mul(float *res, const float *a, const float *b) {
    /*
    for (int j = 0; j < 4; ++ j) {
        for (int i = 0; i < 4; ++ i) {
            mat4_get(res, i, j) = 
                mat4_get(a, i, 0) * mat4_get(b, 0, j) + 
                mat4_get(a, i, 1) * mat4_get(b, 1, j) + 
                mat4_get(a, i, 2) * mat4_get(b, 2, j) + 
                mat4_get(a, i, 3) * mat4_get(b, 3, j);
        }
    }
    */
    mat4_get(res, 0, 0) = 
        mat4_get(a, 0, 0) * mat4_get(b, 0, 0) + 
        mat4_get(a, 0, 1) * mat4_get(b, 1, 0) + 
        mat4_get(a, 0, 2) * mat4_get(b, 2, 0) + 
        mat4_get(a, 0, 3) * mat4_get(b, 3, 0);

    mat4_get(res, 1, 0) = 
        mat4_get(a, 1, 0) * mat4_get(b, 0, 0) + 
        mat4_get(a, 1, 1) * mat4_get(b, 1, 0) + 
        mat4_get(a, 1, 2) * mat4_get(b, 2, 0) + 
        mat4_get(a, 1, 3) * mat4_get(b, 3, 0);

    mat4_get(res, 2, 0) = 
        mat4_get(a, 2, 0) * mat4_get(b, 0, 0) + 
        mat4_get(a, 2, 1) * mat4_get(b, 1, 0) + 
        mat4_get(a, 2, 2) * mat4_get(b, 2, 0) + 
        mat4_get(a, 2, 3) * mat4_get(b, 3, 0);

    mat4_get(res, 3, 0) = 
        mat4_get(a, 3, 0) * mat4_get(b, 0, 0) + 
        mat4_get(a, 3, 1) * mat4_get(b, 1, 0) + 
        mat4_get(a, 3, 2) * mat4_get(b, 2, 0) + 
        mat4_get(a, 3, 3) * mat4_get(b, 3, 0);


    mat4_get(res, 0, 1) = 
        mat4_get(a, 0, 0) * mat4_get(b, 0, 1) + 
        mat4_get(a, 0, 1) * mat4_get(b, 1, 1) + 
        mat4_get(a, 0, 2) * mat4_get(b, 2, 1) + 
        mat4_get(a, 0, 3) * mat4_get(b, 3, 1);

    mat4_get(res, 1, 1) = 
        mat4_get(a, 1, 0) * mat4_get(b, 0, 1) + 
        mat4_get(a, 1, 1) * mat4_get(b, 1, 1) + 
        mat4_get(a, 1, 2) * mat4_get(b, 2, 1) + 
        mat4_get(a, 1, 3) * mat4_get(b, 3, 1);

    mat4_get(res, 2, 1) = 
        mat4_get(a, 2, 0) * mat4_get(b, 0, 1) + 
        mat4_get(a, 2, 1) * mat4_get(b, 1, 1) + 
        mat4_get(a, 2, 2) * mat4_get(b, 2, 1) + 
        mat4_get(a, 2, 3) * mat4_get(b, 3, 1);

    mat4_get(res, 3, 1) = 
        mat4_get(a, 3, 0) * mat4_get(b, 0, 1) + 
        mat4_get(a, 3, 1) * mat4_get(b, 1, 1) + 
        mat4_get(a, 3, 2) * mat4_get(b, 2, 1) + 
        mat4_get(a, 3, 3) * mat4_get(b, 3, 1);


    mat4_get(res, 0, 2) = 
        mat4_get(a, 0, 0) * mat4_get(b, 0, 2) + 
        mat4_get(a, 0, 1) * mat4_get(b, 1, 2) + 
        mat4_get(a, 0, 2) * mat4_get(b, 2, 2) + 
        mat4_get(a, 0, 3) * mat4_get(b, 3, 2);

    mat4_get(res, 1, 2) = 
        mat4_get(a, 1, 0) * mat4_get(b, 0, 2) + 
        mat4_get(a, 1, 1) * mat4_get(b, 1, 2) + 
        mat4_get(a, 1, 2) * mat4_get(b, 2, 2) + 
        mat4_get(a, 1, 3) * mat4_get(b, 3, 2);

    mat4_get(res, 2, 2) = 
        mat4_get(a, 2, 0) * mat4_get(b, 0, 2) + 
        mat4_get(a, 2, 1) * mat4_get(b, 1, 2) + 
        mat4_get(a, 2, 2) * mat4_get(b, 2, 2) + 
        mat4_get(a, 2, 3) * mat4_get(b, 3, 2);

    mat4_get(res, 3, 2) = 
        mat4_get(a, 3, 0) * mat4_get(b, 0, 2) + 
        mat4_get(a, 3, 1) * mat4_get(b, 1, 2) + 
        mat4_get(a, 3, 2) * mat4_get(b, 2, 2) + 
        mat4_get(a, 3, 3) * mat4_get(b, 3, 2);


    mat4_get(res, 0, 3) = 
        mat4_get(a, 0, 0) * mat4_get(b, 0, 3) + 
        mat4_get(a, 0, 1) * mat4_get(b, 1, 3) + 
        mat4_get(a, 0, 2) * mat4_get(b, 2, 3) + 
        mat4_get(a, 0, 3) * mat4_get(b, 3, 3);

    mat4_get(res, 1, 3) = 
        mat4_get(a, 1, 0) * mat4_get(b, 0, 3) + 
        mat4_get(a, 1, 1) * mat4_get(b, 1, 3) + 
        mat4_get(a, 1, 2) * mat4_get(b, 2, 3) + 
        mat4_get(a, 1, 3) * mat4_get(b, 3, 3);

    mat4_get(res, 2, 3) = 
        mat4_get(a, 2, 0) * mat4_get(b, 0, 3) + 
        mat4_get(a, 2, 1) * mat4_get(b, 1, 3) + 
        mat4_get(a, 2, 2) * mat4_get(b, 2, 3) + 
        mat4_get(a, 2, 3) * mat4_get(b, 3, 3);

    mat4_get(res, 3, 3) = 
        mat4_get(a, 3, 0) * mat4_get(b, 0, 3) + 
        mat4_get(a, 3, 1) * mat4_get(b, 1, 3) + 
        mat4_get(a, 3, 2) * mat4_get(b, 2, 3) + 
        mat4_get(a, 3, 3) * mat4_get(b, 3, 3);
}

void mat3_identity(float *m) {
    // column-major, 3 by 3 as an array
    memset(m, 0, 9 * sizeof(float));
    mat3_get(m, 0, 0) = 
    mat3_get(m, 1, 1) = 
    mat3_get(m, 2, 2) = 
    1.0f;
}

void mat3_to_mat4_topleft(float *m4, const float *m3) {
    memset(m4, 0, 16 * sizeof(float));
    memcpy(m4    , m3    , 3 * sizeof(float));
    memcpy(m4 + 4, m3 + 3, 3 * sizeof(float));
    memcpy(m4 + 8, m3 + 6, 3 * sizeof(float));
    mat4_get(m4, 3, 3) = 1.;
}

void mat4_topleft_to_mat3(float *m3, const float *m4) {
    memcpy(m3    , m4    , 3 * sizeof(float));
    memcpy(m3 + 3, m4 + 4, 3 * sizeof(float));
    memcpy(m3 + 6, m4 + 8, 3 * sizeof(float));
}

void mat3_make_cross(float *m, const float *a) {
    mat3_get(m, 0, 0) =  0.f;
    mat3_get(m, 1, 0) =  a[2];
    mat3_get(m, 2, 0) = -a[1];
    mat3_get(m, 0, 1) = -a[2];
    mat3_get(m, 1, 1) =  0.f;
    mat3_get(m, 2, 1) =  a[0];
    mat3_get(m, 0, 2) =  a[1];
    mat3_get(m, 1, 2) = -a[0];
    mat3_get(m, 2, 2) =  0.f;
}

void mat3_add(float *res, const float *a, const float *b) {
    res[0] = a[0] + b[0];
    res[1] = a[1] + b[1];
    res[2] = a[2] + b[2];
    res[3] = a[3] + b[3];
    res[4] = a[4] + b[4];
    res[5] = a[5] + b[5];
    res[6] = a[6] + b[6];
    res[7] = a[7] + b[7];
    res[8] = a[8] + b[8];
}

void mat3_add_inplace(float *res, const float *a) {
    res[0] += a[0];
    res[1] += a[1];
    res[2] += a[2];
    res[3] += a[3];
    res[4] += a[4];
    res[5] += a[5];
    res[6] += a[6];
    res[7] += a[7];
    res[8] += a[8];
}

void mat3_sub(float *res, const float *a, const float *b) {
    res[0] = a[0] - b[0];
    res[1] = a[1] - b[1];
    res[2] = a[2] - b[2];
    res[3] = a[3] - b[3];
    res[4] = a[4] - b[4];
    res[5] = a[5] - b[5];
    res[6] = a[6] - b[6];
    res[7] = a[7] - b[7];
    res[8] = a[8] - b[8];
}

void mat3_sub_inplace(float *res, const float *a) {
    res[0] -= a[0];
    res[1] -= a[1];
    res[2] -= a[2];
    res[3] -= a[3];
    res[4] -= a[4];
    res[5] -= a[5];
    res[6] -= a[6];
    res[7] -= a[7];
    res[8] -= a[8];
}

void mat3_scale(float *m, float scale) {
    m[0] *= scale;
    m[1] *= scale;
    m[2] *= scale;
    m[3] *= scale;
    m[4] *= scale;
    m[5] *= scale;
    m[6] *= scale;
    m[7] *= scale;
    m[8] *= scale;
}

void mat3_scale_inverse(float *m, float scale) {
    if (scale == 0.f) return;
    m[0] /= scale;
    m[1] /= scale;
    m[2] /= scale;
    m[3] /= scale;
    m[4] /= scale;
    m[5] /= scale;
    m[6] /= scale;
    m[7] /= scale;
    m[8] /= scale;
}


void vec3_outer(float *res, const float *a, const float *b) {
    mat3_get(res, 0, 0) = a[0] * b[0];
    mat3_get(res, 1, 0) = a[1] * b[0];
    mat3_get(res, 2, 0) = a[2] * b[0];
    mat3_get(res, 0, 1) = a[0] * b[1];
    mat3_get(res, 1, 1) = a[1] * b[1];
    mat3_get(res, 2, 1) = a[2] * b[1];
    mat3_get(res, 0, 2) = a[0] * b[2];
    mat3_get(res, 1, 2) = a[1] * b[2];
    mat3_get(res, 2, 2) = a[2] * b[2];
}

float vec3_included_angle_cos(const float *a, const float *b) {
    return vec3_dot(a, b) / (vec3_length(a) * vec3_length(b));
}

float vec3_included_angle(const float *a, const float *b) {
    return acos(vec3_included_angle_cos(a, b));
}

/*
void mat3_set_rotation(float *m, const float *axis, float angle) {
    // assume axis is normalized
    float ret[9], temp[9];
    float c = cosf(angle);
    float s = sinf(angle);
    mat3_identity(ret);
    vec3_outer(temp, axis, axis); // n @ n.T
    mat3_sub_inplace(temp, ret); // n @ n.T - I
    mat3_scale(temp, 1.f - c); // (1 - \cos\theta)(n @ n.T - I)
    mat3_add_inplace(ret, temp); // I + (1 - \cos\theta)(n @ n.T - I)
    mat3_make_cross(temp, axis); // K(n)
    mat3_scale(temp, s); // K(n) * \sin\theta
    mat3_add_inplace(ret, temp); // // I + (1 - \cos\theta)(n @ n.T - I) + K(n) * \sin\theta
}
*/

void mat3_set_rotation(float *m, const float *axis, float angle) {
    // assume axis is normalized
    float c = cosf(angle);
    float s = sinf(angle);
    float comp_c = 1. - c;
    mat3_get(m, 0, 0) = c + comp_c * axis[0] * axis[0];
    mat3_get(m, 1, 0) = comp_c * axis[1] * axis[0] + s * axis[2];
    mat3_get(m, 2, 0) = comp_c * axis[2] * axis[0] - s * axis[1];
    mat3_get(m, 0, 1) = comp_c * axis[0] * axis[1] - s * axis[2];
    mat3_get(m, 1, 1) = c + comp_c * axis[1] * axis[1];
    mat3_get(m, 2, 1) = comp_c * axis[2] * axis[1] + s * axis[0];
    mat3_get(m, 0, 2) = comp_c * axis[0] * axis[2] + s * axis[1];
    mat3_get(m, 1, 2) = comp_c * axis[1] * axis[2] - s * axis[0];
    mat3_get(m, 2, 2) = c + comp_c * axis[2] * axis[2];
}

void mat3_set_rotation_not_normalized(float *m, const float *axis_not_normalized, float angle) {
    float axis[3];
    memcpy(axis, axis_not_normalized, 3 * sizeof(float));
    vec3_normalize(axis);
    mat3_set_rotation(m, axis, angle);
}

void mat4_set_rotation_by_cos_angle(float *m, const float *axis, float c) {
    // assume axis is normalized
    float s = sqrtf(1. - c * c);
    float comp_c = 1. - c;
    memset(m, 0, 16 * sizeof(float));
    mat4_get(m, 0, 0) = c + comp_c * axis[0] * axis[0];
    mat4_get(m, 1, 0) = comp_c * axis[1] * axis[0] + s * axis[2];
    mat4_get(m, 2, 0) = comp_c * axis[2] * axis[0] - s * axis[1];
    mat4_get(m, 0, 1) = comp_c * axis[0] * axis[1] - s * axis[2];
    mat4_get(m, 1, 1) = c + comp_c * axis[1] * axis[1];
    mat4_get(m, 2, 1) = comp_c * axis[2] * axis[1] + s * axis[0];
    mat4_get(m, 0, 2) = comp_c * axis[0] * axis[2] + s * axis[1];
    mat4_get(m, 1, 2) = comp_c * axis[1] * axis[2] - s * axis[0];
    mat4_get(m, 2, 2) = c + comp_c * axis[2] * axis[2];
    mat4_get(m, 3, 3) = 1.f;
}

void mat4_set_rotation(float *m, const float *axis, float angle) {
    // assume axis is normalized
    float c = cosf(angle);
    float s = sinf(angle);
    float comp_c = 1. - c;
    memset(m, 0, 16 * sizeof(float));
    mat4_get(m, 0, 0) = c + comp_c * axis[0] * axis[0];
    mat4_get(m, 1, 0) = comp_c * axis[1] * axis[0] + s * axis[2];
    mat4_get(m, 2, 0) = comp_c * axis[2] * axis[0] - s * axis[1];
    mat4_get(m, 0, 1) = comp_c * axis[0] * axis[1] - s * axis[2];
    mat4_get(m, 1, 1) = c + comp_c * axis[1] * axis[1];
    mat4_get(m, 2, 1) = comp_c * axis[2] * axis[1] + s * axis[0];
    mat4_get(m, 0, 2) = comp_c * axis[0] * axis[2] + s * axis[1];
    mat4_get(m, 1, 2) = comp_c * axis[1] * axis[2] - s * axis[0];
    mat4_get(m, 2, 2) = c + comp_c * axis[2] * axis[2];
    mat4_get(m, 3, 3) = 1.f;
}

void mat4_set_rotation_half_turn(float *m, const float *axis) {
    // assume axis is normalized
    memset(m, 0, 16 * sizeof(float));
    mat4_get(m, 0, 0) = -1.f + 2.f * axis[0] * axis[0];
    mat4_get(m, 1, 0) = 2.f * axis[1] * axis[0];
    mat4_get(m, 2, 0) = 2.f * axis[2] * axis[0];
    mat4_get(m, 0, 1) = 2.f * axis[0] * axis[1];
    mat4_get(m, 1, 1) = -1.f + 2.f * axis[1] * axis[1];
    mat4_get(m, 2, 1) = 2.f * axis[2] * axis[1];
    mat4_get(m, 0, 2) = 2.f * axis[0] * axis[2];
    mat4_get(m, 1, 2) = 2.f * axis[1] * axis[2];
    mat4_get(m, 2, 2) = -1.f + 2.f * axis[2] * axis[2];
    mat4_get(m, 3, 3) = 1.f;
}


void mat3_lmul_inplace(float *m, const float *a) {
    float temp[9];
    mat3_copy(temp, m);
    mat3_mul(m, a, temp);
}

void mat3_rmul_inplace(float *m, const float *a) {
    float temp[9];
    mat3_copy(temp, m);
    mat3_mul(m, temp, a);
}

void mat4_lmul_inplace(float *m, const float *a) {
    float temp[16];
    mat4_copy(temp, m);
    mat4_mul(m, a, temp);
}

void mat4_rmul_inplace(float *m, const float *a) {
    float temp[16];
    mat4_copy(temp, m);
    mat4_mul(m, temp, a);
}

void mat3_mul(float *res, const float *a, const float *b) {
    /*
    for (int j = 0; j < 3; ++ j) {
        for (int i = 0; i < 3; ++ i) {
            mat3_get(res, i, j) = 
                mat3_get(a, i, 0) * mat3_get(b, 0, j) + 
                mat3_get(a, i, 1) * mat3_get(b, 1, j) + 
                mat3_get(a, i, 2) * mat3_get(b, 2, j);
        }
    }
    */
    mat3_get(res, 0, 0) = 
        mat3_get(a, 0, 0) * mat3_get(b, 0, 0) + 
        mat3_get(a, 0, 1) * mat3_get(b, 1, 0) + 
        mat3_get(a, 0, 2) * mat3_get(b, 2, 0);

    mat3_get(res, 1, 0) = 
        mat3_get(a, 1, 0) * mat3_get(b, 0, 0) + 
        mat3_get(a, 1, 1) * mat3_get(b, 1, 0) + 
        mat3_get(a, 1, 2) * mat3_get(b, 2, 0);

    mat3_get(res, 2, 0) = 
        mat3_get(a, 2, 0) * mat3_get(b, 0, 0) + 
        mat3_get(a, 2, 1) * mat3_get(b, 1, 0) + 
        mat3_get(a, 2, 2) * mat3_get(b, 2, 0);


    mat3_get(res, 0, 1) = 
        mat3_get(a, 0, 0) * mat3_get(b, 0, 1) + 
        mat3_get(a, 0, 1) * mat3_get(b, 1, 1) + 
        mat3_get(a, 0, 2) * mat3_get(b, 2, 1);

    mat3_get(res, 1, 1) = 
        mat3_get(a, 1, 0) * mat3_get(b, 0, 1) + 
        mat3_get(a, 1, 1) * mat3_get(b, 1, 1) + 
        mat3_get(a, 1, 2) * mat3_get(b, 2, 1);

    mat3_get(res, 2, 1) = 
        mat3_get(a, 2, 0) * mat3_get(b, 0, 1) + 
        mat3_get(a, 2, 1) * mat3_get(b, 1, 1) + 
        mat3_get(a, 2, 2) * mat3_get(b, 2, 1);


    mat3_get(res, 0, 2) = 
        mat3_get(a, 0, 0) * mat3_get(b, 0, 2) + 
        mat3_get(a, 0, 1) * mat3_get(b, 1, 2) + 
        mat3_get(a, 0, 2) * mat3_get(b, 2, 2);

    mat3_get(res, 1, 2) = 
        mat3_get(a, 1, 0) * mat3_get(b, 0, 2) + 
        mat3_get(a, 1, 1) * mat3_get(b, 1, 2) + 
        mat3_get(a, 1, 2) * mat3_get(b, 2, 2);

    mat3_get(res, 2, 2) = 
        mat3_get(a, 2, 0) * mat3_get(b, 0, 2) + 
        mat3_get(a, 2, 1) * mat3_get(b, 1, 2) + 
        mat3_get(a, 2, 2) * mat3_get(b, 2, 2);
}

void mat4_set_rotation_xyz(float *m, float alpha, float beta, float gamma) {
    // rotate along z gamma, then y beta, finally x alpha
    float ca = cosf(alpha), cb = cosf(beta), cc = cosf(gamma);
    float sa = sinf(alpha), sb = sinf(beta), sc = sinf(gamma);

    memset(m, 0, 16 * sizeof(float));
    mat4_get(m, 0, 0) =   cb * cc;
    mat4_get(m, 1, 0) =   cc * sa * sb + ca * sc;
    mat4_get(m, 2, 0) = - ca * cc * sb + sa * sc;
    mat4_get(m, 0, 1) = - cb * sc;
    mat4_get(m, 1, 1) =   ca * cc - sa * sb * sc;
    mat4_get(m, 2, 1) =   cc * sa + ca * sb * sc;
    mat4_get(m, 0, 2) =   sb;
    mat4_get(m, 1, 2) = - cb * sa;
    mat4_get(m, 2, 2) =   ca * cb;
    mat4_get(m, 3, 3) = 1.f;
}

void mat4_set_rotation_yzx(float *m, float alpha, float beta, float gamma) {
    // rotate along x alpha, then z gamma, finally y beta
    float ca = cosf(alpha), cb = cosf(beta), cc = cosf(gamma);
    float sa = sinf(alpha), sb = sinf(beta), sc = sinf(gamma);

    memset(m, 0, 16 * sizeof(float));
    mat4_get(m, 0, 0) =   cb * cc;
    mat4_get(m, 1, 0) =   sc;
    mat4_get(m, 2, 0) = - cc * sb;
    mat4_get(m, 0, 1) =   sa * sb - ca * cb * sc;
    mat4_get(m, 1, 1) =   ca * cc;
    mat4_get(m, 2, 1) =   cb * sa + ca * sb * sc;
    mat4_get(m, 0, 2) =   ca * sb + cb * sa * sc;
    mat4_get(m, 1, 2) = - cc * sa;
    mat4_get(m, 2, 2) =   ca * cb - sa * sb * sc;
    mat4_get(m, 3, 3) = 1.f;
}

void mat4_set_rotation_zxy(float *m, float alpha, float beta, float gamma) {
    // rotate along y beta, then x alpha, finally z gamma
    float ca = cosf(alpha), cb = cosf(beta), cc = cosf(gamma);
    float sa = sinf(alpha), sb = sinf(beta), sc = sinf(gamma);

    memset(m, 0, 16 * sizeof(float));
    mat4_get(m, 0, 0) =   cb * cc - sa * sb * sc;
    mat4_get(m, 1, 0) =   cc * sa * sb + cb * sc;
    mat4_get(m, 2, 0) = - ca * sb;
    mat4_get(m, 0, 1) = - ca * sc;
    mat4_get(m, 1, 1) =   ca * cc;
    mat4_get(m, 2, 1) =   sa;
    mat4_get(m, 0, 2) =   cc * sb + cb * sa * sc;
    mat4_get(m, 1, 2) = - cb * cc * sa + sb * sc;
    mat4_get(m, 2, 2) =   ca * cb;
    mat4_get(m, 3, 3) = 1.f;
}

void mat4_set_rotation_zyx(float *m, float alpha, float beta, float gamma) {
    // rotate along x alpha, then y beta, finally z gamma
    float ca = cosf(alpha), cb = cosf(beta), cc = cosf(gamma);
    float sa = sinf(alpha), sb = sinf(beta), sc = sinf(gamma);

    memset(m, 0, 16 * sizeof(float));
    mat4_get(m, 0, 0) =   cb * cc;
    mat4_get(m, 1, 0) =   cb * sc;
    mat4_get(m, 2, 0) = - sb;
    mat4_get(m, 0, 1) =   cc * sa * sb - ca * sc;
    mat4_get(m, 1, 1) =   ca * cc + sa * sb * sc;
    mat4_get(m, 2, 1) =   cb * sa;
    mat4_get(m, 0, 2) =   ca * cc * sb + sa * sc;
    mat4_get(m, 1, 2) = - cc * sa + ca * sb * sc;
    mat4_get(m, 2, 2) =   ca * cb;
    mat4_get(m, 3, 3) = 1.f;
}

void mat4_set_rotation_xzy(float *m, float alpha, float beta, float gamma) {
    // rotate along y beta, then z gamma, finally x alpha
    float ca = cosf(alpha), cb = cosf(beta), cc = cosf(gamma);
    float sa = sinf(alpha), sb = sinf(beta), sc = sinf(gamma);

    memset(m, 0, 16 * sizeof(float));
    mat4_get(m, 0, 0) =   cb * cc;
    mat4_get(m, 1, 0) =   sa * sb - ca * cb * sc;
    mat4_get(m, 2, 0) = - ca * sb + cb * sa * sc;
    mat4_get(m, 0, 1) = - sc;
    mat4_get(m, 1, 1) =   ca * cc;
    mat4_get(m, 2, 1) =   cc * sa;
    mat4_get(m, 0, 2) =   cc * sb;
    mat4_get(m, 1, 2) = - cb * sa + ca * sb * sc;
    mat4_get(m, 2, 2) =   ca * cb + sa * sb * sc;
    mat4_get(m, 3, 3) = 1.f;
}

void mat4_set_rotation_yxz(float *m, float alpha, float beta, float gamma) {
    // rotate along z gamma, then x alpha, finally y beta
    float ca = cosf(alpha), cb = cosf(beta), cc = cosf(gamma);
    float sa = sinf(alpha), sb = sinf(beta), sc = sinf(gamma);

    memset(m, 0, 16 * sizeof(float));
    mat4_get(m, 0, 0) =   cb * cc + sa * sb * sc;
    mat4_get(m, 1, 0) =   ca * sc;
    mat4_get(m, 2, 0) = - cc * sb + cb * sa * sc;
    mat4_get(m, 0, 1) =   cc * sa * sb - cb * sc;
    mat4_get(m, 1, 1) =   ca * cc;
    mat4_get(m, 2, 1) =   cb * cc * sa + sb * sc;
    mat4_get(m, 0, 2) =   ca * sb;
    mat4_get(m, 1, 2) = - sa;
    mat4_get(m, 2, 2) =   ca * cb;
    mat4_get(m, 3, 3) = 1.f;
}

