/* psp5 authentic – the pieces that turn the samples of an MP4 into what the PSP's decoders take: H.264 as an Annex B stream with its parameter sets, AAC frames with an ADTS header, and the
 * video as an MPEG program stream in 2048 byte sectors (the way the PSP's own PMF files are cut), which the PSP's sceMpeg library reads from its ring buffer. */
#ifndef MPEGPS_H
#define MPEGPS_H

#include <stdint.h>

/* One H.264 sample (NAL units, each with a length of nal_size bytes in front) -> Annex B in out: an access unit delimiter, the parameter sets (params, of n bytes, when this is a key
 * frame) and the NAL units each behind a start code. Returns the bytes written, or -1 when it does not fit in cap or the sample is damaged. out must not be the sample. */
int mpegps_annexb(const uint8_t *sample, int len, int nal_size, const uint8_t *params, int params_len, int key, uint8_t *out, int cap);

/* the 7 byte ADTS header of an AAC frame of `payload` bytes, from the AudioSpecificConfig of the MP4 (AAC-LC and the other object types of 1 to 4 bits); 0 when the config is not understood */
int mpegps_adts(const uint8_t *asc, int asc_len, int payload, uint8_t out[7]);

/* The program stream writer: it cuts one access unit after another into 2048 byte sectors, each a pack header and one PES packet (the first sector of a unit has its time stamps,
 * the others none; a short last sector is filled up with a padding packet). Times are in ticks of 90 kHz. */
typedef struct {
    const uint8_t *data;
    int len, pos;
    int64_t pts, dts;
    int first;
} PsWriter;

void ps_init(PsWriter *w);
void ps_au(PsWriter *w, const uint8_t *annexb, int len, int64_t pts, int64_t dts); /* the data must stay where it is until ps_pending() is 0 */
int ps_pending(const PsWriter *w);                                                  /* bytes of the unit that are not in a sector yet */
void ps_sector(PsWriter *w, uint8_t out[2048]);                                     /* the next sector of the unit (ps_pending() > 0) */

#endif
