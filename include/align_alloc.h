#pragma once
#ifndef ALIGN_ALLOC_H
#define ALIGN_ALLOC_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

static const size_t SIMD_alignment = 64; // AVX512

#ifdef USE_ALIGN_ALLOC
void *my_malloc(size_t nbytes);
void my_free(void *ptr);
#else
void *my_malloc(size_t nbytes);
void my_free(void *ptr);
#endif

#endif
