/* psp5 – made by Mille Weber */
/* Färgteman. PS5 har inga egna färgteman, men konsolen säljs med skal i olika färger
 * (Midnight Black, Cosmic Red, Nova Pink, Galactic Purple, Deep Earth-
 * serien Volcanic Red/Cobalt Blue/Sterling Silver och Chroma Teal),
 * och inställningen Utseende låter hemskärmen se ut som PS1 (grå konsol och knapparnas
 * symboler), PS2 (mörka "torn" och glödande klot), PS3 (XMB-vågen) eller PS4 (blå
 * vågor och partiklar). Temana här bygger på dem – allt ritas i kod, inga bilder. */
#include <math.h>
#include <string.h>
#include "theme.h"

#define PI 3.14159265f
#define C(r, g, b) RGBA(r, g, b, 255)

unsigned char g_theme = 0;
unsigned char g_theme_art = 1;

static const Theme THEMES[] = {
    /* code, namn, intro, surf_top, surf_bottom, glow, haze, sheen, accent, card, deco, beam, dust */
    { "ps5", "PS5", C(36, 41, 52), C(48, 46, 50), C(22, 23, 29),
      RGBA(215, 200, 190, 95), RGBA(92, 126, 132, 80), 0, C(238, 238, 242), C(40, 41, 48), DECO_SPOT, C(255, 216, 184), C(255, 178, 100) },
    { "black", "Midnight Black", C(26, 26, 29), C(30, 30, 34), C(9, 9, 11),
      RGBA(150, 156, 172, 62), RGBA(90, 96, 112, 32), 0, C(206, 210, 220), C(30, 30, 35), DECO_SPOT, C(206, 210, 226), C(178, 186, 214) },
    { "red", "Cosmic Red", C(83, 19, 29), C(96, 22, 32), C(26, 7, 11),
      RGBA(255, 128, 128, 72), RGBA(210, 60, 72, 42), 0, C(238, 76, 88), C(62, 22, 28), DECO_SPOT, C(255, 196, 188), C(255, 140, 120) },
    { "pink", "Nova Pink", C(99, 33, 75), C(112, 40, 84), C(30, 10, 25),
      RGBA(255, 172, 214, 78), RGBA(232, 110, 172, 42), 0, C(246, 136, 190), C(70, 30, 56), DECO_SPOT, C(255, 208, 232), C(255, 160, 205) },
    { "purple", "Galactic Purple", C(61, 35, 107), C(68, 40, 114), C(17, 9, 35),
      RGBA(204, 172, 255, 78), RGBA(142, 100, 222, 42), 0, C(176, 136, 244), C(50, 36, 80), DECO_SPOT, C(226, 208, 255), C(190, 160, 255) },
    { "volcanic", "Volcanic Red", C(97, 37, 21), C(106, 42, 22), C(28, 10, 6),
      RGBA(255, 156, 96, 82), RGBA(232, 92, 42, 44), 0, C(252, 116, 64), C(70, 32, 22), DECO_SPOT, C(255, 214, 180), C(255, 160, 90) },
    { "cobalt", "Cobalt Blue", C(23, 43, 131), C(30, 50, 142), C(7, 13, 46),
      RGBA(142, 172, 255, 82), RGBA(62, 104, 242, 44), 0, C(86, 136, 255), C(28, 40, 96), DECO_SPOT, C(188, 208, 255), C(130, 170, 255) },
    { "silver", "Sterling Silver", C(90, 94, 104), C(98, 102, 110), C(29, 31, 35),
      RGBA(236, 242, 252, 82), RGBA(172, 182, 198, 42), 0, C(214, 220, 230), C(64, 68, 76), DECO_SPOT, C(240, 244, 252), C(214, 222, 238) },
    { "teal", "Chroma Teal", C(17, 79, 85), C(22, 86, 92), C(5, 23, 25),
      RGBA(146, 244, 232, 78), RGBA(42, 182, 172, 42), RGBA(126, 126, 255, 40), C(66, 214, 204), C(20, 60, 62), DECO_SPOT, C(190, 248, 240), C(110, 230, 215) },
    /* PS1–PS4, som PS5:s Utseende */
    { "ps1", "PS1", C(108, 108, 116), C(116, 116, 124), C(46, 46, 52),
      RGBA(244, 244, 248, 56), RGBA(176, 176, 186, 30), 0, C(224, 224, 232), C(80, 80, 88), DECO_PS1, C(246, 246, 252), 0 },
    { "ps2", "PS2", C(6, 10, 28), C(14, 20, 46), C(2, 2, 8),
      RGBA(60, 110, 255, 58), RGBA(40, 80, 210, 40), 0, C(96, 146, 255), C(18, 24, 48), DECO_PS2, C(150, 190, 255), 0 },
    { "ps3", "PS3", C(26, 42, 86), C(36, 54, 100), C(8, 13, 32),
      RGBA(190, 210, 255, 50), RGBA(120, 150, 220, 34), 0, C(156, 194, 255), C(30, 40, 66), DECO_PS3, C(206, 220, 255), 0 },
    { "ps4", "PS4", C(6, 50, 134), C(18, 62, 144), C(4, 18, 58),
      RGBA(124, 184, 255, 70), RGBA(60, 140, 255, 42), 0, C(76, 156, 255), C(20, 46, 96), DECO_PS4, C(160, 208, 255), 0 },
};
#define THEME_COUNT (int)(sizeof(THEMES) / sizeof(THEMES[0]))
_Static_assert(THEME_COUNT == THEME_FIRST_CLASSIC + 4, "PS1-PS4 ska komma sist");

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

