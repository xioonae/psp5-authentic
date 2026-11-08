/* psp5 authentic – the timing the PSP's own H.264 streams have, written into a stream that has none.
 *
 * The PSP's sceMpeg library gets its pictures from streams made by Sony's tools. In those, the SPS has a VUI with timing and HRD parameters (NAL and VCL, the same), and every picture has a
 * "picture timing" SEI message (how long it waits to be decoded and to be shown), every key frame also a "buffering period" and a "recovery point" message. Streams without all that
 * (what x264 and other ordinary encoders make) froze the PSP or gave the decoder's fatal error. The PSP's XMB plays such files by another route (sceMp4), which does not need them.
 * Everything here is read off the PMF of PPSSPP's tests (see DEVELOPER-NOTES) and gives the same bytes as that stream has. */
#ifndef H264SEI_H
#define H264SEI_H

#include <stdint.h>

/* The clock tick of the stream: num_units / time_scale seconds. The PSP's own streams count in fields: 4004 / 240000 for 29.97 pictures a second (one tick = one field). */
typedef struct {
    unsigned num_units, time_scale;
} H264Tick;

/* 1 when the SPS (a NAL unit with its header byte, no start code) announces HRD parameters: such a stream is made the way the PSP's own are and is left as it is. 0 when it does not (or
 * cannot be read). */
int h264_sps_has_hrd(const uint8_t *nal, int len);

/* The SPS with the VUI of the PSP's own streams (square pixels, timing of `tick`, NAL and VCL HRD, picture structure) instead of the one it has; everything before the VUI is as it was. The result
 * is a NAL unit too (same header byte). Returns its length, -1 when the SPS cannot be read or the result does not fit in cap. */
int h264_sps_rewrite(const uint8_t *nal, int len, H264Tick tick, uint8_t *out, int cap);

/* SEI NAL units (header byte included, no start code); each returns its length or -1 */
int h264_sei_buffering(uint8_t *out, int cap);                                                 /* buffering period of a key frame: the first delay is one second (90000) */
int h264_sei_pic_timing(unsigned cpb_removal_delay, unsigned dpb_output_delay, uint8_t *out, int cap); /* in ticks: the decoding time since the key frame, the time from decoding to showing */
int h264_sei_recovery(uint8_t *out, int cap);                                                  /* recovery point: decoding can start here */

#endif
