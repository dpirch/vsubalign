#ifndef DICT_H_
#define DICT_H_

#include "common.h"

typedef struct dict {
    struct hashtable *words;
    struct var_alloc *wordalloc;
} dict_t;

typedef struct dictword {
    hashval_t hashval;
    struct subnode *subnodes;
    char string[];
} dictword_t;

dict_t *dict_create(void);
void dict_delete(dict_t *dict);

dictword_t *dict_lookup(const dict_t *dict, const char *str);
dictword_t *dict_lookup_or_add(dict_t *dict, const char *str);

bool dict_read(dict_t *dict, const char *filename);



#endif /* DICT_H_ */
