#ifndef WAVEOUT_H_
#define WAVEOUT_H_

#include "common.h"

typedef struct waveout waveout_t;

waveout_t *waveout_open(const char *filename, unsigned samplerate);
void waveout_write(waveout_t *wf, const int16_t *samples, unsigned nsamples);
void waveout_close(waveout_t *wf);


#endif /* WAVEOUT_H_ */
