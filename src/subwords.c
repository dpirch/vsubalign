#include "subwords.h"

#include "alloc.h"
#include "dict.h"
#include "hashtable.h"

static void reset_word_subnodes(void *item, void *userptr)
{
    ((dictword_t*)item)->subnodes = NULL;
    (void)userptr;
}

struct swnodelist *swnodelist_create(dict_t *dict)
{
    hashtable_foreach(dict->words, reset_word_subnodes, NULL, NULL);

    struct swnodelist *wl = xmalloc(sizeof *wl);
    *wl = (struct swnodelist){
        .alloc = fixed_allocator_create(sizeof (struct swnode), 256)
    };
    return wl;
}

void swnodelist_delete(struct swnodelist *wl)
{
    fixed_allocator_delete(wl->alloc);
    free(wl);
}

void swnodelist_add(struct swnodelist *wl, dictword_t *word,
        unsigned minstarttime, unsigned maxendtime)
{
    struct swnode *sw = fixed_alloc(wl->alloc);
    *sw = (struct swnode) {
        .word = word,
        .word_next = word->subnodes,
        .minstarttime = minstarttime,
        .maxendtime = maxendtime
    };

    word->subnodes = sw;

    if (wl->last) {
        wl->last->seq_next = sw;
        sw->seqnum = wl->last->seqnum + 1;
        wl->last = sw;
    } else {
        wl->first = wl->last = sw;
    }
}


