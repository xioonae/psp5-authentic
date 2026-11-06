/* psp5 – made by Mille Weber */
/* Colour themes. The PS5 has no colour themes of its own, but the console is sold with covers in different colours (Midnight Black, Cosmic Red, Nova Pink,
 * Galactic Purple, the Deep Earth series Volcanic Red / Cobalt Blue / Sterling Silver, and Chroma Teal), and the Appearance setting lets the home screen look
 * like the PS1 (grey console and the symbols of the buttons), the PS2 (dark "towers" and glowing dots), the PS3 (the XMB wave) or the PS4 (blue waves). The
 * themes here are built on those: everything is drawn in code, no pictures. Every theme has the same background (the surface, the cone of light from the lamp
 * and the haze: scene_ready); what differs is its colours, and the part that moves on top of it (the dust of the Default and the colour themes, the symbols
 * of PS1, the dots of PS2, the ribbon of PS3, the wave of PS4). */
#include <math.h>
#include <string.h>
#include "theme.h"
#include "themedata.h"

#define PI 3.14159265f
#define TAU 6.2831853f
#define C(r, g, b) RGBA(r, g, b, 255)

unsigned char g_theme = 0;
unsigned char g_theme_art = 1;

/* The strengths are the same in every theme: the lamp's glow, the haze at the bottom left and the second light at the lower right are drawn with these alphas
 * whatever the theme is, and a theme gives only the colours (and a sheen of its own, or none). */
#define GLOW_A 95
#define HAZE_A 80
#define SHEEN_A 230

static const Theme THEMES[] = {
    /* code, name, intro, surf_top, surf_bottom, glow, haze, sheen, accent, deco, beam, dust */
    { "ps5", "Default", C(36, 41, 52), C(48, 46, 50), C(22, 23, 29),
      RGBA(215, 200, 190, GLOW_A), RGBA(92, 126, 132, HAZE_A), 0, C(238, 238, 242), DECO_SPOT, C(255, 216, 184), C(255, 178, 100) },
    { "black", "Midnight Black", C(26, 26, 29), C(30, 30, 34), C(9, 9, 11),
      RGBA(111, 115, 127, GLOW_A), RGBA(47, 50, 58, HAZE_A), 0, C(206, 210, 220), DECO_SPOT, C(206, 210, 226), C(178, 186, 214) },
    { "red", "Cosmic Red", C(83, 19, 29), C(96, 22, 32), C(26, 7, 11),
      RGBA(218, 104, 106, GLOW_A), RGBA(133, 39, 48, HAZE_A), 0, C(238, 76, 88), DECO_SPOT, C(255, 196, 188), C(255, 140, 120) },
    { "pink", "Nova Pink", C(99, 33, 75), C(112, 40, 84), C(30, 10, 25),
      RGBA(230, 149, 191, GLOW_A), RGBA(147, 69, 111, HAZE_A), 0, C(246, 136, 190), DECO_SPOT, C(255, 208, 232), C(255, 160, 205) },
    { "purple", "Galactic Purple", C(61, 35, 107), C(68, 40, 114), C(17, 9, 35),
      RGBA(181, 149, 231, GLOW_A), RGBA(90, 63, 143, HAZE_A), 0, C(176, 136, 244), DECO_SPOT, C(226, 208, 255), C(190, 160, 255) },
    { "volcanic", "Volcanic Red", C(97, 37, 21), C(106, 42, 22), C(28, 10, 6),
      RGBA(235, 141, 86, GLOW_A), RGBA(150, 61, 29, HAZE_A), 0, C(252, 116, 64), DECO_SPOT, C(255, 214, 180), C(255, 160, 90) },
    { "cobalt", "Cobalt Blue", C(23, 43, 131), C(30, 50, 142), C(7, 13, 46),
      RGBA(127, 156, 239, GLOW_A), RGBA(42, 69, 164, HAZE_A), 0, C(86, 136, 255), DECO_SPOT, C(188, 208, 255), C(130, 170, 255) },
    { "silver", "Sterling Silver", C(90, 94, 104), C(98, 102, 110), C(29, 31, 35),
      RGBA(217, 223, 233, GLOW_A), RGBA(113, 119, 130, HAZE_A), 0, C(214, 220, 230), DECO_SPOT, C(240, 244, 252), C(214, 222, 238) },
    { "teal", "Chroma Teal", C(17, 79, 85), C(22, 86, 92), C(5, 23, 25),
      RGBA(125, 217, 208, GLOW_A), RGBA(29, 116, 111, HAZE_A), RGBA(28, 47, 72, SHEEN_A), C(66, 214, 204), DECO_SPOT, C(190, 248, 240), C(110, 230, 215) },
    /* PS1-PS4, as the Appearance of the PS5. The colours were measured from the originals (the four colour pictures of the real PS1-PS4 themes): a grey console with a
     * warm lamp, black with blue-violet lights, black with gold and cream, and blue-violet with purple at the lower right. They are drawn with the same strengths as
     * the colour themes, so the colours were chosen for those (the mean colour of the four corners of each theme is that of its picture: gfx/theme_test.c). */
    { "ps1", "PS1", C(52, 52, 50), C(53, 51, 49), C(41, 45, 48),
      RGBA(255, 255, 220, GLOW_A), RGBA(73, 74, 65, HAZE_A), 0, C(222, 220, 214), DECO_PS1, C(255, 255, 221), 0 },
    { "ps2", "PS2", C(3, 3, 44), C(1, 0, 58), C(0, 0, 6),
      RGBA(45, 36, 255, GLOW_A), RGBA(116, 112, 255, HAZE_A), RGBA(0, 0, 0, SHEEN_A), C(104, 100, 255), DECO_PS2, C(203, 226, 255), 0 },
    { "ps3", "PS3", C(14, 13, 11), C(24, 21, 17), C(0, 0, 1),
      RGBA(255, 255, 255, GLOW_A), RGBA(163, 155, 139, HAZE_A), 0, C(226, 200, 150), DECO_PS3, C(255, 255, 219), 0 },
    { "ps4", "PS4", C(14, 38, 100), C(20, 41, 93), C(22, 6, 56),
      RGBA(229, 220, 255, GLOW_A), RGBA(171, 255, 255, HAZE_A), RGBA(26, 10, 59, SHEEN_A), C(116, 150, 255), DECO_PS4, C(227, 255, 255), 0 },
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

/* The scene: a soft beam of light from the lamp at the top left and a cool haze at the bottom left: the same in every theme (the colours are the theme's), and
 * the same picture every frame, so there is nothing to compute. `k` scales how strong it is (every screen gets the same: 1; only the beam of light is softer). */
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

    gfx_blend_add(0);
}

