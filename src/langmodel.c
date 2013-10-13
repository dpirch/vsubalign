#include "langmodel.h"

#include <math.h>
#include <errno.h>

#include "hashtable.h"
#include "alloc.h"
#include "subwords.h"
#include "dict.h"

#define N 3

struct ngram {
    struct ngram *base;              // (n-1)-gram without last word
    const dictword_t *word;          // last word

    const struct ngram *back;        // (n-1)-gram without first word
    const struct ngram *baseof_list; // list of (n+1)-grams with this as base
    const struct ngram *baseof_next; // next n-gram with same base

    int n;
    hashval_t hashval;
    unsigned count;
    float prob;                      // computed probability
    float alpha;                     // computed back-off weight
};

struct ngramkey {
    struct ngram *base;
    const dictword_t *word;
};

struct lmbuilder_t {
    hashtable_t *ngrams[N];
    fixed_allocator_t *ngramalloc;
    struct ngram *state[N-1];        // last unigram .. (N-1)-gram
    unsigned unisum;                 // sum of counts of all unigrams
    bool sentence_started;

    const dictword_t *sentence_start_word;
    const dictword_t *sentence_end_word;
    const dictword_t *unknown_word;
};



static hashval_t hashfunc(const struct ngramkey *key)
{
    hashval_t h = key->word->hashval;
    if (key->base) h = h * 31 + key->base->hashval;
    return h;
}

static bool match_ngram(const void *item, const void *key)
{
    const struct ngramkey *ngramkey = key;
    const struct ngram *ngram = (const struct ngram*)item;
    return ngramkey->base == ngram->base && ngramkey->word == ngram->word;
}

static void *create_ngram(const void *key, void *userptr)
{
    const struct ngramkey *ngramkey = key;
    struct ngram *ngram = fixed_alloc(userptr);
    *ngram = (struct ngram) { .base = ngramkey->base, .word = ngramkey->word };
    return ngram;
}


lmbuilder_t *lmbuilder_create(dict_t *dict)
{
    lmbuilder_t *lmb = xmalloc(sizeof *lmb);
    *lmb = (lmbuilder_t) {
        .ngramalloc = fixed_allocator_create(sizeof (struct ngram), 256),
        .sentence_start_word = dict_lookup_or_add(dict, "<s>"),
        .sentence_end_word = dict_lookup_or_add(dict, "</s>"),
        .unknown_word = dict_lookup_or_add(dict, "<UNK>")
    };

    for (int i = 0; i < N; i++)
        lmb->ngrams[i] = hashtable_create(offsetof(struct ngram, hashval));

    return lmb;
}

void lmbuilder_delete(lmbuilder_t *lmb)
{
    for (int i = 0; i < N; i++)
        hashtable_delete(lmb->ngrams[i]);
    fixed_allocator_delete(lmb->ngramalloc);
    free(lmb);
}

void lmbuilder_addword(lmbuilder_t *lmb, const dictword_t *word)
{
    if (!lmb->sentence_started) {
        lmb->sentence_started = true;
        lmbuilder_addword(lmb, lmb->sentence_start_word);
    }

    if (!word) word = lmb->unknown_word;

    struct ngram *oldstate[N - 1];
    for (int i = 0; i < N - 1; i++)
        oldstate[i] = lmb->state[i];

    for (int i = 0; i < N; i++) {

        if (i > 0 && !oldstate[i - 1]) break;

        struct ngramkey key = {
                .base = i > 0 ? oldstate[i - 1] : NULL,
                .word = word };

        struct ngram *ngram = hashtable_lookup_or_add(
                lmb->ngrams[i], hashfunc(&key),
                match_ngram, &key, create_ngram, lmb->ngramalloc);

        if (ngram->count == 0) { // just created
            ngram->n = i + 1;
            ngram->back = i > 0 ? lmb->state[i - 1] : NULL;
            if (ngram->base) {
                ngram->baseof_next = ngram->base->baseof_list;
                ngram->base->baseof_list = ngram;
            }
        }

        ngram->count++;
        if (i < N - 1) lmb->state[i] = ngram;
    }
    lmb->unisum++;
}

