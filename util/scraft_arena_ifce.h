#ifndef     SCRAFT_ARENA_IFCE_H_
#define     SCRAFT_ARENA_IFCE_H_

#include    "./scraft_arena.h"

struct scraft_arena *scraft_arena_new(struct scraft_model_alloc allocator, uint32_t basesize, uint32_t nfail, uint32_t nlarge, uint32_t nbasehooks);
void scraft_arena_delete(struct scraft_arena *arena);
void *scraft_arena_allocate(struct scraft_arena *arena, size_t blocksize);
void scraft_arena_tlinit(uint32_t cap);
struct scraft_arena_response scraft_arena_delay_dtor(struct scraft_arena *arena, void *object, void (*dtor)(void *, void *), void *user_arg);

#endif      // SCRAFT_ARENA_IFCE_H_
