#include "ffdecode.h"

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>



static AVFormatContext *open_format(const char *filename)
{
    AVFormatContext *fc = NULL;
    int r = avformat_open_input(&fc, filename, NULL, NULL);
    if (r < 0) {
        error("Could not open input file: %s", av_err2str(r));
        return NULL;
    }

    r = avformat_find_stream_info(fc, NULL);
    if (r < 0) {
        error("Could not find stream information: %s", av_err2str(r));
        avformat_close_input(&fc);
        return NULL;
    }

    return fc;
}

unsigned ffdec_count_audiostreams(const char *filename)
{
    AVFormatContext *fc = open_format(filename);
    if (!fc) return 0;

    unsigned n = 0;
    for (unsigned i = 0; i < fc->nb_streams; i++)
        if (fc->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
            n++;

    avformat_close_input(&fc);
    return n;
}

static bool select_audiostream(const AVFormatContext *fc,
        unsigned audiostream, unsigned *streamindex)
{
    for (unsigned i = 0; i < fc->nb_streams; i++)
        if (fc->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
            if (!audiostream--) { *streamindex = i; return true; }

    error("Selected audio stream does not exist in source file");
    return false;
}


static AVCodecContext *open_codec(
        const AVFormatContext *fc, unsigned stream_idx)
{
    // find decoder for selected stream
    AVCodecContext *cc = fc->streams[stream_idx]->codec;
    AVCodec *dec = avcodec_find_decoder(cc->codec_id);
    if (!dec) {
        error("Cannot find codec for audio stream");
        return NULL;
    }

    // configure and open decoder
    //cc->refcounted_frames = 1;
    cc->request_channel_layout = AV_CH_LAYOUT_MONO;
    cc->request_sample_fmt = AV_SAMPLE_FMT_S16;
    int r = avcodec_open2(cc, dec, NULL);
    if (r < 0) {
        error("Cannot open audio codec: %s", av_err2str(r));
        return NULL;
    }

    return cc;
}


#define SWROPT(type, name, val) do { \
        int rv = av_opt_set_ ## type(sc, name, val, 0); \
        if (rv) { error("Could not set swresample option %s: %s", \
                name, av_err2str(rv)); goto fail; } \
    } while (0)


static SwrContext *open_swr(const AVCodecContext *cc, unsigned out_srate)
{
    SwrContext *sc = swr_alloc();
    if (!sc) { error("swr_alloc failed"); return NULL; }

    SWROPT(int, "in_channel_layout", cc->channel_layout);
    SWROPT(int, "out_channel_layout", AV_CH_LAYOUT_MONO);
    SWROPT(int, "in_sample_rate", cc->sample_rate);
    SWROPT(int, "out_sample_rate", out_srate);
    SWROPT(sample_fmt, "in_sample_fmt", cc->sample_fmt);
    SWROPT(sample_fmt, "out_sample_fmt", AV_SAMPLE_FMT_S16);

    // using integer internal format is much faster than float
    SWROPT(sample_fmt, "internal_sample_fmt", AV_SAMPLE_FMT_S16P);

    int rv = swr_init(sc);
    if (rv < 0) {
        error("Could not initialize swresample context: %s", av_err2str(rv));
        goto fail;
    }

    return sc;

fail:
    swr_free(&sc);
    return NULL;
}


struct ffdec {
    AVFormatContext *fc;
    AVCodecContext *cc;
    SwrContext *sc;
    AVFrame *frame;

    AVPacket pkt;
    bool have_pkt;

    unsigned streamindex;
    unsigned decfails;
};


ffdec_t *ffdec_open(const char *filename,
        unsigned audiostream, unsigned samplerate)
{
    ffdec_t *ff = xmalloc(sizeof *ff);
    *ff = (ffdec_t){0};
    av_init_packet(&ff->pkt);

    ff->frame = avcodec_alloc_frame();
    if (!ff->frame) { error("avcodec_alloc_frame failed"); goto fail; }

    ff->fc = open_format(filename);
    if (!ff->fc) goto fail;

    if (!select_audiostream(ff->fc, audiostream, &ff->streamindex))
        goto fail;

    ff->cc = open_codec(ff->fc, ff->streamindex);
    if (!ff->cc) goto fail;

    ff->sc = open_swr(ff->cc, samplerate);
    if (!ff->sc) goto fail;

    return ff;

fail:
    if (ff->cc) avcodec_close(ff->cc);
    if (ff->fc) avformat_close_input(&ff->fc);
    if (ff->frame) avcodec_free_frame(&ff->frame);
    free(ff);
    return NULL;
}

void ffdec_close(ffdec_t *ff)
{
    if (ff->decfails)
        warning("%u packet(s) could not be decoded", ff->decfails);

    swr_free(&ff->sc);
    avcodec_close(ff->cc);
    avformat_close_input(&ff->fc);
    avcodec_free_frame(&ff->frame);
    if (ff->have_pkt) av_free_packet(&ff->pkt);
    free(ff);
}


static bool read_frame(ffdec_t *ff)
{
    for (;;) {

        // try to read a packet unless we still have one
        while (!ff->have_pkt) {
            int rv = av_read_frame(ff->fc, &ff->pkt);
            if (rv == AVERROR_EOF) {
                // continue with empty packet to flush buffer
                ff->pkt.data = NULL;
                ff->pkt.size = 0;
                break;
            } else if (rv < 0) {
                warning("Failed to read packet: %s", av_err2str(rv));
                return false;
            } else if ((unsigned)ff->pkt.stream_index != ff->streamindex) {
                // wrong stream, discard packet
                av_free_packet(&ff->pkt);
            } else {
                ff->have_pkt = true;
            }
        }

        int got_frame = 0;
        int rv = avcodec_decode_audio4(ff->cc, ff->frame, &got_frame, &ff->pkt);
        if (rv < 0) ff->decfails++;

        if (!ff->have_pkt) {                      // flushing at eof
            if (!got_frame) return false;
        } else if (rv >= 0 && rv < ff->pkt.size) {   // continue with pkt later
            ff->pkt.data += rv;
            ff->pkt.size -= rv;
        } else {                                     // done with this packet
            av_free_packet(&ff->pkt);
            ff->have_pkt = false;
        }

        if (got_frame) return true;
    }
}


unsigned ffdec_read(ffdec_t *ff, int16_t *buf, unsigned buflen)
{
    unsigned fill = 0;

    // copy buffered samples
    const uint8_t **inbuf = (const uint8_t**)ff->frame->data;
    uint8_t *outbuf = (uint8_t*)buf;
    int rv = swr_convert(ff->sc, &outbuf, buflen, inbuf, 0);
    if (rv < 0) goto convfail;
    fill = rv;

    // read more frames until buffer filled
    while (fill < buflen) {
        bool got_frame = read_frame(ff);

        // if we did not read a frame due to eof or error,
        // we set inbuf and inlen to 0 to flush the swr buffer
        inbuf = got_frame ? (const uint8_t**)ff->frame->data : NULL;
        int inlen = got_frame ? ff->frame->nb_samples : 0;

        outbuf = (uint8_t*)(buf + fill);
        int rv = swr_convert(ff->sc, &outbuf, buflen - fill, inbuf, inlen);
        if (rv < 0) goto convfail;
        fill += rv;
        if (!got_frame) break;
    }

    return fill;

convfail:
    error("swr_convert failed: %s", av_err2str(rv));
    return fill;
}
