/* psp5 – made by Mille Weber */
#ifndef GFX_H
#define GFX_H

#include <psptypes.h>
#include <pspgu.h>
#include "icons.h"

#define SCREEN_W 480
#define SCREEN_H 272

/* GU-färger är ABGR */
#define RGBA(r, g, b, a) ((u32)(r) | ((u32)(g) << 8) | ((u32)(b) << 16) | ((u32)(a) << 24))
#define COLOR_ALPHA(c, a) (((c) & 0x00FFFFFF) | ((u32)(a) << 24))

typedef struct {
    int w, h;     /* bildens storlek */
    int tw, th;   /* tvåpotens ≥ w/h (för GU) */
    int stride;   /* pixlar per rad i minnet */
    int psm;      /* GU_PSM_8888 eller GU_PSM_5650 */
    void *data;
    int swizzled; /* the pixels are in the order the GE reads fastest (tex_swizzle), not row after row: nothing may read or write them as rows any more */
    int vram;     /* the pixels are in video memory, which the GE reads much faster than main memory, and are not this texture's to free (the shapes of gfx_init) */
} Texture;

enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

void gfx_init(void);
void gfx_term(void);
void gfx_begin(u32 clear_color);
void gfx_end(void); /* väntar på vblank och byter buffert */
/* Som gfx_end, men anropar after_draw när bilden är ritad (t.ex. systemets tangentbord) */
void gfx_end_with(void (*after_draw)(void));

void gfx_rect(float x, float y, float w, float h, u32 color);
void gfx_gradient_v(float x, float y, float w, float h, u32 top, u32 bottom);
/* Som gfx_gradient_v men utan blandning: bägge färgerna måste ha alfa 255 */
void gfx_gradient_v_opaque(float x, float y, float w, float h, u32 top, u32 bottom);
void gfx_gradient_h(float x, float y, float w, float h, u32 left, u32 right);
void gfx_line(float x0, float y0, float x1, float y1, u32 color);
/* A thin line through the points xy[2n] (not smoothed), each point with its own colour: gfx_polyline() is for many lines (16 bytes of the display list a point). */
void gfx_polyline(const float *xy, const u32 *colors, int n);
void gfx_tex(const Texture *t, float x, float y, float w, float h, u32 tint);
/* A picture of the whole screen, kept in video memory, for something that looks the same every frame (the background):
 * draw it once between gfx_snapshot_begin() and gfx_snapshot_end() (the drawing goes into the picture, not onto the screen),
 * then gfx_snapshot_draw() puts the picture on the screen - one opaque image, far cheaper than drawing it all again. The
 * picture is lost when gfx_init() runs again; gfx_snapshot_serial() has changed by then. */
unsigned gfx_snapshot_serial(void);
void gfx_snapshot_begin(void);
void gfx_snapshot_end(void);
void gfx_snapshot_draw(void);
/* The same picture, but every pixel is the mean of 2 x 2 pixels of it (half a pixel down and to the right; the first column and row are as they were): lines that were drawn one pixel wide, which have steps, come out smooth. */
void gfx_snapshot_draw_soft(void);
/* Rundad rektangel med kantutjämning: kanterna på hela pixlar, hörnen exakt kantutjämnade */
void gfx_round_rect(float x, float y, float w, float h, float r, u32 color);
/* Mjuk skugga/glöd: rektangeln utökas med `spread` åt alla håll */
void gfx_glow(float x, float y, float w, float h, float spread, u32 color);
void gfx_circle(float cx, float cy, float r, u32 color);
/* Tunn rundad ram (ca 2 px vid r = 10) */
void gfx_ring(float x, float y, float w, float h, float r, u32 color);
/* A straight frame, on whole pixels: every side is `t` rounded (at least 1) pixels thick, wherever the frame is */
void gfx_frame(float x, float y, float w, float h, float t, u32 color);
/* Fylld polygon runt (cx, cy): n punkter i xy[2*n] */
void gfx_fan(float cx, float cy, const float *xy, int n, u32 color);
/* Kugghjul med radien r (med ett riktigt hål, `hole` används bara om bilden inte kan göras) */
void gfx_gear(float cx, float cy, float r, u32 color, u32 hole);
/* Ring med fast tjocklek precis innanför rektangeln – ramen runt det valda kortet: 2,5 px, eller 2 px om thin
 * är satt (de mindre korten i biblioteksrutnätet) */
