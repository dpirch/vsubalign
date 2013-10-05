#ifndef COMMON_H_
#define COMMON_H_

#include <stdbool.h>
#include <stdnoreturn.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <setjmp.h>


#define MIN(a, b) ((a)<(b)?(a):(b))
#define MAX(a, b) ((a)>(b)?(a):(b))

#define SWAP(type, a, b) do { type _tmp = a; a = b; b = _tmp; } while(0)


void warning(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
void error(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
noreturn void die(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
noreturn void die_from_check(const char *expr, const char *infunc);
#define CHECK(expr) (((expr)) ? (void)0 : die_from_check(# expr, __func__))

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);

void *grow_array(void *restrict ptr, size_t membsize,
        size_t *restrict capacity, size_t mincapacity);


typedef unsigned long hashval_t;

typedef struct hashnode {
    struct hashnode *next;
    hashval_t hashval;
} hashnode_t;



#define APPEND(array, alloc, length) \
    (((length) < (alloc) ? (array) : (array) = grow_array((array), \
            sizeof *(array), &(alloc), (length) + 1))[(length)++])


#define FOREACH(structtype, name, head, next) \
    for (structtype *name = (head), *_next; \
        name && (_next = name->next, 1); name = _next)
/*
#define MAKE_DYNARRAY(name, type) \
    typedef struct { type *array; size_t size, capacity; } name ## _t; \
    static inline void name ## _destroy(name ## _t *a) { free(a->array); } \
    static inline type *name ## _append(name ## _t *a) { \
        if (a->size == a->capacity) a->array = \
            grow_array(a->array, sizeof (type), &a->capacity, a->size + 1); \
        return a->array + a->size++; } \
    static inline type *name ## _nappend(name ## _t *a, size_t n) { \
        if (a->size + n > a->capacity) a->array = \
            grow_array(a->array, sizeof (type), &a->capacity, a->size + n); \
        type *new = a->array + a->size; \
        a->size += n; \
        return new; }


MAKE_DYNARRAY(stringbuf, char)
*/


/*
typedef struct {
    char *str;
    size_t len;
    size_t alloc;
} stringbuf_t;

void stringbuf_destroy(stringbuf_t *sb);
void stringbuf_append(stringbuf_t *sb, char c);
void stringbuf_term(stringbuf_t *sb);
*/







//typedef struct {
//    void *data, buffer

//} chunk_t;



#endif /* COMMON_H_ */
