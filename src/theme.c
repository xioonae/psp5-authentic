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

/* The scene above never changes (the same theme gives the same picture every frame), so it is drawn once into a picture in
 * video memory and put on the screen from there: one opaque image instead of some 500 draw calls that touch every pixel
 * three or four times. It is drawn again when the theme (or `a`) is another, or when the picture has been lost. */
static struct {
    Theme theme;
    float a;
    unsigned serial; /* gfx_snapshot_serial() when it was drawn */
    int valid;
} s_scene;

void theme_backdrop_forget(void)
{
    s_scene.valid = 0;
}

void theme_backdrop(const Theme *t, float time, float a)
{
    if (!s_scene.valid || s_scene.serial != gfx_snapshot_serial() || s_scene.a != a || memcmp(&s_scene.theme, t, sizeof *t)) {
        gfx_snapshot_begin();
        draw_scene(t, a);
        gfx_snapshot_end();
        memcpy(&s_scene.theme, t, sizeof *t);
        s_scene.a = a;
        s_scene.serial = gfx_snapshot_serial();
        s_scene.valid = 1;
    }
    gfx_snapshot_draw();
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
