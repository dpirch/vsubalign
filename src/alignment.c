#include "alignment.h"

#include "alloc.h"
#include "subwords.h"
#include "lattice.h"
#include "dict.h"

static unsigned cc[10] = {0};

static inline struct alnode *ref(struct alnode *node)
{
    node->refcount++;
    return node;
}

static void delete(struct alnode *node, struct alignment *al);

static inline void unref(struct alnode *node, struct alignment *al)
{
    if (--node->refcount == 0)
        delete(node, al);
}

static void delete(struct alnode *node, struct alignment *al)
{
    if (node->ispath) {
        struct alpathnode *pn = node->tail;
        while (pn && --pn->refcount == 0) {
            struct alpathnode *pred = pn->pred;
            pool_free(node, al->pnalloc);
            pn = pred;
        }
    }
    else {
        unref(node->left, al);
        unref(node->right, al);
    }
    pool_free(node, al->alloc);
}


static struct alnode *make_tree(struct alnode *left, struct alnode *right,
        struct alignment *al)
{
    struct alnode *node = pool_alloc(al->alloc);
    *node = (struct alnode) {
        .minscore = left ? left->minscore : 0,
        .maxscore = right->maxscore,
        .refcount = 1,
        .left = ref(left),
        .right = ref(right)
    };
    return node;
}


static void merge_path_complete(struct alnode **dest,
        struct alnode *path, struct alignment *al)
{
    if ((*dest)->ispath) {
        if (path->minscore > (*dest)->minscore) {
            unref(*dest, al);
            *dest = ref(path);
        }
    }
    else if (path->minscore >= (*dest)->maxscore) {
        unref(*dest, al);
        *dest = ref(path);
    }
    else if (path->minscore > (*dest)->minscore) {
        struct alnode *left = ref((*dest)->left);
        struct alnode *right = ref((*dest)->right);
        unref(*dest, al);

        merge_path_complete(&left, path, al);
        merge_path_complete(&right, path, al);
        *dest = make_tree(left, right, al);

        unref(left, al);
        unref(right, al);
    }
}

static void merge_path_partial(struct alnode **dest, unsigned width,
        struct alnode *path, unsigned pos, struct alignment *al)
{
    assert(pos < width);
    if (pos == 0) {
        merge_path_complete(dest, path, al);
    }
    else if (path->minscore > (*dest)->minscore) {
        struct alnode *left, *right;
        if ((*dest)->ispath) {
            left = ref(*dest);
            right = ref(*dest);
        } else {
            left = ref((*dest)->left);
            right = ref((*dest)->right);
        }
        unref(*dest, al);

        unsigned splitpos = width / 2;
        if (pos < splitpos) {
            merge_path_partial(&left, splitpos, path, pos, al);
            merge_path_complete(&right, path, al);
        } else {
            merge_path_partial(&right, splitpos, path, pos - splitpos, al);
        }
        *dest = make_tree(left, right, al);
        unref(left, al);
        unref(right, al);
    }
}



static void merge_tree(struct alnode **dest,
        struct alnode *tree, struct alignment *al)
{
    if (*dest == tree) {}
    else if (tree->maxscore <= (*dest)->minscore) {}
    else if (tree->minscore >= (*dest)->maxscore) {
        unref(*dest, al);
        *dest = ref(tree);
    }
    else if (tree->ispath) {
        merge_path_complete(dest, tree, al);
    }
    else if ((*dest)->ispath) {
        struct alnode *new = ref(tree);
        merge_path_complete(&new, *dest, al);
        unref(*dest, al);
        *dest = new;
    }
    else if ((*dest)->left == tree->left && (*dest)->right == tree->right) {}
    else {
        struct alnode *left = ref((*dest)->left);
        struct alnode *right = ref((*dest)->right);

        merge_tree(&left, tree->left, al);
        merge_tree(&right, tree->right, al);

        if ((*dest)->left != left || (*dest)->right != right) {
            unref(*dest, al);
            *dest = make_tree(left, right, al);
        }

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
        width = splitpos;
    }
    return tree;
}


struct alignment *alignment_create(struct swlist *swl)
{
    struct alignment *al = xmalloc(sizeof *al);
    *al = (struct alignment) {
        .alloc = pool_allocator_create(sizeof (struct alnode), 256),
        .pnalloc = pool_allocator_create(sizeof (struct alpathnode), 256)
    };

