#include "math_utils.h"
#include "geometry.h"
#include "align_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef HAVE_UTHASH
void subdivide_icosphere_once(float **vertices, int *vertex_count,
                            GLuint **indices, int *index_count) {
    // 这是用了 Hash 表去重的版本，对于纹理贴图情况这是必须的。
    float *verts_old = *vertices;
    int vert_count_old = *vertex_count;
    GLuint *idx_old = *indices;
    int idx_count_old = *index_count;

    int num_faces_old = idx_count_old / 3;
    int num_faces = num_faces_old * 4;
    // 每个面都取三条边中点，构成4个新的三角形，然后归一化。
    // 每个面的三个原来的顶点都沿用原来的序号，新的顶点追加到后面
    int vert_count = vert_count_old + num_faces_old / 2 * 3;
    // 每个三角形有 3 条边，都要取中点，但是每个中点被相邻的 2 个三角形共用
    // 实际按照 Euler 公式，更一般的新点数为 vert_count_old + (vert_count_old + num_faces_old - 2)，这里两者是相同的。
    int idx_count = num_faces * 3;

    // 新的顶点和索引
    float *verts = (float *)my_malloc(vert_count * 6 * sizeof(float));
    GLuint *idx = (GLuint *)my_malloc(idx_count * sizeof(GLuint));
    if (!verts || !idx) {
        fprintf(stderr, "Error! Cannot allocate memory.\n");
        exit(EXIT_FAILURE);
    }

    // 先拷贝旧的顶点
    memcpy(verts, verts_old, vert_count_old * 6 * sizeof(float));

    // 顶点数组索引，从新的顶点开始，注意不是顶点索引，每个顶点有 6 个 float
    int v = vert_count_old * 6;
    // 顶点序号索引
    int k = 0;

    // 第几个顶点，新的边中心从原来顶点后面开始计数
    int vert_idx = vert_count_old;

    // 建立 Hash 表
    edge_map *edges = NULL;   // Hash 表（初始为空）

    for (int idx_face = 0; idx_face < num_faces_old; ++ idx_face) {
        // 原有的三个顶点的序号
        int n1 = idx_old[idx_face * 3];
        int n2 = idx_old[idx_face * 3 + 1];
        int n3 = idx_old[idx_face * 3 + 2];

        // n1-n2、n2-n3、n3-n1 中点，新的顶点序号
        int n4 = get_or_create_midpoint_index(&edges, verts, &vert_idx, &v, n1, n2);
        int n5 = get_or_create_midpoint_index(&edges, verts, &vert_idx, &v, n2, n3);
        int n6 = get_or_create_midpoint_index(&edges, verts, &vert_idx, &v, n3, n1);
        
        // 设置新三边中点顶点索引
        idx[k ++] = n1; idx[k ++] = n4; idx[k ++] = n6;
        idx[k ++] = n4; idx[k ++] = n2; idx[k ++] = n5;
        idx[k ++] = n6; idx[k ++] = n5; idx[k ++] = n3;
        idx[k ++] = n4; idx[k ++] = n5; idx[k ++] = n6;
    }

    // 释放 Hash 表每个顶点分配的内存，清理 Hash 表
    edge_map *next_node = NULL, *e = NULL;
    HASH_ITER(hh, edges, e, next_node) {
        HASH_DEL(edges, e);
        my_free(e);
    }

    e = NULL; next_node = NULL;

    // 释放旧数组内存，交换成新数组
    my_free(verts_old); verts_old = NULL;
    my_free(idx_old); idx_old = NULL;

    *vertices = verts;
    *vertex_count = vert_count;
    *indices = idx;
    *index_count = idx_count;

    verts = NULL; idx = NULL;
}

