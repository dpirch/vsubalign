#include "vsubalign.h"

#include <pthread.h>
#include <pocketsphinx.h>
#include <sphinxbase/err.h>

#include "ffdecode.h"
#include "audio.h"
#include "aqueue.h"
#include "langmodel.h"
#include "subtitle.h"
#include "subwords.h"
#include "dict.h"

#define SAMPLERATE 16000
#define BLOCKLEN (SAMPLERATE / 20)
#define SEGMENTMIN (10 * SAMPLERATE / BLOCKLEN)
#define SEGMENTMAX (30 * SAMPLERATE / BLOCKLEN)



static struct audioblock *getblock(void *userptr)
{
    struct ffdec *ff = userptr;
    struct audioblock *ab = xmalloc(
            sizeof *ab + BLOCKLEN * sizeof *ab->samples);

    unsigned read = ffdec_read(ff, ab->samples, BLOCKLEN);

    if (read == 0) { free(ab); return NULL; }

    for (unsigned i = read; i < BLOCKLEN; i++)
        ab->samples[i] = 0;

    return ab;

    // TODO: time
}


static void deletesegment(void *ptr)
{
    FOREACH(struct audioblock, block, ptr, next)
        free(block);
}

struct decode_arg
{
    const char *infilename;
    unsigned audiostream;
    struct aqueue *segments;
    bool success;
};

/*
 * Audio decoding thread.
 * Reads source file, generates segments and pushes them to queue.
 */
void *decode(void *ptr)
{
    struct decode_arg *arg = ptr;
    arg->success = false;

    av_register_all();
    struct ffdec *ff = ffdec_open(
            arg->infilename, arg->audiostream, SAMPLERATE);
    if (!ff) goto end;

    struct audiosplitter *sp = audiosplitter_create(
            BLOCKLEN, SEGMENTMIN, SEGMENTMAX, getblock, ff);

    unsigned pos = 0;
    struct audioblock *seg;
    while ((seg = audiosplitter_next_segment(sp))) {
        if (!aqueue_push(arg->segments, seg, pos++)) {
            deletesegment(seg);
            break;
        }
    }

    audiosplitter_delete(sp);
    ffdec_close(ff);
    arg->success = true;
end:
    aqueue_close(arg->segments);
    return NULL;
}


static bool build_langmodel(const struct vsubalign_opt *opt,
        struct dict *dict, struct swlist *wl)
{
    bool success = false;
    struct dict *srcdict = dict_create();
    struct lmbuilder *lmb = lmbuilder_create();

    if (!dict_read(srcdict, opt->dic_infilename))
        goto end;

    if (!subtitle_readwords(opt->subtitle_infilename, wl, dict, srcdict))
        goto end;

    if (!dict_write(dict, opt->dic_outfilename))
        goto end;

    lmbuilder_add_subnodes(lmb, wl);
    lmbuilder_compute_model(lmb, 0.5f);

    if (!lmbuilder_write_model(lmb, opt->lm_outfilename))
        goto end;

    success = true;
end:
    lmbuilder_delete(lmb);
    dict_delete(srcdict);
    return success;
}



struct voicerec_arg
{
    pthread_t thread;
    const struct vsubalign_opt *opt;
    struct aqueue *segments;
    bool success;
};


/*
 * voice recognition thread
 */
void *voicerec(void *ptr)
{
    struct voicerec_arg *arg = ptr;
    arg->success = false;

    ps_decoder_t *ps = NULL;
    struct audioblock *segment = NULL;

    fprintf(stderr, "init ps...\n");
    err_set_logfp(NULL); // turn off pocketsphinx output, this is thread-specific
    cmd_ln_t *config = cmd_ln_init(NULL, ps_args(), TRUE,
            "-hmm", arg->opt->hmm_infilename,
            "-lm", arg->opt->lm_outfilename,
            "-dict", arg->opt->dic_outfilename,
            NULL);
    if (!config) { error("cmd_ln_init failed"); goto end; }

    ps = ps_init(config);
    if (!ps) { error("ps_init failed"); goto end; }

    fprintf(stderr, "init ps done\n");

    unsigned pos;
    while ((segment = aqueue_pop(arg->segments, &pos))) {

        fprintf(stderr, "process segment %u\n", pos);
        if (ps_start_utt(ps, NULL) < 0) {
            error("ps_start_utt failed"); goto end;
        }

        FOREACH(struct audioblock, block, segment, next) {
            if (ps_process_raw(ps, block->samples, BLOCKLEN, 0, 0) < 0) {
                error("ps_process_raw failed"); goto end;
            }
        }

        deletesegment(segment);
        segment = NULL;

        if (ps_end_utt(ps) < 0) { error("ps_end_utt failed"); goto end; }

        fprintf(stderr, "segment %u done\n", pos);

        /*fprintf(stderr, "get lattice\n");
        ps_lattice_t *pslat = ps_get_lattice(ps);
        CHECK(pslat);
        struct lattice *lat = lattice_create(pslat, 100, dict);
        fprintf(stderr, "alignment\n");
        alignment_add_lattice(alignment, lat);
        fprintf(stderr, "alignment done\n");
        lattice_delete(lat);*/
    }

    arg->success = true;
end:
    aqueue_close(arg->segments);
    if (ps) ps_free(ps);
    deletesegment(segment);
    return NULL;
}



bool vsubalign(const struct vsubalign_opt *opt)
{
    bool success = false;
    struct aqueue *segments = aqueue_create(8);
    struct dict *dict = dict_create();
    struct swlist *swlist = swlist_create();
    struct voicerec_arg *voicerec_args = NULL;

    // start decode thread
    struct decode_arg decode_arg = {
            .infilename = opt->video_infilename,
            .audiostream = opt->audiostream,
            .segments = segments };
    pthread_t decode_thread;
    CHECK(!pthread_create(&decode_thread, NULL, decode, &decode_arg));

    // preparation for voice recognition
    if (!build_langmodel(opt, dict, swlist)) {
        aqueue_close(segments);
        goto end;
    }

    // start voice recognition threads
    voicerec_args = xmalloc(sizeof *voicerec_args * opt->n_voicerec_threads);
    for (unsigned i = 0; i < opt->n_voicerec_threads; i++) {
        voicerec_args[i] = (struct voicerec_arg) {
            .opt = opt, .segments = segments };
        CHECK(!pthread_create(&voicerec_args[i].thread,
                NULL, voicerec, &voicerec_args[i]));
    }


    success = true;
end:
    CHECK(!pthread_join(decode_thread, NULL));
    success &= decode_arg.success;

    if (voicerec_args) {
        for (unsigned i = 0; i < opt->n_voicerec_threads; i++) {
            CHECK(!pthread_join(voicerec_args[i].thread, NULL));
            success &= voicerec_args[i].success;
        }
        free(voicerec_args);
    }

    aqueue_delete(segments, deletesegment);
    swlist_delete(swlist);
    dict_delete(dict);

    return success;
}

