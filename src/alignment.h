#ifndef ALIGNMENT_H_
#define ALIGNMENT_H_

#include "common.h"
struct swlist;
struct lattice;


struct alnode {
    unsigned refcount;
    bool ispath;

    union {
        struct {
            unsigned minscore, maxscore;
            struct alnode *left, *right; // left may be null
        };
        struct {
            unsigned score;
            timestamp_t time;
            struct swnode *swnode;
            struct alnode *pred; // path node, or null for beginning of path
        };
    };
};


struct alignment {
    struct pool_allocator *alloc;
    struct alnode *pathes;
    unsigned width;
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