int get_or_create_midpoint_index(edge_map **edges_ptr, float *verts, int *vert_idx_ptr, int *v_ptr, int a, int b) {
    // 返回 a 和 b 中点的序号
    // 如果已经在 Hash 表，就返回其序号，否则向 verts 里面填充新的中点坐标和法线，让 *vert_idx_ptr 当成新中点序号（返回），
    // 同时修改 *vert_idx_ptr 让其为下一次添加中点时用的序号，修改 *v_ptr 指向下一次要修改的顶点坐标和法线数组 verts 的位置。
    // 第一个参数传入指向 Hash 表的指针，因为 Hash 表本身也要被修改。

    // 给 Hash 表用的键
    int key[2];
    // 用来查询 Hash 表的结构体指针
    edge_map *e = NULL;

    if (a > b) {
        key[0] = b;
        key[1] = a;
    } else {
        key[0] = a;
        key[1] = b;
    }

    int ret;

    HASH_FIND(hh, *edges_ptr, key, 2 * sizeof(int), e);
    if (e) {
        // 这个中点已在 Hash 表索引中，直接拿来用。
        ret = e->mid;
    } else {
        // 未命中
        // 新的边中点坐标
        float mid_point[3];

        mid_point[0] = (verts[a * 6    ] + verts[b * 6    ]) / 2.f;
        mid_point[1] = (verts[a * 6 + 1] + verts[b * 6 + 1]) / 2.f;
        mid_point[2] = (verts[a * 6 + 2] + verts[b * 6 + 2]) / 2.f;

        vec3_normalize(mid_point);
        memcpy(verts + *v_ptr, mid_point, 3 * sizeof(float)); *v_ptr += 3;
        memcpy(verts + *v_ptr, mid_point, 3 * sizeof(float)); *v_ptr += 3;
        // 新建键值对
        e = my_malloc(sizeof(edge_map));                  
        if (!e) {
            fprintf(stderr, "Error! Cannot allocate memory.\n");
            exit(EXIT_FAILURE);
        }
        // 设置新键值对的键和值
        memcpy(e->ends, key, 2 * sizeof(int));
        ret = (*vert_idx_ptr) ++;
        e->mid = ret;
        // 添加到 Hash 表里
        HASH_ADD(hh, *edges_ptr, ends, 2 * sizeof(int), e);
    }

    e = NULL;
    return ret;
}