/* ---- The moving shapes ---------------------------------------------------------------------------------------------------------------------------------
 * The surface and the light never move, so they are drawn once into a picture in video memory (scene_ready). What moves is drawn on top of it every frame, and
 * each of these is a pure function of the time: the dust of the Default and the colour themes, the four symbols of PS1, the ring of dots of PS2, the ribbon of
 * PS3 and the wave of PS4. They are drawn after the reference animations of the maintainer (the curves are in themedata.h) and every one of them loops: the
 * parts go a whole number of turns in the loop (LOOP_S = 8 s; the ribbon of PS3 has 6 s), so the last frame is followed by the first with no jump.
 * Angles are in turns (1 = a whole circle) and come from a table: there are a few thousand sines a frame. */
#define LOOP_S 8.0f
#define SIN_STEPS 1024

static float s_sin[SIN_STEPS + 1];

static float tsin(float turns)
{
    if (s_sin[SIN_STEPS / 4] == 0)
        for (int i = 0; i <= SIN_STEPS; i++)
            s_sin[i] = sinf(TAU * i / SIN_STEPS);
    float p = (turns - floorf(turns)) * SIN_STEPS;
    if (!(p >= 0 && p < SIN_STEPS)) /* (also NaN) */
        return 0;
    int i = (int)p;
    return s_sin[i] + (s_sin[i + 1] - s_sin[i]) * (p - i);
}

/* where the time is in a loop of `period` seconds, 0 to 1 */
static float loop_pos(float time, float period)
{
    float t = fmodf(time, period);
    return (t < 0 ? t + period : t) / period;
}

/* ---- The dust of the Default and the colour themes: specks, soft discs and a few stars that twinkle and drift a few pixels in the loop. They lie in the wide
 * area that the maintainer drew on the reference: it rises from the bottom left, is flat and wide in the middle and tall at the right. */
#define DUST_SPECKS 270
#define DUST_DISCS 36
#define DUST_STARS 14

typedef struct {
    float x, y;        /* where it floats */
    float size;        /* the radius of a speck and of a disc */
    float bright;      /* 0-1 */
    float warm;        /* a speck: 0 = whiter, 1 = the colour of the dust */
    float ax, ay;      /* how far it drifts, in pixels */
    float p1, p2, p3;  /* phases, in turns: drift in x, drift in y, twinkle */
    unsigned char h1, h2; /* turns of the drift and of the twinkle in the loop */
} Dust;

static Dust s_dust[DUST_SPECKS + DUST_DISCS + DUST_STARS];
static int s_dust_ready;

