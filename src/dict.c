#include "dict.h"

#include <stdalign.h>
#include <ctype.h>
#include <errno.h>

#include "hashtable.h"
#include "alloc.h"
#include "text.h"


static hashval_t hashfunc(const char *str)
{
    hashval_t hash = 5381;
    while (*str) hash = hash * 33 + *str++;
    return hash;
}

static bool matchword(const void *item, const void *key)
{
    const struct dictword *word = item;
    return strcmp(key, word->string) == 0;
}

static void *createword(const void *key, void *userptr)
{
    size_t len = strlen(key);
    struct dictword *word = var_alloc(sizeof (struct dictword) + len + 1,
            alignof (struct dictword), userptr);
    *word = (struct dictword){0};
    memcpy(word->string, key, len + 1);
    return word;
}

struct createword_copy_arg { struct dict *dict; const struct dict *srcdict; };

static void *createword_copy(const void *key, void *userptr)
{
    struct createword_copy_arg *arg = userptr;

    struct dictword *srcword = dict_lookup(arg->srcdict, key);
    if (!srcword) return NULL;

    struct dictword *word = createword(key, arg->dict->wordalloc);

    for (struct dictpron *sp = srcword->pronlist, **pp = &word->pronlist;
            sp; sp = sp->next, pp = &(*pp)->next) {
        *pp = var_alloc(sizeof (struct dictpron) + strlen(sp->string) + 1,
                alignof (struct dictpron), arg->dict->pronalloc);
        (*pp)->next = NULL;
        strcpy((*pp)->string, sp->string);
    }
    return word;
}

struct dict *dict_create(void)
{
    struct dict *dict = xmalloc(sizeof *dict);
    dict->wordalloc = var_allocator_create(4096);
    dict->pronalloc = var_allocator_create(4096);
    dict->words = hashtable_create(offsetof(struct dictword, hashval));
    return dict;
}

void dict_delete(struct dict *dict)
{
    hashtable_delete(dict->words);
    var_allocator_delete(dict->wordalloc);
    var_allocator_delete(dict->pronalloc);
}

struct dictword *dict_lookup(const struct dict *dict, const char *str)
{
    return hashtable_lookup(dict->words, hashfunc(str), matchword, str);
}


struct dictword *dict_lookup_or_add(struct dict *dict, const char *str)
{
    return hashtable_lookup_or_add(
            dict->words, hashfunc(str), matchword, str,
            createword, dict->wordalloc);
}

struct dictword *dict_lookup_or_copy(struct dict *dict, const char *str,
        const struct dict *srcdict)
{
    struct createword_copy_arg arg = { dict, srcdict };

    return hashtable_lookup_or_add(
            dict->words, hashfunc(str), matchword, str,
            createword_copy, &arg);
}



bool parse_line(char *line, struct dict *dict)
{
    char *tab = strchr(line, '\t');
    if (!tab || tab == line) return false;
    *tab = '\0';

    // remove number in parentheses
    if (tab >= line + 4 && tab[-1] == ')' && isdigit(tab[-2])) {
        char *p = tab - 3;
        while (p >= line + 2 && isdigit(*p)) p--;
        if (*p == '(') *p = '\0';
    }

    // add to dictionary
    struct dictword *word = dict_lookup_or_add(dict, line);

    // add pronunciation
    char *pronstr = tab + 1;
    for (struct dictpron **pnptr = &word->pronlist;; pnptr = &(*pnptr)->next) {
        if (!*pnptr) {
            *pnptr = var_alloc(sizeof (struct dictpron) + strlen(pronstr) + 1,
                    alignof (struct dictpron), dict->pronalloc);
            (*pnptr)->next = NULL;
            strcpy((*pnptr)->string, pronstr);
            break;
        }
        if (!strcmp((*pnptr)->string, pronstr))
            break;
    }
    return true;
}

bool dict_read(struct dict *dict, const char *filename)
{
    linereader_t *lr = linereader_open(filename);
    if (!lr) return false;

    bool success = true;
    for (char *line; line = linereader_getline(lr), line;) {
        if (!*line) continue; // empty line
        if (!parse_line(line, dict)) {
            error("Parse error while reading dictionary '%s', line %u",
                    filename, linereader_linenum(lr));
            success = false;
            break;
        }
    }

    //hashmap_printstats(dict->words);

    success = success && !linereader_error(lr);
    linereader_close(lr);
    return success;
}

static void writeword(void *item, void *userptr)
{
    const struct dictword *word = item;
    FILE *file = userptr;

    unsigned i = 1;
    for (struct dictpron *pron = word->pronlist; pron; pron = pron->next) {
        if (i == 1)
            fprintf(file, "%s\t%s\n", word->string, pron->string);
        else
            fprintf(file, "%s(%u)\t%s\n", word->string, i, pron->string);
        i++;
    }
}

bool dict_write(const struct dict *dict, const char *filename)
{
    FILE *file = fopen(filename, "w");
    if (!file) {
        error("Could not write to '%s': %s", filename, strerror(errno));
        return false;
    }

    hashtable_foreach(dict->words, writeword, file, NULL);

    bool err = ferror(file);
    if (err) error("Error while writing to '%s'", filename);

    fclose(file);
    return !err;
}