#else // HAVE_UTHASH
void subdivide_icosphere_once(float **vertices, int *vertex_count,
                            GLuint **indices, int *index_count) {
    // 这里只用排序+二分查找去重，理论复杂度从 O(N) 掉到 O(N log N)，但是不依赖于 Hash 库
    float *verts_old = *vertices;
    int vert_count_old = *vertex_count;
    GLuint *idx_old = *indices;
    int idx_count_old = *index_count;

    int num_faces_old = idx_count_old / 3;
    int num_faces = num_faces_old * 4;
    // 每个面都取三条边中点，构成4个新的三角形，然后归一化。
    // 每个面的三个原来的顶点都沿用原来的序号，新的顶点追加到后面
    int num_append_verts = num_faces_old / 2 * 3;
    int vert_count = vert_count_old + num_append_verts;
    // 每个三角形有 3 条边，都要取中点，但是每个中点被相邻的 2 个三角形共用
    // 实际按照 Euler 公式，更一般的新点数为 vert_count_old + (vert_count_old + num_faces_old - 2)，这里两者是相同的。
    int idx_count = num_faces * 3;

    // 新的顶点和索引
    float *verts = (float *)my_malloc(vert_count * 6 * sizeof(float));
    GLuint *idx = (GLuint *)my_malloc(idx_count * sizeof(GLuint));
    if (!verts || !idx) {
        fprintf(stderr, "Error! Cannot allocate memory.\n");
        exit(EXIT_FAILURE);
    }

    // 先拷贝旧的顶点
    memcpy(verts, verts_old, vert_count_old * 6 * sizeof(float));

    // 建立索引数组，有 3 * num_append_verts 个元素，记录 (a, b)。
    // 先遍历一遍所有 a < b 的 (a, b)，然后排序。这样第i组(a, b) 对应的 mid 就是 vert_count_old + i
    // 排序后按顺序填充 mid，然后使用的时候去里面查

    int *vert_map = (int *)my_malloc(num_append_verts * 2 * sizeof(int));
    if (!vert_map) {
        fprintf(stderr, "Error! Cannot allocate memory.\n");
        exit(EXIT_FAILURE);
    }

    int idx_mid = 0;
    for (int idx_face = 0; idx_face < num_faces_old; ++ idx_face) {
        // 原有的三个顶点的序号
        int n1 = idx_old[idx_face * 3];
        int n2 = idx_old[idx_face * 3 + 1];
        int n3 = idx_old[idx_face * 3 + 2];

        // 只对顺序的加入列表
        if (n1 < n2) {
            vert_map[idx_mid * 2    ] = n1;
            vert_map[idx_mid * 2 + 1] = n2;
            ++ idx_mid;
        }

        if (n2 < n3) {
            vert_map[idx_mid * 2    ] = n2;
            vert_map[idx_mid * 2 + 1] = n3;
            ++ idx_mid;
        }

        if (n3 < n1) {
            vert_map[idx_mid * 2    ] = n3;
            vert_map[idx_mid * 2 + 1] = n1;
            ++ idx_mid;
        }
    }

    // 顶点数组索引，从新的顶点开始，注意不是顶点索引，每个顶点有 6 个 float
    int v = vert_count_old * 6;

    // 对端点排序，以及设置对应的顶点坐标/法线
    qsort(vert_map, num_append_verts, 2 * sizeof(int), comp_vert_end);
    for (int idx_mid = 0; idx_mid < num_append_verts; ++ idx_mid) {
        int a = vert_map[idx_mid * 2], b = vert_map[idx_mid * 2 + 1];
        float mid_point[3];
        mid_point[0] = (verts[a * 6    ] + verts[b * 6    ]) / 2.f;
        mid_point[1] = (verts[a * 6 + 1] + verts[b * 6 + 1]) / 2.f;
        mid_point[2] = (verts[a * 6 + 2] + verts[b * 6 + 2]) / 2.f;
        vec3_normalize(mid_point);

        memcpy(verts + v, mid_point, 3 * sizeof(float)); v += 3;
        memcpy(verts + v, mid_point, 3 * sizeof(float)); v += 3;
    }

    // 顶点序号索引
    int k = 0;

    // 构建新的顶点索引
    for (int idx_face = 0; idx_face < num_faces_old; ++ idx_face) {
        // 原有的三个顶点的序号
        int n1 = idx_old[idx_face * 3];
        int n2 = idx_old[idx_face * 3 + 1];
        int n3 = idx_old[idx_face * 3 + 2];

        // n1-n2、n2-n3、n3-n1 中点，新的顶点序号
        int n4, n5, n6;

        int ends[2];
        int *pos = NULL;
        
        if (n1 > n2) {
            ends[0] = n2;
            ends[1] = n1;
        } else {
            ends[0] = n1;
            ends[1] = n2;
        }
        pos = bsearch(ends, vert_map, num_append_verts, 2 * sizeof(int), comp_vert_end);
        if (!pos) {
            fprintf(stderr, "Error! Cannot get midpoint index.\n");
            exit(EXIT_FAILURE);
        }
        n4 = vert_count_old + (pos - vert_map) / 2;

        if (n2 > n3) {
            ends[0] = n3;
            ends[1] = n2;
        } else {
            ends[0] = n2;
            ends[1] = n3;
        }
        pos = bsearch(ends, vert_map, num_append_verts, 2 * sizeof(int), comp_vert_end);
        if (!pos) {
            fprintf(stderr, "Error! Cannot get midpoint index.\n");
            exit(EXIT_FAILURE);
        }
        n5 = vert_count_old + (pos - vert_map) / 2;

        if (n3 > n1) {
            ends[0] = n1;
            ends[1] = n3;
        } else {
            ends[0] = n3;
            ends[1] = n1;
        }
        pos = bsearch(ends, vert_map, num_append_verts, 2 * sizeof(int), comp_vert_end);
        if (!pos) {
            fprintf(stderr, "Error! Cannot get midpoint index.\n");
            exit(EXIT_FAILURE);
        }
        n6 = vert_count_old + (pos - vert_map) / 2;

        // 设置新三边中点顶点索引
        idx[k ++] = n1; idx[k ++] = n4; idx[k ++] = n6;
        idx[k ++] = n4; idx[k ++] = n2; idx[k ++] = n5;
        idx[k ++] = n6; idx[k ++] = n5; idx[k ++] = n3;
        idx[k ++] = n4; idx[k ++] = n5; idx[k ++] = n6;
    }

    // 释放 (a, b) 的内存
    my_free(vert_map); vert_map = NULL;

    // 释放旧数组内存，交换成新数组
    my_free(verts_old); verts_old = NULL;
    my_free(idx_old); idx_old = NULL;

    *vertices = verts;
    *vertex_count = vert_count;
    *indices = idx;
    *index_count = idx_count;

    verts = NULL; idx = NULL;
}

