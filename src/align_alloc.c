#include "align_alloc.h"

static inline int is_pow_of_2(size_t x) {
    return x && ((x & (x - 1)) == 0);
}

static inline size_t round_up(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

static inline size_t align_block_size(size_t nbytes, size_t alignment) {
    return round_up(nbytes, alignment);
}

#ifdef USE_ALIGN_ALLOC
void *my_malloc(size_t nbytes) {
    if (!is_pow_of_2(SIMD_alignment) || SIMD_alignment < sizeof(void *)) {
        fprintf(stderr, "Error! Alignment must be a power of 2 and no less than %zu.\n", sizeof(void *));
        exit(EXIT_FAILURE);
    }

    size_t usable = align_block_size(nbytes, SIMD_alignment);

    void *raw = malloc(usable + SIMD_alignment + sizeof(void *));
    if (!raw) return NULL;

    uintptr_t raw_addr = (uintptr_t)raw;

    uintptr_t aligned = (raw_addr + sizeof(void *) + SIMD_alignment - 1) & ~(uintptr_t)(SIMD_alignment - 1);

    *((void **)aligned - 1) = raw;

    return (void *)aligned;
}
#else
void *my_malloc(size_t nbytes) {return malloc(nbytes);}
#endif

#ifdef USE_ALIGN_ALLOC
void my_free(void *ptr) {
    if (!ptr) return;
    void *raw = *((void **)ptr - 1);
    free(raw);
}
#else
void my_free(void *ptr) {free(ptr);}
#endif
