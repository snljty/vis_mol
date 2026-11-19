#pragma once
#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <glad/glad.h>
#ifdef HAVE_UTHASH 
#include <uthash.h>
#endif

// 做顶点去重的版本
void subdivide_icosphere_once(float **vertices, int *vertex_count,
                            GLuint **indices, int *index_count);

#ifdef HAVE_UTHASH

// 给棱心顶点去重用的含有 Hash 表的结构体
typedef struct {
    int ends[2];          // 排序后的两个顶点序号 (min, max)
    int mid;              // 这条边中点对应的顶点序号
    UT_hash_handle hh;    // UTHash 句柄
} edge_map;

// 给上面函数用的辅助函数，用来查找两个端点的中点序号，如果找不到，就新建一个，并弄好顶点位置和法线
int get_or_create_midpoint_index(edge_map **edges_ptr, float *verts, int *vert_idx_ptr, int *v_ptr, int a, int b);

#else // HAVE_UTHASH

// 排序和二分查找用的辅助函数
int comp_vert_end(const void *a, const void *b);

#endif // HAVE_UTHASH

// void generate_geosphere(int sectors, int stacks,
//                     float **vertices, int *vertex_count,
//                     GLuint **indices, int *index_count);

void generate_icosphere(int num_subdivisions_iter, 
                    float **vertices, int *vertex_count,
                    GLuint **indices, int *index_count);

void generate_cylinder(int sectors,
                      float **vertices, int *vertex_count,
                      GLuint **indices, int *index_count);

#endif
