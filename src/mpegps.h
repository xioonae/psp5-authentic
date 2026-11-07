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
#define PS_TABLE_MAX 59 /* the most units one table lists (the PSP's own streams have at most 59 or 60 pictures in a group) */

typedef struct {
    const uint8_t *data;
    int len, pos;
    int64_t pts, dts;
    int first;
    int table_n; /* units in the table that the first sector of the next unit carries (0 = none) */
    uint16_t table[PS_TABLE_MAX];
    uint16_t words[4];
} PsWriter;

void ps_init(PsWriter *w);
/* Every group of pictures (GOP) of a PSP's own PMF file starts with a system header and a "private stream 2" packet that holds a table: the size in bytes of each picture of the group, and in
 * four numbers the sector (counted from this one) each of the first four pictures ends in. The PSP's library very likely cuts the units of the video out of the stream by that table, and a stream
 * without it gave no unit at all on a real PSP. ps_group() makes the table of `n` units (sizes in bytes of the Annex B data, the units as ps_au() will get them) and the first sector of the next
 * unit carries it. n at most PS_TABLE_MAX, and every size at most 65535 (it does not fit the table otherwise: nothing is written then). */
void ps_group(PsWriter *w, const int *sizes, int n);
/* Plans the group that starts at unit `i` of `count`: unit(arg, j, &key) gives the size of unit j as ps_au() will get it (-1: the unit is left out) and whether it is a key frame; the group ends
 * before the next key frame or after PS_TABLE_MAX units. The table is made (ps_group). Returns the number of units the group has. */
int ps_group_plan(PsWriter *w, int i, int count, int (*unit)(void *arg, int j, int *key), void *arg);
void ps_au(PsWriter *w, const uint8_t *annexb, int len, int64_t pts, int64_t dts); /* the data must stay where it is until ps_pending() is 0 */
int ps_pending(const PsWriter *w);                                                  /* bytes of the unit that are not in a sector yet */
void ps_sector(PsWriter *w, uint8_t out[2048]);                                     /* the next sector of the unit (ps_pending() > 0) */

#endif