int comp_vert_end(const void *a, const void *b) {
    int v1 = ((int *)a)[0], v2 = ((int *)b)[0];
    if (v1 < v2) {
        return -1;
    } else if (v1 > v2) {
        return 1;
    } else {
        v1 = ((int *)a)[1]; v2 = ((int *)b)[1];
        if (v1 < v2) {
            return -1;
        } else if (v1 > v2) {
            return 1;
        } else {
            return 0; // 这应该永远不会被碰到
        }
    }
}

#endif // HAVE_UTHASH

void generate_icosphere(int num_subdivisions_iter, 
                        float **vertices, int *vertex_count, 
                        GLuint **indices, int *index_count) { 
    // 棱角球，或称正二十面体细分球
    if (num_subdivisions_iter < 0) {
        fprintf(stderr, "Error! Number of subdivision iterations must not be negative.\n");
        exit(EXIT_FAILURE);
    }

    // 正二十面体有12个顶点，20个面
    int vert_count = 12;
    int idx_count = 20 * 3;
    float *verts = (float *)my_malloc(vert_count * 6 * sizeof(float));
    GLuint *idx = (GLuint *)my_malloc(idx_count * sizeof(GLuint));
    if (!verts || !idx) {
        fprintf(stderr, "Error! Cannot allocate memory.\n");
        exit(EXIT_FAILURE);
    }

    const float varphi = (1.f + sqrtf(5.f)) / 2.f;
    const float norm_fac = sqrtf((5.f + sqrtf(5.f)) / 2.f);

    // 顶点坐标和法线索引。
    // 正二十面体本身每两个面之间是有棱角的，如果要画纯粹的正二十面体，
    // 每个顶点必须被重复5次，每个面用自己的那份顶点拷贝，每个面三个顶点的法线都设置为面的法线方向。
    // 但是我们的目的是尽量像球，所以只要一份顶点，对于归一化的（每个顶点在单位球面上）每个顶点法线就是顶点坐标。
    // 这样每个片元的法向量自动由顶点的法向量插值得到，看起来比较光滑。
    int v = 0;
    verts[v ++] = 0.f; verts[v ++] =  1.f / norm_fac; verts[v ++] =  varphi / norm_fac;
    verts[v ++] = 0.f; verts[v ++] =  1.f / norm_fac; verts[v ++] =  varphi / norm_fac;
    verts[v ++] = 0.f; verts[v ++] = -1.f / norm_fac; verts[v ++] =  varphi / norm_fac;
    verts[v ++] = 0.f; verts[v ++] = -1.f / norm_fac; verts[v ++] =  varphi / norm_fac;
    verts[v ++] = 0.f; verts[v ++] =  1.f / norm_fac; verts[v ++] = -varphi / norm_fac;
    verts[v ++] = 0.f; verts[v ++] =  1.f / norm_fac; verts[v ++] = -varphi / norm_fac;
    verts[v ++] = 0.f; verts[v ++] = -1.f / norm_fac; verts[v ++] = -varphi / norm_fac;
    verts[v ++] = 0.f; verts[v ++] = -1.f / norm_fac; verts[v ++] = -varphi / norm_fac;
    verts[v ++] =  1.f / norm_fac; verts[v ++] =  varphi / norm_fac; verts[v ++] = 0.f;
    verts[v ++] =  1.f / norm_fac; verts[v ++] =  varphi / norm_fac; verts[v ++] = 0.f;
    verts[v ++] = -1.f / norm_fac; verts[v ++] =  varphi / norm_fac; verts[v ++] = 0.f;
    verts[v ++] = -1.f / norm_fac; verts[v ++] =  varphi / norm_fac; verts[v ++] = 0.f;
    verts[v ++] =  1.f / norm_fac; verts[v ++] = -varphi / norm_fac; verts[v ++] = 0.f;
    verts[v ++] =  1.f / norm_fac; verts[v ++] = -varphi / norm_fac; verts[v ++] = 0.f;
    verts[v ++] = -1.f / norm_fac; verts[v ++] = -varphi / norm_fac; verts[v ++] = 0.f;
    verts[v ++] = -1.f / norm_fac; verts[v ++] = -varphi / norm_fac; verts[v ++] = 0.f;
    verts[v ++] =  varphi / norm_fac; verts[v ++] = 0.f; verts[v ++] =  1.f / norm_fac;
    verts[v ++] =  varphi / norm_fac; verts[v ++] = 0.f; verts[v ++] =  1.f / norm_fac;
    verts[v ++] =  varphi / norm_fac; verts[v ++] = 0.f; verts[v ++] = -1.f / norm_fac;
    verts[v ++] =  varphi / norm_fac; verts[v ++] = 0.f; verts[v ++] = -1.f / norm_fac;
    verts[v ++] = -varphi / norm_fac; verts[v ++] = 0.f; verts[v ++] =  1.f / norm_fac;
    verts[v ++] = -varphi / norm_fac; verts[v ++] = 0.f; verts[v ++] =  1.f / norm_fac;
    verts[v ++] = -varphi / norm_fac; verts[v ++] = 0.f; verts[v ++] = -1.f / norm_fac;
    verts[v ++] = -varphi / norm_fac; verts[v ++] = 0.f; verts[v ++] = -1.f / norm_fac;

    // 片元三角形顶点索引
    int k = 0;
    idx[k ++] =  0; idx[k ++] =  1; idx[k ++] =  8;
    idx[k ++] =  0; idx[k ++] =  8; idx[k ++] =  4;
    idx[k ++] =  0; idx[k ++] =  4; idx[k ++] =  5;
    idx[k ++] =  0; idx[k ++] =  5; idx[k ++] = 10;
    idx[k ++] =  0; idx[k ++] = 10; idx[k ++] =  1;
    idx[k ++] = 10; idx[k ++] =  7; idx[k ++] =  1;
    idx[k ++] =  1; idx[k ++] =  7; idx[k ++] =  6;
    idx[k ++] =  1; idx[k ++] =  6; idx[k ++] =  8;
    idx[k ++] =  8; idx[k ++] =  6; idx[k ++] =  9;
    idx[k ++] =  8; idx[k ++] =  9; idx[k ++] =  4;
    idx[k ++] =  4; idx[k ++] =  9; idx[k ++] =  2;
    idx[k ++] =  4; idx[k ++] =  2; idx[k ++] =  5;
    idx[k ++] =  5; idx[k ++] =  2; idx[k ++] = 11;
    idx[k ++] =  5; idx[k ++] = 11; idx[k ++] = 10;
    idx[k ++] = 10; idx[k ++] = 11; idx[k ++] =  7;
    idx[k ++] =  3; idx[k ++] = 11; idx[k ++] =  2;
    idx[k ++] =  3; idx[k ++] =  2; idx[k ++] =  9;
    idx[k ++] =  3; idx[k ++] =  9; idx[k ++] =  6;
    idx[k ++] =  3; idx[k ++] =  6; idx[k ++] =  7;
    idx[k ++] =  3; idx[k ++] =  7; idx[k ++] = 11;

    *vertices = verts;
    *vertex_count = vert_count;
    *indices = idx;
    *index_count = idx_count;

    verts = NULL; idx = NULL;

    for (int idx_subdivide = 0; idx_subdivide < num_subdivisions_iter; ++ idx_subdivide) {
        subdivide_icosphere_once(vertices, vertex_count, indices, index_count);
    }
}


