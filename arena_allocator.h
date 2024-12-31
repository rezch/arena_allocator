#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>


#define MIN_CHUNK_SIZE  16

#define PTR_MOVE(ptr, bytes) (uint8_t*)(ptr) + (bytes)
#define PTR_MOVE_BACK(ptr, bytes) (uint8_t*)(ptr) - (bytes)


struct chunk_t {
    size_t          size;
    void            *mem;
    struct chunk_t  *next;
    struct chunk_t  *prev;
    bool            used;
    bool            alloc;
};

struct mem_arena_t {
    size_t          ccnt;
    struct chunk_t  *head;
    size_t          free_size;
    struct chunk_t  *chunks;
    struct chunk_t  *free_chunk;
};

static void mark_chunk(struct mem_arena_t *arena, struct chunk_t *chunk);

static struct chunk_t *add_chunk(struct mem_arena_t *arena);

static void pop_chunk(struct mem_arena_t *arena, struct chunk_t *chunk);

static struct chunk_t *acquire_chunk(struct mem_arena_t *arena);

static void release_chunk(struct mem_arena_t *arena, struct chunk_t *chunk);

static struct chunk_t *merge_chunks(struct mem_arena_t *arena, struct chunk_t *lhs, struct chunk_t *rhs);

static struct chunk_t *split_chunk(struct mem_arena_t *arena, struct chunk_t *chunk, size_t size);

static struct chunk_t *try_split_chunk(struct mem_arena_t *arena, struct chunk_t *chunk, size_t size);

static struct chunk_t *find_chunk(struct mem_arena_t *arena, void *p);

struct mem_arena_t *arena_init(void *buf, size_t size);

void *arena_alloc(struct mem_arena_t *arena, size_t size);

void arena_free(struct mem_arena_t *arena, void *p);
