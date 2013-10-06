#ifndef ALLOC_H_
#define ALLOC_H_

#include "common.h"

typedef struct fixed_allocator fixed_allocator_t;
typedef struct pool_allocator pool_allocator_t;
typedef struct var_allocator var_allocator_t;

fixed_allocator_t *fixed_allocator_create(size_t membsize, size_t chunklen);
void fixed_allocator_delete(fixed_allocator_t *al);
void *fixed_alloc(fixed_allocator_t *al);

pool_allocator_t *pool_allocator_create(size_t membsize, size_t chunklen);
void pool_allocator_delete(pool_allocator_t *al);
void *pool_alloc(pool_allocator_t *al);
void pool_free(void *object, pool_allocator_t *al);

var_allocator_t *var_allocator_create(size_t chunksize);
void var_allocator_delete(var_allocator_t *al);
void *var_alloc(size_t size, size_t align, var_allocator_t *al);

#endif /* ALLOC_H_ */
