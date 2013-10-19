#ifndef DICT_H_
#define DICT_H_

#include "common.h"

struct dict {
    struct hashtable *words;
    struct var_allocator *wordalloc;
    struct var_allocator *pronalloc;
};

struct dictword {
    hashval_t hashval;
    struct swnode *subnodes;
    struct dictpron *pronlist;
    char string[];
};

struct dictpron {
    struct dictpron *next;
    char string[];
};

struct dict *dict_create(void);
void dict_delete(struct dict *dict);

struct dictword *dict_lookup(const struct dict *dict, const char *str);
struct dictword *dict_lookup_or_add(struct dict *dict, const char *str);

struct dictword *dict_lookup_or_copy(struct dict *dict, const char *str,
        const struct dict *srcdict);

bool dict_read(struct dict *dict, const char *filename);

bool dict_write(const struct dict *dict, const char *filename);


#endif /* DICT_H_ */
