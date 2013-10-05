#include "waveout.h"
#include <stdio.h>
#include <errno.h>

static inline void le16(uint8_t buf[2], uint16_t n)
{
    buf[0] = n;
    buf[1] = n >> 8;
}

static inline void le32(uint8_t buf[4], uint32_t n)
{
    buf[0] = n;
    buf[1] = n >> 8;
    buf[2] = n >> 16;
    buf[3] = n >> 24;
}

static void write_header(FILE *file, unsigned samplerate, unsigned nsamples)
{
    uint8_t header[11][4] = {{0}};

    memcpy(header[0], "RIFF", 4);
    le32(header[1], nsamples * 2 + 36);
    memcpy(header[2], "WAVE", 4);
    memcpy(header[3], "fmt ", 4);
    header[4][0] = 16;
    header[5][0] = 1, header[5][2] = 1;
    le32(header[6], samplerate);
    le32(header[7], samplerate * 2);
    header[8][0] = 2, header[8][2] = 16;
    memcpy(header[9], "data", 4);
    le32(header[10], nsamples * 2);

    fwrite(header, sizeof header, 1, file);
}

struct waveout {
    FILE *file;
    unsigned samplerate;
    unsigned nsamples;
};

waveout_t *waveout_open(const char *filename, unsigned samplerate)
{
    FILE *file = fopen(filename, "wb");
    if (!file) {
        error("Could not open output file \"%s\": %s", filename, strerror(errno));
        return NULL;
    }

    write_header(file, samplerate, 0);

    waveout_t *wf = xmalloc(sizeof *wf);
    *wf = (waveout_t) {
        .file = file,
        .samplerate = samplerate
    };

    return wf;
}

#define BUFLEN 256

void waveout_write(waveout_t *wf, const int16_t *samples, unsigned nsamples)
{
    uint8_t buf[BUFLEN][2];

    wf->nsamples += nsamples;
    while (nsamples > 0) {
        unsigned n = MIN(BUFLEN, nsamples);
        for (unsigned i = 0; i < n; i++)
            le16(buf[i], samples[i]);

        fwrite(buf, 2, n, wf->file);
        nsamples -= n;
        samples += n;
    }
}

void waveout_close(waveout_t *wf)
{
    if (!fseek(wf->file, 0, SEEK_SET))
        write_header(wf->file, wf->samplerate, wf->nsamples);
    else
        warning("Could not rewind WAVE file to rewrite header: %s", strerror(errno));

    if (ferror(wf->file))
        error("Error while writing WAVE file");

    fclose(wf->file);
    free(wf);
}