int theme_count(void) { return THEME_COUNT; }
const Theme *theme_get(int i) { return &THEMES[i >= 0 && i < THEME_COUNT ? i : 0]; }
const Theme *theme_cur(void) { return theme_get(g_theme); }

int theme_from_code(const char *code)
{
    for (int i = 0; i < THEME_COUNT; i++) {
        int n = strlen(THEMES[i].code);
        if (!strncmp(code, THEMES[i].code, n) && (code[n] == 0 || code[n] == '\n' || code[n] == '\r'))
            return i;
    }
    return 0;
}

/* färgkanal k (0 = röd) och färg med ny alfa */
static int ch(u32 c, int k) { return (c >> (k * 8)) & 0xFF; }
static u32 alpha(u32 c, float a) { return COLOR_ALPHA(c, (int)(ch(c, 3) * clampf(a, 0, 1))); }
static u32 scaled(u32 c, float k, int a)
{
    return RGBA((int)(ch(c, 0) * k), (int)(ch(c, 1) * k), (int)(ch(c, 2) * k), a);
}

u32 theme_toggle_on(const Theme *t)
{
    return scaled(t->accent, 0.63f, 255);
}

/* --- mönster --- */

/* PS1: handkontrollens fyra symboler i sina klassiska färger, som en ruta på konsolen */
static void deco_ps1(float time, float a, float cx, float cy, float k)
{
    const float d = 24 * k, r = 10.5f * k, w = 2.2f * (0.5f + 0.5f * k);
    float bob = sinf(time * 0.8f) * 2;
    u32 cols[4] = { RGBA(64, 226, 160, 0), RGBA(255, 102, 102, 0), RGBA(124, 178, 232, 0), RGBA(255, 120, 232, 0) };
    float pos[4][2] = { { cx, cy - d }, { cx + d, cy }, { cx, cy + d }, { cx - d, cy } };
    for (int n = 0; n < 4; n++) {
        float x = pos[n][0], y = pos[n][1] + bob;
        u32 c = COLOR_ALPHA(cols[n], (int)(150 * a));
        u32 cc[2] = { c, c };
        gfx_glow(x - r, y - r, r * 2, r * 2, 8, COLOR_ALPHA(cols[n], (int)(28 * a)));
        if (n == 0) { /* △ */
            float tri[4][2] = { { x, y - r }, { x + r * 0.95f, y + r * 0.7f }, { x - r * 0.95f, y + r * 0.7f }, { x, y - r } };
            for (int e = 0; e < 3; e++) {
                float seg[4] = { tri[e][0], tri[e][1], tri[e + 1][0], tri[e + 1][1] };
                gfx_stroke(seg, cc, 2, w);
            }
        } else if (n == 1) { /* ○ */
            gfx_ring(x - r, y - r, r * 2, r * 2, r, c);
        } else if (n == 2) { /* ✕ */
            float s = r * 0.8f;
            float l1[4] = { x - s, y - s, x + s, y + s }, l2[4] = { x + s, y - s, x - s, y + s };
            gfx_stroke(l1, cc, 2, w);
            gfx_stroke(l2, cc, 2, w);
        } else { /* □ */
            float s = r * 0.78f;
            gfx_frame(x - s, y - s, s * 2, s * 2, w * 0.8f, c);
        }
    }
}

/* PS2: mörka glastorn som stiger och sjunker, och blå klot som driver i mörkret */
static void deco_ps2(float time, float a)
{
    gfx_blend_add(1);
    for (int i = 0; i < 5; i++) {
        float x = 240 + 220 * (0.5f + 0.5f * sinf(i * 2.1f + time * 0.07f * (1 + i * 0.3f)));
        float y = 60 + 150 * (0.5f + 0.5f * sinf(i * 1.3f + 1 + time * 0.05f * (1 + i * 0.2f)));
        float rr = 30 + i * 9;
        gfx_dot(x, y, rr, RGBA(36, 76, 220, (int)(34 * a)), 1);
        gfx_dot(x, y, rr * 0.22f, RGBA(120, 170, 255, (int)(40 * a)), 1);
    }
    for (int i = 0; i < 13; i++) {
        float x = 236 + i * 18.5f;
        float h = 28 + 80 * (0.5f + 0.5f * sinf(i * 1.7f + time * (0.25f + (i % 4) * 0.06f)));
        float wdt = 9 + (i % 3) * 2;
        gfx_gradient_v(x, SCREEN_H - h, wdt, h, RGBA(60, 110, 255, 0), RGBA(60, 110, 255, (int)(46 * a)));
        gfx_rect(x, SCREEN_H - h, wdt, 1, RGBA(150, 190, 255, (int)(60 * a)));
    }
    gfx_blend_add(0);
}

