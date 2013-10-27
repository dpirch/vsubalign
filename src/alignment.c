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

/*
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
}*/

static struct alnode *make_tree_haverefs(struct alnode *left, struct alnode *right,
        struct alignment *al)
{
    struct alnode *node = pool_alloc(al->alloc);
    *node = (struct alnode) {
        .minscore = left ? left->minscore : 0,
        .maxscore = right->maxscore,
        .refcount = 1,
        .left = left,
        .right = right
    };
    return node;
}


static struct alnode *merge_path_complete(struct alnode *into,
        struct alnode *path, struct alignment *al)
{
    assert(path->ispath && path->minscore > into->minscore);

    if (into->ispath) {
        return ref(path);
    }
    else if (path->minscore >= into->maxscore) {
        return ref(path);
    }
    else {
        struct alnode *left = merge_path_complete(into->left, path, al);
        struct alnode *right = path->minscore > into->right->minscore ?
                merge_path_complete(into->right, path, al) :
                ref(into->right);

        return make_tree_haverefs(left, right, al);
    }
}


static struct alnode *merge_pathes(struct alnode *into, unsigned width,
        struct alnode *path, unsigned pos, struct alignment *al)
{
    assert(into->ispath && path->ispath &&
            pos > 0 && pos < width && into->minscore < path->minscore);

    unsigned splitpos = width / 2;

    struct alnode *left = pos < splitpos ?
            merge_pathes(into, splitpos, path, pos, al) :
            ref(into);

    struct alnode *right = pos > splitpos ?
            merge_pathes(into, splitpos, path, pos - splitpos, al) :
            ref(path);

    return make_tree_haverefs(left, right, al);
}

static struct alnode *merge_path_partial(struct alnode *into, unsigned width,
        struct alnode *path, unsigned pos, struct alignment *al)
{
    assert(path->ispath &&
            pos > 0 && pos < width &&
            path->minscore > into->minscore);

    if (into->ispath) {
        return merge_pathes(into, width, path, pos, al);
    }
    else {
        unsigned splitpos = width / 2;

        struct alnode *left;
        if (pos < splitpos) {
            left = merge_path_partial(into->left, splitpos, path, pos, al);
        } else {
            left = NULL;
        }

        struct alnode *right;
        if (path->minscore > into->right->minscore) {
            if (pos > splitpos) {
                right = merge_path_partial(
                        into->right, splitpos, path, pos - splitpos, al);
            } else {
                right = merge_path_complete(into->right, path, al);
            }
        } else {
            right = NULL;
        }

        if (left || right) {
            return make_tree_haverefs(
                    left ? left : ref(into->left),
                    right ? right : ref(into->right), al);
        } else {
            return NULL;
        }
    }
}



static struct alnode *merge_tree(struct alnode *into,
        struct alnode *tree, struct alignment *al)
{
    assert(tree->maxscore > into->minscore);

    if (tree->minscore >= into->maxscore) {
        return ref(tree);
    }
    else if (tree->ispath) {
        return merge_path_complete(into, tree, al);
    }
    else if (into->ispath) {
        if (into->minscore > tree->minscore) {
            return merge_path_complete(tree, into, al);
        } else {
            return ref(tree);
        }
    }
    else {
        struct alnode *left;
        if (tree->left != into->left &&
                tree->left->maxscore > into->left->minscore) {
            left = merge_tree(into->left, tree->left, al);
        } else {
            left = NULL;
        }

        struct alnode *right;
        if (tree->right != into->right &&
                tree->right->maxscore > into->right->minscore) {
            right = merge_tree(into->right, tree->right, al);
        } else {
            right = NULL;
        }

        if (left || right) {
            return make_tree_haverefs(
                    left ? left : ref(into->left),
                    right ? right : ref(into->right), al);
        } else {
            return NULL;
        }
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

            // add to post-segment result
            if (node->pathes->maxscore > al->pathes->minscore) {
                struct alnode *pathes =
                        merge_tree(al->pathes, node->pathes, al);
                if (pathes) {
                    unref(al->pathes, al);
                    al->pathes = pathes;
                }
            }
        }
        else {
            // todo: audio scores!

            FOREACH(struct latlink, link, node->exits_head, exits_next) {
                struct latnode *dest = link->to;

                if (node->pathes->maxscore > dest->pathes->minscore) {
                    struct alnode *pathes =
                            merge_tree(dest->pathes, node->pathes, al);
                    if (pathes) {
                        unref(dest->pathes, al);
                        dest->pathes = pathes;
                    }
                }

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

                        if (newpath->minscore > dest->pathes->minscore) {
                            struct alnode *pathes;
                            if (swnode->position == 0) {
                                pathes = merge_path_complete(
                                        dest->pathes, newpath, al);
                            } else {
                                pathes = merge_path_partial(
                                        dest->pathes, al->width, newpath,
                                        swnode->position, al);
                            }

                            if (pathes) {
                                unref(dest->pathes, al);
                                dest->pathes = pathes;
                                newpath = NULL;
                                tail->refcount++;
                            }
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
        fprintf(stderr, "cc[%u]:%10u\n", i, cc[i]);
}
