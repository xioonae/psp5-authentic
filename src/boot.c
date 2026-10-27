/* psp5 – made by Mille Weber */
/* Startskärm i samma stil och ordning som en PS5:s uppstart:
 *   1. en liten grå logga mitt på svart (PlayStation-loggan, ritad här av sin kontur)
 *   2. svart, sedan en liten grå hälsovarning
 *   3. stora, mörkt marinblå bokeh-cirklar som glider långsamt
 *   4. små ljusa bokeh-cirklar och gyllene damm som virvlar upp
 * och därefter tonar startskärmen ut till svart och launchern in. Allt ritas här, valfri knapp hoppar över.
 * The circles and the dust take the colours of the theme (the logo and the notice stay grey). */
#include <pspkernel.h>
#include <pspctrl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "boot.h"
#include "gfx.h"
#include "icons.h"
#include "theme.h"
#include "sound.h"
#include "lang.h"
#include "testrun.h"

#define PI 3.14159265f

/* tidslinje (sekunder) */
#define T_LOGO_IN    0.15f
#define T_LOGO_OUT   2.10f
#define T_WARN       2.55f /* varning + ljud börjar */
#define T_WARN_OUT   4.55f
#define T_BOKEH      2.75f
#define T_DUST       4.55f /* dammet virvlar upp */
#define T_CALM       5.50f /* the swirl calms down, the large circles go */
#define T_FADE       6.60f /* the intro fades to black and the menu comes in; the sound (5.2 s from T_WARN) is over by then */
#define T_END        7.15f

#define BIG_BOKEH   4
#define SMALL_BOKEH 9
#define DUST        110

/* the logo is a quarter of the screen high */
#define LOGO_H (SCREEN_H / 4)

typedef struct { float x, y, r, vx, vy, a; } Bokeh;
typedef struct { float x, y, size, warm, phase, speed, swirl; } Dust;

