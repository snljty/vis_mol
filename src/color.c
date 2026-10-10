#include "color.h"
#include <math.h>

// 根据色相所在 60° 区间，选 (R',G',B')，再加 m 得到最终 RGB
// c: chroma 色度，颜色的纯色强度，也就是剥离白色的亮度之后，剩下的那个点纯色有多强。
// HSV: c = v × s（明度 × 饱和度）。
// HSL: c = (1 - |2l - 1|) × s。
// x: 过渡色，处理"两个相邻纯色之间"的渐变 X = C × (1 - |(H/60 mod 2) - 1|)。
// m: 基础亮度: 把纯色"放到正确明暗上"。
// HSV: m = v - c。
// HSL: m = 1 - c / 2。
static void hue_sector(float h, float c, float x, float m,
                       float *r, float *g, float *b) {
    float rr, gg, bb;
    if (h < 60.0f)       { rr = c; gg = x; bb = 0.0f; }
    else if (h < 120.0f) { rr = x; gg = c; bb = 0.0f; }
    else if (h < 180.0f) { rr = 0.0f; gg = c; bb = x; }
    else if (h < 240.0f) { rr = 0.0f; gg = x; bb = c; }
    else if (h < 300.0f) { rr = x; gg = 0.0f; bb = c; }
    else                 { rr = c; gg = 0.0f; bb = x; }
    *r = rr + m;
    *g = gg + m;
    *b = bb + m;
}

// 把色相归一化到 [0, 360)
static float normalize_hue(float h) {
    // 第一次fmodf后是(-360.f, 360.f)，平移到(0.f, 720.f)，最后归一化到[0.f, 360.f)
    return fmodf(fmodf(h, 360.0f) + 360.0f, 360.0f);
}

void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b) {
    h = normalize_hue(h);
    float c = v * s;                                  // 色度（饱和度×明度）
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;                                  // 添加的"基础亮度"
    hue_sector(h, c, x, m, r, g, b);
}

void hsl_to_rgb(float h, float s, float l, float *r, float *g, float *b) {
    h = normalize_hue(h);
    float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;    // 色度
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = l - c / 2.0f;                           // 基础亮度
    hue_sector(h, c, x, m, r, g, b);
}

void rgb_to_gray(float r, float g, float b, float *gray) {
    // 最经典的亮度公式（Rec.601 / NTSC）
    *gray = 0.299f * r + 0.587f * g + 0.114f * b;
}