#define WAVE_PTS 33

/* Ett vågband över skärmen: brett och svagt, med en tunn ljus linje i mitten */
static void wave(float time, float base, float amp, float speed, float phase, u32 color,
                 float band_w, int band_a, int line_a, float a)
{
    float xy[WAVE_PTS * 2];
    u32 band[WAVE_PTS], line[WAVE_PTS];
    for (int i = 0; i < WAVE_PTS; i++) {
        float x = -10 + i * (500.0f / (WAVE_PTS - 1));
        float y = base + amp * sinf(x * 0.011f + time * speed + phase)
                + amp * 0.45f * sinf(x * 0.026f - time * speed * 1.4f + phase * 2);
        float edge = sinf(PI * clampf(x / SCREEN_W, 0, 1)); /* bleknar mot kanterna */
        xy[i * 2] = x;
        xy[i * 2 + 1] = y;
        band[i] = COLOR_ALPHA(color, (int)(band_a * edge * a));
        line[i] = COLOR_ALPHA(color, (int)(line_a * edge * a));
    }
    gfx_stroke(xy, band, WAVE_PTS, band_w);
    gfx_stroke(xy, line, WAVE_PTS, 1.3f);
}

/* PS3: XMB-vågen – tre tunna, ljusa band som böljar långsamt */
static void deco_ps3(float time, float a)
{
    gfx_blend_add(1);
    for (int w = 0; w < 3; w++)
        wave(time, 150 + w * 9, 20 - w * 5, 0.32f + w * 0.07f, w * 1.7f, RGBA(255, 255, 255, 0),
             30 - w * 8, 12, 110 - w * 25, a);
    gfx_blend_add(0);
}

/* PS4: breda, mjuka ljusblå vågor och små partiklar som stiger */
static void deco_ps4(float time, float a)
{
    gfx_blend_add(1);
    wave(time, 176, 26, 0.22f, 0.4f, RGBA(140, 200, 255, 0), 70, 14, 60, a);
    wave(time, 196, 18, 0.28f, 2.2f, RGBA(140, 200, 255, 0), 44, 12, 46, a);
    for (int i = 0; i < 16; i++) {
        float speed = 9 + (i % 5) * 4;
        float y = SCREEN_H + 10 - fmodf(time * speed + i * 37.0f, SCREEN_H + 30);
        float x = fmodf(i * 61.7f, SCREEN_W) + sinf(time * 0.6f + i) * 8;
        float fade = clampf((SCREEN_H + 10 - y) / 60, 0, 1) * clampf(y / 60, 0, 1);
        gfx_circle(x, y, 1.3f + (i % 3) * 0.6f, RGBA(180, 220, 255, (int)(150 * fade * a))); /* three sizes, so few images */
    }
    gfx_blend_add(0);
}

/* The scene: a soft beam of light from the lamp at the top left, a cool haze at the bottom left and - for the PS5
 * colours, which have a dust colour - a band of golden dust across the middle, like the startup screen (boot.c) but
 * standing still: the same picture every frame, so there is nothing to compute. The light, the haze and the dust take the
 * colours of the theme. `k` scales how strong it is (every screen gets the same: 1; only the beam of light is softer). */
static float spot_rnd(unsigned *seed)
{
    *seed = *seed * 1664525u + 1013904223u;
    return ((*seed >> 8) & 0xFFFF) / 65535.0f;
}

static u32 mix_rgb(u32 a, u32 b, float t, int alpha_value)
{
    return RGBA((int)(ch(a, 0) + (ch(b, 0) - ch(a, 0)) * t), (int)(ch(a, 1) + (ch(b, 1) - ch(a, 1)) * t),
                (int)(ch(a, 2) + (ch(b, 2) - ch(a, 2)) * t), alpha_value);
}

static float smooth_step(float a, float b, float x)
{
    float t = clampf((x - a) / (b - a), 0, 1);
    return t * t * (3 - 2 * t);
}

/* The beam from the lamp: one mesh of small quads, rows down the screen and columns across the beam, each corner with its own
 * brightness. (A beam of a few large quads showed a hard edge along the diagonal of every quad: the colours of a triangle
 * are blended between its three corners only, so the part of the quad past the diagonal was nearly dark.) The beam leans to
 * the right and widens downwards like the light of a lamp in haze: the right edge is soft but clear and ends far enough out
 * at the bottom (x = 400) to cover the haze there, and the left side is a long gentle slope that runs off the screen. */