void lmbuilder_break(lmbuilder_t *lmb)
{
    if (lmb->sentence_started) {
        lmbuilder_addword(lmb, lmb->sentence_end_word);
        lmb->sentence_started = false;
    }

    for (int i = 0; i < N - 1; i++)
        lmb->state[i] = NULL;
}


void lmbuilder_add_subnodes(lmbuilder_t *lmb, const struct swlist *list)
{
    FOREACH(struct swnode, node, list->first, seq_next) {
        lmbuilder_addword(lmb, node->word);
        if (!node->seq_next ||
                node->maxendtime + 200 < node->seq_next->minstarttime)
            lmbuilder_break(lmb);
    }
}



struct compute_prob_arg { unsigned unisum; float deflator; };

static void compute_prob(void *item, void *userptr)
{
    struct ngram *ngram = item;
    const struct compute_prob_arg *arg = userptr;

    if (!ngram->base) // unigram
        ngram->prob = arg->deflator * ngram->count / arg->unisum;
    else
        ngram->prob = arg->deflator * ngram->count / ngram->base->count;
}

struct compute_alpha_arg { float discount; };

static void compute_alpha(void *item, void *userptr)
{
    struct ngram *ngram = item;
    const struct compute_alpha_arg *arg = userptr;

    float sum_denon = 0.0f;
    FOREACH(const struct ngram, up, ngram->baseof_list, baseof_next)
        sum_denon += up->back->prob;
    ngram->alpha = arg->discount / (1.0f - sum_denon);
}

void lmbuilder_compute_model(const lmbuilder_t *lmb, float discount)
{
    struct compute_prob_arg probarg = { lmb->unisum, 1.0f - discount };
    for (int i = 0; i < N; i++)
        hashtable_foreach(lmb->ngrams[i], compute_prob, &probarg, NULL);

    struct compute_alpha_arg alphaarg = { discount };
    for (int i = 0; i < N - 1; i++)
        hashtable_foreach(lmb->ngrams[i], compute_alpha, &alphaarg, NULL);
}


static void print_ngram_words(const struct ngram *ngram, FILE *file)
{
    if (ngram->base) {
        print_ngram_words(ngram->base, file);
        fputs(" ", file);
    }
    fputs(ngram->word->string, file);
}

static void write_ngram(void *item, void *userptr)
{
    struct ngram *ngram = item;
    FILE *file = userptr;

    fprintf(file, "%.4f ", log10f(ngram->prob));
    print_ngram_words(ngram, file);

    if (ngram->n < N)
        fprintf(file, " %.4f", log10f(ngram->alpha));

    fputs("\n", file);
}

static int ngram_compar(const void *p1, const void *p2)
{
    const struct ngram *ng1 = *(const void *const*)p1;
    const struct ngram *ng2 = *(const void *const*)p2;

    if (ng1->base != ng2->base) {
        assert(ng1->base && ng2->base); // only compare n-grams with same n
        const void *b1 = ng1->base, *b2 = ng2->base;
        int basecomp = ngram_compar(&b1, &b2);
        if (basecomp) return basecomp;
    }
    return strcmp(ng1->word->string, ng2->word->string);
}

bool lmbuilder_write_model(const lmbuilder_t *lmb, const char *filename)
{
    FILE *file = fopen(filename, "w");
    if (!file) {
        error("Could not write to '%s': %s", filename, strerror(errno));
        return false;
    }

    fputs("\\data\\\n", file);
    for (int i = 0; i < N; i++)
        fprintf(file, "ngram %d=%u\n", i + 1, lmb->ngrams[i]->count);

    for (int i = 0; i < N; i++) {
        fprintf(file, "\n\\%d-grams:\n", i + 1);
        hashtable_foreach(lmb->ngrams[i], write_ngram, file, ngram_compar);
    }
    fputs("\n\\end\\\n", file);

    bool err = ferror(file);
    fclose(file);
    if (err) error("Error while writing to '%s'", filename);
    return !err;
}
