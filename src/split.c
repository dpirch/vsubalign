#include "split.h"

#include <math.h>
#include <float.h>




struct audiosplitter {

    audioblock_t *(*readblock)(void *userptr);
    void *userptr;

    unsigned skiplen;
    unsigned scanlen;

    /**
     * Number of buffered blocks.
     * `nblocks <= skiplen + scanlen`.
     */
    unsigned nblocks;

    /**
     * Head of the linked of buffered blocks.
     * Undefined if there are not buffered blocks.
     * The next pointer of the last list element is undefined and *not* set
     * to NULL, the list length is determined by `nblocks` instead.
     */
    audioblock_t *blocks;

    /**
     * Buffered block list append pointer.
     * Points to next pointer of last buffered block, or to `blocks` if
     * the list is empty.
     */
    audioblock_t **blocks_append;

    /**
     * Last sample of most recently read block.
     * Used for pre-emphasis. 0 after initialization.
     */
    int16_t prev_sample;

    float *powers;
    unsigned offset;

};

audiosplitter_t *audiosplitter_create(unsigned minblocks, unsigned maxblocks,
        audioblock_t *(*readblock)(void *userptr), void *userptr)
{
    assert(minblocks > 0 && maxblocks >= minblocks);

    audiosplitter_t *sp = xmalloc(sizeof *sp);
    *sp = (audiosplitter_t) {
        .readblock = readblock,
        .userptr = userptr,
        .skiplen = minblocks - 1,
        .scanlen = maxblocks - minblocks + 2,
        .blocks_append = &sp->blocks
    };
    sp->powers = xmalloc(sp->scanlen * sizeof *sp->powers);
    return sp;
}

audioblock_t *audiosplitter_delete(audiosplitter_t *sp)
{
    audioblock_t *blocks = sp->blocks;
    free(sp->powers);
    free(sp);
    return blocks;
}


static float sum_squares_preemph(const audioblock_t *block, int16_t prev)
{
    float sum = 0.0f;
    float fprev = prev;
    for (unsigned i = 0; i < block->nsamples; i++) {
        float fsamp = block->samples[i];
        sum += (fsamp - fprev) * (fsamp - fprev);
        fprev = fsamp;
    }
    return sum;
}


static unsigned find_splitpoint(const float *powers,
        unsigned scanlen, unsigned offset)
{
    float min_power = FLT_MAX;
    unsigned min_i = 1;

    unsigned offset_l = offset, offset_r;
    float power_l = powers[offset_l], power_r;

    for (unsigned i = 1; i < scanlen; i++) {
        offset_r = offset_l + 1;
        if (offset_r >= scanlen) offset_r = 0;
        power_r = powers[offset_r];

        float power = MAX(power_l, power_r);
        if (power <= min_power) {
            min_i = i;
            min_power = power;
        }

        offset_l = offset_r;
        power_l = power_r;
    }

    return min_i;
}

audioblock_t *audiosplitter_next_segment(audiosplitter_t *sp)
{

    // refill blocks list
    while (sp->nblocks < sp->skiplen + sp->scanlen) {

        audioblock_t *block = sp->readblock(sp->userptr);

        if (!block) {   // eof -> return whole list
            *sp->blocks_append = NULL; // terminate list
            audioblock_t *seg = sp->blocks;
            sp->nblocks = 0;
            sp->blocks_append = &sp->blocks;
            return seg;
        }

        assert(block->nsamples > 0);

        // append to list
        sp->nblocks++;
        *sp->blocks_append = block;
        sp->blocks_append = &block->next;

        // compute power
        if (sp->nblocks >= sp->skiplen) {
            unsigned idx =
                    (sp->nblocks - sp->skiplen + sp->offset) % sp->scanlen;
            sp->powers[idx] = //logf(
                    sum_squares_preemph(block, sp->prev_sample) /
                    block->nsamples;//);
        }

        sp->prev_sample = block->samples[block->nsamples - 1];
    }

    // determine segment length
    unsigned seglen = sp->skiplen +
            find_splitpoint(sp->powers, sp->scanlen, sp->offset);
    assert(seglen > sp->skiplen && seglen < sp->nblocks);

    // shift segment blocks from block list into list to return
    audioblock_t *seg = sp->blocks;
    audioblock_t *seg_last = seg;
    for (unsigned i = seglen - 1; i > 0; i--)
        seg_last = seg_last->next;
    sp->blocks = seg_last->next;
    seg_last->next = NULL;
    sp->nblocks -= seglen;

    // shift powers ring buffer
    sp->offset = (sp->offset + seglen) % sp->scanlen;

    return seg;
}