#define LAMP_X (SCREEN_W * 0.17f + 2)
#define BEAM_STRENGTH 0.6f /* how strong the beam of light is next to everything else on the surface */
#define BEAM_ROWS 8
#define BEAM_COLS 11
static void deco_beam(const Theme *t, float k)
{
    static const float row_y[BEAM_ROWS + 1] = { -2, 14, 32, 56, 88, 128, 176, 226, SCREEN_H };
    /* across the beam: -1 = where the light ends on the left, 0 = the brightest line, 1 = where the right edge is */
    static const float col_u[BEAM_COLS + 1] = { -1, -0.75f, -0.5f, -0.25f, 0, 0.3f, 0.6f, 0.85f, 1.0f, 1.12f, 1.25f, 1.4f };
    float vx[BEAM_ROWS + 1][BEAM_COLS + 1];
    u32 vc[BEAM_ROWS + 1][BEAM_COLS + 1];
    for (int j = 0; j <= BEAM_ROWS; j++) {
        float y = row_y[j], yy = y > 0 ? y : 0;
        float ridge = LAMP_X + 0.45f * y;          /* the brightest line leans to the right */
        float left = 65 + 1.07f * yy, right = 66 + 0.5f * yy; /* from it to where the light ends, left and right */
        float peak = (0.15f + 0.30f * expf(-yy / 75)) * (1 - 0.35f * smooth_step(200, SCREEN_H, y)) * 255 * k; /* brightest at the lamp, fainter further down */
        for (int i = 0; i <= BEAM_COLS; i++) {
            float u = col_u[i], p;
            if (u < 0)
                p = 1 + u;
            else
                p = (1 - 0.55f * u) * (1 - smooth_step(0.82f, 1.12f, u));
            vx[j][i] = ridge + u * (u < 0 ? left : right);
            vc[j][i] = COLOR_ALPHA(t->beam, (int)(peak * p));
        }
    }
    for (int j = 0; j < BEAM_ROWS; j++)
        for (int i = 0; i < BEAM_COLS; i++) {
            float xy[8] = { vx[j][i], row_y[j], vx[j][i + 1], row_y[j], vx[j + 1][i], row_y[j + 1], vx[j + 1][i + 1], row_y[j + 1] };
            u32 c[4] = { vc[j][i], vc[j][i + 1], vc[j + 1][i], vc[j + 1][i + 1] };
            gfx_quad(xy, c);
        }
}

static void deco_spot(const Theme *t, float k)
{
    gfx_blend_add(1);
    const float kb = k * BEAM_STRENGTH; /* the beam of light (and the lamp it comes from) is softer than the rest */
    deco_beam(t, kb);
    gfx_dot(LAMP_X - 2, -10, 70, COLOR_ALPHA(t->beam, (int)(120 * kb)), 1);
    gfx_dot(LAMP_X - 2, -6, 26, COLOR_ALPHA(t->beam, (int)(140 * kb)), 1);
    /* the cool haze at the bottom left */
    gfx_glow(-150, 226, 540, 44, 90, COLOR_ALPHA(t->haze, (int)(ch(t->haze, 3) * 0.55f * k)));
    gfx_glow(-60, 262, 380, 10, 40, COLOR_ALPHA(t->haze, (int)(ch(t->haze, 3) * 0.3f * k)));

    if (!t->dust) { /* the classic consoles have the light and the haze but no dust */
        gfx_blend_add(0);
        return;
    }

    /* the dust: a band rising from the lower left to the right, thickest in the middle; mostly specks, some soft discs */
    unsigned seed = 0x7d5a1u;
    for (int i = 0; i < 280; i++) {
        float x = SCREEN_W * (-0.02f + 1.04f * spot_rnd(&seed));
        float centre = SCREEN_H * (0.72f - 0.24f * (x / SCREEN_W));
        float spread = SCREEN_H * (0.04f + 0.13f * spot_rnd(&seed) * spot_rnd(&seed));
        float y = centre + (spot_rnd(&seed) + spot_rnd(&seed) + spot_rnd(&seed) - 1.5f) * 2 * spread;
        float size = 0.55f + spot_rnd(&seed) * spot_rnd(&seed) * 1.3f;
        float warm = spot_rnd(&seed), bright = 0.3f + 0.7f * spot_rnd(&seed);
        u32 col = mix_rgb(t->dust, RGBA(255, 255, 255, 0), 0.4f * (1 - warm), 0);
        gfx_dot(x, y, size * 1.35f, COLOR_ALPHA(col, (int)(255 * bright * k)), 1);
        if (size > 1.0f)
            gfx_dot(x, y, size * 4, COLOR_ALPHA(col, (int)(34 * bright * k)), 1);
    }
    for (int i = 0; i < 34; i++) {
        float x = SCREEN_W * (0.02f + 0.96f * spot_rnd(&seed));
        float y = SCREEN_H * (0.72f - 0.24f * (x / SCREEN_W)) + (spot_rnd(&seed) - 0.5f) * 90 * spot_rnd(&seed);
        float r = 1.6f + spot_rnd(&seed) * spot_rnd(&seed) * 5.4f, bright = 0.4f + 0.6f * spot_rnd(&seed);
        gfx_dot(x, y, r, COLOR_ALPHA(t->dust, (int)(52 * bright * k)), 0);
        gfx_dot(x, y, r * 1.3f, COLOR_ALPHA(t->dust, (int)(18 * bright * k)), 1);
    }
    gfx_blend_add(0);
}