// ==================== 生成几何体数据 ====================
/*
// 生成单位球体，南北极分别只用一个点（半径=1，中心在原点，Y轴向上）
void generate_geosphere(int sectors, int stacks,
                    float **vertices, int *vertex_count,
                    GLuint **indices, int *index_count) {

    if (stacks < 2 || sectors < 3) {
        fprintf(stderr, "Error! At least 2 stacks and 3 sectors are required.\n");
        exit(EXIT_FAILURE);
    }
    // 经纬球。sectors是多少个经度区域，stacks是多少个纬度区域。
    // 顶点个数。南北极各自只用一个点。
    int vert_count = (stacks - 1) * (sectors + 1) + 2;
    // 每个片段都按四边形处理，每个四边形拆成两个三角形，所以共6个索引。但是最上层和最下层分别少了sectors个小三角，所以要减去6 * sectors
    int idx_count = (stacks - 1) * sectors * 3 * 2;

    // verts依次排每个顶点，每个顶点用6个坐标，前3个位置，后3个法线。大循环纬度从北纬0°到南纬0°。小循环是里面经度。
    float *verts = (float *)my_malloc(vert_count * 6 * sizeof(float));
    GLuint *idx = (GLuint *)my_malloc(idx_count * sizeof(GLuint));

    if (!verts || !idx) {
        fprintf(stderr, "Error! Cannot allocate memory.\n");
        exit(EXIT_FAILURE);
    }
    float sector_step = 2. * (float)M_PI / sectors;
    float stack_step = (float)M_PI / stacks;

    // OpenGL 用y向上，x向右，z向前。不像数学中通常用z向上。
    // 顶点索引，x y z坐标然后x y z法线
    int v = 0;
    // 北极
    // 位置
    verts[v ++] = 0.f; verts[v ++] = 1.f; verts[v ++] = 0.f;
    // 法线
    verts[v ++] = 0.f; verts[v ++] = 1.f; verts[v ++] = 0.f;
    // 中间层
    for (int i = 1; i < stacks; ++ i) {
        float stack_angle = (float)M_PI / 2. - i * stack_step; // 从 pi/2 到 -pi/2
        float horiz_radius = cosf(stack_angle);  // 水平面上的半径
        float y = sinf(stack_angle);   // 高度

        for (int j = 0; j <= sectors; ++ j) {
            float sector_angle = j * sector_step;
            float x = horiz_radius * cosf(sector_angle);
            float z = horiz_radius * sinf(sector_angle);

            // 位置
            verts[v ++] = x;
            verts[v ++] = y;
            verts[v ++] = z;
            // 法线（单位球，所以法线等于位置）
            verts[v ++] = x;
            verts[v ++] = y;
            verts[v ++] = z;
        }
    }
    // 南极
    // 位置
    verts[v ++] = 0.f; verts[v ++] = -1.f; verts[v ++] = 0.f;
    // 法线
    verts[v ++] = 0.f; verts[v ++] = -1.f; verts[v ++] = 0.f;

    // 片元索引（三角形）
    int k = 0;
    // 顶层上三角
    int top_pole = 0;
    int top_ring = 1;
    for (int j = 0; j < sectors; ++ j, ++ top_ring) {
        idx[k ++] = top_pole;
        idx[k ++] = top_ring + 1;
        idx[k ++] = top_ring;
    }
    // 第i层和第i+1层之间的三角形
    for (int i = 1; i <= stacks - 2; ++ i) {
        int k1 = (i - 1) * (sectors + 1) + 1; // 上层第0个顶点序号
        int k2 = k1 + sectors + 1; // 下层第0个顶点序号
        // 逆时针三角形
        //  k1 + 1  -  k1
        //     |       |
        //  k2 + 1  -  k2    
        for (int j = 0; j < sectors; ++ j, ++ k1, ++ k2) {
            idx[k ++] = k1; // 上层第j个顶点
            idx[k ++] = k1 + 1; // 上层第j+1个顶点
            idx[k ++] = k2; // 下层第j个顶点
            idx[k ++] = k1 + 1;
            idx[k ++] = k2 + 1;
            idx[k ++] = k2;
        }
    }
    // 底层三角形
    int bottom_pole = vert_count - 1;
    int bottom_ring = vert_count - 1 - (sectors + 1);
    for (int j = 0; j < sectors; ++ j, ++ bottom_ring) {
        idx[k ++] = bottom_ring;
        idx[k ++] = bottom_ring + 1;
        idx[k ++] = bottom_pole;
    }

    *vertices = verts;
    *vertex_count = vert_count;
    *indices = idx;
    *index_count = idx_count;

    verts = NULL; idx = NULL;
}
*/

