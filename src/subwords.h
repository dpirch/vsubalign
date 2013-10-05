#ifndef SUBWORDS_H_
#define SUBWORDS_H_

#include "common.h"

struct dict;
struct dictword;


typedef struct subnode {
    struct subnode *seq_next;
    struct subnode *word_next;
    struct dictword *word;
    unsigned seqnum;
    unsigned minstarttime;
    unsigned maxendtime;
} subnode_t;

typedef struct subnodelist {
    struct fixed_alloc *alloc;
    struct subnode *first, *last;
} subnodelist_t;


subnodelist_t *subnodelist_create(struct dict *dict);

void subnodelist_delete(subnodelist_t *wl);

void subnodelist_add(subnodelist_t *wl, struct dictword *word,
        unsigned minstarttime, unsigned maxendtime);


#endif