/* What is the same every frame: the surface, the glows, and the beam of light with its haze and dust. */
static void draw_scene(const Theme *t, float a)
{
    /* the surface is the same on every screen: the menu, Library, Music, Settings, Information, the file manager ... */
    gfx_gradient_v_opaque(0, 0, SCREEN_W, SCREEN_H, t->surf_top, t->surf_bottom);
    gfx_gradient_h(SCREEN_W / 2, 0, SCREEN_W / 2, SCREEN_H, scaled(t->surf_bottom, 0.8f, 0), scaled(t->surf_bottom, 0.8f, 150));
    /* strålkastarljus uppe till vänster och ett svagt dis nere */
    gfx_glow(70, -60, 60, 60, 120, alpha(t->glow, 0.84f));
    gfx_glow(90, -30, 20, 30, 40, RGBA(ch(t->glow, 0) + (255 - ch(t->glow, 0)) / 2, ch(t->glow, 1) + (255 - ch(t->glow, 1)) / 2,
                                       ch(t->glow, 2) + (255 - ch(t->glow, 2)) / 2, 50));
    gfx_glow(-30, 236, 280, 30, 46, alpha(t->haze, 0.93f));
    if (t->sheen)
        gfx_glow(330, 170, 160, 80, 120, alpha(t->sheen, 0.8f));
    if (t->beam)
        deco_spot(t, a); /* the light cone and the haze: every theme has them, under its own decorations */
}

/* ---- The Library's own background ------------------------------------------------------------------------------------------------------------------
 * Behind the Library there is no theme surface and no picture of a game: a ribbon of fine light on black, at the right, drawn in code like everything
 * else (the picture it follows is a render of a twisted sheet: a petal of nested arcs, a fan of lines and big rings, with a copper light along its edges
 * and a few specks of dust). It is the same in every theme; the theme gives it its colours: the lines are white with some of the theme's light, the
 * warm light is the gold of the PS5 colours (the accent for the old consoles), the black has a trace of the theme's own dark. Like the surface it never
 * moves, so it is drawn once into the picture in video memory; only the little star of light twinkles (lib_star). The coordinates are fractions of the
 * screen (u across, v down). */
static float lib_x(float u) { return u * SCREEN_W; }
static float lib_y(float v) { return v * SCREEN_H; }
static float lerp_f(float a, float b, float t) { return a + (b - a) * t; }

typedef struct {
    u32 line; /* the colour of the fine lines (alpha 0) */
    u32 rim;  /* the bright edges */
    u32 warm; /* the copper light */
} LibPalette;

static LibPalette lib_palette(const Theme *t)
{
    LibPalette p;
    p.line = mix_rgb(RGBA(255, 255, 255, 0), t->beam, 0.28f, 0);
    p.warm = t->dust ? t->dust : t->accent;
    p.rim = mix_rgb(RGBA(255, 255, 255, 0), p.warm, 0.14f, 0);
    return p;
}

static u32 lib_col(u32 rgb, float a)
{
    a = a > 0 ? (a < 1 ? a : 1) : 0; /* (also for NaN: it would be cast to an int, and on the PSP that gives a full alpha) */
    return COLOR_ALPHA(rgb, (int)(255 * a + 0.5f));
}

/* sin(PI * t) ^ e for t from 0 to 1, a bell that is 0 at both ends. sinf(PI) is a little below 0, and a power of a negative number is NaN: it is cut at 0 */
static float lib_bell(float t, float e)
{
    return powf(fmaxf(0.0f, sinf(PI * t)), e);
}

#define LIB_PTS 48
/* A fine line through the n points, its brightness `peak` in the middle and fading to the ends (the ends fade as sin^0.6; `lean` > 0 makes it brighter towards the last point) */
static void lib_hair(const float *xy, int n, u32 rgb, float peak, float lean)
{
    u32 c[LIB_PTS];
    for (int i = 0; i < n; i++) {
        float t = n > 1 ? (float)i / (n - 1) : 0;
        c[i] = lib_col(rgb, peak * lib_bell(t, 0.6f) * (1 + lean * (t - 0.5f)));
    }
    gfx_polyline(xy, c, n);
}

/* One cubic curve of the petal: from the crown (s along it, 0 = the left rim, 1 = the right edge) down to the foot. The ends are spread along the crown and the foot,
 * and the two inner points bulge to the left for the rim and to the right for the edge, a little twisted: the sheet is seen at a slant. */
static void lib_petal_point(float s, float t, float *x, float *y)
{
    float x0 = lib_x(lerp_f(0.676f, 0.742f, s)), y0 = lib_y(lerp_f(0.262f, 0.272f, s));
    float x3 = lib_x(lerp_f(0.650f, 0.712f, s)), y3 = lib_y(lerp_f(0.606f, 0.588f, s));
    float b1 = lib_x(lerp_f(-0.072f, 0.070f, s)), b2 = lib_x(lerp_f(-0.060f, 0.052f, s)) - lib_x(0.014f);
    float x1 = x0 + b1, y1 = y0 + lib_y(0.15f), x2 = x3 + b2, y2 = y3 - lib_y(0.17f);
    float u = 1 - t;
    *x = u * u * u * x0 + 3 * u * u * t * x1 + 3 * u * t * t * x2 + t * t * t * x3;
    *y = u * u * u * y0 + 3 * u * u * t * y1 + 3 * u * t * t * y2 + t * t * t * y3;
}

