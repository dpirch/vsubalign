#include "alignment.h"

#include "alloc.h"
#include "subwords.h"
#include "lattice.h"
#include "dict.h"

static inline bool isleaf(const struct aln_treenode *n) { return !n->null; }

static inline struct aln_pathnode *path_ref(struct aln_pathnode *path)
{
    path->refcount++;
    return path;
}

static void path_unref(struct aln_pathnode *tail, struct alignment *al)
{
    while (--tail->refcount == 0) {
        struct aln_pathnode *pred = tail->pred;
        pool_free(tail, al->pathalloc);
        if (!pred) break;
        tail = pred;
    };
}

static inline struct aln_treenode *tree_ref(struct aln_treenode *tree)
{
    if (tree) tree->refcount++;
    return tree;
}

static void tree_unref(struct aln_treenode *root, struct alignment *al)
{
    if (!root || --root->refcount > 0) return;

    if (isleaf(root)) {
        path_unref(root->path, al);
    } else {
        tree_unref(root->left, al);
        tree_unref(root->right, al);
    }

    pool_free(root, al->treealloc);
}


static struct aln_treenode *make_tree(
        struct aln_treenode *left, struct aln_treenode *right,
        struct alignment *al)
{
    struct aln_treenode *node = pool_alloc(al->treealloc);
    *node = (struct aln_treenode) {
        .minscore = left ? left->minscore : 0,
        .maxscore = right->maxscore,
        .refcount = 1,
        .left = tree_ref(left),
        .right = tree_ref(right)
    };
    return node;
}


static struct aln_treenode *make_leaf(struct aln_treenode *baseleaf,
        struct swnode *swnode, timestamp_t time, struct alignment *al)
{
    // todo
    unsigned score = baseleaf ? baseleaf->minscore + 1 : 1;

    struct aln_pathnode *path = pool_alloc(al->pathalloc);
    *path = (struct aln_pathnode) {
        .swnode = swnode,
        .pred = baseleaf ? path_ref(baseleaf->path) : NULL,
        .time = time,
        .refcount = 1
    };

    struct aln_treenode *leaf = pool_alloc(al->treealloc);
    *leaf = (struct aln_treenode) {
        .minscore = score,
        .maxscore = score,
        .refcount = 1,
        .null = NULL,
        .path = path
    };

    return leaf;
}


static void merge_path_complete(struct aln_treenode **tree,
        struct aln_treenode *leaf, struct alignment *al)
{
    if (!*tree || leaf->minscore > (*tree)->maxscore) {
        tree_unref(*tree, al);
        *tree = tree_ref(leaf);
    }
    else if (leaf->minscore > (*tree)->minscore) {
        assert(!isleaf(*tree));
        struct aln_treenode *left = tree_ref((*tree)->left);
        struct aln_treenode *right = tree_ref((*tree)->right);
        tree_unref(*tree, al);

        merge_path_complete(&left, leaf, al);
        merge_path_complete(&right, leaf, al);
        *tree = make_tree(left, right, al);

        tree_unref(left, al);
        tree_unref(right, al);
    }
}

static void merge_path_partial(struct aln_treenode **tree, unsigned width,
        struct aln_treenode *leaf, unsigned pos, struct alignment *al)
{
    assert(pos < width);
    if (pos == 0) {
        merge_path_complete(tree, leaf, al);
    }
    else if (leaf->minscore > (*tree)->minscore) {
        struct aln_treenode *left, *right;
        if (isleaf(*tree)) {
            left = tree_ref(*tree);
            right = tree_ref(*tree);
        } else {
            left = tree_ref((*tree)->left);
            right = tree_ref((*tree)->right);
        }
        tree_unref(*tree, al);

        unsigned splitpos = width / 2;

        if (pos < splitpos) {
            merge_path_partial(&left, splitpos, leaf, pos, al);
            merge_path_complete(&right, leaf, al);
        } else {
            merge_path_partial(&right, splitpos, leaf, pos - splitpos, al);
        }
        *tree = make_tree(left, right, al);
        tree_unref(left, al);
        tree_unref(right, al);
    }
}

static void merge_tree(struct aln_treenode **tree,
        struct aln_treenode *other, struct alignment *al)
{
    if (!other) {}
    else if (!*tree) {
        *tree = tree_ref(other);
    } else if (isleaf(other)) {
        merge_path_complete(tree, other, al);
    } else if (isleaf(*tree)) {
        struct aln_treenode *new = tree_ref(other);
        merge_path_complete(&new, *tree, al);
        tree_unref(*tree, al);
        *tree = new;
    } else {
        struct aln_treenode *left = tree_ref((*tree)->left);
        struct aln_treenode *right = tree_ref((*tree)->right);
        tree_unref(*tree, al);

        merge_tree(&left, other->left, al);
        merge_tree(&right, other->right, al);
        *tree = make_tree(left, right, al);

        tree_unref(left, al);
        tree_unref(right, al);
    }
}

static struct aln_treenode *tree_lookup(
        struct aln_treenode *tree, unsigned width, unsigned pos)
{
    while (tree && !isleaf(tree)) {
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
        .pathalloc = pool_allocator_create(sizeof (struct aln_pathnode), 256),
        .treealloc = pool_allocator_create(sizeof (struct aln_treenode), 256)
    };

    al->width = 1;
    while (al->width < swl->length)
        al->width *= 2;

    return al;
}

void alignment_delete(struct alignment *al)
{
    // TODO: deallocation/check?

    pool_allocator_delete(al->pathalloc);
    pool_allocator_delete(al->treealloc);
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
            node->pathes = tree_ref(al->pathes);
        }
    }

    // reset al->pathes, will be used to store pathes at end of segment
    tree_unref(al->pathes, al);
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
                struct aln_treenode *base = NULL;
                if (swnode->position > 0)
                    tree_lookup(node->pathes, al->width, swnode->position - 1);

                struct aln_treenode *newpath = make_leaf(
                        base, swnode, node->time, al);

                FOREACH(struct latlink, link, node->exits_head, exits_next) {
                    struct latnode *dest = link->to;
                    merge_path_partial(&dest->pathes, al->width,
                            newpath, swnode->position, al);
                }
                tree_unref(newpath, al);
            }
        }

        tree_unref(node->pathes, al);
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