/* the area, top and bottom edge every 30 px (480 x 272 pixels), and how thick the dust is along it (a twelfth of the width at a time) */
static const float DUST_TOP[17] = { 182, 162, 137, 117, 105, 101, 101, 104, 105, 104, 101, 93, 86, 78, 71, 65, 63 };
static const float DUST_BOT[17] = { 249, 272, 267, 222, 197, 183, 179, 180, 188, 195, 204, 212, 219, 225, 225, 218, 198 };
static const float DUST_X[12] = { 19, 60, 101, 139, 180, 221, 259, 300, 341, 379, 420, 461 };
static const float DUST_THICK[12] = { 0.28f, 0.30f, 0.52f, 0.47f, 0.39f, 0.42f, 0.47f, 0.50f, 0.81f, 1.00f, 0.61f, 0.25f };

static float dust_edge(const float *v, float x) /* smooth through the 17 values */
{
    float u = clampf(x / 30, 0, 15.999f), f;
    int i = (int)u, i0 = i > 0 ? i - 1 : 0, i3 = i + 2 < 17 ? i + 2 : 16;
    f = u - i;
    return 0.5f * ((2 * v[i]) + (-v[i0] + v[i + 1]) * f + (2 * v[i0] - 5 * v[i] + 4 * v[i + 1] - v[i3]) * f * f + (-v[i0] + 3 * v[i] - 3 * v[i + 1] + v[i3]) * f * f * f);
}

static float dust_thick(float x)
{
    if (x <= DUST_X[0])
        return DUST_THICK[0];
    for (int i = 0; i < 11; i++)
        if (x < DUST_X[i + 1])
            return DUST_THICK[i] + (DUST_THICK[i + 1] - DUST_THICK[i]) * (x - DUST_X[i]) / (DUST_X[i + 1] - DUST_X[i]);
    return DUST_THICK[11];
}

static void dust_place(unsigned *seed, float stray, float *ox, float *oy)
{
    float x, top, bot;
    do {
        x = SCREEN_W * spot_rnd(seed);
        top = dust_edge(DUST_TOP, x);
        bot = dust_edge(DUST_BOT, x);
    } while (spot_rnd(seed) * 180 > (bot - top) * (0.65f + 0.35f * dust_thick(x)));
    float u = 0.5f * spot_rnd(seed) + 0.25f * (spot_rnd(seed) + spot_rnd(seed)); /* even in the middle of the area, thinner towards its edges */
    float y = top + (bot - top) * u;
    if (spot_rnd(seed) < stray)
        y += (spot_rnd(seed) - 0.5f) * 50; /* a few drift out of the area */
    *ox = x;
    *oy = y;
}

static void dust_make(void)
{
    unsigned seed = 0x9d3cu;
    int n = 0;
    for (int i = 0; i < DUST_SPECKS; i++, n++) {
        Dust *d = &s_dust[n];
        dust_place(&seed, 0.10f, &d->x, &d->y);
        float sz = spot_rnd(&seed);
        d->size = 0.55f + 1.7f * sz * sz * sz;
        d->bright = 0.42f + 0.58f * spot_rnd(&seed);
        d->warm = spot_rnd(&seed);
        d->h1 = (unsigned char)(1 + (int)(spot_rnd(&seed) * 2.99f));
        d->h2 = (unsigned char)(1 + (int)(spot_rnd(&seed) * 3.99f));
        d->ax = 1.4f + 2.2f * spot_rnd(&seed);
        d->ay = 1.4f + 2.2f * spot_rnd(&seed);
        d->p1 = spot_rnd(&seed);
        d->p2 = spot_rnd(&seed);
        d->p3 = spot_rnd(&seed);
    }
    for (int i = 0; i < DUST_DISCS; i++, n++) {
        Dust *d = &s_dust[n];
        dust_place(&seed, 0.25f, &d->x, &d->y);
        d->size = 1.7f + 2.0f * spot_rnd(&seed);
        d->bright = 0.45f + 0.55f * spot_rnd(&seed);
        d->p1 = spot_rnd(&seed);
        d->p2 = spot_rnd(&seed);
        d->p3 = spot_rnd(&seed);
    }
    for (int i = 0; i < DUST_STARS; i++, n++) {
        Dust *d = &s_dust[n];
        dust_place(&seed, 0.05f, &d->x, &d->y);
        d->p1 = spot_rnd(&seed);
        d->p2 = spot_rnd(&seed);
        d->p3 = spot_rnd(&seed);
    }
    s_dust_ready = 1;
}