static void lib_petal(const LibPalette *p)
{
    const int K = 19, N = 24;
    unsigned seed = 0x2f61u;
    for (int k = 0; k < K; k++) {
        float s = (float)k / (K - 1);
        float t0 = 0.02f + 0.16f * spot_rnd(&seed) * s, t1 = 0.98f - 0.20f * spot_rnd(&seed) * s;
        float xy[2 * LIB_PTS];
        for (int i = 0; i < N; i++)
            lib_petal_point(s, lerp_f(t0, t1, (float)i / (N - 1)), &xy[i * 2], &xy[i * 2 + 1]);
        lib_hair(xy, N, p->line, 0.06f + 0.13f * (1 - s) * (1 - s) + 0.05f * spot_rnd(&seed), 0.5f);
    }
}

static void lib_stroke(const float *xy, int n, u32 rgb, float peak, float width, float lean)
{
    u32 c[LIB_PTS];
    for (int i = 0; i < n; i++) {
        float t = n > 1 ? (float)i / (n - 1) : 0;
        c[i] = lib_col(rgb, peak * lib_bell(t * 0.94f + 0.03f, 0.5f) * (1 + lean * (t - 0.5f)));
    }
    gfx_stroke(xy, c, n, width);
}

static void lib_rims(const LibPalette *p)
{
    /* the left rim of the petal: the first arc, bright and a little thick */
    float xy[2 * LIB_PTS];
    const int N = 24;
    for (int i = 0; i < N; i++)
        lib_petal_point(0, 0.01f + 0.98f * i / (N - 1), &xy[i * 2], &xy[i * 2 + 1]);
    lib_stroke(xy, N, p->rim, 0.5f, 1.2f, -0.4f);
    /* the foot: the thick curve under the petal that runs on to the left and down */
    static const float foot[][2] = { { 0.616f, 0.612f }, { 0.642f, 0.612f }, { 0.668f, 0.606f }, { 0.694f, 0.598f }, { 0.716f, 0.584f }, { 0.738f, 0.562f } };
    for (int i = 0; i < 6; i++) { xy[i * 2] = lib_x(foot[i][0]); xy[i * 2 + 1] = lib_y(foot[i][1]); }
    lib_stroke(xy, 6, p->rim, 0.8f, 2.0f, 0.2f);
    for (int i = 0; i < 6; i++) { xy[i * 2] = lib_x(foot[i][0] + 0.012f); xy[i * 2 + 1] = lib_y(foot[i][1] + 0.012f); }
    lib_stroke(xy, 6, p->line, 0.3f, 1.0f, 0.2f);
    /* the crown: a bright ring at the top of the petal */
    for (int i = 0; i < 18; i++) {
        float a = lerp_f(3.3f, 6.1f, (float)i / 17);
        xy[i * 2] = lib_x(0.688f) + cosf(a) * lib_x(0.032f);
        xy[i * 2 + 1] = lib_y(0.264f) + sinf(a) * lib_y(0.026f);
    }
    lib_stroke(xy, 18, p->rim, 0.6f, 1.3f, 0.3f);
}

/* The fan: lines from the tip on the right to a curved edge at the left, brightest at the tip */
static void lib_fan(const LibPalette *p)
{
    const float fx = lib_x(0.945f), fy = lib_y(0.413f);
    const int K = 15;
    for (int k = 0; k < K; k++) {
        float s = (float)k / (K - 1);
        float ax = lib_x(lerp_f(0.766f, 0.800f, s) + 0.012f * sinf(PI * s)), ay = lib_y(lerp_f(0.255f, 0.430f, s));
        float xy[2 * 6];
        u32 c[6];
        for (int i = 0; i < 6; i++) {
            float t = (float)i / 5;
            xy[i * 2] = lerp_f(ax, fx, t);
            xy[i * 2 + 1] = lerp_f(ay, fy, t) + sinf(PI * t) * lib_y(0.012f) * (1 - s);
            c[i] = lib_col(p->line, (0.03f + 0.16f * t * t) * (0.55f + 0.45f * s));
        }
        gfx_polyline(xy, c, 6);
    }
    float xy[2 * LIB_PTS];
    static const float base[][2] = { { 0.800f, 0.430f }, { 0.837f, 0.421f }, { 0.885f, 0.396f }, { 0.927f, 0.394f }, { 0.945f, 0.413f } };
    for (int i = 0; i < 5; i++) { xy[i * 2] = lib_x(base[i][0]); xy[i * 2 + 1] = lib_y(base[i][1]); }
    lib_stroke(xy, 5, p->rim, 0.9f, 1.5f, 0.6f);
    static const float top[][2] = { { 0.770f, 0.252f }, { 0.830f, 0.285f }, { 0.890f, 0.340f }, { 0.945f, 0.413f } };
    for (int i = 0; i < 4; i++) { xy[i * 2] = lib_x(top[i][0]); xy[i * 2 + 1] = lib_y(top[i][1]); }
    lib_stroke(xy, 4, p->line, 0.45f, 1.0f, 0.5f);
}

