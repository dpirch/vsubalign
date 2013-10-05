#ifndef ALLOC_H_
#define ALLOC_H_

#include "common.h"

typedef struct fixed_alloc fixed_alloc_t;
typedef struct var_alloc var_alloc_t;

fixed_alloc_t *fixed_alloc_create(size_t membsize, size_t chunklen);
void fixed_alloc_delete(fixed_alloc_t *al);
void *alloc_fixed(fixed_alloc_t *al);
void free_fixed(void *node, fixed_alloc_t *al);

var_alloc_t *var_alloc_create(size_t chunksize);
void var_alloc_delete(var_alloc_t *al);
void *alloc_var(size_t size, size_t align, var_alloc_t *al);

#endif /* ALLOC_H_ */