    al->width = 1;
    while (al->width < swl->length)
        al->width *= 2;

    al->empty = pool_alloc(al->alloc);
    *al->empty = (struct alnode) {
        .ispath = true,
        .minscore = 0,
        .maxscore = 0,
        .refcount = 1,
        .tail = NULL
    };

    al->pathes = ref(al->empty);

    return al;
}

void alignment_delete(struct alignment *al)
{
    unref(al->pathes, al);
    unref(al->empty, al);

    pool_allocator_delete(al->alloc);
    pool_allocator_delete(al->pnalloc);
    free(al);
}

/*
static void dump_path(struct alnode *path)
{
    do {
        printf(" %u.%s",
                path->swnode->position, path->swnode->word->string);
        path = path->pred;
    } while (path);
    printf("\n");
}

static void dump_tree(struct alnode *tree, unsigned width, unsigned pos)
{
    if (tree && tree->ispath && pos == tree->swnode->position) {
        printf("%u:", pos);
        dump_path(tree);
    }
    else if (tree && !tree->ispath) {
        dump_tree(tree->left, width / 2, pos);
        dump_tree(tree->right, width / 2, pos + width / 2);
    }
}
*/

void alignment_add_lattice(struct alignment *al, struct lattice *lat)
{
    // list of lattice nodes with all predecesors already processed
    struct latnode *ready = NULL;

    // init ready list and init pathes for these nodes
    FOREACH(struct latnode, node, lat->nodelist, next) {
        node->nentries_remain = node->nentries;
        if (node->nentries_remain == 0) {
            node->ready_next = ready;
            ready = node;
            node->pathes = ref(al->pathes);
        } else {
            node->pathes = ref(al->empty);
        }
    }

    // reset al->pathes, will be used to store pathes at end of segment
    unref(al->pathes, al);
    al->pathes = ref(al->empty);

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

            if (node->word) {
                FOREACH(struct swnode, swnode, node->word->subnodes, word_next) {
                    struct alnode *base = NULL;
                    if (swnode->position > 0)
                        base = tree_lookup(
                                node->pathes, al->width, swnode->position - 1);

                    struct alpathnode *tail = pool_alloc(al->pnalloc);
                    *tail = (struct alpathnode) {
                        .refcount = 0,
                        .time = node->time,
                        .swnode = swnode,
                        .pred = base ? base->tail : NULL };

                    // tree node to reuse until it is actually stored in a tree
                    struct alnode *newpath = NULL;

                    FOREACH(struct latlink, link, node->exits_head, exits_next) {

                        // todo
                        unsigned score = base ? base->minscore + 1 : 1;

                        if (!newpath) {
                            newpath = pool_alloc(al->alloc);
                            *newpath = (struct alnode) {
                                .ispath = true,
                                .minscore = score, .maxscore = score,
                                .refcount = 1,
                                .tail = tail };
                        } else {
                            newpath->minscore = newpath->maxscore = score;
                        }

                        struct latnode *dest = link->to;
                        merge_path_partial(&dest->pathes, al->width,
                                newpath, swnode->position, al);

                        if (newpath->refcount > 1) { // has been stored in tree
                            newpath = NULL;
                            tail->refcount++;
                        }
                    }
                    // todo measure
                    if (newpath) pool_free(newpath, al->alloc);

                    if (tail->refcount == 0) pool_free(tail, al->pnalloc);
                    else if (tail->pred) tail->pred->refcount++;
                }
            }
        }

        unref(node->pathes, al);
    }

}


void alignment_dump_final(const struct alignment *al)
{
    struct alnode *path = tree_lookup(al->pathes, al->width, al->width - 1);
    if (path) {
        struct alpathnode *pn = path->tail;
        while (pn) {
            printf("%u:%02u.%02u: %s (%.2f)\n",
                    pn->time / 60000, pn->time / 1000 % 60, pn->time / 10 % 100,
                    pn->swnode->word->string,
                    ((double)pn->time - pn->swnode->minstarttime) / ((double)pn->swnode->maxendtime - pn->swnode->minstarttime));

            pn = pn->pred;
        }
    }

    for (unsigned i = 0; i < sizeof cc / sizeof *cc; i++)
        fprintf(stderr, "cc[%u]: %u\n", i, cc[i]);
}
