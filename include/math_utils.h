#pragma once
#ifndef MATH_UTILS_H
#define MATH_UTILS_H

#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846264
#endif

#define mat3_get(m, row, col) (m)[(col) * 3 + (row)]
#define mat4_get(m, row, col) (m)[(col) * 4 + (row)]

void mat4_set_rotation_xyz(float *m, float alpha, float beta, float gamma);
void mat4_set_rotation_yzx(float *m, float alpha, float beta, float gamma);
void mat4_set_rotation_zxy(float *m, float alpha, float beta, float gamma);
void mat4_set_rotation_zyx(float *m, float alpha, float beta, float gamma);
void mat4_set_rotation_xzy(float *m, float alpha, float beta, float gamma);
void mat4_set_rotation_yxz(float *m, float alpha, float beta, float gamma);
void mat4_mul(float *res, const float *a, const float *b);
void mat4_lmul_inplace(float *m, const float *a);
void mat4_rmul_inplace(float *m, const float *a);
void mat4_set_rotation(float *m, const float *axis, float angle);
void mat4_set_rotation_half_turn(float *m, const float *axis);
void mat4_set_rotation_by_cos_angle(float *m, const float *axis, float cos_angle);
void mat4_set_perspective(float *m, float fov, float aspect, float near_, float far_);
void mat4_set_orthographic(float *m, float left, float right, float bottom, float top, float near_, float far_);
void mat4_set_look_at(float *m, const float *eye, const float *center, const float *up);
void mat4_topleft_to_mat3(float *m3, const float *m4);
static inline void mat4_identity(float *m);
static inline void mat4_set_translation(float *m, float x, float y, float z);
static void mat4_prepend_translation(float *m, const float *trans);
static inline void mat4_set_rotation_x(float *m, float angle);
static inline void mat4_set_rotation_y(float *m, float angle);
static inline void mat4_set_rotation_z(float *m, float angle);
static inline void mat4_set_rotation_x_half_turn(float *m);
static inline void mat4_set_rotation_y_half_turn(float *m);
static inline void mat4_set_rotation_z_half_turn(float *m);
static inline void mat4_set_scale(float *m, float sx, float sy, float sz);
static inline void mat4_copy(float *m, const float *old);
static inline void mat4_append_scale(float *m, const float *scale);
static inline void mat4_topleft_transpose_mul_vec3(float *res, const float *m, const float *v);

static inline float vec3_dot(const float *a, const float *b);
static inline void vec3_cross(float *res, const float *a, const float *b);
static inline void vec3_add(float *res, const float *a, const float *b);
static inline void vec3_sub(float *res, const float *a, const float *b);
static inline void vec3_scale(float *res, float s);
static inline void vec3_scale_inverse(float *res, float s);
static inline float vec3_length(const float *v);
static inline void vec3_normalize(float *v);
static inline void vec3_copy(float *v, const float *old);
static inline void vec3_opposite(float *v, const float *old);
float vec3_included_angle_cos(const float *a, const float *b);
float vec3_included_angle(const float *a, const float *b);

void mat3_identity(float *m);
void mat3_to_mat4_topleft(float *m4, const float *m3);
void mat3_make_cross(float *m, const float *a);
void mat3_add(float *res, const float *a, const float *b);
void mat3_add_inplace(float *res, const float *a);
void mat3_sub(float *res, const float *a, const float *b);
void mat3_sub_inplace(float *res, const float *a);
void mat3_mul(float *res, const float *a, const float *b);
void mat3_mul_vec(float *res, const float *m, const float *v);
void mat3_scale(float *m, float scale);
void mat3_scale_inverse(float *m, float scale);
void vec3_outer(float *res, const float *a, const float *b);
void mat3_set_rotation(float *m, const float *axis, float angle);
void mat3_set_rotation_not_normalized(float *m, const float *axis_not_normalized, float angle);
void mat3_lmul_inplace(float *m, const float *a);
void mat3_rmul_inplace(float *m, const float *a);
static inline void mat3_copy(float *m, const float *old);
static inline void mat3_append_scale(float *m, const float *scale);
static inline void mat3_append_inverse_scale(float *m, const float *scale);
static inline void mat3_transpose(float *m, const float *old);
static inline void mat3_transpose_inplace(float *m);

