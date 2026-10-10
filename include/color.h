#pragma once
#ifndef COLOR_H
#define COLOR_H

// HSV → RGB。H∈[0,360)，S,V∈[0,1]，输出 rgb 各分量 ∈[0,1]
void hsv_to_rgb(float h, float s, float v, float *r, float *g, float *b);

// HSL → RGB。H∈[0,360)，S,L∈[0,1]，输出 rgb 各分量 ∈[0,1]
void hsl_to_rgb(float h, float s, float l, float *r, float *g, float *b);

// RGB → 灰度（最经典的亮度公式）。r,g,b ∈[0,1]
void rgb_to_gray(float r, float g, float b, float *gray);

#endif // COLOR_H
