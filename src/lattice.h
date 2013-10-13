#ifndef LATTICE_H_
#define LATTICE_H_

#include "common.h"

struct ps_lattice_s;
struct dict;

struct latnode {
    struct dictword *word;
    timestamp_t time;
    struct latlink *exits_head;
    unsigned nentries;
    struct latnode *next;

    union {
        struct { // for generation
            hashval_t hashval;
            const struct ps_latnode_s *psnode;
        };
        struct { // for alignment
            struct aln_treenode *pathes;
            unsigned nentries_remain;
            struct latnode *ready_next;
        };
    };

};

struct latlink {
    struct latnode *to;
    struct latlink *exits_next;
    // TODO: audio score
};

struct lattice {
    struct latnode *nodelist;
    struct fixed_allocator *node_alloc;
    struct fixed_allocator *link_alloc;
};


struct lattice *lattice_create(
        struct ps_lattice_s *pslattice, unsigned framerate,
        const struct dict *dict);

void lattice_delete(struct lattice *lat);


#endif /* LATTICE_H_ */
