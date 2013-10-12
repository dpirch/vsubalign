#ifndef AUDIO_H_
#define AUDIO_H_

#include "common.h"

struct audioblock {
    timestamp_t starttime; // presentation time, in ms
    struct audioblock *next;
    int16_t samples[];  // same fixed size for all blocks, append zeroes at end
};

struct audiosplitter;

struct audiosplitter *audiosplitter_create(
        size_t blocklen, size_t segmin, size_t segmax,
        struct audioblock *(*getblock)(void *userptr), void *userptr);

struct audioblock *audiosplitter_delete(struct audiosplitter *sp);

struct audioblock *audiosplitter_next_segment(struct audiosplitter *sp);





#endif /* AUDIO_H_ */
