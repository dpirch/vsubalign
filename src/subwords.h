#ifndef SUBWORDS_H_
#define SUBWORDS_H_

#include "common.h"

struct dict;
struct dictword;


struct swnode {
    struct swnode *seq_next;
    struct swnode *word_next;
    struct dictword *word;
    unsigned seqnum;
    unsigned minstarttime;
    unsigned maxendtime;
};

struct swnodelist {
    struct fixed_allocator *alloc;
    struct swnode *first, *last;
};


struct swnodelist *swnodelist_create(struct dict *dict);

void swnodelist_delete(struct swnodelist *wl);

void swnodelist_add(struct swnodelist *wl, struct dictword *word,
        unsigned minstarttime, unsigned maxendtime);


#endif
