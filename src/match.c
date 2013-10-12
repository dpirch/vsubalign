#include "match.h"

#include "alloc.h"

/*
struct matchstore *matchstore_create(void)
{
    struct matchstore *store = xmalloc(sizeof *store);
    store->nodealloc = pool_allocator_create(sizeof (struct matchnode), 1024);
    return store;
}

void matchstore_delete(struct matchstore *store)
{
    pool_allocator_delete(store->nodealloc);
    free(store);
}


struct matchnode * matchnode_create(unsigned time, struct subnode *subnode,
        struct matchnode *pred, struct matchstore *store)
{
    struct matchnode *node = pool_alloc(store->nodealloc);
    *node = (struct matchnode) {
        .subnode = subnode,
        .pred = pred,
        .time = time,
        .pathlength = pred ? pred->pathlength + 1: 1,
        .refcount = 1
    };
    if (pred) pred->refcount++;
    return node;
}

*/
void path_unref(struct pathnode *tail, struct pathstore *store)
{
    do {
        if (--tail->refcount > 0) break;
        struct pathnode *pred = tail->pred;
        pool_free(tail, store->path_alloc);
        tail = pred;
    } while (tail);
}

void pathtree_unref(struct pathtreenode *root, struct pathstore *store)
{
    if (--root->refcount > 0) return;

    if (root->isleaf) {
        if (root->path) path_unref(root->path, store);
    } else {
        if (root->left) pathtree_unref(root->left, store);
        if (root->right) pathtree_unref(root->right, store);
    }

    pool_free(root, store->pathtree_alloc);
}