static void deco_dust(const Theme *t, float time, float a)
{
    if (!t->dust || a <= 0)
        return;
    if (!s_dust_ready)
        dust_make();
    float u = loop_pos(time, LOOP_S);
    gfx_blend_add(1);
    for (int i = 0; i < DUST_SPECKS; i++) { /* specks: bright points, the larger ones with a halo */
        const Dust *d = &s_dust[i];
        float x = d->x + d->ax * tsin(d->h1 * u + d->p1), y = d->y + d->ay * tsin(d->h1 * u + d->p2);
        float tw = 0.5f + 0.5f * tsin(d->h2 * u + d->p3), al = d->bright * (0.35f + 0.65f * tw * (0.7f + 0.3f * tw)) * a;
        u32 col = mix_rgb(t->dust, RGBA(255, 255, 255, 0), 0.4f * (1 - d->warm), 0);
        gfx_dot(x, y, d->size * 1.3f, COLOR_ALPHA(col, (int)(255 * al)), 1);
        if (d->size > 1.0f)
            gfx_dot(x, y, d->size * 3.5f, COLOR_ALPHA(col, (int)(34 * al)), 1);
    }
    for (int i = DUST_SPECKS; i < DUST_SPECKS + DUST_DISCS; i++) { /* soft discs, out of focus */
        const Dust *d = &s_dust[i];
        float x = d->x + 3 * tsin(u + d->p1), y = d->y + 3 * tsin(u + d->p2);
        float tw = 0.5f + 0.5f * tsin(u + d->p3), al = d->bright * (0.3f + 0.7f * tw * (0.8f + 0.2f * tw)) * a;
        gfx_dot(x, y, d->size, COLOR_ALPHA(t->dust, (int)(70 * al)), 0);
        gfx_dot(x, y, d->size * 1.35f, COLOR_ALPHA(t->dust, (int)(26 * al)), 1);
    }
    u32 white = mix_rgb(t->dust, RGBA(255, 255, 255, 0), 0.65f, 0);
    for (int i = DUST_SPECKS + DUST_DISCS; i < DUST_SPECKS + DUST_DISCS + DUST_STARS; i++) { /* a few bright stars that flare */
        const Dust *d = &s_dust[i];
        float x = d->x + 2 * tsin(u + d->p1), y = d->y + 2 * tsin(u + d->p2);
        float tw = 0.5f + 0.5f * tsin(2 * u + d->p3);
        tw *= tw;
        gfx_dot(x, y, 1.1f, COLOR_ALPHA(white, (int)(255 * (0.25f + 0.75f * tw) * a)), 1);
        gfx_dot(x, y, 4.2f, COLOR_ALPHA(white, (int)(70 * tw * a)), 1);
    }
    gfx_blend_add(0);
}

/* ---- PS1: the four symbols of the controller in their classic colours, a cluster at the right below the row of cards of Home. Everything is on whole pixels
 * (the centre, the distances, the sizes, the width of the lines, and the slow bob is in whole pixels too), so that the lines and the corners are sharp. */
#define PS1_X 376
#define PS1_Y 150
static void deco_ps1(float time, float a)
{
    const int d = 24, r = 10, s = 8; /* distance from the centre, the radius of the circle and the triangle, half the side of the cross and the square */
    const float w = 2;
    int bob = (int)floorf(sinf(time * 0.8f) * 2 + 0.5f);
    u32 cols[4] = { RGBA(64, 226, 160, 0), RGBA(255, 102, 102, 0), RGBA(124, 178, 232, 0), RGBA(255, 120, 232, 0) };
    int pos[4][2] = { { PS1_X, PS1_Y - d }, { PS1_X + d, PS1_Y }, { PS1_X, PS1_Y + d }, { PS1_X - d, PS1_Y } };
    for (int n = 0; n < 4; n++) {
        int x = pos[n][0], y = pos[n][1] + bob;
        u32 c = COLOR_ALPHA(cols[n], (int)(150 * a));
        u32 cc[2] = { c, c };
        gfx_glow((float)(x - r), (float)(y - r), (float)(r * 2), (float)(r * 2), 8, COLOR_ALPHA(cols[n], (int)(28 * a)));
        if (n == 0) { /* △ */
            float tri[4][2] = { { (float)x, (float)(y - r) }, { (float)(x + r), (float)(y + 7) }, { (float)(x - r), (float)(y + 7) }, { (float)x, (float)(y - r) } };
            for (int e = 0; e < 3; e++) {
                float seg[4] = { tri[e][0], tri[e][1], tri[e + 1][0], tri[e + 1][1] };
                gfx_stroke(seg, cc, 2, w);
            }
        } else if (n == 1) { /* ○ */
            gfx_ring((float)(x - r), (float)(y - r), (float)(r * 2), (float)(r * 2), (float)r, c);
        } else if (n == 2) { /* ✕ */
            float l1[4] = { (float)(x - s), (float)(y - s), (float)(x + s), (float)(y + s) }, l2[4] = { (float)(x + s), (float)(y - s), (float)(x - s), (float)(y + s) };
            gfx_stroke(l1, cc, 2, w);
            gfx_stroke(l2, cc, 2, w);
        } else { /* □ */
            gfx_frame((float)(x - s), (float)(y - s), (float)(s * 2), (float)(s * 2), w, c);
        }
    }
}

