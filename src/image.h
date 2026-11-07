/* psp5 – made by Mille Weber */
#ifndef IMAGE_H
#define IMAGE_H

#include <psptypes.h>
#include "gfx.h"

/* RGBA8888-bild i vanligt minne (CPU-sidan, innan den blir textur). */
typedef struct {
    int w, h;
    u32 *px; /* ABGR, som GU */
} Image;

int img_decode_png(const void *data, u32 size, Image *out);
/* The picture of a file for the image viewer, never bigger than max_w x max_h (smaller ones are left as they are): JPEG (decoded at 1/2, 1/4 or 1/8 of its size when that is enough, read
 * a row at a time; turned the way the camera's EXIF says), PNG and BMP (1, 4, 8, 16 (5-5-5), 24 and 32 bits, not packed: read a row at a time and made smaller as it is read). 0 when it is
 * not a picture this can read, is too big for the memory (a progressive JPEG over 5 MB of coefficients, a PNG over 2.2 million pixels) or the file cannot be read. *out is freed by img_free. */
int img_load_fit(const char *path, int max_w, int max_h, Image *out);
/* the largest size with the proportions of w x h that is inside mw x mh and not bigger than w x h */
void img_fit_down(int w, int h, int mw, int mh, int *ow, int *oh);
/* Omslag (JPEG eller PNG, efter innehållet). JPEG skalas ner i avkodaren men blir minst
 * min_side på den kortaste sidan. */
int img_decode_cover(const void *data, u32 size, Image *out, int min_side);
/* Beskär till en kvadrat runt mitten. */
int img_crop_square(Image *img);
void img_free(Image *img);
/* Skalar till dw×dh (ytmedelvärde vid förminskning, bilinjärt vid förstoring). */
int img_resize(const Image *src, int dw, int dh, Image *out);
void img_blur(Image *img, int radius);
void img_round_corners(Image *img, float radius);
/* Skär hörnen på en 8888-textur till en rundad rektangel med radien r (exakt täckning per pixel, via alfakanalen) */
void tex_round_corners(Texture *t, float r);

/* Skapar textur (8888 eller 5650) från bilden. */
Texture *img_to_texture(const Image *img, int psm);

/* NTX: psp5:s egna råa texturfiler – laddas utan avkodning.
 * Header (16 byte): "NTX1", u16 w, u16 h, u16 psm, u16 stride, u32 reserv.
 * Därefter stride*h pixlar (4 byte för 8888, 2 för 5650). */
Texture *ntx_load(const char *path);
int ntx_save(const char *path, const Texture *t);

#endif