/* The big rings at the bottom right, partly off the screen: nested ellipses, brightest on their left side */
static void lib_rings(const LibPalette *p)
{
    const float cx = lib_x(0.875f), cy = lib_y(0.845f), rot = -0.45f, cr = cosf(rot), sr = sinf(rot);
    const int K = 9, N = 44;
    for (int k = 0; k < K; k++) {
        float s = (float)k / (K - 1), rx = lib_x(lerp_f(0.085f, 0.205f, s)), ry = lib_y(lerp_f(0.115f, 0.260f, s));
        float xy[2 * LIB_PTS];
        u32 c[LIB_PTS];
        for (int i = 0; i < N; i++) {
            float a = 2 * PI * i / (N - 1), ex = rx * cosf(a), ey = ry * sinf(a);
            xy[i * 2] = cx + ex * cr - ey * sr + lib_x(0.012f) * s;
            xy[i * 2 + 1] = cy + ex * sr + ey * cr - lib_y(0.02f) * s;
            float side = fmaxf(0, cosf(a - PI - 0.3f));
            c[i] = lib_col(p->line, 0.025f + (0.05f + 0.17f * s) * side * side);
        }
        gfx_polyline(xy, c, N);
    }
}

/* long faint lines under the petal and the fan, from the petal's lower right edge to the loop with the star */
static void lib_sweeps(const LibPalette *p)
{
    for (int k = 0; k < 7; k++) {
        float s = k / 6.0f, xy[2 * 6];
        u32 c[6];
        for (int i = 0; i < 6; i++) {
            float t = i / 5.0f;
            xy[i * 2] = lib_x(lerp_f(0.735f + 0.02f * s, 0.862f - 0.01f * s, t));
            xy[i * 2 + 1] = lib_y(lerp_f(0.35f + 0.05f * s, 0.505f - 0.012f * s, t) - 0.03f * sinf(PI * t) * (1 - s));
            c[i] = lib_col(p->line, 0.12f * lib_bell(t, 0.7f) * (0.5f + 0.5f * s));
        }
        gfx_polyline(xy, c, 6);
    }
}

static void lib_dust(const LibPalette *p)
{
    unsigned seed = 0x51ab7u;
    for (int i = 0; i < 80; i++) {
        float x = lib_x(0.60f + 0.40f * spot_rnd(&seed)), y = lib_y(0.06f + 0.92f * spot_rnd(&seed));
        float size = 0.45f + 0.7f * spot_rnd(&seed) * spot_rnd(&seed), bright = 0.25f + 0.75f * spot_rnd(&seed);
        u32 col = spot_rnd(&seed) < 0.3f ? p->warm : p->line;
        gfx_dot(x, y, size, lib_col(col, 0.65f * bright), 1);
    }
    /* the dotted surface of the petal: specks that are thickest at its right edge, where the copper light is */
    for (int i = 0; i < 240; i++) {
        float t = 0.05f + 0.9f * spot_rnd(&seed), side = powf(spot_rnd(&seed), 2.2f); /* along the petal, and how far from the right edge towards the left */
        float edge = 0.745f + 0.034f * sinf(PI * t);
        float x = lib_x(edge - 0.115f * side), y = lib_y(0.265f + 0.305f * t - 0.02f * side);
        u32 col = side < 0.25f ? p->warm : p->line;
        gfx_dot(x, y, 0.5f, lib_col(col, (0.55f - 0.35f * side) * (0.5f + 0.5f * sinf(PI * t))), 1);
    }
    /* the dotted copper edge on the right of the petal */
    for (int i = 0; i < 46; i++) {
        float t = (float)i / 45;
        float x = lib_x(0.752f + 0.032f * sinf(PI * t) + 0.004f * (spot_rnd(&seed) - 0.5f) * 2), y = lib_y(0.265f + 0.305f * t + 0.006f * (spot_rnd(&seed) - 0.5f) * 2);
        gfx_dot(x, y, 0.7f, lib_col(p->warm, 0.4f + 0.4f * lib_bell(t, 0.5f)), 1);
    }
}

static void lib_scene(const Theme *t)
{
    LibPalette p = lib_palette(t);
    gfx_gradient_v_opaque(0, 0, SCREEN_W, SCREEN_H, scaled(t->surf_top, 0.16f, 255), scaled(t->surf_bottom, 0.10f, 255)); /* black with a trace of the theme */
    gfx_blend_add(1);
    gfx_glow(lib_x(0.74f), lib_y(0.15f), lib_x(0.07f), lib_y(0.12f), 60, lib_col(p.warm, 0.30f)); /* the copper light above the crown */
    gfx_glow(lib_x(0.80f), lib_y(0.30f), lib_x(0.10f), lib_y(0.20f), 80, lib_col(p.line, 0.07f));
    gfx_blend_add(0);
    lib_rings(&p);
    lib_sweeps(&p);
    lib_fan(&p);
    lib_petal(&p);
    lib_rims(&p);
    lib_dust(&p);
}

