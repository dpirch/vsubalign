#ifndef ALIGNMENT_H_
#define ALIGNMENT_H_

#include "common.h"
struct swlist;
struct lattice;

struct aln_pathnode {
    struct swnode *swnode;
    struct aln_pathnode *pred;
    timestamp_t time;
    unsigned refcount;
};


struct aln_treenode {
    unsigned minscore, maxscore;
    unsigned refcount;
    union {
        struct {
            struct aln_treenode *null; // always NULL for leaf
            struct aln_pathnode *path; // last node of best path, not null
        };
        struct {
            struct aln_treenode *right; // not null
            struct aln_treenode *left;  // may be null
        };
    };
};


struct alignment {
    struct pool_allocator *pathalloc;
    struct pool_allocator *treealloc;
    struct aln_treenode *tree;
    unsigned treewidth;
};

struct alignment *alignment_create(struct swlist *swl);

void alignment_delete(struct alignment *al);

void alignment_add_lattice(struct alignment *al, struct lattice *lat);





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
