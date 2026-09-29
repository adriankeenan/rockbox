#ifndef TS_PARSER_H
#define TS_PARSER_H

/* Audio coding of the selected audio stream */
enum audio_codecs
{
    AUDIO_CODEC_MPA = 0,   /* MPEG audio layers I-III (libmad) */
    AUDIO_CODEC_AAC,       /* AAC-LC in ADTS framing (libfaad) */
};

extern int audio_codec;

/* Detects an MPEG transport stream at the start of the file and picks out
 * the H.264 video and MPEG/AAC audio PIDs from the PAT/PMT. */
bool ts_probe(void);

int ts_next_data(struct stream *str, enum stream_parse_mode type);

/* Same contract as the program stream scanners: scan from sk->pos in the
 * given direction for a PES packet start of the stream with a PTS */
uint32_t ts_scan_pts(struct stream_scan *sk, unsigned id);
/* Video packet starting an IDR access unit (has an SPS) with a PTS */
uint32_t ts_scan_keyframe(struct stream_scan *sk);

#endif /* TS_PARSER_H */
