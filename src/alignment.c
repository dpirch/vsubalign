#include "alignment.h"

#include "alloc.h"
#include "subwords.h"
#include "lattice.h"

static inline bool isleaf(const struct aln_treenode *n) { return !n->null; }



static inline struct aln_pathnode *path_ref(struct aln_pathnode *path)
{
    if (path) path->refcount++;
    return path;
}

static void path_unref(struct aln_pathnode *tail, struct alignment *al)
{
    while (tail && --tail->refcount == 0) {
        struct aln_pathnode *pred = tail->pred;
        pool_free(tail, al->pathalloc);
        tail = pred;
    };
}

static inline struct aln_treenode *tree_ref(struct aln_treenode *tree)
{
    tree->refcount++;
    return tree;
}

static void tree_unref(struct aln_treenode *root, struct alignment *al)
{
    if (--root->refcount > 0) return;

    if (isleaf(root)) {
        if (root->path) path_unref(root->path, al);
    } else {
        tree_unref(root->left, al);
        tree_unref(root->right, al);
    }

    pool_free(root, al->treealloc);
}


static struct aln_treenode *make_leaf(
        struct aln_pathnode *path, unsigned score, struct alignment *al)
{
    struct aln_treenode *leaf = pool_alloc(al->treealloc);
    *leaf = (struct aln_treenode) {
        .minscore = score,
        .maxscore = score,
        .refcount = 1,
        .null = NULL,
        .path = path_ref(path)
    };
    return leaf;
}

static struct aln_treenode *make_tree(
        struct aln_treenode *left, struct aln_treenode *right,
        struct alignment *al)
{
    struct aln_treenode *node = pool_alloc(al->treealloc);
    *node = (struct aln_treenode) {
        .minscore = left->minscore,
        .maxscore = right->maxscore,
        .refcount = 1,
        .left = tree_ref(left),
        .right = tree_ref(right)
    };
    return node;
}


struct alignment *alignment_create(struct swlist *swl)
{
    struct alignment *al = xmalloc(sizeof *al);
    *al = (struct alignment) {
        .pathalloc = pool_allocator_create(sizeof (struct aln_pathnode), 256),
        .treealloc = pool_allocator_create(sizeof (struct aln_treenode), 256)
    };

    while (1u << al->level < swl->length)
        al->level++;

    al->root = make_leaf(NULL, 0, al);

    return al;
}

void alignment_delete(struct alignment *al)
{
    pool_allocator_delete(al->pathalloc);
    pool_allocator_delete(al->treealloc);
    free(al);
}



// splits a leaf at level > 0 into inner node
/*static struct aln_treenode *split_leaf(
        struct aln_treenode *leaf, struct alignment *al)
{
    unsigned score = leaf->minscore;
    struct aln_treenode *node = pool_alloc(al->treealloc);
    *node = (struct aln_treenode) {
        .minscore = score,
        .maxscore = score,
        .refcount = 1,
        .left = leaf,
        .right = leaf
    };
    leaf->refcount++;
    return node;
}
*/

static struct aln_pathnode *tree_lookup(
        const struct aln_treenode *root, unsigned level,
        unsigned maxendpos, unsigned *score)
{
    while (!isleaf(root)) {
        unsigned splitpos = 1u << --level;
        if (maxendpos < splitpos)
            root = root->left;
        else
            root = root->right, maxendpos -= splitpos;
    }
    *score = root->minscore;
    return root->path;
}


static struct aln_pathnode *tree_bestpath(
        const struct aln_treenode *root, unsigned *score)
{
    while (!isleaf(root))
        root = root->right;

    *score = root->minscore;
    return root->path;
}


static void tree_add_path(struct aln_treenode **root, unsigned level,
        struct aln_pathnode *path, unsigned endpos, unsigned score,
        struct alignment *al)
{
    assert(endpos < (1u << level));

    if (endpos == 0 && score > (*root)->maxscore) {

        tree_unref(*root, al);
        *root = make_leaf(path, score, al);

    } else if (score > (*root)->minscore){
        assert(level > 0);

        struct aln_treenode *left, *right;
        if (isleaf(*root)) {
            left = tree_ref(*root);
            right = tree_ref(*root);
        } else {
            left = tree_ref((*root)->left);
            right = tree_ref((*root)->right);
        }
        tree_unref(*root, al);

        unsigned splitpos = 1u << (level - 1);

        if (endpos < splitpos)
            tree_add_path(&left, level - 1, path, endpos, score, al);

        if (score > right->minscore)
            tree_add_path(&right, level - 1, path,
                    MAX(splitpos, endpos) - splitpos, score, al);

        *root = make_tree(left, right, al);
        tree_unref(left, al);
        tree_unref(right, al);
    }
}

static void tree_merge(struct aln_treenode **root, unsigned level,
        struct aln_treenode *other, struct alignment *al)
{
    if (isleaf(other)) {
        if (other->path)
            tree_add_path(root, level, other->path, 0, other->minscore, al);
    } else if (isleaf(*root)) {
        struct aln_treenode *node = tree_ref(other);
        if ((*root)->path)
            tree_add_path(&node, level, (*root)->path, 0, (*root)->minscore, al);
        tree_unref(*root, al);
        *root = node;
    } else {
        struct aln_treenode *left = tree_ref((*root)->left);
        struct aln_treenode *right = tree_ref((*root)->right);
        tree_unref(*root, al);

        tree_merge(&left, level - 1, other->left, al);
        tree_merge(&right, level - 1, other->right, al);

        *root = make_tree(left, right, al);
        tree_unref(left, al);
        tree_unref(right, al);
    }
}


void alignment_add_lattice(struct alignment *al, struct lattice *lat)
{
    struct latnode *readylist = NULL;
    FOREACH(struct latnode, node, lat->nodelist, next) {
        node->nentries_remain = node->nentries;
        if (node->nentries_remain == 0) {
            node->ready_next = readylist;
            readylist = node;
        }
    }




    // what to do with <s> etc.?

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