/* ---- PS2: eight glowing dots go round a ring that tumbles in 3D about a fixed axis. Seen from the front the ring is an ellipse, and every 1.7 s or so it turns
 * edge-on: the dots gather into a line, then it opens again on the other side, which makes the dots look as if they turn round (as in the reference). They go
 * round 3 times in the loop, each a little faster and slower than the next; the moments it is edge-on are the ones measured in the reference. Nothing is drawn but the dots. */
typedef struct {
    float th0, wob, pw, pf, pr; /* turns: where on the ring it starts, how much it wanders, and phases of the wandering, the flicker and the radius */
    unsigned char hw, hf, hr;
} Ps2Dot;

static Ps2Dot s_ps2[8];
static int s_ps2_ready;

/* the tilt of the ring (radians): pi/2, which is edge-on, at 1.33, 3.07, 4.80 and 6.55 s; smooth; four half turns in the loop */
static float ps2_tilt(float t)
{
    static const float tk[5] = { 1.33f, 3.07f, 4.80f, 6.55f, 9.33f }, ak[5] = { 1.5707963f, 4.712389f, 7.853982f, 10.995574f, 14.137167f };
    float sl[5], tt = t < tk[0] ? t + LOOP_S : t;
    int k = 0;
    for (int n = 0; n < 4; n++) {
        int m = (n + 3) % 4;
        sl[n] = 1.5707963f * (1 / (tk[m + 1] - tk[m]) + 1 / (tk[n + 1] - tk[n]));
    }
    sl[4] = sl[0];
    while (k < 3 && tt >= tk[k + 1])
        k++;
    float h = tk[k + 1] - tk[k], u = (tt - tk[k]) / h, u2 = u * u, u3 = u2 * u;
    return (2 * u3 - 3 * u2 + 1) * ak[k] + (u3 - 2 * u2 + u) * h * sl[k] + (-2 * u3 + 3 * u2) * ak[k + 1] + (u3 - u2) * h * sl[k + 1];
}

static void deco_ps2(float time, float a)
{
    if (!s_ps2_ready) {
        unsigned seed = 0x51c7u;
        for (int i = 0; i < 8; i++) {
            Ps2Dot *d = &s_ps2[i];
            d->th0 = (float)i / 8 + 0.42f * (spot_rnd(&seed) - 0.5f) / TAU;
            d->wob = 0.28f * (0.5f + spot_rnd(&seed)) / TAU;
            d->pw = spot_rnd(&seed);
            d->pf = spot_rnd(&seed);
            d->pr = spot_rnd(&seed);
            d->hw = (unsigned char)(1 + (int)(spot_rnd(&seed) * 2.99f));
            d->hf = (unsigned char)(2 + (int)(spot_rnd(&seed) * 3.99f));
            d->hr = (unsigned char)(1 + (int)(spot_rnd(&seed) * 2.99f));
        }
        s_ps2_ready = 1;
    }
    float t = fmodf(time, LOOP_S), u = loop_pos(time, LOOP_S);
    if (t < 0)
        t += LOOP_S;
    float tilt = ps2_tilt(t), ca = cosf(tilt), sa = sinf(tilt);
    const float e1x = -0.92718f, e1y = 0.37461f, e2x = -0.37461f, e2y = -0.92718f; /* the long side of the ellipse on the screen: 158 degrees */
    float cx = 196 + 3 * tsin(u + 0.0955f), cy = 135 + 3 * tsin(2 * u + 0.3024f), R = 55 + 2.5f * tsin(2 * u + 0.0477f);
    gfx_blend_add(1);
    for (int i = 0; i < 8; i++) {
        const Ps2Dot *d = &s_ps2[i];
        float th = d->th0 + 3 * u + d->wob * tsin(d->hw * u + d->pw); /* round the ring 3 times in the loop */
        float ri = R * (1 + 0.07f * tsin(d->hr * u + d->pr));
        float sin_th = tsin(th), pu = ri * tsin(th + 0.25f), pv = ri * sin_th * ca, z = sin_th * sa; /* z: towards or away from the viewer */
        float x = cx + pu * e1x + pv * e2x, y = cy + pu * e1y + pv * e2y;
        float flick = 0.76f + 0.24f * tsin(d->hf * u + d->pf), s = 1.0f + 0.22f * z, br = 0.86f + 0.14f * z;
        gfx_dot(x, y, 26.0f * s, RGBA(60, 110, 255, (int)(105 * flick * br * a)), 1);
        gfx_dot(x, y, 13.0f * s, RGBA(150, 200, 255, (int)(255 * flick * br * a)), 1);
        gfx_dot(x, y, 6.5f * s, RGBA(255, 255, 255, (int)(255 * br * a)), 1);
    }
    gfx_blend_add(0);
}

