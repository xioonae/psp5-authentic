/* psp5 authentic – the video player: a video of a folder on the whole screen (MP4 with H.264 video and AAC sound, and PMF / PSMF), the bar to move in it, L / R for the video before
 * and after, ○ leaves; a video that was left in the middle asks whether to go on from there (videomem.h). The overlay (L1, the number of the video, R1 at the top; the time, the bar and the
 * length at the bottom) shows when a video starts or a button is pressed and goes away after a moment. */
#ifndef VIDEOPLAY_H
#define VIDEOPLAY_H

#include <psptypes.h>
#include "music.h"

typedef struct VideoPlayer VideoPlayer;

/* dir and files[count] are the lines of the Videos screen (they stay as they are while the player is open); the video `index` starts first. NULL: no memory. The music is stopped. */
VideoPlayer *videoplay_open(const char *dir, const MusicEntry *files, int count, int index);
/* once a frame, with the buttons; returns 1 when the player is to be closed (○) */
int videoplay_update(VideoPlayer *v, u32 pressed, u32 held, int lx, int ly, float dt);
/* draws the player on the whole screen (no gfx_begin / gfx_end) */
void videoplay_draw(VideoPlayer *v);
void videoplay_close(VideoPlayer *v);
/* the PSP goes to sleep / wakes up: the video is paused (and goes on when it wakes if it played) */
void videoplay_sleep(VideoPlayer *v, int asleep);

#endif