static inline float vec3_dot(const float *a, const float *b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static inline void vec3_cross(float *res, const float *a, const float *b) {
    res[0] = a[1] * b[2] - a[2] * b[1];
    res[1] = a[2] * b[0] - a[0] * b[2];
    res[2] = a[0] * b[1] - a[1] * b[0];
}

static inline void vec3_add(float *res, const float *a, const float *b) {
    res[0] = a[0] + b[0];
    res[1] = a[1] + b[1];
    res[2] = a[2] + b[2];
}

static inline void vec3_sub(float *res, const float *a, const float *b) {
    res[0] = a[0] - b[0];
    res[1] = a[1] - b[1];
    res[2] = a[2] - b[2];
}

static inline void vec3_scale(float *res, float s) {
    res[0] *= s;
    res[1] *= s;
    res[2] *= s;
}

static inline void vec3_scale_inverse(float *res, float s) {
    if (s == 0.f) return;
    res[0] /= s;
    res[1] /= s;
    res[2] /= s;
}

static inline float vec3_length(const float *v) {
    return sqrtf(vec3_dot(v, v));
}

static inline void vec3_normalize(float *v) {
    float len = vec3_length(v);
    if (len > 0.f) vec3_scale_inverse(v, len);
}

static inline void vec3_copy(float *v, const float *old) {
    memcpy(v, old, 3 * sizeof(float));
}

static inline void vec3_opposite(float *v, const float *old) {
    v[0] = -old[0];
    v[1] = -old[1];
    v[2] = -old[2];
}


static inline void mat4_identity(float *m) {
    // column-major, 4 by 4 as an array
    // mat @ (x, y, z, 1).T
    memset(m, 0, 16 * sizeof(float));
    mat4_get(m, 0, 0) = 
    mat4_get(m, 1, 1) = 
    mat4_get(m, 2, 2) = 
    mat4_get(m, 3, 3) = 
    1.0f;
}

static void mat4_prepend_translation(float *m, const float *trans) {
    // prepend translation to a matrix
    // note that other parts of m should be set before.
    mat4_get(m, 0, 3) += trans[0];
    mat4_get(m, 1, 3) += trans[1];
    mat4_get(m, 2, 3) += trans[2];
}

static inline void mat4_set_translation(float *m, float x, float y, float z) {
    /*
    R     T
          r
      o   a
          n
        t s
    0 0 0 1
    */
    // Rot contains scale (and reflexction)
    // rotation is anti-clockwise.
    // 0 0 0 1 if projection, otherwise affine
    mat4_identity(m);
    mat4_get(m, 0, 3) = x;
    mat4_get(m, 1, 3) = y;
    mat4_get(m, 2, 3) = z;
}

// angle in unit radians

static inline void mat4_set_rotation_x(float *m, float angle) {
    mat4_identity(m);
    float c = cosf(angle);
    float s = sinf(angle);
    mat4_get(m, 1, 1) =  c;
    mat4_get(m, 2, 1) =  s;
    mat4_get(m, 1, 2) = -s;
    mat4_get(m, 2, 2) =  c;
}

static inline void mat4_set_rotation_y(float *m, float angle) {
    mat4_identity(m);
    float c = cosf(angle);
    float s = sinf(angle);
    mat4_get(m, 0, 0) =  c;
    mat4_get(m, 2, 0) = -s;
    mat4_get(m, 0, 2) =  s;
    mat4_get(m, 2, 2) =  c;
}

static inline void mat4_set_rotation_z(float *m, float angle) {
    mat4_identity(m);
    float c = cosf(angle);
    float s = sinf(angle);
    mat4_get(m, 0, 0) =  c;
    mat4_get(m, 1, 0) =  s;
    mat4_get(m, 0, 1) = -s;
    mat4_get(m, 1, 1) =  c;
}

static inline void mat4_set_rotation_x_half_turn(float *m) {
    memset(m, 0, 16 * sizeof(float));
    mat4_get(m, 0, 0) =  1.f;
    mat4_get(m, 1, 1) = -1.f;
    mat4_get(m, 2, 2) = -1.f;
    mat4_get(m, 3, 3) =  1.f;
}

static inline void mat4_set_rotation_y_half_turn(float *m) {
    memset(m, 0, 16 * sizeof(float));
    mat4_get(m, 0, 0) = -1.f;
    mat4_get(m, 1, 1) =  1.f;
    mat4_get(m, 2, 2) = -1.f;
    mat4_get(m, 3, 3) =  1.f;
}

static inline void mat4_set_rotation_z_half_turn(float *m) {
    memset(m, 0, 16 * sizeof(float));
    mat4_get(m, 0, 0) = -1.f;
    mat4_get(m, 1, 1) = -1.f;
    mat4_get(m, 2, 2) =  1.f;
    mat4_get(m, 3, 3) =  1.f;
}


static inline void mat4_set_scale(float *m, float sx, float sy, float sz) {
    float scale = sx * sy * sz;
    if (scale == 0.) {
        fprintf(stderr, "Note: decreasing dimension.\n");
    } else if (scale < 0.) {
        fprintf(stderr, "Note: inversing chiral.\n");
    }
    mat4_identity(m);
    mat4_get(m, 0, 0) = sx;
    mat4_get(m, 1, 1) = sy;
    mat4_get(m, 2, 2) = sz;
}

static inline void mat4_append_scale(float *m, const float *scale) {
    // append scale to a matrix
    // note that other parts of m should be set before.
    // note m is not write-only, it will be read then written.
    float f = scale[0] * scale[1] * scale[2];
    if (f == 0.) {
        fprintf(stderr, "Note: decreasing dimension.\n");
    } else if (f < 0.) {
        fprintf(stderr, "Note: inversing chiral.\n");
    }
    mat4_get(m, 0, 0) *= scale[0];
    mat4_get(m, 1, 0) *= scale[0];
    mat4_get(m, 2, 0) *= scale[0];
    mat4_get(m, 0, 1) *= scale[1];
    mat4_get(m, 1, 1) *= scale[1];
    mat4_get(m, 2, 1) *= scale[1];
    mat4_get(m, 0, 2) *= scale[2];
    mat4_get(m, 1, 2) *= scale[2];
    mat4_get(m, 2, 2) *= scale[2];
}

static inline void mat4_topleft_transpose_mul_vec3(float *res, const float *m, const float *v) {
    res[0] = vec3_dot(m    , v);
    res[1] = vec3_dot(m + 4, v);
    res[2] = vec3_dot(m + 8, v);
}


static inline void mat3_transpose(float *m, const float *old) {
    mat3_get(m, 0, 0) = mat3_get(old, 0, 0);
    mat3_get(m, 0, 1) = mat3_get(old, 1, 0);
    mat3_get(m, 0, 2) = mat3_get(old, 2, 0);
    mat3_get(m, 1, 0) = mat3_get(old, 0, 1);
    mat3_get(m, 1, 1) = mat3_get(old, 1, 1);
    mat3_get(m, 1, 2) = mat3_get(old, 2, 1);
    mat3_get(m, 2, 0) = mat3_get(old, 0, 2);
    mat3_get(m, 2, 1) = mat3_get(old, 1, 2);
    mat3_get(m, 2, 2) = mat3_get(old, 2, 2);
}

static inline void mat3_transpose_inplace(float *m) {
    float t;
    t = mat3_get(m, 0, 1); mat3_get(m, 0, 1) = mat3_get(m, 1, 0); mat3_get(m, 1, 0) = t;
    t = mat3_get(m, 0, 2); mat3_get(m, 0, 2) = mat3_get(m, 2, 0); mat3_get(m, 2, 0) = t;
    t = mat3_get(m, 1, 2); mat3_get(m, 1, 2) = mat3_get(m, 2, 1); mat3_get(m, 2, 1) = t;
}

static inline void mat3_append_scale(float *m, const float *scale) {
    // append scale to a matrix
    // note that other parts of m should be set before.
    // note m is not write-only, it will be read then written.
    float f = scale[0] * scale[1] * scale[2];
    if (f == 0.) {
        fprintf(stderr, "Note: decreasing dimension.\n");
    } else if (f < 0.) {
        fprintf(stderr, "Note: inversing chiral.\n");
    }
    mat3_get(m, 0, 0) *= scale[0];
    mat3_get(m, 1, 0) *= scale[0];
    mat3_get(m, 2, 0) *= scale[0];
    mat3_get(m, 0, 1) *= scale[1];
    mat3_get(m, 1, 1) *= scale[1];
    mat3_get(m, 2, 1) *= scale[1];
    mat3_get(m, 0, 2) *= scale[2];
    mat3_get(m, 1, 2) *= scale[2];
    mat3_get(m, 2, 2) *= scale[2];
}

static inline void mat3_append_inverse_scale(float *m, const float *scale) {
    // append 1 / scale to a matrix
    // note that other parts of m should be set before.
    // note m is not write-only, it will be read then written.
    float f = scale[0] * scale[1] * scale[2];
    if (f == 0.) {
        fprintf(stderr, "Note: decreasing dimension.\n");
    } else if (f < 0.) {
        fprintf(stderr, "Note: inversing chiral.\n");
    }
    mat3_get(m, 0, 0) /= scale[0];
    mat3_get(m, 1, 0) /= scale[0];
    mat3_get(m, 2, 0) /= scale[0];
    mat3_get(m, 0, 1) /= scale[1];
    mat3_get(m, 1, 1) /= scale[1];
    mat3_get(m, 2, 1) /= scale[1];
    mat3_get(m, 0, 2) /= scale[2];
    mat3_get(m, 1, 2) /= scale[2];
    mat3_get(m, 2, 2) /= scale[2];
}

static inline void mat4_copy(float *m, const float *old) {
    memcpy(m, old, 16 * sizeof(float));
}

static inline void mat3_copy(float *m, const float *old) {
    memcpy(m, old, 9 * sizeof(float));
}

#endif

