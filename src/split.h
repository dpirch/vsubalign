#ifndef SPLIT_H_
#define SPLIT_H_

#include "common.h"


typedef struct audioblock {
    struct audioblock *next;
    unsigned nsamples; // > 0
    int16_t samples[];
} audioblock_t;


typedef struct audiosplitter audiosplitter_t;

audiosplitter_t *audiosplitter_create(unsigned minblocks, unsigned maxblocks,
        audioblock_t *(*readblock)(void *userptr), void *userptr);

audioblock_t *audiosplitter_delete(audiosplitter_t *sp);

audioblock_t *audiosplitter_next_segment(audiosplitter_t *sp);


#endif /* SPLIT_H_ */