static Bokeh s_big[BIG_BOKEH], s_small[SMALL_BOKEH];
static Dust s_dust[DUST];

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float smooth(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
/* 0→1 mellan a och b, 1→0 mellan c och d */
static float window(float t, float a, float b, float c, float d)
{
    return smooth((t - a) / (b - a)) * (1 - smooth((t - c) / (d - c)));
}

static unsigned s_seed = 0x5eed5;
static float rnd(void)
{
    s_seed = s_seed * 1664525u + 1013904223u;
    return (s_seed >> 8) / 16777216.0f;
}

/* --- the logo --- */

/* Small, matt grey and without glow – like the console's own start logo. It is drawn from the outline of its three shapes into one small image the
 * first time (gfx_logo, icons.c) and is one sprite after that, so it costs next to nothing and is as smooth as the letters of the font. */
static void draw_logo(float alpha)
{
    int h = LOGO_H, w = icon_logo_width(h);
    gfx_logo((SCREEN_W - w) / 2, (SCREEN_H - h) / 2, h, RGBA(178, 178, 180, (int)(255 * alpha)));
}

/* --- hälsovarningen --- */

/* The warning is one paragraph that is wrapped to the screen here (the line breaks of the texts are only
 * for the source), as big as fits in a few lines: it is only on screen for a couple of seconds, so it has
 * to be easy to read. The layout is made once per language, as measuring text is not free. */
#define WARN_W 400.0f        /* widest line */
#define WARN_SIZE 0.5f       /* text size to start from */
#define WARN_MIN_SIZE 0.36f
#define WARN_LINES 7         /* lines it may take at the bigger sizes */
#define WARN_MAX_LINES 14    /* at the smallest size it may take more, so that no word is ever lost */

static struct {
    const char *src; /* the text this layout is for */
    char buf[400];
    char *line[WARN_MAX_LINES];
    int n;
    float size, x;
} s_warn;

/* Breaks s_warn.buf at spaces into lines no wider than WARN_W at `size`; 0 if that takes more than
 * `max_lines` lines. */
static int warn_wrap(float size, int max_lines)
{
    char *p = s_warn.buf;
    int n = 0;
    while (*p) {
        if (n == max_lines) {
            s_warn.n = n;
            return 0;
        }
        s_warn.line[n++] = p;
        char *fit = NULL, *q = p; /* the end of the last word that still fits */
        for (;;) {
            char *e = strchr(q, ' ');
            if (!e)
                e = q + strlen(q);
            char save = *e;
            *e = 0;
            float w = text_width(size, 0, p);
            *e = save;
            if (w > WARN_W && fit)
                break;
            fit = e;
            if (!*e)
                break;
            q = e + 1;
        }
        if (*fit) {
            *fit = 0;
            p = fit + 1;
            while (*p == ' ')
                p++;
        } else {
            p = fit;
        }
    }
    s_warn.n = n;
    return 1;
}

static void warn_layout(void)
{
    const char *src = L(S_HEALTH);
    if (s_warn.src == src)
        return;
    s_warn.src = src;
    for (float size = WARN_SIZE;; size -= 0.02f) {
        snprintf(s_warn.buf, sizeof(s_warn.buf), "%s", src);
        for (char *c = s_warn.buf; *c; c++)
            if (*c == '\n')
                *c = ' ';
        s_warn.size = size;
        int smallest = size - 0.02f < WARN_MIN_SIZE;
        if (warn_wrap(size, smallest ? WARN_MAX_LINES : WARN_LINES) || smallest)
            break;
    }
    /* the block of left-aligned lines sits in the middle of the screen */
    float widest = 0;
    for (int i = 0; i < s_warn.n; i++)
        widest = fmaxf(widest, text_width(s_warn.size, 0, s_warn.line[i]));
    s_warn.x = floorf((SCREEN_W - widest) / 2);
}

static void draw_warning(float alpha)
{
    if (alpha <= 0.01f)
        return;
    warn_layout();
    float size = s_warn.size, step = size * 28; /* the distance between lines */
    /* the middle of the block is a little above the middle of the screen; 17.5 px is the cap height at size 1 */
    float y = SCREEN_H * 0.48f - ((s_warn.n - 1) * step - 17.5f * size) / 2;
    for (int i = 0; i < s_warn.n; i++)
        text_draw(s_warn.x, y + i * step, size, 0, RGBA(150, 152, 158, (int)(220 * alpha)), ALIGN_LEFT, s_warn.line[i]);
}

/* --- bokeh och damm --- */

static void init_scene(void)
{
    s_seed = 0x5eed5;
    for (int i = 0; i < BIG_BOKEH; i++) {
        Bokeh *b = &s_big[i];
        b->x = SCREEN_W * (0.25f + 0.5f * rnd());
        b->y = SCREEN_H * (0.3f + 0.4f * rnd());
        b->r = 150 + rnd() * 70;
        b->vx = (rnd() - 0.5f) * 22;
        b->vy = (rnd() - 0.5f) * 10;
        b->a = 0.5f + rnd() * 0.5f;
    }
    for (int i = 0; i < SMALL_BOKEH; i++) {
        Bokeh *b = &s_small[i];
        b->x = SCREEN_W * (0.3f + 0.6f * rnd());
        b->y = SCREEN_H * (0.15f + 0.7f * rnd());
        b->r = 9 + rnd() * 16;
        b->vx = (rnd() - 0.6f) * 26;
        b->vy = (rnd() - 0.5f) * 14;
        b->a = 0.4f + rnd() * 0.6f;
    }
    for (int i = 0; i < DUST; i++) {
        Dust *d = &s_dust[i];
        d->x = SCREEN_W * (0.1f + 0.85f * rnd());
        d->y = SCREEN_H * (0.05f + 0.9f * rnd());
        d->size = 0.45f + rnd() * rnd() * 1.6f;
        d->warm = rnd();
        d->phase = rnd() * 2 * PI;
        d->speed = 0.4f + rnd();
        d->swirl = 20 + rnd() * 50;
    }
}

/* The colours of the intro come from the theme, so a red console boots in red: the large dark circles from its intro colour
 * (the dark tone of the surface, without the grey lift that the colours of the dark themes have), the small light ones from its haze, the
 * dust from its dust and light colours (the PS1-PS4 themes have no dust colour: they use their accent colour). */
typedef struct { int big[3], haze[3], dust[3], pale[3]; } Palette;

static void make_palette(Palette *p)
{
    const Theme *th = theme_cur();
    float top[3], lift = 255, k = 1.7f, peak = 0;
    for (int c = 0; c < 3; c++) {
        top[c] = (th->intro >> (8 * c)) & 0xFF;
        lift = fminf(lift, top[c]);
    }
    for (int c = 0; c < 3; c++) {
        top[c] -= 0.6f * lift;
        peak = fmaxf(peak, top[c] * k);
    }
    if (peak > 160)
        k *= 160 / peak; /* a light theme (silver, pearl) gets grey circles, not bright ones */
    for (int c = 0; c < 3; c++) {
        int accent = (th->accent >> (8 * c)) & 0xFF;
        p->big[c] = (int)(top[c] * k);
        p->haze[c] = (int)(((th->haze >> (8 * c)) & 0xFF) * 0.8f);
        p->dust[c] = th->dust ? (th->dust >> (8 * c)) & 0xFF : accent;
        p->pale[c] = th->beam ? (th->beam >> (8 * c)) & 0xFF : accent + (255 - accent) * 6 / 10;
    }
}

static void draw_bokeh(float t)
{
    Palette pal;
    make_palette(&pal);
    /* stora, mjuka och mycket mörka – ljuset bygger upp sig i lager */
    float big = window(t, T_BOKEH, T_BOKEH + 1.3f, T_CALM - 0.6f, T_CALM + 0.5f);
    if (big > 0.01f) {
        float dt = t - T_BOKEH;
        for (int i = 0; i < BIG_BOKEH; i++) {
            Bokeh *b = &s_big[i];
            float x = b->x + b->vx * dt, y = b->y + b->vy * dt;
            gfx_dot(x, y, b->r * 1.12f, RGBA(pal.big[0], pal.big[1], pal.big[2], (int)(70 * big * b->a)), 1);
            gfx_dot(x, y, b->r, RGBA(pal.big[0] * 9 / 10, pal.big[1] * 9 / 10, pal.big[2] * 9 / 10, (int)(120 * big * b->a)), 0);
        }
    }
    /* små ljusa cirklar strax innan dammet */
    float small = window(t, T_DUST - 0.7f, T_DUST - 0.1f, T_DUST + 0.5f, T_DUST + 1.3f);
    if (small > 0.01f) {
        float dt = t - (T_DUST - 0.7f);
        gfx_blend_add(1);
        for (int i = 0; i < SMALL_BOKEH; i++) {
            Bokeh *b = &s_small[i];
            float x = b->x + b->vx * dt, y = b->y + b->vy * dt;
            gfx_dot(x, y, b->r, RGBA(pal.haze[0], pal.haze[1], pal.haze[2], (int)(70 * small * b->a)), 0);
            gfx_dot(x, y, b->r * 1.3f, RGBA(pal.haze[0], pal.haze[1], pal.haze[2], (int)(25 * small * b->a)), 1);
        }
        gfx_blend_add(0);
    }
}

static void draw_dust(float t)
{
    float vis = smooth((t - T_DUST + 0.4f) / 0.6f);
    if (vis <= 0.01f)
        return;
    float dt = t - (T_DUST - 0.4f);
    /* först ett virvlande moln, sedan lugnt svävande i ljuset */
    float burst = window(t, T_DUST - 0.4f, T_DUST + 0.2f, T_DUST + 0.6f, T_CALM + 0.6f);
    float cx = SCREEN_W * 0.62f, cy = SCREEN_H * 0.45f;
    Palette pal;
    make_palette(&pal);
    gfx_blend_add(1);
    for (int i = 0; i < DUST; i++) {
        Dust *d = &s_dust[i];
        float x = d->x + sinf(d->phase + dt * d->speed * 0.6f) * 8 + dt * 4;
        float y = d->y + cosf(d->phase * 1.3f + dt * d->speed * 0.5f) * 6 + dt * 2.5f;
        /* virveln drar partiklarna i en båge kring molnets mitt */
        float a = d->phase + dt * d->speed * 1.4f;
        x = x + (cx + cosf(a) * d->swirl * 2.2f - x) * burst * 0.55f;
        y = y + (cy + sinf(a) * d->swirl - y) * burst * 0.55f;
        float twinkle = 0.65f + 0.35f * sinf(d->phase * 3 + t * (2 + d->speed * 3));
        int col[3]; /* the dust colour, paler for the specks that are not "warm", all of it a little towards white */
        for (int c = 0; c < 3; c++) {
            float v = pal.dust[c] + (pal.pale[c] - pal.dust[c]) * (1 - d->warm);
            col[c] = (int)(v + (255 - v) * 0.12f);
        }
        int r = col[0], g = col[1], b = col[2];
        float al = vis * twinkle * (0.55f + 0.45f * burst);
        gfx_dot(x, y, d->size, RGBA(r, g, b, (int)(255 * clampf(al, 0, 1))), 0);
        if (d->size > 1.1f)
            gfx_dot(x, y, d->size * 4, RGBA(r, g, b, (int)(50 * clampf(al, 0, 1))), 1);
    }
    gfx_blend_add(0);
}

static void draw_frame(float t, float out)
{
    gfx_begin(RGBA(0, 0, 0, 255));
    if (t < T_WARN) {
        draw_logo(window(t, T_LOGO_IN, T_LOGO_IN + 0.35f, T_LOGO_OUT, T_LOGO_OUT + 0.3f) * out);
    } else {
        draw_bokeh(t);
        draw_dust(t);
        draw_warning(window(t, T_WARN, T_WARN + 0.25f, T_WARN_OUT - 0.4f, T_WARN_OUT));
        if (out < 1)
            gfx_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(255 * (1 - out))));
    }
    gfx_end();
}