// 生成单位圆柱体（半径=1，高度=1，中心在原点，Y轴为轴，底面y=-0.5，顶面y=0.5）
void generate_cylinder(int sectors,
                      float **vertices, int *vertex_count,
                      GLuint **indices, int *index_count) {
    if (sectors < 3) {
        fprintf(stderr, "Error! At least 3 sectors are required.\n");
        exit(EXIT_FAILURE);
    }
    int side_verts = 2 * (sectors + 1); // 侧面顶点，上下都是sectors + 1个
    int cap_verts = sectors + 2;          // 中心 + 圆环
    // 注意这里圆环和上面圆环法线方向不同，因为表面不连续。交界处每个顶点得算两次。
    int total_verts = side_verts + 2 * cap_verts;
    int side_idx = sectors * 6;
    int cap_idx = sectors * 3;
    int total_idx = side_idx + 2 * cap_idx;

    float *verts = (float *)my_malloc(total_verts * 6 * sizeof(float));
    GLuint *idx = (GLuint *)my_malloc(total_idx * sizeof(GLuint));

    if (!verts || !idx) {
        fprintf(stderr, "Error! Cannot allocate memory.\n");
        exit(EXIT_FAILURE);
    }

    float sector_step = 2. * (float)M_PI / sectors;
    float half_height = 0.5f;

    int v = 0;
    // 侧面顶点（每个扇区两个：底和顶，法线径向向外）
    // 竖着从下到上一条一条来
    for (int j = 0; j <= sectors; ++ j) {
        float angle = j * sector_step;
        float x = cosf(angle);
        float z = sinf(angle);
        // y是竖着向上的方向，所以x和z实际上是从从下往上才是二维的右手系，角度也得这么看。
        // 下面，然后上面
        // 底部
        verts[v ++] = x; verts[v ++] = -half_height; verts[v ++] = z;
        verts[v ++] = x; verts[v ++] = 0.0f        ; verts[v ++] = z; // 法线
        // 顶部
        verts[v ++] = x; verts[v ++] =  half_height; verts[v ++] = z;
        verts[v ++] = x; verts[v ++] = 0.0f        ; verts[v ++] = z; // 法线
    }

    // 底部圆盖（法线向下）
    verts[v ++] = 0.0f; verts[v ++] = -half_height; verts[v ++] = 0.0f; // 中心
    verts[v ++] = 0.0f; verts[v ++] = -1.0f       ;  verts[v ++] = 0.0f;
    for (int j = 0; j <= sectors; ++ j) {
        float angle = j * sector_step;
        float x = cosf(angle);
        float z = sinf(angle);
        verts[v ++] = x   ; verts[v ++] = -half_height; verts[v ++] = z;
        verts[v ++] = 0.0f; verts[v ++] = -1.0f       ; verts[v ++] = 0.0f;
    }

    // 顶部圆盖（法线向上）
    verts[v ++] = 0.0f; verts[v ++] = half_height; verts[v ++] = 0.0f; // 中心
    verts[v ++] = 0.0f; verts[v ++] = 1.0f       ; verts[v ++] = 0.0f;
    for (int j = 0; j <= sectors; ++ j) {
        float angle = j * sector_step;
        float x = cosf(angle), z = sinf(angle);
        verts[v ++] = x   ; verts[v ++] = half_height; verts[v ++] = z;
        verts[v ++] = 0.0f; verts[v ++] = 1.0f       ; verts[v ++] = 0.0f;
    }

    // 索引
    int k = 0;
    // 侧面
    // next + 1  -  base + 1
    //     |           |
    //   next    -   base
    for (int j = 0; j < sectors; ++ j) {
        int base = j * 2; // 下面序号
        int next = base + 2;
        idx[k ++] = base;
        idx[k ++] = base + 1;
        idx[k ++] = next;
        idx[k ++] = next;
        idx[k ++] = base + 1;
        idx[k ++] = next + 1;
    }
    // 底部圆盖（三角形扇）
    int bottom_center = side_verts;
    int bottom_ring = bottom_center + 1;
    for (int j = 0; j < sectors; ++ j) {
        idx[k ++] = bottom_center;
        idx[k ++] = bottom_ring + j;
        idx[k ++] = bottom_ring + j + 1;
    }
    // 顶部圆盖（注意顶点顺序以保证法线向外）
    int top_center = side_verts + cap_verts;
    int top_ring = top_center + 1;
    for (int j = 0; j < sectors; ++ j) {
        idx[k ++] = top_center;
        idx[k ++] = top_ring + j + 1;
        idx[k ++] = top_ring + j;
    }

    *vertices = verts;
    *vertex_count = total_verts;
    *indices = idx;
    *index_count = total_idx;
}
