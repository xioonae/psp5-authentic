/* psp5 authentic – an AAC file in ADTS form (.aac): the raw frames of the stream one after the other, each with a little header of its own (ISO/IEC 14496-3 1.A.3). The frames come out as an MP4 stores
 * them (without the header), so the AAC decoder of the video player (aac.c) takes them as they are. Only AAC-LC with the channel configurations 1 to 7 (mono up to 7.1: the decoder mixes more than two channels down) and one frame to a header (the decoder does no more).
 * An ID3v2 tag in front of the stream is skipped; a stream with a damaged place goes on at the next frame that is found. */
#ifndef ADTS_H
#define ADTS_H

typedef struct Adts Adts;

/* NULL when the file has no ADTS stream that aac.c can decode */
Adts *adts_open(const char *path);
void adts_close(Adts *a);
/* opens the file again at the frame that comes next (a Memory Stick that has been asleep gives errors until then): 1 = done */
int adts_reopen(Adts *a);

int adts_rate(const Adts *a);
int adts_channels(const Adts *a);
/* the two bytes of the AudioSpecificConfig of the stream (what aac_new() wants) */
const unsigned char *adts_config(const Adts *a);
/* the length in seconds, estimated from the length of the first frames (a stream has no table) */
float adts_duration(const Adts *a);
/* where the next frame is, in seconds (exact when the stream was read from the start without a gap, else as good as the estimate) */
float adts_time(const Adts *a);

/* The next frame into buf (cap bytes, 8192 is always enough): its size, 0 at the end, -1 when the file cannot be read. */
int adts_next(Adts *a, unsigned char *buf, int cap);
/* Goes to about `sec` seconds (the byte that the estimate gives, then the next frame that is found there). 0 = done, -1 = past the end. */
int adts_seek(Adts *a, float sec);

#endif
