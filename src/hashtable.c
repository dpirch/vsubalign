#include "hashtable.h"




static void resize(hashtable_t *ht, size_t newsize)
{
    assert(newsize >= ht->count);

    void **newtable = xmalloc(newsize * sizeof *newtable);
    for (size_t i = 0; i < newsize; i++)
        newtable[i] = NULL;

    for (size_t i = 0; i < ht->tablesize; i++) {
        if (!ht->table[i]) continue;
        hashval_t h = *(hashval_t*)((char*)ht->table[i] + ht->hashval_offset);

        size_t j = h & (newsize - 1);
        while (newtable[j])
            j = (j + 1) & (newsize - 1);

        newtable[j] = ht->table[i];
    }

    free(ht->table);
    ht->table = newtable;
    ht->tablesize = newsize;
}


hashtable_t *hashtable_create(size_t hashval_offset)
{
    hashtable_t *ht = xmalloc(sizeof *ht);
    *ht = (hashtable_t){ .hashval_offset = hashval_offset };
    resize(ht, 256);
    return ht;
}


void hashtable_delete(hashtable_t *ht)
{
    free(ht->table);
    free(ht);
}


static void **lookup_ptr(const hashtable_t *ht, hashval_t hashval,
        bool (*match)(const void *item, const void *key), const void *key)
{
    size_t i = hashval & (ht->tablesize - 1);
    while (ht->table[i]) {
        if (match(ht->table[i], key)) break;
        i = (i + 1) & (ht->tablesize - 1);
    }
    return &ht->table[i];
}

void *hashtable_lookup(const hashtable_t *ht, hashval_t hashval,
        bool (*match)(const void *item, const void *key), const void *key)
{
    return *lookup_ptr(ht, hashval, match, key);
}

void *hashtable_lookup_or_add(hashtable_t *ht, hashval_t hashval,
        bool (*match)(const void *item, const void *key), const void *key,
        void *(*create)(const void *key, void *userptr), void *userptr)
{
    void **slot = lookup_ptr(ht, hashval, match, key);
    void *item = *slot;
    if (!item) {
        item = create(key, userptr);
        if (item) {
            *slot = item;
            *(hashval_t*)((char*)item + ht->hashval_offset) = hashval;
            if (++ht->count > ht->tablesize / 2)
                resize(ht, ht->tablesize * 4);
        }
    }
    return item;
}

size_t hashtable_count(const hashtable_t *ht) { return ht->count; }

void hashtable_foreach(const hashtable_t *ht,
        void (*func)(void *item, void *userptr), void *userptr,
        int (*ptr_compar)(const void *, const void *))
{
    if (ptr_compar) {
        void **buf = xmalloc(sizeof *buf * ht->count);
        void **p = buf;
        for (size_t i = 0; i < ht->tablesize; i++)
            if (ht->table[i])
                *p++ = ht->table[i];

        qsort(buf, ht->count, sizeof *buf, ptr_compar);
        for (size_t i = 0; i < ht->count; i++)
            func(buf[i], userptr);

        free(buf);
    } else {
        for (size_t i = 0; i < ht->tablesize; i++)
            if (ht->table[i])
                func(ht->table[i], userptr);
    }
}

