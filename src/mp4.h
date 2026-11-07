/* psp5 authentic – an MP4 file read for playback: the tables of its first H.264 video track and its first AAC audio track, and the samples by number.
 * Only what the video player needs; no writing, no fragmented files (moof). The tables are kept as they are in the file's movie box (not unpacked),
 * so a long film needs about 4 bytes per sample, and every sample is found by walking the table of runs. */
#ifndef MP4_H
#define MP4_H

typedef struct Mp4 Mp4;

/* NULL when the file is not an MP4 with an H.264 video track that psp5 can read (damaged, fragmented, or a movie box over 16 MB) */
Mp4 *mp4_open(const char *path);
void mp4_close(Mp4 *m);

int mp4_width(const Mp4 *m);
int mp4_height(const Mp4 *m);
float mp4_duration(const Mp4 *m); /* seconds (of the video track) */

/* the video track */
int mp4_video_count(const Mp4 *m);
int mp4_nal_size(const Mp4 *m); /* the bytes of the length in front of each NAL unit of a sample (1 .. 4) */
/* every SPS and PPS of the track as an Annex B stream (00 00 00 01 + NAL), or 0 when they do not fit in cap: the number of bytes */
int mp4_param_sets(const Mp4 *m, unsigned char *out, int cap);
int mp4_video_size(Mp4 *m, int i);
int mp4_video_max_size(const Mp4 *m); /* the biggest sample of the video track, in bytes */
int mp4_video_read(Mp4 *m, int i, unsigned char *buf, int cap); /* the sample as it is stored (NAL units with their lengths): its size, or -1 */
void mp4_video_times(Mp4 *m, int i, float *dts, float *pts, int *key); /* seconds; key: 1 for a sync sample */
int mp4_video_at(Mp4 *m, float t); /* the sync sample at or before time t (0 when there is none before) */

/* the audio track (AAC): none, when the file has none or it is not AAC */
int mp4_has_audio(const Mp4 *m);
int mp4_audio_rate(const Mp4 *m);
int mp4_audio_channels(const Mp4 *m);
const unsigned char *mp4_audio_config(const Mp4 *m, int *len); /* the AudioSpecificConfig */
int mp4_audio_count(const Mp4 *m);
int mp4_audio_size(Mp4 *m, int i);
int mp4_audio_read(Mp4 *m, int i, unsigned char *buf, int cap);
float mp4_audio_time(Mp4 *m, int i); /* where frame i begins, in seconds */
int mp4_audio_at(Mp4 *m, float t); /* the frame that is playing at time t */

#endif
