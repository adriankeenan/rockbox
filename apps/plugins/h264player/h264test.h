#ifndef H264TEST_H
#define H264TEST_H

#include "h264dec.h"

#ifdef SIMULATOR
/* Called by the video thread after each frame is drawn */
void h264test_frame(const mpeg2_sequence_t *seq, uint8_t *const *buf,
                    uint32_t tag);
/* Unattended run; returns a plugin status */
int h264test_run(const char *file);
#define H264TEST_FRAME(seq, buf, tag) h264test_frame(seq, buf, tag)
#else
#define H264TEST_FRAME(seq, buf, tag) ((void)0)
#endif

#endif /* H264TEST_H */
