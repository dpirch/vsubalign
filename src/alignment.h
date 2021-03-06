#ifndef ALIGNMENT_H_
#define ALIGNMENT_H_

#include "common.h"
struct swlist;
struct lattice;


struct alpathnode {
    unsigned refcount;
    timestamp_t time;
    struct swnode *swnode;
    struct alpathnode *pred; // null for beginning of path
};

struct alnode {
    bool ispath;
    unsigned minscore, maxscore; // same for path nodes
    unsigned refcount;

    union {
        struct {
            struct alnode *left, *right;
        };
        struct {
            struct alpathnode *tail; // null for empty path
        };
    };
};


struct alignment {
    struct pool_allocator *alloc;
    struct pool_allocator *pnalloc;
    struct alnode *pathes;
    struct alnode *empty;
    unsigned width;
};

struct alignment *alignment_create(struct swlist *swl);

void alignment_delete(struct alignment *al);

void alignment_add_lattice(struct alignment *al, struct lattice *lat);

void alignment_dump_final(const struct alignment *al);




/*struct matchstore *matchstore_create(void);
void matchstore_delete(struct matchstore *store);

struct matchnode *matchnode_create(unsigned time, struct subnode *subnode,
        struct matchnode *pred, struct matchstore *store);*/

/*void path_unref(struct alpathnode *end, struct pathtreestore *store);

void pathtree_unref(struct altreenode *root, struct pathtreestore *store);

struct alpathnode *path_lookup(unsigned maxpos, struct altreenode *root,
        unsigned rootlevel, unsigned rootoffset);
*/

#endif /* ALIGNMENT_H_ */
