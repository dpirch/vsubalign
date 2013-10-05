#ifndef SUBTITLE_H_
#define SUBTITLE_H_

#include "subwords.h"
#include "dict.h"

bool subtitle_readwords(const char *filename,
        subnodelist_t *wl, const dict_t *dict);


/*typedef struct { unsigned start, end; } subtitle_cuetime_t;

typedef struct subword {
    struct subword *next;
    dictword_t *word;
    subtitle_cuetime_t cuetime;
    bool first_in_cue;
    bool first_in_sequence;
} subtitle_word_t;

typedef struct {
    subtitle_word_t *head;
    bool error;
    fixed_alloc_t allocator;
} subtitle_wordlist_t;

void subtitle_wordlist_init(subtitle_wordlist_t *list,
        linereader_t *lr, dict_t *dict);

void subtitle_wordlist_destroy(subtitle_wordlist_t *list);
*/
//struct subword *read_subtitle_words(const char *filename,
//        dict_t *stdict, const dict_t *srcdict, fixed_alloc_t *stwalloc);

// gets dict!
//void subtitle_read_cues(const char *filename);




#endif /* SUBTITLE_H_ */
