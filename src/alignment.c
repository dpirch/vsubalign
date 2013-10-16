#include "alignment.h"

#include "alloc.h"
#include "subwords.h"
#include "lattice.h"
#include "dict.h"

static inline struct alnode *ref(struct alnode *node)
{
    if (node) node->refcount++;
    return node;
}

static void unref(struct alnode *node, struct alignment *al)
{
    if (!node || --node->refcount > 0) return;

    if (node->ispath) {
        do {
            struct alnode *pred = node->pred;
            pool_free(node, al->alloc);
            node = pred;
        } while (node && --node->refcount == 0);
    }
    else {
        unref(node->left, al);
        unref(node->right, al);
        pool_free(node, al->alloc);
    }
}

static struct alnode *make_tree(struct alnode *left, struct alnode *right,
        struct alignment *al)
{
    struct alnode *node = pool_alloc(al->alloc);
    *node = (struct alnode) {
        .refcount = 1,
        .minscore = left ? left->ispath ? left->score : left->minscore : 0,
        .maxscore = right->ispath ? right->score : right->maxscore,
        .left = ref(left),
        .right = ref(right)
    };
    return node;
}


static struct alnode *make_path(struct alnode *base,
        struct swnode *swnode, timestamp_t time, struct alignment *al)
{
    // todo
    unsigned score = base ? base->score + 1 : 1;

    struct alnode *path = pool_alloc(al->alloc);
    *path = (struct alnode) {
        .refcount = 1,
        .ispath = true,
        .score = score,
        .time = time,
        .swnode = swnode,
        .pred = ref(base)
    };
    return path;
}


static void merge_path_complete(struct alnode **tree,
        struct alnode *path, struct alignment *al)
{
    if (!*tree || ((*tree)->ispath && path->score > (*tree)->score) ||
            (!(*tree)->ispath && path->score >= (*tree)->maxscore)) {
        unref(*tree, al);
        *tree = ref(path);
    }
    else if (!(*tree)->ispath && path->score > (*tree)->minscore) {
        struct alnode *left = ref((*tree)->left);
        struct alnode *right = ref((*tree)->right);
        unref(*tree, al);

        merge_path_complete(&left, path, al);
        merge_path_complete(&right, path, al);
        *tree = make_tree(left, right, al);

        unref(left, al);
        unref(right, al);
    }
}

static void merge_path_partial(struct alnode **tree, unsigned width,
        struct alnode *path, unsigned pos, struct alignment *al)
{
    assert(pos < width);
    if (pos == 0) {
        merge_path_complete(tree, path, al);
    }
    else if (!*tree || ((*tree)->ispath && path->score > (*tree)->score) ||
            (!(*tree)->ispath && path->score > (*tree)->minscore)) {
        struct alnode *left, *right;
        if (!*tree) {
            left = right = NULL;
        } else if ((*tree)->ispath) {
            left = ref(*tree);
            right = ref(*tree);
        } else {
            left = ref((*tree)->left);
            right = ref((*tree)->right);
        }
        unref(*tree, al);

        unsigned splitpos = width / 2;

        if (pos < splitpos) {
            merge_path_partial(&left, splitpos, path, pos, al);
            merge_path_complete(&right, path, al);
        } else {
            merge_path_partial(&right, splitpos, path, pos - splitpos, al);
        }
        *tree = make_tree(left, right, al);
        unref(left, al);
        unref(right, al);
    }
}

static void merge_tree(struct alnode **tree,
        struct alnode *other, struct alignment *al)
{
    if (!other) {}
    else if (!*tree) {
        *tree = ref(other);
    } else if (other->ispath) {
        merge_path_complete(tree, other, al);
    } else if ((*tree)->ispath) {
        struct alnode *new = ref(other);
        merge_path_complete(&new, *tree, al);
        unref(*tree, al);
        *tree = new;
    } else {
        struct alnode *left = ref((*tree)->left);
        struct alnode *right = ref((*tree)->right);
        unref(*tree, al);

        merge_tree(&left, other->left, al);
        merge_tree(&right, other->right, al);
        *tree = make_tree(left, right, al);

        unref(left, al);
        unref(right, al);
    }
}

static struct alnode *tree_lookup(
        struct alnode *tree, unsigned width, unsigned pos)
{
    while (tree && !tree->ispath) {
        unsigned splitpos = width / 2;
        if (pos < splitpos)
            tree = tree->left;
        else
            tree = tree->right, pos -= splitpos;
    }
    return tree;
}


struct alignment *alignment_create(struct swlist *swl)
{
    struct alignment *al = xmalloc(sizeof *al);
    *al = (struct alignment) {
        .alloc = pool_allocator_create(sizeof (struct alnode), 256),
    };

    al->width = 1;
    while (al->width < swl->length)
        al->width *= 2;

    return al;
}

void alignment_delete(struct alignment *al)
{
    // TODO: deallocation/check?

    pool_allocator_delete(al->alloc);
    free(al);
}




/*
static struct aln_pathnode *tree_bestpath(
        const struct aln_treenode *root, unsigned *score)
{
    while (xx !isleaf(root))
        root = root->right;

    *score = root->minscore;
    return root->path;
}*/




void alignment_add_lattice(struct alignment *al, struct lattice *lat)
{
    // list of lattice nodes with all predecesors already processed
    struct latnode *ready = NULL;

    // init ready list and init pathes for these nodes
    FOREACH(struct latnode, node, lat->nodelist, next) {
        node->pathes = NULL;
        node->nentries_remain = node->nentries;
        if (node->nentries_remain == 0) {
            node->ready_next = ready;
            ready = node;
            node->pathes = ref(al->pathes);
        }
    }

    // reset al->pathes, will be used to store pathes at end of segment
    unref(al->pathes, al);
    al->pathes = NULL;


    // traverse lattice
    while (ready) {
        struct latnode *node = ready;
        ready = node->ready_next;

        if (!node->exits_head) {
            merge_tree(&al->pathes, node->pathes, al);
        }
        else {

            // todo: audio scores!

            FOREACH(struct latlink, link, node->exits_head, exits_next) {
                struct latnode *dest = link->to;
                merge_tree(&dest->pathes, node->pathes, al);

                if (--dest->nentries_remain == 0)
                    dest->ready_next = ready, ready = dest;
            }

            FOREACH(struct swnode, swnode, node->word->subnodes, word_next) {
                struct alnode *base = NULL;
                if (swnode->position > 0)
                    tree_lookup(node->pathes, al->width, swnode->position - 1);

                struct alnode *newpath = make_path(
                        base, swnode, node->time, al);

                FOREACH(struct latlink, link, node->exits_head, exits_next) {
                    struct latnode *dest = link->to;
                    merge_path_partial(&dest->pathes, al->width,
                            newpath, swnode->position, al);
                }
                unref(newpath, al);
            }
        }

        unref(node->pathes, al);
    }

}





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
