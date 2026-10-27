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
void gfx_tex(const Texture *t, float x, float y, float w, float h, u32 tint);
/* A picture of the whole screen, kept in video memory, for something that looks the same every frame (the background):
 * draw it once between gfx_snapshot_begin() and gfx_snapshot_end() (the drawing goes into the picture, not onto the screen),
 * then gfx_snapshot_draw() puts the picture on the screen - one opaque image, far cheaper than drawing it all again. The
 * picture is lost when gfx_init() runs again; gfx_snapshot_serial() has changed by then. */
unsigned gfx_snapshot_serial(void);
void gfx_snapshot_begin(void);
void gfx_snapshot_end(void);
void gfx_snapshot_draw(void);
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
/* Mjukkantad triangel i rutan x, y, w, h: dir > 0 pekar åt höger (spetsen mitt på högra sidan), dir < 0 åt vänster */
void gfx_triangle(float x, float y, float w, float h, int dir, u32 color);
/* Musiknot i en ruta med sidan `size`, centrerad i (cx, cy) – en enda bild, så inget blandas dubbelt där delarna möts */
void gfx_note(float cx, float cy, float size, u32 color);
/* Högtalare (9 × 15 px): vänsterkanten vid x, mitten på höjden y */
void gfx_speaker(float x, float y, u32 color);
/* The picture of the Library card (six squares with a game controller in front), centred in the card whose pixels are the whole-pixel
 * box l, t, w, h, about two thirds of its height. Everything in it is on whole pixels and it is one image, so it looks the same
 * wherever the card is and the controller is exactly the same on its left and right. */
void gfx_library_icon(int l, int t, int w, int h, u32 color);
/* The picture of a folder card (a folder with a tab), centred in the card box l, t, w, h like the Library picture, and made the same
 * way: one image for exactly this size, on whole pixels. `pic` (can be NULL) gets where it went and how it is built, so that the
 * letter of the folder can be put in the middle of its body. */
void gfx_folder_icon(int l, int t, int w, int h, u32 color, FolderPic *pic);
/* Where the capital letter of a folder (one letter or a pair like "Ch") goes on the folder that gfx_folder_icon has just drawn (`pic` is what it
 * gave back), in the middle of its body (the part under the tab): the point to give text_draw with ALIGN_CENTER (the middle of the letter sideways, and
 * its baseline: half the height of a capital below the middle of the body) and the size of the text. Whole pixels. */
void gfx_folder_letter_at(const FolderPic *pic, float *x, float *y, float *size);

/* Additiv blandning (ljus som lyser upp det som redan ritats) på/av */
void gfx_blend_add(int on);
/* Tjock linje genom n punkter (xy[2*n]) med en färg per punkt */
/* Prick med radien r i en enda sprite – billig för många partiklar. soft = mjuk kant */
void gfx_dot(float cx, float cy, float r, u32 color, int soft);
/* Fyrhörning med en färg per hörn, ordning som triangelremsa (0-1 överst, 2-3 nederst) */
void gfx_quad(const float *xy, const u32 *colors);
/* Ritar bara inom rektangeln tills gfx_clip_reset */
void gfx_clip(float x, float y, float w, float h);
void gfx_clip_reset(void);
void gfx_stroke(const float *xy, const u32 *colors, int n, float width);

enum { BTN_CROSS, BTN_CIRCLE, BTN_TRIANGLE, BTN_SQUARE };
/* Egenritad knappsymbol i en rund platta */
void gfx_button(float cx, float cy, float r, int kind, u32 plate, u32 mark);

Texture *tex_create(int w, int h, int psm);
void tex_free(Texture *t);
void tex_flush(Texture *t); /* skriv tillbaka dcache efter CPU-ändringar */

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
