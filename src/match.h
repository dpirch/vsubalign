#ifndef MATCH_H_
#define MATCH_H_

#include "common.h"

struct matchnode {
    struct subnode *subnode;
    struct matchnode *pred;
    unsigned time;
    // todo: scores...
    unsigned refcount;
};

struct matchstore {
    struct pool_allocator *alloc;
};

struct matchstore *matchstore_create(void);
void matchstore_delete(struct matchstore *store);

struct matchnode *matchnode_create(unsigned time, struct subnode *subnode,
        struct matchnode *pred, struct matchstore *store);

void matchnode_unref(struct matchnode *node, struct matchstore *store);


#endif /* MATCH_H_ */