/* ---- PS3: a silky ribbon that twists across the screen. Its upper edge, its lower edge and its bright strand are smooth curves (themedata.h); the silk between
 * them is soft strips (a hard edge would show its steps) with two fine threads, and the strand is sampled twice as finely so that its bends are smooth. */
#define PS3_N 51 /* points along the screen: every 9.6 px */

static void ps3_eval(float t, const float (*tab)[PS3F_MX], float *out)
{
    static float cs[PS3_N][PS3F_MX];
    static int ready;
    if (!ready) {
        for (int j = 0; j < PS3_N; j++)
            for (int m = 0; m < PS3F_MX; m++)
                cs[j][m] = tsin(0.25f + 0.5f * m * j / (PS3_N - 1)); /* cos(pi m x / W) */
        ready = 1;
    }
    float basis[PS3F_NT], g[PS3F_MX], u = t / PS3F_PERIOD;
    basis[0] = 1;
    for (int n = 1; 2 * n < PS3F_NT; n++) {
        basis[2 * n - 1] = tsin(n * u + 0.25f);
        basis[2 * n] = tsin(n * u);
    }
    for (int m = 0; m < PS3F_MX; m++) {
        g[m] = 0;
        for (int i = 0; i < PS3F_NT; i++)
            g[m] += basis[i] * tab[i][m];
    }
    for (int j = 0; j < PS3_N; j++) {
        float y = 0;
        for (int m = 0; m < PS3F_MX; m++)
            y += g[m] * cs[j][m];
        out[j] = y;
    }
}

static float soft_max(float v, float lo) /* max(v, lo) without a corner */
{
    return 0.5f * (v + lo + sqrtf((v - lo) * (v - lo) + 16.0f));
}

static void deco_ps3(float time, float a)
{
    const u32 rgb = RGBA(255, 236, 205, 0); /* warm cream */
    float t = fmodf(time, PS3F_PERIOD);
    if (t < 0)
        t += PS3F_PERIOD;
    float T[PS3_N], B[PS3_N], M[PS3_N], X[PS3_N], R[PS3_N];
    ps3_eval(t, ps3f_top, T);
    ps3_eval(t, ps3f_bot, B);
    ps3_eval(t, ps3f_main, M);
    for (int j = 0; j < PS3_N; j++) {
        X[j] = SCREEN_W * (float)j / (PS3_N - 1);
        R[j] = 0.55f + 0.45f * (1 - X[j] / SCREEN_W); /* a little brighter at the left, where the ribbon is thickest */
        float dd = soft_max(B[j] - T[j], 8.0f);
        B[j] = T[j] + dd;
        M[j] = T[j] + clampf(soft_max(M[j] - T[j], 3.0f), 3.0f, dd - 3.0f); /* the strand stays inside the ribbon */
    }
    gfx_blend_add(1);
    for (int j = 0; j < PS3_N - 2; j += 2) { /* the silk, every second point: feathered edges, rows of vertices with their own brightness; it reaches 1.5 px past the middle of the strand on both sides */
        float y0[6], y1[6], al0[6], al1[6];
        for (int e = 0; e < 2; e++) {
            int q = j + 2 * e;
            float *y = e ? y1 : y0, *al = e ? al1 : al0;
            y[0] = T[q] - 4; y[1] = T[q]; y[2] = fmaxf(T[q], M[q] - 1.5f); y[3] = fminf(B[q], M[q] + 1.5f); y[4] = B[q]; y[5] = B[q] + 4;
            al[0] = 0; al[1] = 56 * R[q]; al[2] = 18 * R[q]; al[3] = 18 * R[q]; al[4] = 44 * R[q]; al[5] = 0;
        }
        for (int r = 0; r < 5; r++) {
            if (r == 2)
                continue; /* the middle is the strand, below */
            float xy[8] = { X[j], y0[r], X[j + 2], y1[r], X[j], y0[r + 1], X[j + 2], y1[r + 1] };
            u32 c[4] = { COLOR_ALPHA(rgb, (int)(al0[r] * a)), COLOR_ALPHA(rgb, (int)(al1[r] * a)), COLOR_ALPHA(rgb, (int)(al0[r + 1] * a)), COLOR_ALPHA(rgb, (int)(al1[r + 1] * a)) };
            gfx_quad(xy, c);
        }
    }
    for (int j = 0; j < PS3_N - 1; j++) { /* the bright strand: a soft strip, at every point */
        for (int r = 0; r < 2; r++) {
            float s0 = r ? 0 : -3.5f, s1 = r ? 3.5f : 0, a0 = r ? 125 : 0, a1 = r ? 0 : 125; /* (the rows are 0, 125, 0: the silk is under the strand's edges, so that no pixel along them is left out or lit twice) */
            float k0 = 0.7f + 0.3f * R[j], k1 = 0.7f + 0.3f * R[j + 1];
            float xy[8] = { X[j], M[j] + s0, X[j + 1], M[j + 1] + s0, X[j], M[j] + s1, X[j + 1], M[j + 1] + s1 };
            u32 c[4] = { COLOR_ALPHA(rgb, (int)(a0 * (r ? k0 : R[j]) * a)), COLOR_ALPHA(rgb, (int)(a0 * (r ? k1 : R[j + 1]) * a)),
                         COLOR_ALPHA(rgb, (int)(a1 * (r ? R[j] : k0) * a)), COLOR_ALPHA(rgb, (int)(a1 * (r ? R[j + 1] : k1) * a)) };
            gfx_quad(xy, c);
        }
    }
    static const float frac[2] = { 0.34f, 0.72f }, bright[2] = { 64.0f, 56.0f };
    for (int s = 0; s < 2; s++) /* two fine threads inside the silk, as soft strips (a line would be stepped) */
        for (int j = 0; j < PS3_N - 2; j += 2) {
            float y0 = T[j] + (B[j] - T[j]) * frac[s], y1 = T[j + 2] + (B[j + 2] - T[j + 2]) * frac[s];
            float k0 = bright[s] * (0.6f + 0.4f * X[j] / SCREEN_W) * a, k1 = bright[s] * (0.6f + 0.4f * X[j + 2] / SCREEN_W) * a;
            for (int r = 0; r < 2; r++) {
                float o0 = r ? 0 : -1.8f, o1 = r ? 1.8f : 0;
                float xy[8] = { X[j], y0 + o0, X[j + 2], y1 + o0, X[j], y0 + o1, X[j + 2], y1 + o1 };
                u32 c[4] = { COLOR_ALPHA(rgb, (int)(r ? k0 : 0)), COLOR_ALPHA(rgb, (int)(r ? k1 : 0)), COLOR_ALPHA(rgb, (int)(r ? 0 : k0)), COLOR_ALPHA(rgb, (int)(r ? 0 : k1)) };
                gfx_quad(xy, c);
            }
        }
    gfx_blend_add(0);
}

