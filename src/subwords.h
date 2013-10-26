#ifndef SUBWORDS_H_
#define SUBWORDS_H_

#include "common.h"

struct dictword;


struct swnode {
    struct swnode *seq_next;
    struct swnode *word_next;
    struct dictword *word;
    unsigned position;
    unsigned minstarttime;
    unsigned maxendtime;
};

struct swlist {
    struct fixed_allocator *alloc;
    struct swnode *first, *last;
    unsigned length;
};


struct swlist *swlist_create(void);

void swlist_delete(struct swlist *wl);

void swlist_append(struct swlist *wl, struct dictword *word,
        unsigned minstarttime, unsigned maxendtime);


#endif
