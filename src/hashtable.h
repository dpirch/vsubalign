#ifndef HASHTABLE_H_
#define HASHTABLE_H_

#include "common.h"

typedef struct hashtable {
    void **table;
    size_t tablesize;            // current table size, power of two
    size_t count;            // number of items in table
    size_t hashval_offset;  // offset of hashval within individual items
} hashtable_t;

hashtable_t *hashtable_create(size_t hashval_offset);
void hashtable_delete(hashtable_t *ht);

void *hashtable_lookup(const hashtable_t *ht, hashval_t hashval,
        bool (*match)(const void *item, const void *key), const void *key);

void *hashtable_lookup_or_add(hashtable_t *ht, hashval_t hashval,
        bool (*match)(const void *item, const void *key), const void *key,
        void *(*create)(const void *key, void *userptr), void *userptr);

void hashtable_foreach(const hashtable_t *ht,
        void (*func)(void *item, void *userptr), void *userptr,
        int (*ptr_compar)(const void *, const void *));


#endif /* HASHTABLE_H_ */