/* The little star at the end of the loop on the right: it twinkles, so it is drawn on top every frame */
#define LIB_STAR_U 0.868f
#define LIB_STAR_V 0.516f
static void lib_star(const Theme *t, float time)
{
    LibPalette p = lib_palette(t);
    float pulse = 0.75f + 0.25f * sinf(time * 2.3f), x = lib_x(LIB_STAR_U), y = lib_y(LIB_STAR_V);
    gfx_blend_add(1);
    gfx_dot(x, y, 7.0f * pulse, lib_col(p.rim, 0.5f), 1);
    gfx_dot(x, y, 2.2f, lib_col(RGBA(255, 255, 255, 0), 0.9f), 1);
    for (int k = 0; k < 4; k++) {
        float a = k * PI / 4 + 0.2f, len = (k % 2 ? 9.0f : 16.0f) * pulse;
        float line[6] = { x - cosf(a) * len, y - sinf(a) * len, x, y, x + cosf(a) * len, y + sinf(a) * len };
        u32 c[3] = { lib_col(p.rim, 0.0f), lib_col(p.rim, 0.7f), lib_col(p.rim, 0.0f) }; /* a ray: dark at both ends, bright where it crosses the star */
        gfx_polyline(line, c, 3);
    }
    gfx_blend_add(0);
}

/* The scene above never changes (the same theme gives the same picture every frame), so it is drawn once into a picture in
 * video memory and put on the screen from there: one opaque image instead of some 500 draw calls that touch every pixel
 * three or four times. It is drawn again when the theme (or `a`) is another, or when the picture has been lost. */
static struct {
    Theme theme;
    float a;
    int library; /* 1 = the Library's own background, 0 = the theme's surface */
    unsigned serial; /* gfx_snapshot_serial() when it was drawn */
    int valid;
} s_scene;

void theme_backdrop_forget(void)
{
    s_scene.valid = 0;
}

/* the picture in video memory is the one of `library` for the theme t and `a`: draws it first when it is not */
static void scene_ready(const Theme *t, float a, int library)
{
    if (!s_scene.valid || s_scene.serial != gfx_snapshot_serial() || s_scene.a != a || s_scene.library != library || memcmp(&s_scene.theme, t, sizeof *t)) {
        gfx_snapshot_begin();
        if (library)
            lib_scene(t);
        else
            draw_scene(t, a);
        gfx_snapshot_end();
        memcpy(&s_scene.theme, t, sizeof *t);
        s_scene.a = a;
        s_scene.library = library;
        s_scene.serial = gfx_snapshot_serial();
        s_scene.valid = 1;
    }
    gfx_snapshot_draw();
}

void theme_backdrop_library(const Theme *t, float time)
{
    scene_ready(t, 1, 1);
    lib_star(t, time);
}

void theme_backdrop(const Theme *t, float time, float a)
{
    scene_ready(t, a, 0);
    /* the old consoles' shapes move, so they are drawn on top every frame; the PS1 symbols sit at the right, below the row of cards
     * of Home and clear of the clock and the battery (the same on every screen) */
    switch (t->deco) {
    case DECO_PS1: deco_ps1(time, a, 418, 130, 0.6f); break;
    case DECO_PS2: deco_ps2(time, a); break;
    case DECO_PS3: deco_ps3(time, a); break;
    case DECO_PS4: deco_ps4(time, a); break;
    }
}

void theme_swatch(const Theme *t, float x, float y, float w, float h, float a)
{
    /* en liten "skärm" med temats toning och accentfärgen som en list – inte rund som ett reglage */
    gfx_rect(x, y, w, h, alpha(t->surf_top, a));
    float gy = floorf(h * 0.35f + 0.5f); /* (the gradient starts on a whole pixel) */
    gfx_gradient_v(x, y + gy, w, h - gy, alpha(COLOR_ALPHA(t->surf_bottom, 0), a), alpha(t->surf_bottom, a));
    gfx_rect(x + 3, y + h - 4, w - 6, 2, alpha(t->accent, a));
    if (t->deco != DECO_SPOT) { /* klassiska konsoler: en liten våg som kännetecken */
        float l[6] = { x + 3, y + h * 0.55f, x + w * 0.5f, y + h * 0.3f, x + w - 3, y + h * 0.5f };
        u32 c[3] = { alpha(RGBA(255, 255, 255, 60), a), alpha(RGBA(255, 255, 255, 170), a), alpha(RGBA(255, 255, 255, 60), a) };
        gfx_stroke(l, c, 3, 1.1f);
    }
    gfx_frame(x, y, w, h, 1, alpha(RGBA(255, 255, 255, 70), a)); /* syns även på mörka teman */
}
