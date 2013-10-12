#ifndef LATTICE_H_
#define LATTICE_H_

#include "common.h"

struct ps_lattice_s;
struct dict;

struct latnode {
    struct dictword *word;
    timestamp_t time;
    struct latlink *out_head;

    // for generation
    hashval_t hashval;
    const struct ps_latnode_s *psnode;

};

struct latlink {
    struct latnode *to;
    struct latlink *out_next;
    // TODO: audio score
};

struct lattice {
    struct latnode *start;
    struct fixed_allocator *node_alloc;
    struct fixed_allocator *link_alloc;
};


struct lattice *lattice_create(
        struct ps_lattice_s *pslattice, unsigned framerate,
        const struct dict *dict);

void lattice_delete(struct lattice *lat);


#endif /* LATTICE_H_ */
