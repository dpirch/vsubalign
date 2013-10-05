#include "alloc.h"

struct fixed_alloc {
    size_t membsize;
    size_t chunklen;
    void *recycled;
    struct chunk *chunks;
    size_t pos;
};

struct var_alloc {
    size_t chunksize;
    struct chunk *chunks;
    size_t pos;
};

struct chunk {
    struct chunk *next;
    max_align_t buffer[];
};


fixed_alloc_t *fixed_alloc_create(size_t membsize, size_t chunklen)
{
    fixed_alloc_t *al = xmalloc(sizeof *al);
    *al = (fixed_alloc_t){
        .membsize = MAX(membsize, sizeof (void*)),
        .chunklen = chunklen
    };
    return al;
}

void fixed_alloc_delete(fixed_alloc_t *al)
{
    FOREACH(struct chunk, chunk, al->chunks, next)
        free(chunk);
    free(al);
}

void *alloc_fixed(fixed_alloc_t *al)
{
    void *ret;
    if (al->recycled) {
        // return recycled node if available
        ret = al->recycled;
        al->recycled = *(void**)ret;

    } else {
        // create a new chunk if the current chunk is full
        if (!al->chunks || al->pos >= al->chunklen) {
            struct chunk *chunk =
                    xmalloc(sizeof *chunk + al->chunklen * al->membsize);
            chunk->next = al->chunks;
            al->chunks = chunk;
            al->pos = 0;
        }

        // return the next free slot
        ret = (char*)al->chunks->buffer + al->pos++ * al->membsize;
    }
    return ret;
}

void free_fixed(void *node, fixed_alloc_t *al)
{
    *(void**)node = al->recycled;
    al->recycled = node;
}


var_alloc_t *var_alloc_create(size_t chunksize)
{
    var_alloc_t *al = xmalloc(sizeof *al);
    *al = (var_alloc_t){ .chunksize = chunksize };
    return al;
}

void var_alloc_delete(var_alloc_t *al)
{
    FOREACH(struct chunk, chunk, al->chunks, next)
        free(chunk);
    free(al);
}

void *alloc_var(size_t size, size_t align, var_alloc_t *al)
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

