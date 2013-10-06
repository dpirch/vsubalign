#include "match.h"

#include "alloc.h"

struct matchstore *matchstore_create(void)
{
    struct matchstore *store = xmalloc(sizeof *store);
    store->alloc = pool_allocator_create(sizeof (struct matchnode), 1024);
    return store;
}

void matchstore_delete(struct matchstore *store)
{
    pool_allocator_delete(store->alloc);
    free(store);
}


struct matchnode * matchnode_create(unsigned time, struct subnode *subnode,
        struct matchnode *pred, struct matchstore *store)
{
    struct matchnode *node = pool_alloc(store->alloc);
    *node = (struct matchnode) {
        .subnode = subnode,
        .pred = pred,
        .time = time,
        .refcount = 1
    };
    if (pred) pred->refcount++;
    return node;
}


void matchnode_unref(struct matchnode *node, struct matchstore *store)
{
    while (node && --node->refcount == 0) {
        struct matchnode *pred = node->pred;
        pool_free(node, store->alloc);
        node = pred;
    }
}
