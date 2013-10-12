#include "audio.h"

#include <float.h>


struct audiosplitter {

    size_t blocklen;
    struct audioblock *(*getblock)(void *userptr);
    void *getblock_userptr;

    // number of currently buffered blocks, <= skiplen + scanlen
    size_t nblocks;

    // linked list of the `nblocks` buffered blocks, not terminated
    struct audioblock *blocks;

    // append address for `blocks` list.
    struct audioblock **blocks_append;

    // number of buffered blocks that are not considered for split
    // point search, because the created segment would be too short.
    size_t skiplen;

    // number of buffered blocks after `skiplen` that are considered for search.
    size_t scanlen;

    // Last sample of most recently read block. Used for pre-emphasis.
    int16_t prev_sample;

    // ring buffer of size `scanlen` containing the powers of buffered
    // blocks to consider for split point search.
    float *scanbuf;

    // current start position within ring buffer.
    size_t offset;
};


struct audiosplitter *audiosplitter_create(
        size_t blocklen, size_t segmin, size_t segmax,
        struct audioblock *(*getblock)(void *userptr), void *userptr)
{
    assert(segmin > 0 && segmax >= segmin);

    struct audiosplitter *sp = xmalloc(sizeof *sp);
    *sp = (struct audiosplitter) {
        .blocklen = blocklen,
        .getblock = getblock,
        .getblock_userptr = userptr,
        .skiplen = segmin - 1,
        .scanlen = segmax - segmin + 2,
        .blocks_append = &sp->blocks
    };

    sp->scanbuf = xmalloc(sp->scanlen * sizeof *sp->scanbuf);
    return sp;
}


struct audioblock* audiosplitter_delete(struct audiosplitter *sp)
{
    struct audioblock *blocks = sp->blocks;
    free(sp->scanbuf);
    free(sp);
    return blocks;
}


static float sum_squares_preemph(
        size_t length, int16_t samples[length], int16_t prev_sample)
{
    float sum = 0.0f;
    float fprev = prev_sample;
    for (size_t i = 0; i < length; i++) {
        float fsamp = samples[i];
        sum += (fsamp - fprev) * (fsamp - fprev);
        fprev = fsamp;
    }
    return sum;
}


/**
 * Searches data stored in a ring buffer for position where two consecutive
 * values are minimal.
 * @param length length of the ring buffer, `length >= 2`.
 * @param ringbuf the completely filled ring buffer.
 * @param offset start index of the ring buffer, `offset < length`.
 * @return the split length `n` such that `0 < n < length` and
 *      `MAX(ringbuf[(offset + n - 1) % length], ringbuf[(offset + n) % length]`
 *      is minimal.
 */
static size_t find_splitpoint(size_t length, const float ringbuf[length],
        size_t offset)
{
    float min_val = FLT_MAX;
    size_t min_pos = 1;

    size_t offset_left = offset, offset_right;
    float val_left = ringbuf[offset_left], val_right;

    for (size_t i = 1; i < length; i++) {
        offset_right = offset_left + 1;
        if (offset_right == length) offset_right = 0;
        val_right = ringbuf[offset_right];

        float val = MAX(val_left, val_right);
        if (val <= min_val) {
            min_pos = i;
            min_val = val;
        }

        offset_left = offset_right;
        val_left = val_right;
    }

    return min_pos;
}


static bool fill_buffer(struct audiosplitter *sp)
{
    // refill blocks list
    while (sp->nblocks < sp->skiplen + sp->scanlen) {

        struct audioblock *block = sp->getblock(sp->getblock_userptr);
        if (!block) return false;

        // append to list
        sp->nblocks++;
        *sp->blocks_append = block;
        sp->blocks_append = &block->next;

        // compute power
        if (sp->nblocks >= sp->skiplen) {
            size_t idx =
                    (sp->nblocks - sp->skiplen + sp->offset) % sp->scanlen;
            sp->scanbuf[idx] = //logf(
                    sum_squares_preemph(sp->blocklen, block->samples, sp->prev_sample);
                    // /    sp->blocklen;//);
        }

        sp->prev_sample = block->samples[sp->blocklen - 1];
    }
    return true;
}

// removes and returns a number of blocks from the list of buffer list
static struct audioblock *remove_segment(
        struct audiosplitter *sp, size_t length)
{
    assert(length > 0 && length <= sp->nblocks);

    // find end of segment
    struct audioblock *first = sp->blocks;
    struct audioblock *last = first;
    for (size_t i = 1; i < length; i++)
        last = last->next;

    // shorten list of buffered blocks
    sp->blocks = last->next;
    sp->nblocks -= length;
    if (sp->nblocks == 0) sp->blocks_append = &sp->blocks;

    // advance ring buffer
    sp->offset = (sp->offset + length) % sp->scanlen;

    // terminate and return segment
    last->next = NULL;
    return first;
}

struct audioblock *audiosplitter_next_segment(struct audiosplitter *sp)
{
    if (fill_buffer(sp)) {
        return remove_segment(sp, sp->skiplen +
                    find_splitpoint(sp->scanlen, sp->scanbuf, sp->offset));
    } else if (sp->nblocks > 0) {
        // end of stream found, return all remaining blocks
        return remove_segment(sp, sp->nblocks);
    } else {
        return NULL;
    }
}



