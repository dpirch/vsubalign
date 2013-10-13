#include "lattice.h"

#include <ps_lattice.h>
#include "dict.h"
#include "alloc.h"
#include "hashtable.h"


static hashval_t nodehash(const ps_latnode_t *psnode)
{
    hashval_t hash = (uintptr_t)psnode;
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

static bool nodematch(const void *item, const void *key)
{
    return key == ((const struct latnode*)item)->psnode;
}

struct createarg {
    ps_lattice_t *pslattice;
    unsigned framerate;
    const struct dict *dict;
    struct lattice *lattice;
};

void *nodecreate(const void *key, void *userptr)
{
    ps_latnode_t *psnode = (ps_latnode_t*)key;
    const struct createarg *arg = userptr;
    struct latnode *node = fixed_alloc(arg->lattice->node_alloc);
    *node = (struct latnode) {
        .word = dict_lookup(
                arg->dict, ps_latnode_baseword(arg->pslattice, psnode)),
        .time = ps_latnode_times(psnode, NULL, NULL) * 1000 / arg->framerate,
        .psnode = psnode,
        .next = arg->lattice->nodelist
    };
    arg->lattice->nodelist = node;
    return node;
}


struct lattice *lattice_create(
        struct ps_lattice_s *pslattice, unsigned framerate,
        const struct dict *dict)
{
    // TODO: nentries

    struct lattice *lat = xmalloc(sizeof *lat);
    *lat = (struct lattice){
            .node_alloc = fixed_allocator_create(sizeof (struct latnode), 256),
            .link_alloc = fixed_allocator_create(sizeof (struct latlink), 256)
    };

    struct hashtable *nodes =
            hashtable_create(offsetof(struct latnode, hashval));

    struct createarg createarg = { pslattice, framerate, dict, lat };

    for (ps_latnode_iter_t *psnodeit = ps_latnode_iter(pslattice);
            psnodeit; psnodeit = ps_latnode_iter_next(psnodeit))
    {
        ps_latnode_t *psnode = ps_latnode_iter_node(psnodeit);
        struct latnode *node = hashtable_lookup_or_add(
                nodes, nodehash(psnode), nodematch, psnode,
                nodecreate, &createarg);

        ps_latlink_iter_t *pslinkit = ps_latnode_exits(psnode);

        for (; pslinkit; pslinkit = ps_latlink_iter_next(pslinkit)) {
            ps_latlink_t *pslink = ps_latlink_iter_link(pslinkit);
            ps_latnode_t *psdest = ps_latlink_nodes(pslink, NULL);
            // TODO: int32 ascr; ps_latlink_prob(lattice, link, &ascr); ... ogmath_log_to_ln(logmath, ascr)

            struct latnode *dest = hashtable_lookup_or_add(
                    nodes, nodehash(psdest), nodematch, psdest,
                    nodecreate, &createarg);

            struct latlink *link = fixed_alloc(lat->link_alloc);
            *link = (struct latlink) {
                .to = dest,
                .exits_next = node->exits_head
            };
            node->exits_head = link;
            dest->nentries++;
        }
    }

    hashtable_delete(nodes);
    return lat;
}


void lattice_delete(struct lattice *lat)
{
    fixed_allocator_delete(lat->node_alloc);
    fixed_allocator_delete(lat->link_alloc);
    free(lat);
}
