#include "arena_allocator.h"


static void mark_chunk(struct mem_arena_t *arena, struct chunk_t *chunk) {
    chunk->size  = 0;
    chunk->mem   = NULL;
    chunk->next  = NULL;
    chunk->prev  = NULL;
    chunk->used  = 0;
    chunk->alloc = 1;
    arena->ccnt++;
}

static struct chunk_t *acquire_chunk(struct mem_arena_t *arena) {
    const struct chunk_t *end = arena->chunks + arena->chunks_count * sizeof(struct chunk_t);

    if (arena->free_chunk < end && arena->free_chunk->alloc == 0) {
        mark_chunk(arena, arena->free_chunk);
        return arena->free_chunk++;
    }

    for (struct chunk_t *chunk = arena->chunks; chunk < end; ++chunk) {
        if (chunk->alloc == 0) {
            arena->free_chunk = chunk;
            mark_chunk(arena, arena->free_chunk);
            return arena->free_chunk++;
        }
    }

    return NULL;
}

static void release_chunk(struct mem_arena_t *arena, struct chunk_t *chunk) {
    chunk->alloc = 0;
    arena->ccnt--;
}

static struct chunk_t *merge_chunks(struct mem_arena_t *arena, struct chunk_t *lhs, struct chunk_t *rhs) {
    lhs->size += rhs->size;
    lhs->next = rhs->next;

    if (rhs->next != NULL) {
        rhs->next->prev = lhs;
    }

    release_chunk(arena, rhs);

    return lhs;
}

static struct chunk_t *split_chunk(struct mem_arena_t *arena, struct chunk_t *chunk, size_t size) {
    struct chunk_t *new_chunk = acquire_chunk(arena);
    if (new_chunk == NULL) {
        return NULL;
    }

    new_chunk->size = size;
    new_chunk->mem  = PTR_MOVE(chunk->mem, chunk->size - size);
    new_chunk->next = chunk->next;
    new_chunk->prev = chunk;
    if (new_chunk->next != NULL) {
        new_chunk->next->prev = new_chunk;
    }

    chunk->next = new_chunk;
    chunk->size -= size;

    return new_chunk;
}

static struct chunk_t *try_split_chunk(struct mem_arena_t *arena, struct chunk_t *chunk, size_t size) {
    if (chunk == NULL) {
        return NULL;
    }
    if (chunk->size < size + MIN_CHUNK_SIZE) {
        return chunk;
    }
    return split_chunk(arena, chunk, size);
}

static struct chunk_t *find_chunk(struct mem_arena_t *arena, void *p) {
    for (struct chunk_t *i = arena->head; i != NULL; i = i->next) {
        if (p == i->mem) {
            return i;
        }
    }
    return NULL;
}

struct mem_arena_t *arena_init(void *buf, size_t size) {
    struct mem_arena_t *arena = (struct mem_arena_t *) malloc(sizeof(struct mem_arena_t));
    if (arena == NULL) {
        return NULL;
    }

    arena->chunks_count = size / (sizeof(struct chunk_t) + MIN_CHUNK_SIZE);
    if (arena->chunks_count * sizeof(struct chunk_t) > size) {
        arena->chunks_count = size / sizeof(struct chunk_t);
    }
    arena->chunks = (struct chunk_t *) buf;

    for (struct chunk_t *chunk = arena->chunks; chunk < arena->chunks + arena->chunks_count; ++chunk) {
        chunk->alloc = 0;
    }

    arena->ccnt = 0;
    arena->free_chunk = arena->chunks;
    arena->head = acquire_chunk(arena);

    if (arena->head == NULL) {
        free(arena);
        return NULL;
    }

    arena->free_size = size - arena->chunks_count * sizeof(struct chunk_t);
    arena->buf = PTR_MOVE(buf, arena->chunks_count * sizeof(struct chunk_t));

    arena->head->size = arena->free_size;
    arena->head->mem = arena->buf;

    return arena;
}

void *arena_alloc(struct mem_arena_t *arena, size_t size) {
    if (size < MIN_CHUNK_SIZE) {
        size = MIN_CHUNK_SIZE;
    }

    if (size > arena->free_size) {
        return NULL;
    }

    struct chunk_t *chunk = NULL;
    for (struct chunk_t *curr = arena->head; curr != 0; curr = curr->next) {
        if (curr->used == 0 && curr->size >= size) {
            chunk = curr;
            break;
        }
    }

    chunk = try_split_chunk(arena, chunk, size);
    if (chunk == NULL) {
        return NULL;
    }

    chunk->used = 1;
    arena->free_size -= chunk->size;

    return chunk->mem;
}

void arena_free(struct mem_arena_t *arena, void *p) {
    struct chunk_t *chunk = find_chunk(arena, p);

    if (chunk == NULL) {
        return;
    }

    arena->free_size += chunk->size;
    chunk->used = 0;

    while (chunk->prev != NULL && chunk->prev->used == 0) {
        chunk = merge_chunks(arena, chunk->prev, chunk);
    }

    while (chunk->next != NULL && chunk->next->used == 0) {
        chunk = merge_chunks(arena, chunk, chunk->next);
    }
}