void gfx_edge_ring(float x, float y, float w, float h, float r, int thin, u32 color);
/* Laddningsblixt, 2·r hög och centrerad i (cx, cy), med mjuka kanter */
void gfx_bolt(float cx, float cy, float r, u32 color);
/* The Wi-Fi sign in a box of side 2 r centred on (cx, cy): a small square with three soft quarter circle arcs round it (r = 7 is 14 px) */
void gfx_wifi(float cx, float cy, float r, u32 color);
/* The cube of Settings' System row (see icons.c, ICON_CUBE) in a box of side 2 r centred on (cx, cy): three faces of their own brightness in one soft
 * edged image, in the colour (and opacity) it is given: the top face as it is, the left one four fifths, the right one a half. */
void gfx_cube(float cx, float cy, float r, u32 color);
/* Mjukkantad triangel i rutan x, y, w, h: dir > 0 pekar åt höger (spetsen mitt på högra sidan), dir < 0 åt vänster */
void gfx_triangle(float x, float y, float w, float h, int dir, u32 color);
/* Musiknot i en ruta med sidan `size`, centrerad i (cx, cy) – en enda bild, så inget blandas dubbelt där delarna möts */
void gfx_note(float cx, float cy, float size, u32 color);
/* Högtalare (9 × 15 px): vänsterkanten vid x, mitten på höjden y */
void gfx_speaker(float x, float y, u32 color);
/* The same loudspeaker, `scale` times as big (the player's button is a little smaller than the volume popup's): its left edge at x, the middle at height y. */
void gfx_speaker_scaled(float x, float y, float scale, u32 color);
/* The picture of the Library card (six squares with a game controller in front), centred in the card whose pixels are the whole-pixel
 * box l, t, w, h, about two thirds of its height. Everything in it is on whole pixels and it is one image, so it looks the same
 * wherever the card is and the controller is exactly the same on its left and right. */
void gfx_library_icon(int l, int t, int w, int h, u32 color);
/* The picture of the Music card of the Media tab (the note of the reference), centred in the card box and two thirds of its height (see gfx.c) */
void gfx_music_icon(int l, int t, int w, int h, u32 color);
/* the pictures of the Videos and Images cards of Media (the film frame and the picture frame of the reference pictures), the same way */
void gfx_video_icon(int l, int t, int w, int h, u32 color);
void gfx_images_icon(int l, int t, int w, int h, u32 color);
/* The logo of the boot screen, `h` pixels high (and icon_logo_width(h) wide), its top left corner on the whole pixel (x, y), in one colour (the alpha
 * of the colour fades it). Drawn from its outline into one small image (about 90 x 70 pixels, once: it stays in the icon cache), so it costs a single
 * sprite per frame and is as smooth as the glyphs of the font; with no room for the image (it has to be less than 96 pixels wide with its border,
 * which is a logo up to 74 high), the outline is stroked instead. */
void gfx_logo(int x, int y, int h, u32 color);
/* The picture of a folder card (a folder with a tab), centred in the card box l, t, w, h like the Library picture, and made the same
 * way: one image for exactly this size, on whole pixels. `pic` (can be NULL) gets where it went and how it is built, so that the
 * letter of the folder can be put in the middle of its body. */
void gfx_folder_icon(int l, int t, int w, int h, u32 color, FolderPic *pic);
/* The same folder picture, `h` pixels high (1.38 times as wide) with its top left corner on the whole pixel (x, y), for a row of a list that has no card round it:
 * the file manager's folders. It is one image of exactly that size, like the one on a card, so the two look the same. */
void gfx_folder_picture(int x, int y, int h, u32 color);
/* Where the capital letter of a folder (one letter or a pair like "Ch") goes on the folder that gfx_folder_icon has just drawn (`pic` is what it
 * gave back), in the middle of its body (the part under the tab): the point to give text_draw with ALIGN_CENTER (the middle of the letter sideways, and
 * its baseline: half the height of a capital below the middle of the body) and the size of the text. Whole pixels. */
void gfx_folder_letter_at(const FolderPic *pic, float *x, float *y, float *size);

/* Additiv blandning (ljus som lyser upp det som redan ritats) på/av */
void gfx_blend_add(int on);
/* Tjock linje genom n punkter (xy[2*n]) med en färg per punkt */
/* Prick med radien r i en enda sprite – billig för många partiklar. soft = mjuk kant */
void gfx_dot(float cx, float cy, float r, u32 color, int soft);
/* `n` dots in ONE draw call, in the order given: exactly what `n` calls of gfx_dot() with the same `soft` draw, with the same pixels. A call of the display
 * list costs the PSP two kernel calls (sceGuGetMemory and sceGuDrawArray each tell the GE how far the list goes), so hundreds of dots are cheap in one and
 * slow one by one. At most GFX_DOTS_MAX in a call (more are drawn in several). */
