#include "subwords.h"

#include "alloc.h"
#include "dict.h"
#include "hashtable.h"

static void reset_word_subnodes(void *item, void *userptr)
{
    ((dictword_t*)item)->subnodes = NULL;
    (void)userptr;
}

subnodelist_t *subnodelist_create(dict_t *dict)
{
    hashtable_foreach(dict->words, reset_word_subnodes, NULL, NULL);

    subnodelist_t *wl = xmalloc(sizeof *wl);
    *wl = (subnodelist_t){
        .alloc = fixed_alloc_create(sizeof (subnode_t), 256)
    };
    return wl;
}

void subnodelist_delete(subnodelist_t *wl)
{
    fixed_alloc_delete(wl->alloc);
    free(wl);
}

void subnodelist_add(subnodelist_t *wl, dictword_t *word,
        unsigned minstarttime, unsigned maxendtime)
{
    struct subnode *sw = alloc_fixed(wl->alloc);
    *sw = (struct subnode) {
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