/* ---- PS4: the edge of a big darker circle that rolls to the right. It is a bright rim with a soft band at its left, the whole way from the bottom of the screen
 * to its top right corner, drawn as strips along the curve (themedata.h); it fades in and out at the ends of the loop, as in the reference. */
static float cr4(float p0, float p1, float p2, float p3, float f) /* a smooth curve through the keys in time (Catmull-Rom), not a straight line between them */
{
    return 0.5f * ((2 * p1) + (-p0 + p2) * f + (2 * p0 - 5 * p1 + 4 * p2 - p3) * f * f + (-p0 + 3 * p1 - 3 * p2 + p3) * f * f * f);
}

#define PS4_KEY(tab, j, i) ((float)(tab)[i][j])
#define PS4_AT(tab, j, k0, k1, k2, k3, f) cr4(PS4_KEY(tab, j, k0), PS4_KEY(tab, j, k1), PS4_KEY(tab, j, k2), PS4_KEY(tab, j, k3), f)

static void deco_ps4(float time, float a)
{
    float t = fmodf(time, LOOP_S);
    if (t < 0)
        t += LOOP_S;
    float kf = clampf((t / LOOP_S * 120.0f - 1.5f) / 4.0f, 0, PS4N_K - 1.0f), f;
    int k1 = (int)kf;
    if (k1 > PS4N_K - 2)
        k1 = PS4N_K - 2;
    f = kf - k1;
    int k0 = k1 > 0 ? k1 - 1 : 0, k2 = k1 + 1, k3 = k1 + 2 < PS4N_K ? k1 + 2 : PS4N_K - 1;
    float env = smooth_step(0, 0.5f, t) * smooth_step(0, 0.9f, LOOP_S - t) * a;
    enum { N = PS4N_J + 2 };
    float X[N], Y[N], AE[N], BB[N], NX[N], NY[N];
    for (int j = 0; j < PS4N_J; j++) {
        X[j + 2] = PS4_AT(ps4n_x, j, k0, k1, k2, k3, f) / 10;
        Y[j + 2] = PS4_AT(ps4n_y, j, k0, k1, k2, k3, f) / 10;
        AE[j + 2] = fmaxf(0, PS4_AT(ps4n_ae, j, k0, k1, k2, k3, f)) / 2;
        BB[j + 2] = fmaxf(0, PS4_AT(ps4n_bb, j, k0, k1, k2, k3, f)) / 2;
    }
    for (int j = 1; j >= 0; j--) { /* two more points below the picture, in the direction of the lower end */
        X[j] = X[j + 1] + (X[2] - X[3]);
        Y[j] = Y[j + 1] + (Y[2] - Y[3]);
        AE[j] = AE[2];
        BB[j] = BB[2];
    }
    for (int j = 0; j < N; j++) {
        float dx = X[j < N - 1 ? j + 1 : j] - X[j > 0 ? j - 1 : j], dy = Y[j < N - 1 ? j + 1 : j] - Y[j > 0 ? j - 1 : j];
        float l = sqrtf(dx * dx + dy * dy);
        if (l < 0.001f)
            l = 1;
        NX[j] = -dy / l; /* the normal, towards the right of the wave */
        NY[j] = dx / l;
        if (NX[j] < 0) {
            NX[j] = -NX[j];
            NY[j] = -NY[j];
        }
    }
    static const float OFF[5] = { -56, -30, -4, 3, 24 }; /* across the wave: where the band starts, its middle, the rim, and where it ends */
    const u32 rim = RGBA(10, 132, 255, 0);
    gfx_blend_add(1);
    for (int j = 0; j < N - 1; j++) {
        float al[2][5];
        for (int e = 0; e < 2; e++) {
            al[e][0] = 0;
            al[e][1] = BB[j + e] * 0.9f;
            al[e][2] = AE[j + e] * 0.8f;
            al[e][3] = AE[j + e];
            al[e][4] = 0;
        }
        for (int c = 0; c < 4; c++) {
            float xy[8] = { X[j] + NX[j] * OFF[c], Y[j] + NY[j] * OFF[c], X[j] + NX[j] * OFF[c + 1], Y[j] + NY[j] * OFF[c + 1],
                            X[j + 1] + NX[j + 1] * OFF[c], Y[j + 1] + NY[j + 1] * OFF[c], X[j + 1] + NX[j + 1] * OFF[c + 1], Y[j + 1] + NY[j + 1] * OFF[c + 1] };
            u32 cl[4] = { COLOR_ALPHA(rim, (int)(al[0][c] * env)), COLOR_ALPHA(rim, (int)(al[0][c + 1] * env)),
                          COLOR_ALPHA(rim, (int)(al[1][c] * env)), COLOR_ALPHA(rim, (int)(al[1][c + 1] * env)) };
            gfx_quad(xy, cl);
        }
    }
    gfx_blend_add(0);
}

