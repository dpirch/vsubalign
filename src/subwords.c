#include "subwords.h"

#include "alloc.h"
#include "dict.h"
#include "hashtable.h"

static void reset_word_subnodes(void *item, void *userptr)
{
    ((struct dictword*)item)->subnodes = NULL;
    (void)userptr;
}

struct swlist *swlist_create(struct dict *dict)
{
    hashtable_foreach(dict->words, reset_word_subnodes, NULL, NULL);

    struct swlist *wl = xmalloc(sizeof *wl);
    *wl = (struct swlist){
        .alloc = fixed_allocator_create(sizeof (struct swnode), 256)
    };
    return wl;
}

void swlist_delete(struct swlist *wl)
{
    fixed_allocator_delete(wl->alloc);
    free(wl);
}

void swlist_append(struct swlist *wl, struct dictword *word,
        unsigned minstarttime, unsigned maxendtime)
{
    struct swnode *sw = fixed_alloc(wl->alloc);
    *sw = (struct swnode) {
        .word = word,
        .minstarttime = minstarttime,
        .maxendtime = maxendtime
    };

    if (word) {
        sw->word_next = word->subnodes,
        word->subnodes = sw;
    }

    if (wl->length++) {
        wl->last->seq_next = sw;
        sw->position = wl->last->position + 1;
        wl->last = sw;
    } else {
        wl->first = wl->last = sw;
    }
}


