/* psp5 – made by Mille Weber */
#ifndef THEME_H
#define THEME_H

#include "gfx.h"

/* What moves on the background: the dust of the Default and the colour themes (DECO_SPOT), the symbols of PS1, the dots of PS2, the ribbon of PS3, the wave of PS4 */
enum { DECO_SPOT, DECO_PS1, DECO_PS2, DECO_PS3, DECO_PS4 };

/* A colour theme: the colours of the PS5's own covers (the Default is the console as it comes, the others are the covers) and the look of the four older consoles,
 * like the PS5's Appearance setting. The colours are RGBA() (ABGR). Every theme has the same background (the surface, the cone of light and the haze) drawn by the
 * same code with the same strengths (the alpha of `glow`, `haze` and `sheen` are the same in all of them): a theme is its colours. */
typedef struct {
    const char *code;   /* i psp5.cfg (theme=...) */
    const char *name;   /* a product name, not translated ("Default" for ps5) */
    u32 intro;          /* mörk grundton för startanimationens stora cirklar (boot.c) */
    u32 surf_top, surf_bottom; /* bakgrunden, uppe och nere: densamma på alla skärmar */
    u32 glow;           /* mjukt ljus uppe till vänster */
    u32 haze;           /* svagt dis längs nederkanten */
    u32 sheen;          /* the second light at the lower right (Chroma Teal, PS2, PS4), 0 = none */
    u32 accent;         /* the warm light of the old consoles' backdrop and the dust of the start-up animation (PS1-PS4 have no dust colour): the background only, nothing else is coloured with it (look.h) */
    u8 deco;
    u32 beam;           /* ljusstrålen från lampan uppe till vänster, med dis nere till vänster (alla teman) */
    u32 dust;           /* the colour of the dust that drifts in a wide area across the middle (the Default and the colour themes; PS1-PS4 have none: 0) */
} Theme;

enum { THEME_FIRST_CLASSIC = 9 }; /* PS1-PS4 come after the colours of the PS5 */

/* Valt tema (index) och om spelens bilder visas bakom korten. Sparas i psp5.cfg. */
extern unsigned char g_theme;
extern unsigned char g_theme_art;

int theme_count(void);
const Theme *theme_get(int i);
const Theme *theme_cur(void);
/* "cobalt" -> index; a code that is not a theme gives 0 (the Default; its code is still "ps5"). Reads to the end of the line. */
int theme_from_code(const char *code);

/* The whole background: the surface, the light and what moves on it. It looks exactly the same on every screen. `a` = how visible it is. The surface and the
 * light never move, so they are drawn once into a picture in video memory and shown from there (see gfx_snapshot_*); the dust, the symbols, the dots, the ribbon
 * and the wave are drawn again every frame, each a function of `time` that comes back to where it started (8 s; 6 s for PS3). */
void theme_backdrop(const Theme *t, float time, float a);
/* The Library's own background (behind the Library, in every theme, instead of the surface and instead of any picture of a game): a ribbon of fine light on black
 * at the right, drawn in code. It takes its colours from the theme. It shares the picture in video memory with theme_backdrop(): it is drawn again when the
 * other one was drawn in between. */
void theme_backdrop_library(const Theme *t, float time);
/* The background of the Media tab and of the Music screen (instead of the surface and any picture of a game): a ribbon of fine silky lines, drawn in code from the picture
 * "Media.png" in the colours of the theme (its lamp and its haze). It shares the picture in video memory with the other two and does not move. */
void theme_backdrop_media(const Theme *t, float time);
/* Draw the picture again the next time (after something that may have wiped video memory, like sleep) */
void theme_backdrop_forget(void);
/* Liten förhandsvisning (för inställningarna) */
void theme_swatch(const Theme *t, float x, float y, float w, float h, float a);

#endif
