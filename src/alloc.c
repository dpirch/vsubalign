#include "alloc.h"

struct fixed_allocator {
    size_t membsize;
    size_t chunklen;
    struct chunk *chunks;
    size_t pos;
};

struct pool_allocator {
    struct fixed_allocator fa;
    void *recycled;
};

struct var_allocator {
    size_t chunksize;
    struct chunk *chunks;
    size_t pos;
};

struct chunk {
    struct chunk *next;
    max_align_t buffer[];
};


fixed_allocator_t *fixed_allocator_create(size_t membsize, size_t chunklen)
{
    fixed_allocator_t *al = xmalloc(sizeof *al);
    *al = (fixed_allocator_t){ .membsize = membsize, .chunklen = chunklen };
    return al;
}

void fixed_allocator_delete(fixed_allocator_t *al)
{
    FOREACH(struct chunk, chunk, al->chunks, next)
        free(chunk);
    free(al);
}

void *fixed_alloc(fixed_allocator_t *al)
{
    if (!al->chunks || al->pos >= al->chunklen) {
        struct chunk *chunk =
                xmalloc(sizeof *chunk + al->chunklen * al->membsize);
        chunk->next = al->chunks;
        al->chunks = chunk;
        al->pos = 0;
    }

    return (char*)al->chunks->buffer + al->pos++ * al->membsize;
}


pool_allocator_t *pool_allocator_create(size_t membsize, size_t chunklen)
{
    pool_allocator_t *al = xmalloc(sizeof *al);
    *al = (pool_allocator_t){
        .fa = {
            .membsize = MAX(membsize, sizeof (void*)),
            .chunklen = chunklen
        }
    };
    return al;

}

void pool_allocator_delete(pool_allocator_t *al)
{
    FOREACH(struct chunk, chunk, al->fa.chunks, next)
        free(chunk);
    free(al);
}

void *pool_alloc(pool_allocator_t *al)
{
    if (al->recycled) {
        void *ret = al->recycled;
        al->recycled = *(void**)ret;
        return ret;
    } else {
        return fixed_alloc(&al->fa);
    }
}

void pool_free(void *object, pool_allocator_t *al)
{
    *(void**)object = al->recycled;
    al->recycled = object;
}


var_allocator_t *var_allocator_create(size_t chunksize)
{
    var_allocator_t *al = xmalloc(sizeof *al);
    *al = (var_allocator_t){ .chunksize = chunksize };
    return al;
}

void var_allocator_delete(var_allocator_t *al)
{
    FOREACH(struct chunk, chunk, al->chunks, next)
        free(chunk);
    free(al);
}

void *var_alloc(size_t size, size_t align, var_allocator_t *al)
{
    assert(align > 0 && !((align - 1) & align)); // power of two

    size_t alignedpos = ((al->pos - 1) | (align - 1)) + 1;
    if (al->chunks && alignedpos + size <= al->chunksize) {
        // request fits into current chunk
        void *ret = (char*)al->chunks->buffer + alignedpos;
        al->pos = alignedpos + size;
        return ret;
    }
    else if (al->chunks && size > al->chunksize / 8) {
        // for large requests we allocate an exclusive chunk for the request
        // instead of wasting the remaining space in the current chunk
        struct chunk *chunk = xmalloc(sizeof *chunk + size);
        chunk->next = al->chunks->next;
        al->chunks->next = chunk;
        return chunk->buffer;

    } else {
        // start a new chunk
        struct chunk *chunk = xmalloc(sizeof *chunk + al->chunksize);
        chunk->next = al->chunks;
        al->chunks = chunk;
        al->pos = size;
        return chunk->buffer;
    }
}

