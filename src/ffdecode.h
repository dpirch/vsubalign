#ifndef FFDECODE_H_
#define FFDECODE_H_

#include "common.h"

typedef struct ffdec ffdec_t;

void av_register_all(void);

unsigned ffdec_count_audiostreams(const char *filename);

ffdec_t *ffdec_open(const char *filename,
        unsigned audiostream, unsigned samplerate);

void ffdec_close(ffdec_t *ff);

unsigned ffdec_read(ffdec_t *ff, int16_t *buf, unsigned buflen);

#endif /* FFDECODE_H_ */
