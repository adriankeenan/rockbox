#ifndef AAC_DEC_H
#define AAC_DEC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define AAC_SAMPLE_DEPTH 29   /* fractional bits of the decoder's output */
#define AAC_MAX_FRAME    8192 /* largest ADTS frame (13-bit length) */

struct aac_info
{
    unsigned samplerate;
    unsigned channels;
    unsigned samples;         /* per channel */
    const int32_t *pcm[2];    /* non-interleaved fixed point */
    unsigned consumed;        /* bytes of input used */
};

/* Parse an ADTS header. Returns 0 and fills the outputs when the bytes at p
 * start a valid header. */
int adts_parse(const uint8_t *p, size_t len, unsigned *frame_len,
               unsigned *samplerate, unsigned *channels, unsigned *samples);

/* Give the decoder its private heap and create an instance */
bool aac_open(void *heap, size_t heap_size);
void aac_close(void);
/* Initialise from the first ADTS frame; returns false on failure */
bool aac_init(const uint8_t *frame, size_t len);
/* Decode one frame; <0 on error (info->consumed still valid) */
int aac_decode(const uint8_t *buf, size_t len, struct aac_info *info);
/* Call after a discontinuity (seek) */
void aac_post_seek(void);
size_t aac_heap_used(void);

#endif /* AAC_DEC_H */
