#include "dict.h"

#include <stdalign.h>
#include <ctype.h>

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
    const dictword_t *word = item;
    return strcmp(key, word->string) == 0;
}

static void *createword(const void *key, void *userptr)
{
    size_t len = strlen(key);
    dictword_t *word = alloc_var(sizeof (dictword_t) + len + 1,
            alignof (dictword_t), userptr);
    memcpy(word->string, key, len + 1);
    return word;
}


dict_t *dict_create(void)
{
    dict_t *dict = xmalloc(sizeof *dict);
    dict->wordalloc = var_alloc_create(16384);
    dict->words = hashtable_create(offsetof(dictword_t, hashval));
    return dict;
}

void dict_delete(dict_t *dict)
{
    hashtable_delete(dict->words);
    var_alloc_delete(dict->wordalloc);
}

dictword_t *dict_lookup(const dict_t *dict, const char *str)
{
    return hashtable_lookup(dict->words, hashfunc(str), matchword, str);
}


dictword_t *dict_lookup_or_add(dict_t *dict, const char *str)
{
    return hashtable_lookup_or_add(
            dict->words, hashfunc(str), matchword, str,
            createword, dict->wordalloc);
}


static char *parse_line(char *line)
{
    char *end = strchr(line, '\t');
    if (!end || end == line) return NULL;

    // remove number in parentheses
    if (end >= line + 4 && end[-1] == ')' && isdigit(end[-2])) {
        char *p = end - 3;
        while (p >= line + 2 && isdigit(*p)) p--;
        if (*p == '(') end = p;
    }

    *end = 0;
    return line;
}

bool dict_read(dict_t *dict, const char *filename)
{
    linereader_t *lr = linereader_open(filename);
    if (!lr) return false;

    bool success = true;
    for (char *line; line = linereader_getline(lr), line;) {
        if (!*line) continue; // empty line
        char *wordstr = parse_line(line);
        if (!wordstr) {
            error("Parse error while reading dictionary '%s', line %u",
                    filename, linereader_linenum(lr));
            success = false;
            break;
        }
        dict_lookup_or_add(dict, wordstr);
    }

    //hashmap_printstats(dict->words);

    success = success && !linereader_error(lr);
    linereader_close(lr);
    return success;
}
