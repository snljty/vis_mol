#pragma once
#ifndef GEOMETRY_H
#define GEOMETRY_H

#include <glad/glad.h>
#ifdef HAVE_UTHASH 
#include <uthash.h>
#endif

// 做顶点去重的版本
void subdivideIcoSphereOnce(float **vertices, int *vertexCount,
                            GLuint **indices, int *indexCount);

#ifdef HAVE_UTHASH

// 给棱心顶点去重用的含有 Hash 表的结构体
typedef struct {
    int ends[2];          // 排序后的两个顶点序号 (min, max)
    int mid;              // 这条边中点对应的顶点序号
    UT_hash_handle hh;    // UTHash 句柄
} edgeMap;

// 给上面函数用的辅助函数，用来查找两个端点的中点序号，如果找不到，就新建一个，并弄好顶点位置和法线
int getOrCreateMidpointIndex(edgeMap **edgesPtr, float *verts, int *vertIdxPtr, int *vPtr, int a, int b);

#else // HAVE_UTHASH

// 排序和二分查找用的辅助函数
int compVertEnd(const void *a, const void *b);

#endif // HAVE_UTHASH

// void generateGeoSphere(int sectors, int stacks,
//                     float **vertices, int *vertexCount,
//                     GLuint **indices, int *indexCount);

void generateIcoSphere(int numSubdivisionsIter, 
                    float **vertices, int *vertexCount,
                    GLuint **indices, int *indexCount);

void generateCylinder(int sectors,
                      float **vertices, int *vertexCount,
                      GLuint **indices, int *indexCount);

#endif
