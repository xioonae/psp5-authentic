/* psp5 – made by Mille Weber */
#ifndef THEME_H
#define THEME_H

#include "gfx.h"

/* Bakgrundens mönster */
enum { DECO_SPOT, DECO_PS1, DECO_PS2, DECO_PS3, DECO_PS4 };

/* Ett färgtema: PS5:s egna konsolfärger (skalen) och de fyra äldre konsolernas
 * utseende, som PS5:s inställning Utseende. Färgerna är RGBA() (ABGR). */
typedef struct {
    const char *code;   /* i psp5.cfg (theme=...) */
    const char *name;   /* produktnamn, översätts inte */
    u32 intro;          /* mörk grundton för startanimationens stora cirklar (boot.c) */
    u32 surf_top, surf_bottom; /* bakgrunden, uppe och nere: densamma på alla skärmar */
    u32 glow;           /* mjukt ljus uppe till vänster */
    u32 haze;           /* svagt dis längs nederkanten */
    u32 sheen;          /* andra ljuset nere till höger (Chroma), 0 = inget */
    u32 accent;         /* reglage, förlopp, laddningsprickar */
    u32 card;           /* kort som inte har laddat sin bild än */
    u8 deco;
    u32 beam;           /* ljusstrålen från lampan uppe till vänster, med dis nere till vänster (alla teman) */
    u32 dust;           /* bandet av gyllene damm (bara PS5:s färger, annars 0 = inget damm) */
} Theme;

enum { THEME_FIRST_CLASSIC = 9 }; /* PS1–PS4 kommer efter PS5:s färger */

/* Valt tema (index) och om spelens bilder visas bakom korten. Sparas i psp5.cfg. */
extern unsigned char g_theme;
extern unsigned char g_theme_art;

int theme_count(void);
const Theme *theme_get(int i);
const Theme *theme_cur(void);
/* "cobalt" → index, okänd kod → 0 (PS5). Läser till radslut. */
int theme_from_code(const char *code);

/* The whole background: the surface, the light and the theme's pattern. It looks exactly the same on every screen.
 * `a` = how visible the pattern is. The surface and the light never move, so they are drawn once into a picture in video
 * memory and shown from there (see gfx_snapshot_*); only the old consoles' shapes are drawn again every frame. */
void theme_backdrop(const Theme *t, float time, float a);
/* Draw the picture again the next time (after something that may have wiped video memory, like sleep) */
void theme_backdrop_forget(void);
/* Liten förhandsvisning (för inställningarna) */
void theme_swatch(const Theme *t, float x, float y, float w, float h, float a);
/* Temats färg för ett påslaget reglage */
u32 theme_toggle_on(const Theme *t);

#endif