#if 0

//Log-normal distribution
//pre-emphasis


/**
 * Loads more chunks from a channel and appends them to a list.
 *
 * @param[in,out] *list The head pointer of the list to append to, may be NULL.
 * @param[in] nsamples number of samples the loaded list should contain,
 *      including the samples already present if *list is not NULL when called.
 * @param[in] *chunksrc The channel to read auchunk_t pointers from.
 * @return true if at least the requested number of samples could be loaded,
 *      false if the channel ended before enough samples could be loaded.
 */
static void load_more(auchunk_t **list, unsigned nsamples, channel_t *chunksrc)
{
    for (;;) {
        while (*list) {
            if ((*list)->length >= nsamples) return; // done
            nsamples -= (*list)->length;
            list = &(*list)->next;
        }
        *list = channel_pop(chunksrc);
        if (!*list) return; // eof
    }
}

/**
 * Computes the sum of the squares, with pre-emphasis.
 *
 * @param[in] array Array of input values.
 * @param[in] length Length of array.
 * @param[in,out] *prev Value before first array element; set to last
 *      array element.
 * @return Sum of squares of input values after subtracting the previous value
 *      from each value before it is squared, to filter out low-frequency noise.
 */
static float sum_squares_preemph(
        const int16_t array[], unsigned length, int16_t *prev)
{
    float sum = 0.0f;
    float vprev = *prev;
    *prev = array[length - 1];
    while (length--) {
        float v = *array++;
        sum += (v - vprev) * (v - vprev);
        vprev = v;
    }
    return sum;

}

static bool next_frame_power(float *power, unsigned framelen,
        auchunk_t **endchunk, unsigned *endoffset, int16_t *prev)
{
    unsigned nsamples = 0;
    float sumsqrs = 0.0f;
    for (;;) {
        if (!*endchunk) {
            break;
        } else if (framelen - nsamples <= (*endchunk)->length - *endoffset) {
            sumsqrs += sum_squares_preemph(
                    (*endchunk)->samples + *endoffset,
                    framelen - nsamples, prev);
            nsamples = framelen;
            *endoffset += framelen - nsamples;
            break;
        } else {
            sumsqrs += sum_squares_preemph(
                    (*endchunk)->samples + *endoffset,
                    (*endchunk)->length - *endoffset, prev);
            nsamples += (*endchunk)->length - *endoffset;
            *endchunk = (*endchunk)->next;
            *endoffset = 0;
        }
    }

    if (nsamples == 0) return false;

    *power = logf(sumsqrs / nsamples);
    return true;
}



void split(channel_t *chunks_in, channel_t *chunklists_out)
{
    /*unsigned samplerate =  16000;
    unsigned framelen = samplerate / framespersec;

    // size of scan range in frames, power of two???
    unsigned scansize = 1024;

    // current
    unsigned scanfill = 0;









    // current number of frames, <= maxframes
    unsigned nframes = 0;

    // ring buffer of powers of frames in scan range
    float *framepowers = xmalloc(sizeof *framepowers * nframes);

    // start of scanrange in framepowers buffer
    unsigned framepos = 0;

    // list of buffered sample chunks containing the scan range
    auchunk_t *chunklist = NULL;

    // position within chunklist (at end?) where scan range ends
    auchunk_t *endchunk;

    // offset within endchunk where scan range ends
    unsigned endoffset = 0;

    // for pre-emphasis
    int16_t prev = 0;

    // initially fill scan range
    load_more(&chunklist, nframes * framelen, chunks_in);
    endchunk = chunklist;
    for (unsigned i = 0; i < nframes; i++)
        if (!next_frame_power(
                framepowers + i, framelen, &endchunk, &endoffset, &prev))
            goto final; // todo: don't??

    for (;;) {
        // fook for power minimum, todo: improve
        unsigned best_splitframes = 0;
        float best_splitpower = FLT_MAX;
        for (unsigned si = nframes / 10, fi = framepos + si;
                si < nframes; si++, fi++) {
            if (fi >= nframes) fi -= nframes; // wrap around in ring buffer
            float splitpower = MAX(framepowers[fi - 1xxxx], framepowers[fi])



            if (framepowers[fi] < splitpower) {
                splitpower = framepowers[fi];
                splitfreames = si
            }





        }






    }


final:
*/


    return;

}
#endif
