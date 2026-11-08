/* psp5 authentic – the video engine: opens an MP4 (H.264 + AAC) or a PMF / PSMF, decodes it on the PSP's own decoders and keeps the pictures and the sound in step.
 *
 * Both kinds go through the PSP's sceMpeg library for the picture: a PMF is read from the file as it is; an MP4 is cut into samples (mp4.c) and written as the program stream sceMpeg
 * reads (mpegps.c), a sector at a time, when the ring buffer asks for more. The sound of a PMF comes out of sceMpegAtracDecode, that of an MP4 out of sceAudiocodec (AAC); either way
 * it is put in a queue that a thread of its own plays, and what has been played is the clock the pictures follow.
 *
 * Everything runs from video_update() (once a frame) apart from that thread. */
#ifndef VIDEO_H
#define VIDEO_H

#include "gfx.h"

typedef struct Video Video;

#ifdef VIDEO_DEBUG
void vlog(const char *fmt, ...); /* a line in ms0:/PSP5/video.log (test builds) */
#endif

/* NULL when the file cannot be played (not a video the PSP can decode, damaged, bigger than 512 x 512, no memory, the PSP's decoders not there) */
Video *video_open(const char *path);
void video_close(Video *v);

int video_width(const Video *v);
int video_height(const Video *v);
float video_duration(const Video *v); /* seconds, 0 when not known */
int video_has_sound(const Video *v);

/* starts at `at` seconds (a jump: the decoders start again at the nearest place that can be started at, the pictures before `at` are not shown) */
int video_play(Video *v, float at);
void video_pause(Video *v, int paused);
/* once a frame (also while paused, to be told when a picture that was asked for has come): 1 when there is a new picture to draw */
int video_update(Video *v, float dt);
const Texture *video_picture(const Video *v); /* the last picture, in the size of the video (NULL before the first) */
float video_time(const Video *v);              /* where the video is, in seconds */
int video_ended(const Video *v);
int video_failed(const Video *v); /* the decoder gave up: the video cannot go on */
/* 1 when the PSP's video library gave a fatal error earlier (the decoder's own, or a semaphore of its that is gone): after that every video fails, and video_open() refuses them, until psp5 is started again */
int video_poisoned(void);

#endif