void boot_run(volatile int *running)
{
    init_scene();

    u64 start = sceKernelGetSystemTimeWide();
#ifdef PSP5_TEST
    int frame = 0; /* the test build counts frames (60 a second), so that a script's `wait` and `shot` land on the same moment of the intro every time */
#endif
    int sound_started = 0;
    float skip_at = -1; /* tid då en knapp trycktes */
    u32 old = ~0u;      /* knappar som hålls vid start räknas inte */
    while (*running) {
        float t = (sceKernelGetSystemTimeWide() - start) / 1000000.0f;
#ifdef PSP5_TEST
        t = frame++ / 60.0f;
#endif
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad, 1);
        u32 b = testrun_buttons(pad.Buttons);
        if ((b & ~old) && skip_at < 0 && t > 0.3f) {
            skip_at = t;
            if (sound_started)
                sound_fade_out();
        }
        old = b;
        /* ljudet börjar med varningen (loggan visas i tystnad, som på konsolen);
         * ljuden räknas fram medan loggan visas */
        if (!sound_started && t >= T_WARN && skip_at < 0 && sound_ready()) {
            sound_play(SND_BOOT);
            sound_started = 1;
        }

        float out = 1 - smooth((t - T_FADE) / (T_END - T_FADE));
        if (skip_at >= 0)
            out *= 1 - smooth((t - skip_at) / 0.3f);
        if (t >= T_END || (skip_at >= 0 && t - skip_at >= 0.3f))
            break;
        draw_frame(t, out);
    }
    gfx_begin(RGBA(0, 0, 0, 255));
    gfx_end();
    sound_wait(400);
#ifndef PSP5_TEST /* testbygget sparar ljudet som WAV efteråt (sound_dump) */
    sound_release(SND_BOOT); /* ~450 KB tillbaka (viktigt på PSP-1000) */
#endif
}