typedef struct { float x, y, r; u32 color; } GfxDot;
#define GFX_DOTS_MAX 256
void gfx_dots(const GfxDot *d, int n, int soft);
/* Fyrhörning med en färg per hörn, ordning som triangelremsa (0-1 överst, 2-3 nederst) */
void gfx_quad(const float *xy, const u32 *colors);
/* Ritar bara inom rektangeln tills gfx_clip_reset */
void gfx_clip(float x, float y, float w, float h);
void gfx_clip_reset(void);
/* A name that is wider than its room does not shrink: it stands still for a moment, scrolls to its end, stays there a moment, and starts again.
 * text_marquee_offset() is how far it has gone `t` seconds after it appeared (0 while it waits and when it fits: overflow <= 0);
 * text_draw_marquee() draws it from x on, left aligned, in `room` pixels (nothing is drawn outside them) and scrolled for `t` seconds. */
#define MARQUEE_WAIT  1.2f  /* seconds it stands at the start */
#define MARQUEE_END   1.0f  /* ... and at the end */
#define MARQUEE_SPEED 36.0f /* pixels a second */
float text_marquee_offset(float overflow, float t);
void text_draw_marquee(float x, float y, float size, int bold, u32 color, float room, const char *s, float t);
/* The filled part of a bar: the left end rounded like the bar's (radius r), the right end a straight edge at x + w. Inside the clip that is set now (it is put back). */
void gfx_bar_fill(float x, float y, float w, float h, float r, u32 color);
void gfx_stroke(const float *xy, const u32 *colors, int n, float width);
/* A closed outline through the n points xy[2n] (the last joins the first), one colour, with the same smooth edges as gfx_stroke() at any place (the symbols of PS1 float on fractions of a pixel).
 * Every corner is a mitre and there are no ends, so nothing is drawn twice where it closes: a colour with some alpha has the same alpha all the way round. */
void gfx_stroke_loop(const float *xy, int n, float width, u32 color);

enum { BTN_CROSS, BTN_CIRCLE, BTN_TRIANGLE, BTN_SQUARE, BTN_START, BTN_SELECT };
/* Egenritad knappsymbol i en rund platta. START and SELECT are not round: a small pill with the word in it, 14 high (`r` is ignored for them,
 * the width is gfx_button_width(kind), an even number of pixels, so the pill is on whole pixels when `cx` is). */
void gfx_button(float cx, float cy, float r, int kind, u32 plate, u32 mark);
/* How wide the symbol is: 13 for the four round ones (r = 6.5), more for START and SELECT. A row of hints takes this much room for it. */
float gfx_button_width(int kind);

Texture *tex_create(int w, int h, int psm);
void tex_free(Texture *t);
void tex_flush(Texture *t); /* skriv tillbaka dcache efter CPU-ändringar */
/* Puts the pixels of a finished texture in the order the GE reads fastest: blocks of 16 bytes by 8 rows, one after the other. The GE reads a texture in main memory
 * through a small cache, and with the rows one after the other it has to fetch a new line for every row it steps down (the cards' pictures are shrunk, so it steps
 * down a lot); a swizzled one is read much faster. The picture comes out exactly the same. Do it last: after it nothing can read or write the pixels as rows
 * (Texture.swizzled; tex_round_corners() and ntx_save() refuse). Below the picture the texture gets some rows more (copies of its last row, up to a whole block of 8: at least
 * one). Returns 1 when the texture is swizzled (also when it was), 0 when not (no texture, no memory: it is left as it was). */
int tex_swizzle(Texture *t);

/* Text (intraFont, systemtypsnitt läses från flash0). `need_jpn` laddar japanskt alternativtypsnitt. */
void font_init(int need_jpn);
void font_term(void);
/* Laddar det japanska typsnittet (kanji, kana, kyrilliska) om det inte redan är laddat. */
void font_ensure_cjk(void);
float text_width(float size, int bold, const char *s);
void text_draw(float x, float y, float size, int bold, u32 color, int align, const char *s);
/* Största storlek ≤ size (ner till min_size) där texten ryms inom max_w */
float text_fit(float size, float min_size, int bold, float max_w, const char *s);
/* Kortar texten med "..." tills den ryms inom max_w */
void text_ellipsis(char *out, int out_size, const char *s, float size, float max_w);
/* Radbryter texten till högst max_lines rader (var och en högst 95 byte) inom max_w. Bryter vid mellanslag, och
 * mitt i ett ord bara om ordet inte ryms (japanska har inga mellanslag). Får inte allt plats får sista raden "...".
 * Returnerar antal rader. */
int text_wrap(const char *s, float size, float max_w, char lines[][96], int max_lines);

#endif