/* What is the same every frame: the surface, the glows, and the beam of light with its haze. */
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
        gfx_stroke(line, c, 3, 1.0f); /* (a soft line, like the rest of the picture: a hardware line has steps) */
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
    if (library)
        gfx_snapshot_draw_soft(); /* its lines are one pixel wide: this takes their steps out */
    else
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
    /* what moves is drawn on top every frame. The PS1 symbols sit at the right, below the row of cards of Home (the same on every screen) */
    a = clampf(a, 0, 1);
    switch (t->deco) {
    case DECO_SPOT: deco_dust(t, time, a); break;
    case DECO_PS1: deco_ps1(time, a); break;
    case DECO_PS2: deco_ps2(time, a); break;
    case DECO_PS3: deco_ps3(time, a); break;
    case DECO_PS4: deco_ps4(time, a); break;
    }
}

void theme_swatch(const Theme *t, float x, float y, float w, float h, float a)
{
    /* a small "screen" with the theme's tint and a white strip under it (the same in every theme: the theme is the background only) */
    gfx_rect(x, y, w, h, alpha(t->surf_top, a));
    float gy = floorf(h * 0.35f + 0.5f); /* (the gradient starts on a whole pixel) */
    gfx_gradient_v(x, y + gy, w, h - gy, alpha(COLOR_ALPHA(t->surf_bottom, 0), a), alpha(t->surf_bottom, a));
    gfx_rect(x + 3, y + h - 4, w - 6, 2, alpha(RGBA(255, 255, 255, 255), a));
    if (t->deco != DECO_SPOT) { /* klassiska konsoler: en liten våg som kännetecken */
        float l[6] = { x + 3, y + h * 0.55f, x + w * 0.5f, y + h * 0.3f, x + w - 3, y + h * 0.5f };
        u32 c[3] = { alpha(RGBA(255, 255, 255, 60), a), alpha(RGBA(255, 255, 255, 170), a), alpha(RGBA(255, 255, 255, 60), a) };
        gfx_stroke(l, c, 3, 1.1f);
    }
    gfx_frame(x, y, w, h, 1, alpha(RGBA(255, 255, 255, 70), a)); /* syns även på mörka teman */
}
