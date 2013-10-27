#ifndef VSUBALIGN_H_
#define VSUBALIGN_H_

#include "common.h"


struct vsubalign_opt {
    const char *video_infilename;
    unsigned audiostream;
    const char *subtitle_infilename;
    const char *hmm_infilename;
    const char *dic_infilename;
    const char *dic_outfilename;
    const char *lm_outfilename;
    unsigned n_voicerec_threads;
};




bool vsubalign(const struct vsubalign_opt *opt);



#endif /* VSUBALIGN_H_ */
