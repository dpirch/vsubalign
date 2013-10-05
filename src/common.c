#include "common.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>


void warning(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fputs("Warning: ", stderr);
    vfprintf(stderr, fmt, args);
    fputs("\n", stderr);
}

void error(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fputs("Error: ", stderr);
    vfprintf(stderr, fmt, args);
    fputs("\n", stderr);
}

noreturn void die(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    fputs("Fatal error: ", stderr);
    vfprintf(stderr, fmt, args);
    fputs("\n", stderr);
    exit(EXIT_FAILURE);
}

noreturn void die_from_check(const char *expr, const char *infunc)
{
    die("check failed: (%s), in %s", expr, infunc);
}

#define NOMEM_MESSAGE "Out of memory: failed to allocate %zu bytes"

void *xmalloc(size_t size)
{
    void *ptr = malloc(size);
    if (!ptr && size) die(NOMEM_MESSAGE, size);
    return ptr;
}


void *xrealloc(void *ptr, size_t size)
{
    ptr = realloc(ptr, size);
    if (!ptr && size) die(NOMEM_MESSAGE, size);
    return ptr;
}

void *grow_array(void *restrict ptr, size_t membsize,
        size_t *restrict capacity, size_t mincapacity)
{
    while (mincapacity > *capacity) {
        CHECK(*capacity <= SIZE_MAX / 2);
        *capacity = MAX(*capacity * 2, 64);
    }

    CHECK(!membsize || *capacity <= SIZE_MAX / membsize);
    return xrealloc(ptr, membsize * *capacity);
}

/*
void stringbuf_destroy(stringbuf_t *sb) { free(sb->str); }

void stringbuf_append(stringbuf_t *sb, char c)
{
    if (sb->len >= sb->alloc)
        sb->str = grow_array(sb->str, 1, &sb->alloc, sb->len + 1);
    sb->str[sb->len++] = c;
}

void stringbuf_term(stringbuf_t *sb)
{
    if (sb->len >= sb->alloc)
        sb->str = grow_array(sb->str, 1, &sb->alloc, sb->len + 1);
    sb->str[sb->len] = 0;
}*/



/*

static bool read_line(FILE *restrict file, char **restrict buffer,
        size_t *restrict alloc, size_t *restrict length)
{
    size_t n = 0;
    int ch;

    while (ch = getc(file), ch != EOF && ch != '\n' && ch != '\r')
        *(unsigned char*)&APPEND(*buffer, *alloc, n) = ch;

    if (*length) *length = n;
    bool succeeded = n > 0 || ch != EOF;
    APPEND(*buffer, *alloc, n) = '\0';

    // read CRLF linebreak completely
    if (ch == '\r' && (ch = getc(file)) != '\n') ungetc(ch);

    return succeeded;
}

*/


