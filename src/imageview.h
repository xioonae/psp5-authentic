/* psp5 authentic – the image viewer: one picture of a folder at a time on the whole screen (JPEG, PNG, BMP), L / R (and ← →) for the one before and after, ○ leaves.
 * The little overlay (L1, the number of the picture and R1) shows when a picture is opened or a button is pressed and goes away after a moment. The pictures are made
 * ready by a thread of their own, fitted to the screen, so the buttons never wait for a decoder. */
#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <psptypes.h>
#include "music.h"

typedef struct ImageView ImageView;

/* dir and files[count] are the lines of the Images screen (they stay as they are while the viewer is open); the picture `index` is shown first. NULL: no memory. */
ImageView *imageview_open(const char *dir, const MusicEntry *files, int count, int index);
/* once a frame, with the buttons; returns 1 when the viewer is to be closed (○) */
int imageview_update(ImageView *v, u32 pressed, u32 held, int lx, int ly, float dt);
/* draws the viewer on the whole screen (no gfx_begin / gfx_end) */
void imageview_draw(ImageView *v);
void imageview_close(ImageView *v);

#endif
