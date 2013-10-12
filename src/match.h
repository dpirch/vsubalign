#ifndef MATCH_H_
#define MATCH_H_

#include "common.h"

struct pathnode {
    struct swnode *swnode;
    struct pathnode *pred;
    timestamp_t time;
    unsigned refcount;
};


struct pathtreenode {
    bool isleaf;
    unsigned score; // max score for inner node
    unsigned refcount;

    union {
        struct pathnode *path;
        struct { struct pathtreenode *left, *right; };
    };
};


struct pathstore {
    struct pool_allocator *path_alloc;
    struct pool_allocator *pathtree_alloc;
    unsigned rootlevel;
};

/*struct matchstore *matchstore_create(void);
void matchstore_delete(struct matchstore *store);

struct matchnode *matchnode_create(unsigned time, struct subnode *subnode,
        struct matchnode *pred, struct matchstore *store);*/

void path_unref(struct pathnode *end, struct pathstore *store);

void pathtree_unref(struct pathtreenode *root, struct pathstore *store);

struct pathnode *path_lookup(unsigned maxpos, struct pathtreenode *root,
        unsigned rootlevel, unsigned rootoffset);


#endif /* MATCH_H_ */
