/* psp5 – made by Mille Weber */
/* Liten 2D-renderare direkt mot GU: rektanglar, gradienter, texturer,
 * rundade former via 9-slice och text via intraFont. */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <intraFont.h>
#include <malloc.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "gfx.h"
#include "diacritic.h"
#include "icons.h"
#include "testrun.h"

#define BUF_W 512
#define FB_SIZE (BUF_W * SCREEN_H * 4)
#define SHAPE_SIZE 64

typedef struct { float u, v; u32 color; float x, y, z; } TVert;
typedef struct { u32 color; float x, y, z; } CVert;

static u32 __attribute__((aligned(64))) s_list[0x20000 / 4];
static Texture *s_disc; /* skarp cirkel (rundade hörn) */
static Texture *s_soft; /* mjuk radiell gradient (skugga/glöd) */
static Texture *s_ring; /* tunn cirkelring (markeringsram) */
static intraFont *s_font, *s_bold, *s_jpn;
static void *s_draw; /* bufferten som ritas just nu (relativ VRAM) */
/* Senast bundna textur: många former i rad (rundade rutor, prickar) använder samma, och då
 * behöver GU inte ställas om och texturcachen tömmas igen. intraFont binder sina egna. */
static const Texture *s_bound;
static unsigned s_frame; /* räknas upp i gfx_begin */
static unsigned s_snap_serial; /* räknas upp i gfx_init: bilden i gfx_snapshot_* är då borta */
static int s_filter = -1; /* texturfiltret som GU står på (-1 = okänt) */
static void widths_forget(void); /* textbredder som kommer ihåg – töms när typsnitten byts */

/* --- texturer --- */

static int pow2(int v)
{
    int p = 1;
    while (p < v)
        p <<= 1;
    return p;
}

Texture *tex_create(int w, int h, int psm)
{
    Texture *t = calloc(1, sizeof(Texture));
    if (!t)
        return NULL;
    int bpp = psm == GU_PSM_8888 ? 4 : 2;
    t->w = w;
    t->h = h;
    t->tw = pow2(w);
    t->th = pow2(h);
    t->psm = psm;
    t->stride = (w + 7) & ~7; /* GU kräver 16-bytesjusterade rader */
    /* en extra rad så att bilinjär filtrering vid nederkanten läser giltigt minne */
    t->data = memalign(16, t->stride * (h + 1) * bpp);
    if (!t->data) {
        free(t);
        return NULL;
    }
    memset(t->data, 0, t->stride * (h + 1) * bpp);
    return t;
}

void tex_free(Texture *t)
{
    if (!t)
        return;
    if (t == s_bound)
        s_bound = NULL; /* en ny textur kan få samma adress */
    free(t->data);
    free(t);
}

void tex_flush(Texture *t)
{
    int bpp = t->psm == GU_PSM_8888 ? 4 : 2;
    sceKernelDcacheWritebackRange(t->data, t->stride * (t->h + 1) * bpp);
}

enum { SHAPE_DISC, SHAPE_SOFT, SHAPE_RING };

static Texture *make_shape(int kind)
{
    Texture *t = tex_create(SHAPE_SIZE, SHAPE_SIZE, GU_PSM_8888);
    if (!t)
        return NULL;
    u32 *px = t->data;
    float c = SHAPE_SIZE / 2.0f;
    for (int y = 0; y < SHAPE_SIZE; y++) {
        for (int x = 0; x < SHAPE_SIZE; x++) {
            float dx = x + 0.5f - c, dy = y + 0.5f - c;
            float d = sqrtf(dx * dx + dy * dy);
            float a;
            if (kind == SHAPE_SOFT) {
                float n = d / c;
                a = n >= 1.0f ? 0.0f : expf(-n * n * 4.0f) * (1.0f - n);
            } else if (kind == SHAPE_RING) {
                /* ring mellan radie c-6.5 och c-0.5, kantutjämnad på båda sidor */
                float outer = c - 0.5f - d, inner = d - (c - 6.5f);
                a = fminf(outer, inner);
                a = a < 0 ? 0 : a > 1 ? 1 : a;
            } else {
                a = c - 0.5f - d;
                a = a < 0 ? 0 : a > 1 ? 1 : a;
            }
            px[y * t->stride + x] = RGBA(255, 255, 255, (int)(a * 255.0f + 0.5f));
        }
    }
    tex_flush(t);
    return t;
}

/* --- grundläggande GU --- */

void gfx_init(void)
{
    s_snap_serial++;
    sceGuInit();
    sceGuStart(GU_DIRECT, s_list);
    s_draw = (void *)0;
    sceGuDrawBuffer(GU_PSM_8888, s_draw, BUF_W);
    sceGuDispBuffer(SCREEN_W, SCREEN_H, (void *)FB_SIZE, BUF_W);
    sceGuOffset(2048 - SCREEN_W / 2, 2048 - SCREEN_H / 2);
    sceGuViewport(2048, 2048, SCREEN_W, SCREEN_H);
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuShadeModel(GU_SMOOTH);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);

    s_disc = make_shape(SHAPE_DISC);
    s_soft = make_shape(SHAPE_SOFT);
    s_ring = make_shape(SHAPE_RING);
}

static void icons_free(int only_unused);

static void library_free(void);

void gfx_term(void)
{
    icons_free(0);
    library_free();
    tex_free(s_disc);
    tex_free(s_soft);
    tex_free(s_ring);
    s_disc = s_soft = s_ring = NULL;
    sceGuDisplay(GU_FALSE);
    sceGuTerm();
}

void gfx_begin(u32 clear_color)
{
    s_frame++;
    icons_free(1); /* the last frame is finished, so images it did not use can go */
    sceGuStart(GU_DIRECT, s_list);
    s_bound = NULL;
    s_filter = -1;
    sceGuClearColor(clear_color);
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

void gfx_end(void)
{
    gfx_end_with(NULL);
}

void gfx_end_with(void (*after_draw)(void))
{
    sceGuFinish();
    sceGuSync(0, 0);
    if (after_draw)
        after_draw();
    testrun_frame((const void *)(0x44000000 + (u32)s_draw)); /* bara i testbyggen */
    sceDisplayWaitVblankStart();
    s_draw = sceGuSwapBuffers();
}

/* filter: GU_LINEAR (scaled images) or GU_NEAREST (images drawn pixel for pixel) */
static void bind_filter(const Texture *t, int filter)
{
    sceGuEnable(GU_TEXTURE_2D);
    if (t != s_bound) {
        s_bound = t;
        sceGuTexMode(t->psm, 0, 0, 0);
        sceGuTexImage(0, t->tw, t->th, t->stride, t->data);
        sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
        sceGuTexWrap(GU_CLAMP, GU_CLAMP);
        sceGuTexFlush();
    }
    if (filter != s_filter) { /* intraFont sets its own, so s_filter is forgotten whenever it has drawn */
        s_filter = filter;
        sceGuTexFilter(filter, filter);
    }
}

static void bind(const Texture *t)
{
    bind_filter(t, GU_LINEAR);
}

#define TVERT_FMT (GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)
#define CVERT_FMT (GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D)

void gfx_rect(float x, float y, float w, float h, u32 color)
{
    gfx_gradient_v(x, y, w, h, color, color);
}

void gfx_gradient_v(float x, float y, float w, float h, u32 top, u32 bottom)
{
    CVert *v = sceGuGetMemory(4 * sizeof(CVert));
    v[0] = (CVert){ top, x, y, 0 };
    v[1] = (CVert){ top, x + w, y, 0 };
    v[2] = (CVert){ bottom, x, y + h, 0 };
    v[3] = (CVert){ bottom, x + w, y + h, 0 };
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_TRIANGLE_STRIP, CVERT_FMT, 4, 0, v);
}

/* A fully opaque fill (both colours must have alpha 255): drawn without blending, which costs the GPU
 * about half as much per pixel – worth it for the full-screen backdrop. */
void gfx_gradient_v_opaque(float x, float y, float w, float h, u32 top, u32 bottom)
{
    sceGuDisable(GU_BLEND);
    gfx_gradient_v(x, y, w, h, top, bottom);
    sceGuEnable(GU_BLEND);
}

void gfx_gradient_h(float x, float y, float w, float h, u32 left, u32 right)
{
    CVert *v = sceGuGetMemory(4 * sizeof(CVert));
    v[0] = (CVert){ left, x, y, 0 };
    v[1] = (CVert){ right, x + w, y, 0 };
    v[2] = (CVert){ left, x, y + h, 0 };
    v[3] = (CVert){ right, x + w, y + h, 0 };
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_TRIANGLE_STRIP, CVERT_FMT, 4, 0, v);
}

void gfx_line(float x0, float y0, float x1, float y1, u32 color)
{
    CVert *v = sceGuGetMemory(2 * sizeof(CVert));
    v[0] = (CVert){ color, x0, y0, 0 };
    v[1] = (CVert){ color, x1, y1, 0 };
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_LINES, CVERT_FMT, 2, 0, v);
}

void gfx_tex(const Texture *t, float x, float y, float w, float h, u32 tint)
{
    if (!t)
        return;
    /* A picture wider than 64 texels is drawn in vertical strips of about 64 texels, one sprite each, all in one call. The
     * GE reads a texture through a small cache, and a sprite as wide as the screen makes it fetch the same rows over and
     * over; narrow strips are much faster (the artwork behind the cards is the biggest picture drawn). The picture looks
     * the same: the strips follow one mapping, and where two meet the edge is moved to a whole pixel so that they join
     * without a gap or an overlap (the texture coordinate there is the one that goes with the moved edge, so the picture
     * does not jump either). */
    int n = t->w > 64 && w > 0 ? (t->w + 63) / 64 : 1;
    TVert *v = sceGuGetMemory(2 * n * sizeof(TVert));
    float scale = n > 1 ? w / (t->w - 1) : 0; /* screen pixels per texel */
    float xs = x, us = 0.5f;                  /* where the strip starts: on the screen, and in the texture */
    for (int k = 0; k < n; k++) {
        float xe = x + w, ue = t->w - 0.5f; /* a half texel in from the edge so that the filtering never reads outside the picture */
        if (k < n - 1) {
            xe = floorf(x + (64 * (k + 1) - 0.5f) * scale + 0.5f);
            xe = xe < xs ? xs : xe > x + w ? x + w : xe;
            ue = 0.5f + (xe - x) / scale;
        }
        v[k * 2] = (TVert){ us, 0.5f, tint, xs, y, 0 };
        v[k * 2 + 1] = (TVert){ ue, t->h - 0.5f, tint, xe, y + h, 0 };
        xs = xe;
        us = ue;
    }
    bind(t);
    /* en bild utan alfakanal (bakgrunder) med full täckning behöver ingen blandning */
    int opaque = t->psm == GU_PSM_5650 && (tint >> 24) == 255;
    if (opaque)
        sceGuDisable(GU_BLEND);
    sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2 * n, 0, v);
    if (opaque)
        sceGuEnable(GU_BLEND);
}

/* --- the snapshot: a picture of the screen in video memory --- */

/* It lies behind the two screen buffers (they take 2 x 557 056 bytes of the 2 MB) and is laid out like one: 512 pixels a
 * row, 8888. 0x04000000 is where the video memory starts for the GE when it reads a texture. */
#define SNAP_OFFSET (2 * FB_SIZE)
static Texture s_snap = { SCREEN_W, SCREEN_H, 512, 512, BUF_W, GU_PSM_8888, (void *)(0x04000000 + SNAP_OFFSET) };

unsigned gfx_snapshot_serial(void)
{
    return s_snap_serial;
}

void gfx_snapshot_begin(void)
{
    /* not sceGuDrawBuffer: that would also change the buffer gfx_end swaps */
    sceGuDrawBufferList(GU_PSM_8888, (void *)SNAP_OFFSET, BUF_W);
    sceGuClearColor(RGBA(0, 0, 0, 255));
    sceGuClear(GU_COLOR_BUFFER_BIT);
}

void gfx_snapshot_end(void)
{
    sceGuDrawBufferList(GU_PSM_8888, s_draw, BUF_W);
    /* the GE may still hold an older picture in its texture cache, and the picture must be bound again to get it flushed */
    s_bound = NULL;
    sceGuTexFlush();
}

void gfx_snapshot_draw(void)
{
    /* In strips of 64 pixels, one sprite each, all in one call: the GE reads a wide image slowly (see gfx_tex). One
     * texel on one screen pixel, so the picture comes out exactly as it was drawn. */
    const int n = (SCREEN_W + 63) / 64;
    TVert *v = sceGuGetMemory(2 * n * sizeof(TVert));
    const u32 white = RGBA(255, 255, 255, 255);
    for (int k = 0; k < n; k++) {
        float x0 = 64.0f * k, x1 = x0 + 64 > SCREEN_W ? SCREEN_W : x0 + 64;
        v[k * 2] = (TVert){ x0, 0, white, x0, 0, 0 };
        v[k * 2 + 1] = (TVert){ x1, SCREEN_H, white, x1, SCREEN_H, 0 };
    }
    bind_filter(&s_snap, GU_NEAREST);
    sceGuDisable(GU_BLEND); /* the picture covers the whole screen and has no see-through parts */
    sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2 * n, 0, v);
    sceGuEnable(GU_BLEND);
}

/* 9-slice av en form-textur: hörnen får radien r, mitten sträcks ut. */
static void nine_slice(const Texture *t, float x, float y, float w, float h, float r, u32 color)
{
    if (r * 2 > w) r = w / 2;
    if (r * 2 > h) r = h / 2;
    const float S = SHAPE_SIZE, H = SHAPE_SIZE / 2.0f;
    /* källa (u) och mål (x) för tre kolumner/rader: kant, mitt, kant */
    float su[4] = { 0, H - 0.5f, H + 0.5f, S };
    float dx[4] = { x, x + r, x + w - r, x + w };
    float dy[4] = { y, y + r, y + h - r, y + h };

    TVert *v = sceGuGetMemory(18 * sizeof(TVert));
    int n = 0;
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            v[n++] = (TVert){ su[col], su[row], color, dx[col], dy[row], 0 };
            v[n++] = (TVert){ su[col + 1], su[row + 1], color, dx[col + 1], dy[row + 1], 0 };
        }
    }
    bind(t);
    sceGuDrawArray(GU_SPRITES, TVERT_FMT, n, 0, v);
}

void gfx_glow(float x, float y, float w, float h, float spread, u32 color)
{
    /* den mjuka texturen faller av över halva radien – därför 2×spread */
    nine_slice(s_soft, x - spread, y - spread, w + spread * 2, h + spread * 2, spread * 2, color);
}

/* The frame is put on whole pixels (a pixel is covered when its middle is, which is where the hardware puts the edge of a
 * rectangle): all four sides are the same whole number of pixels thick, `t` rounded and at least 1, wherever the frame is.
 * On fractions of a pixel one side would come out 1 px thick and the next 2 px. */
void gfx_frame(float x, float y, float w, float h, float t, u32 color)
{
    float x0 = ceilf(x - 0.5f), y0 = ceilf(y - 0.5f), x1 = ceilf(x + w - 0.5f), y1 = ceilf(y + h - 0.5f);
    float th = fmaxf(1, floorf(t + 0.5f));
    if (x1 - x0 <= th * 2 || y1 - y0 <= th * 2) {
        gfx_rect(x0, y0, x1 - x0, y1 - y0, color); /* too small for an inside */
        return;
    }
    gfx_rect(x0, y0, x1 - x0, th, color);
    gfx_rect(x0, y1 - th, x1 - x0, th, color);
    gfx_rect(x0, y0 + th, th, y1 - y0 - th * 2, color);
    gfx_rect(x1 - th, y0 + th, th, y1 - y0 - th * 2, color);
}

void gfx_fan(float cx, float cy, const float *xy, int n, u32 color)
{
    CVert *v = sceGuGetMemory((n + 2) * sizeof(CVert));
    v[0] = (CVert){ color, cx, cy, 0 };
    for (int i = 0; i <= n; i++)
        v[i + 1] = (CVert){ color, xy[(i % n) * 2], xy[(i % n) * 2 + 1], 0 };
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_TRIANGLE_FAN, CVERT_FMT, n + 2, 0, v);
}

/* --- smooth icons ---
 * The button symbols, the cog and the small key caps are small, and a big round texture shrunk to
 * their size (what gfx_circle does) only samples a few of its pixels, so their edges come out
 * jagged and the shapes lose their form. icons.c instead computes the exact coverage of every
 * pixel of the shape; the result is kept as a small white alpha image and drawn pixel for pixel,
 * tinted with the colour, which is as smooth as the glyphs of the font. The images are made
 * the first time a size is needed and reused after that. If the cache is ever full, or something
 * is too big, the old drawing is used instead. */

#define ICON_CACHE 96
#define ICON_MAX 96 /* biggest image, in pixels */

typedef struct {
    Texture *tex;
    int kind, w4, h4, r4, phase_x, phase_y; /* what it shows, in quarter pixels */
    unsigned last;                          /* the frame it was last used in */
} IconEnt;

static IconEnt s_icons[ICON_CACHE];

static void icons_free(int only_unused)
{
    int used = 0;
    for (int i = 0; i < ICON_CACHE; i++)
        used += s_icons[i].tex != NULL;
    if (only_unused && used < ICON_CACHE * 3 / 4)
        return; /* plenty of room */
    for (int i = 0; i < ICON_CACHE; i++) {
        if (s_icons[i].tex && (!only_unused || s_icons[i].last + 1 < s_frame)) {
            tex_free(s_icons[i].tex);
            s_icons[i].tex = NULL;
        }
    }
}

static float quarter(float v)
{
    return floorf(v * 4 + 0.5f) / 4;
}

/* The radius of a rounded rectangle: to a quarter pixel when small, to half a pixel from 4 px up (the
 * difference cannot be seen, and an animated radius then needs half as many images). */
static float round_radius(float v)
{
    return v < 4 ? quarter(v) : floorf(v * 2 + 0.5f) / 2;
}

/* The cached image with these properties (sizes in quarter pixels), made the first time. `tw`, `th` is
 * its size and (cx, cy) where the centre of the shape is in it. NULL if there is no room. */
static const Texture *cache_get(int kind, int w4, int h4, int r4, int phase_x, int phase_y, int tw, int th, float cx,
                                float cy)
{
    static unsigned char cov[ICON_MAX * ICON_MAX];
    IconEnt *slot = NULL;
    for (int i = 0; i < ICON_CACHE; i++) {
        IconEnt *e = &s_icons[i];
        if (!e->tex) {
            if (!slot)
                slot = e;
        } else if (e->kind == kind && e->w4 == w4 && e->h4 == h4 && e->r4 == r4 && e->phase_x == phase_x &&
                   e->phase_y == phase_y) {
            e->last = s_frame;
            return e->tex;
        }
    }
    if (!slot || tw > ICON_MAX || th > ICON_MAX)
        return NULL;
    Texture *t = tex_create(tw, th, GU_PSM_8888);
    if (!t)
        return NULL;
    icon_render(kind, w4 / 4.0f, h4 / 4.0f, r4 / 4.0f, cx, cy, tw, th, cov);
    u32 *px = t->data;
    for (int y = 0; y < th; y++)
        for (int x = 0; x < tw; x++)
            px[y * t->stride + x] = RGBA(255, 255, 255, cov[y * tw + x]);
    tex_flush(t);
    *slot = (IconEnt){ t, kind, w4, h4, r4, phase_x, phase_y, s_frame };
    return t;
}

/* The image of `kind` for the box b (made with icon_box from the same w, h, r, which must already
 * be multiples of 0.25), or NULL. */
static const Texture *icon_get(int kind, float w, float h, float r, const IconBox *b)
{
    return cache_get(kind, (int)(w * 4), (int)(h * 4), (int)(r * 4), b->phase_x, b->phase_y, b->tw, b->th, b->cx,
                     b->cy);
}

/* One image pixel on one screen pixel – no filtering, so nothing gets blurred. */
static void icon_blit(const Texture *t, const IconBox *b, u32 color)
{
    TVert *v = sceGuGetMemory(2 * sizeof(TVert));
    v[0] = (TVert){ 0, 0, color, (float)b->x, (float)b->y, 0 };
    v[1] = (TVert){ (float)t->w, (float)t->h, color, (float)(b->x + t->w), (float)(b->y + t->h), 0 };
    bind_filter(t, GU_NEAREST);
    sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, v);
}

/* A rounded rectangle (or just its ring) on whole pixels, drawn in up to 9 pieces of one small image
 * (see icon_nine). The edges are where the old 9-slice put them - a pixel belongs to the shape when
 * its centre does - and the corners are exact. Returns 0 if it has to be drawn the old way. */
static int shape_round(int kind, float x, float y, float w, float h, float r, u32 color)
{
    int l = (int)ceilf(x - 0.5f), t = (int)ceilf(y - 0.5f);
    int pw = (int)ceilf(x + w - 0.5f) - l, ph = (int)ceilf(y + h - 0.5f) - t;
    if (pw <= 0 || ph <= 0)
        return 1; /* nothing to draw */
    float rq = round_radius(fmaxf(0, fminf(r, fminf(pw, ph) / 2.0f)));
    if (rq > ICON_NINE_MAX_R)
        return 0;
    int k = icon_nine_k(rq), d = 2 * k + 1;
    const Texture *tex = cache_get(kind, (2 * k - 1) * 4, (2 * k - 1) * 4, (int)(rq * 4), 0,
                                   0, d, d, k + 0.5f, k + 0.5f);
    if (!tex)
        return 0;
    IconCell c[9];
    int n = icon_nine(pw, ph, k, c);
    TVert *v = sceGuGetMemory(n * 2 * sizeof(TVert));
    for (int i = 0; i < n; i++) {
        v[i * 2] = (TVert){ c[i].u0, c[i].v0, color, (float)(l - 1) + c[i].x0, (float)(t - 1) + c[i].y0, 0 };
        v[i * 2 + 1] = (TVert){ c[i].u1, c[i].v1, color, (float)(l - 1) + c[i].x1, (float)(t - 1) + c[i].y1, 0 };
    }
    bind_filter(tex, GU_NEAREST); /* every piece is drawn 1:1 or is one single colour */
    sceGuDrawArray(GU_SPRITES, TVERT_FMT, n * 2, 0, v);
    return 1;
}

void gfx_round_rect(float x, float y, float w, float h, float r, u32 color)
{
    if (!shape_round(ICON_RRECT, x, y, w, h, r, color))
        nine_slice(s_disc, x, y, w, h, r, color);
}

void gfx_ring(float x, float y, float w, float h, float r, u32 color)
{
    if (!shape_round(ICON_RRING, x, y, w, h, r, color))
        nine_slice(s_ring, x, y, w, h, r, color);
}

/* A ring of a fixed thickness just inside the rectangle - for the frame around the selected card: ICON_FRAME_T
 * (2.5 px) for the big card of the row, ICON_FRAME_T_S (2 px) when `thin` is set, for the library grid. */
void gfx_edge_ring(float x, float y, float w, float h, float r, int thin, u32 color)
{
    if (w < 12 || h < 12 || !shape_round(thin ? ICON_RFRAME_S : ICON_RFRAME, x, y, w, h, r, color)) /* (too small for a fixed thickness) */
        nine_slice(s_ring, x, y, w, h, r, color);
}

/* A circle: one image, with the exact edge for where the centre is (to half a pixel) and the radius (to a
 * quarter of a pixel). */
void gfx_circle(float cx, float cy, float r, u32 color)
{
    float rq = quarter(r);
    if (rq > 0 && rq <= ICON_NINE_MAX_R) {
        IconBox b;
        icon_box(ICON_DISC, cx, cy, 0, 0, rq, &b);
        const Texture *tex = icon_get(ICON_DISC, 0, 0, rq, &b);
        if (tex) {
            icon_blit(tex, &b, color);
            return;
        }
    }
    nine_slice(s_disc, cx - r, cy - r, r * 2, r * 2, r, color);
}

void gfx_gear(float cx, float cy, float r, u32 color, u32 hole)
{
    IconBox b;
    float rq = quarter(r);
    icon_box(ICON_GEAR, cx, cy, 0, 0, rq, &b);
    const Texture *tex = icon_get(ICON_GEAR, 0, 0, rq, &b);
    if (tex) { /* the hole is really a hole, so `hole` is not needed */
        icon_blit(tex, &b, color);
        return;
    }
    enum { TEETH = 8, PTS = TEETH * 4 };
    float xy[PTS * 2];
    for (int i = 0; i < PTS; i++) {
        /* varje kugge: två punkter ute, två inne */
        float a = (i + 0.5f) * 2.0f * 3.14159265f / PTS;
        float rr = (i % 4 == 1 || i % 4 == 2) ? r : r * 0.74f;
        xy[i * 2] = cx + cosf(a) * rr;
        xy[i * 2 + 1] = cy + sinf(a) * rr;
    }
    gfx_fan(cx, cy, xy, PTS, color);
    gfx_circle(cx, cy, r * 0.34f, hole);
}

void gfx_bolt(float cx, float cy, float r, u32 color)
{
    IconBox b;
    float rq = quarter(r);
    icon_box(ICON_BOLT, cx, cy, 0, 0, rq, &b);
    const Texture *tex = icon_get(ICON_BOLT, 0, 0, rq, &b);
    if (tex) {
        icon_blit(tex, &b, color);
        return;
    }
    const float s = r / 5.2f; /* the same corners as icons.c, drawn the old way */
    const float xy[12] = { cx + 1.6f * s, cy - 5.2f * s, cx - 2.6f * s, cy + 0.9f * s, cx - 0.2f * s, cy + 0.9f * s,
                           cx - 1.6f * s, cy + 5.2f * s, cx + 2.6f * s, cy - 0.9f * s, cx + 0.2f * s, cy - 0.9f * s };
    gfx_fan(cx, cy, xy, 6, color);
}

/* The Wi-Fi sign in a box of side 2 r centred on (cx, cy): one image like the cog wheel (see sd_wifi in icons.c). */
void gfx_wifi(float cx, float cy, float r, u32 color)
{
    IconBox b;
    float rq = quarter(r);
    icon_box(ICON_WIFI, cx, cy, 0, 0, rq, &b);
    const Texture *tex = icon_get(ICON_WIFI, 0, 0, rq, &b);
    if (tex) {
        icon_blit(tex, &b, color);
        return;
    }
    const float u = r / 7, px = cx - 5.5f * u, py = cy + 5.5f * u; /* the old way: the square, and each arc as 8 flat pieces */
    gfx_rect(px - 1.5f * u, py - 1.5f * u, 3 * u, 3 * u, color);
    for (int i = 0; i < 3; i++) {
        float r1 = (12.5f - 3.7f * i) * u, r0 = r1 - 2 * u;
        for (int k = 0; k < 8; k++) {
            float a0 = k * 3.14159265f / 16, a1 = (k + 1) * 3.14159265f / 16;
            const float q[8] = { px + sinf(a0) * r0, py - cosf(a0) * r0, px + sinf(a0) * r1, py - cosf(a0) * r1,
                                 px + sinf(a1) * r1, py - cosf(a1) * r1, px + sinf(a1) * r0, py - cosf(a1) * r0 };
            gfx_fan((q[0] + q[2] + q[4] + q[6]) / 4, (q[1] + q[3] + q[5] + q[7]) / 4, q, 4, color);
        }
    }
}

/* a quad in one colour (the corners go 1, 2, 4, 3: gfx_quad draws a strip) */
static void flat_quad(const float *xy, u32 color)
{
    const u32 c[4] = { color, color, color, color };
    gfx_quad(xy, c);
}

/* The cube in a box of side 2 r centred on (cx, cy): one image like the cog wheel (see cube_render in icons.c). The old way, when there is no room for
 * the image, is three flat faces of the same brightnesses. */
void gfx_cube(float cx, float cy, float r, u32 color)
{
    IconBox b;
    float rq = quarter(r);
    icon_box(ICON_CUBE, cx, cy, 0, 0, rq, &b);
    const Texture *tex = icon_get(ICON_CUBE, 0, 0, rq, &b);
    if (tex) {
        icon_blit(tex, &b, color);
        return;
    }
    const float w = 0.875f * r, h = 0.5f * r;
    const float top[8] = { cx, cy - r, cx + w, cy - h, cx - w, cy - h, cx, cy };
    const float left[8] = { cx - w, cy - h, cx, cy, cx - w, cy + h, cx, cy + r };
    const float right[8] = { cx, cy, cx + w, cy - h, cx, cy + r, cx + w, cy + h };
    const u32 a = (color >> 24) & 255;
    flat_quad(top, color);
    flat_quad(left, COLOR_ALPHA(color, a * 199 / 255));
    flat_quad(right, COLOR_ALPHA(color, a * 128 / 255));
}

/* The straight back edge is put on a whole pixel line (it moves by half a pixel at most, and the width is
 * a whole number of pixels), so only the two slanted sides need smoothing. */
void gfx_triangle(float x, float y, float w, float h, int dir, u32 color)
{
    int kind = dir > 0 ? ICON_TRI_R : ICON_TRI_L;
    float wq = fmaxf(1, floorf(w + 0.5f)), hq = quarter(h);
    float back = floorf((dir > 0 ? x : x + w) + 0.5f);
    float cx = back + (dir > 0 ? wq : -wq) / 2, cy = y + h / 2;
    IconBox b;
    icon_box(kind, cx, cy, wq, hq, 0, &b);
    const Texture *tex = icon_get(kind, wq, hq, 0, &b);
    if (tex) {
        icon_blit(tex, &b, color);
        return;
    }
    const float tip = dir > 0 ? x + w : x, bk = dir > 0 ? x : x + w;
    const float xy[6] = { bk, y, tip, y + h / 2, bk, y + h };
    gfx_fan(cx, y + h / 2, xy, 3, color);
}

/* The music note, a square of side `size` centred on (cx, cy). It is one image, so where the heads, stems
 * and beam meet there is no double blending when the colour is translucent. */
void gfx_note(float cx, float cy, float size, u32 color)
{
    /* The centre goes on a whole pixel (it moves by half a pixel at most): the thin stems then always land
     * the same way on the pixels, and the note looks the same wherever it is. */
    cx = floorf(cx + 0.5f);
    cy = floorf(cy + 0.5f);
    IconBox b;
    float rq = quarter(size / 2);
    icon_box(ICON_NOTE, cx, cy, 0, 0, rq, &b);
    const Texture *tex = rq > 0 ? icon_get(ICON_NOTE, 0, 0, rq, &b) : NULL;
    if (tex) {
        icon_blit(tex, &b, color);
        return;
    }
    float r = size * 0.2f, sw = fmaxf(size * 0.09f, 1.2f); /* the old drawing, from separate pieces */
    float lx = cx - size * 0.28f, rx = cx + size * 0.32f;
    float ly = cy + size * 0.32f, ry = cy + size * 0.24f;
    float top_l = cy - size * 0.42f, top_r = cy - size * 0.5f, beam = size * 0.17f;
    gfx_circle(lx, ly, r, color);
    gfx_circle(rx, ry, r, color);
    gfx_rect(lx + r - sw, top_l, sw, ly - top_l, color);
    gfx_rect(rx + r - sw, top_r, sw, ry - top_r, color);
    float xy[8] = { lx + r - sw, top_l, rx + r, top_r, lx + r - sw, top_l + beam, rx + r, top_r + beam };
    u32 c[4] = { color, color, color, color };
    gfx_quad(xy, c);
}

/* The loudspeaker of the volume popup: its left edge is at x and the middle of it at height y; 9 pixels
 * wide and 15 high. One image, for the same reason as the note. */
void gfx_speaker(float x, float y, u32 color)
{
    IconBox b;
    icon_box(ICON_SPEAKER, x + 4.5f, y, 0, 0, 4.5f, &b);
    const Texture *tex = icon_get(ICON_SPEAKER, 0, 0, 4.5f, &b);
    if (tex) {
        icon_blit(tex, &b, color);
        return;
    }
    gfx_rect(x, y - 3, 4, 6, color);
    float cone[8] = { x + 3.5f, y - 3, x + 9, y - 7.5f, x + 3.5f, y + 3, x + 9, y + 7.5f };
    u32 cc[4] = { color, color, color, color };
    gfx_quad(cone, cc);
}

/* The images of the Library picture, one for each height it has been drawn at. They are not in the cache above, which lets go of
 * what was not used for a frame: the card goes through about twenty heights when it grows or shrinks, one new height per frame,
 * and making an image is not something a frame can afford (icon_library_image has the ready-made ones for those heights, so
 * here it is only a copy). They stay until gfx_term. */
#define LIBRARY_MAX_H ICON_MAX
static Texture *s_library[LIBRARY_MAX_H + 1];

static void library_free(void)
{
    for (int i = 0; i <= LIBRARY_MAX_H; i++) {
        tex_free(s_library[i]);
        s_library[i] = NULL;
    }
}

static const Texture *library_image(int ph, int pw)
{
    if (ph < 1 || ph > LIBRARY_MAX_H || pw > ICON_MAX)
        return NULL;
    if (!s_library[ph]) {
        static unsigned char cov[ICON_MAX * ICON_MAX];
        Texture *t = tex_create(pw, ph, GU_PSM_8888);
        if (!t)
            return NULL;
        icon_library_image(ph, cov);
        u32 *px = t->data;
        for (int y = 0; y < ph; y++)
            for (int x = 0; x < pw; x++)
                px[y * t->stride + x] = RGBA(255, 255, 255, cov[y * pw + x]);
        tex_flush(t);
        s_library[ph] = t;
    }
    return s_library[ph];
}

/* The picture of the Library card, centred in the card box l, t, w, h. The size is a whole number of pixels (two thirds of the card's
 * height) and the picture is made for exactly that size, with its corner on a whole pixel, so nothing is ever blurred or shifted. */
void gfx_library_icon(int l, int t, int w, int h, u32 color)
{
    int ph = (int)floorf(h * 0.66f + 0.5f), pw = icon_library_width(ph);
    int x = l + (w - pw + 1) / 2, y = t + (h - ph + 1) / 2;
    IconBox b = { pw, ph, x, y, pw / 2.0f, ph / 2.0f, 0, 0 };
    const Texture *tex = library_image(ph, pw);
    if (tex) {
        icon_blit(tex, &b, color);
        return;
    }
    float u = ph / 77.0f; /* the picture could not be made: plain squares and the body of the controller */
    for (int i = 0; i < 6; i++)
        gfx_round_rect(x + (i % 3) * 25 * u, y + (i / 3) * 25 * u, 19 * u, 19 * u, 3 * u, color);
    gfx_round_rect(x + 27 * u, y + 41 * u, 56 * u, 27 * u, 12 * u, color);
}

/* The folder picture p: from the cache above (a folder is a few hundred pixels and two rounded boxes, cheap to make), and if there is
 * no room, two plain rounded rectangles. */
static void folder_draw(const FolderPic *p, u32 color)
{
    IconBox b = { p->w, p->h, p->x, p->y, p->w / 2.0f, p->h / 2.0f, 0, 0 };
    const Texture *tex = cache_get(ICON_FOLDER, p->w * 4, p->h * 4, 0, 0, 0, p->w, p->h, p->w / 2.0f, p->h / 2.0f);
    if (tex) {
        icon_blit(tex, &b, color);
        return;
    }
    gfx_round_rect(p->x, p->y, p->tab_w, p->tab_h + 2 * p->tab_rl, p->tab_rl, color);
    gfx_round_rect(p->x, p->y + p->tab_h, p->w, p->h - p->tab_h, p->r, color);
}

/* A folder card's picture, centred in the card */
void gfx_folder_icon(int l, int t, int w, int h, u32 color, FolderPic *pic)
{
    FolderPic p;
    icon_folder_pic(l, t, w, h, &p);
    if (pic)
        *pic = p;
    folder_draw(&p, color);
}

/* The same picture at `h` pixels high in a row of a list (the file manager), its top left corner on the whole pixel (x, y) */
void gfx_folder_picture(int x, int y, int h, u32 color)
{
    FolderPic p;
    icon_folder_pic_at(x, y, h, &p);
    folder_draw(&p, color);
}

/* The logo of the boot screen: one image with a transparent pixel round it, made once. If there is no room for it in the cache (it never is, at the
 * start, when the boot screen runs) the outline of the three shapes is stroked, which is plain but still the logo. */
void gfx_logo(int x, int y, int h, u32 color)
{
    int w = icon_logo_width(h), tw = w + 2, th = h + 2;
    IconBox b = { tw, th, x - 1, y - 1, tw / 2.0f, th / 2.0f, 0, 0 };
    const Texture *tex = cache_get(ICON_LOGO, w * 4, h * 4, 0, 0, 0, tw, th, tw / 2.0f, th / 2.0f);
    if (tex) {
        icon_blit(tex, &b, color);
        return;
    }
    for (int i = 0; i < 3; i++) {
        float xy[2 * (ICON_LOGO_MAX_PTS + 1)];
        u32 col[ICON_LOGO_MAX_PTS + 1];
        int n = icon_logo_outline(i, (float)x, (float)y, (float)w, (float)h, xy);
        if (!n)
            continue;
        xy[2 * n] = xy[0], xy[2 * n + 1] = xy[1]; /* round to where it began */
        for (int k = 0; k <= n; k++)
            col[k] = color;
        gfx_stroke(xy, col, n + 1, 2.0f);
    }
}

/* The height of a capital letter of the system font is 13.7 px at size 1.0 (measured on the README screenshots, which PPSSPP made with the
 * real font: the A of the Library title at size 0.66 is 9 px high, the clock's digits at 0.8 are 11 px). */
#define CAP_HEIGHT 13.7f

void gfx_folder_letter_at(const FolderPic *pic, float *x, float *y, float *size)
{
    float body = (float)(pic->h - pic->tab_h);
    float s = 0.037f * body; /* a capital is then about half as high as the body */
    float cx = pic->x + pic->w / 2.0f;
    float cy = pic->y + pic->tab_h + body / 2.0f; /* (the middle of the body, the part under the tab: that is what the eye centres on) */
    /* text_draw with ALIGN_CENTER puts the middle of the letter (its advance) at x and its foot on the baseline y: the middle of a capital
     * is half a capital above that. Both on whole pixels, as the letters of the rest of the Library are. */
    *x = floorf(cx + 0.5f);
    *y = floorf(cy + CAP_HEIGHT * s / 2 + 0.5f);
    *size = s;
}

float gfx_button_width(int kind)
{
    return kind == BTN_START ? 28.0f : kind == BTN_SELECT ? 34.0f : 13.0f;
}

void gfx_button(float cx, float cy, float r, int kind, u32 plate, u32 mark)
{
    static const int symbol[] = { ICON_CROSS, ICON_CIRCLE, ICON_TRIANGLE, ICON_SQUARE }; /* BTN_* */
    IconBox b;
    float rq = quarter(r);
    /* The centre goes on a whole pixel (it moves by half a pixel at most): then every button looks
     * the same wherever it is, and the straight edges of the symbols can be pixel exact. */
    cx = floorf(cx + 0.5f);
    cy = floorf(cy + 0.5f);
    if (kind == BTN_START || kind == BTN_SELECT) { /* a pill with the word on it, the way the PSP prints them next to the button */
        float w = gfx_button_width(kind);
        const char *word = kind == BTN_START ? "START" : "SELECT";
        gfx_round_rect(cx - w / 2, cy - 7, w, 14, 7, plate); /* (14 high: the top and bottom edges are on whole pixels when the centre is) */
        float s = text_fit(0.34f, 0.2f, 1, w - 6, word);
        text_draw(cx, floorf(cy + 6.45f * s + 0.5f), s, 1, mark, ALIGN_CENTER, word); /* (the baseline 6.45 x the size under the middle looks centred) */
        return;
    }
    icon_box(ICON_DISC, cx, cy, 0, 0, rq, &b); /* the symbols have the same box as the plate */
    const Texture *disc = icon_get(ICON_DISC, 0, 0, rq, &b);
    const Texture *sym = disc ? icon_get(symbol[kind], 0, 0, rq, &b) : NULL;
    if (disc && sym) {
        icon_blit(disc, &b, plate);
        icon_blit(sym, &b, mark);
        return;
    }
    gfx_circle(cx, cy, r, plate);
    float s = r * 0.45f;
    switch (kind) {
    case BTN_CIRCLE:
        gfx_circle(cx, cy, r * 0.56f, mark);
        gfx_circle(cx, cy, r * 0.34f, plate);
        break;
    case BTN_CROSS:
        for (float o = -0.5f; o <= 0.5f; o += 0.5f) {
            gfx_line(cx - s + o, cy - s, cx + s + o, cy + s, mark);
            gfx_line(cx + s + o, cy - s, cx - s + o, cy + s, mark);
        }
        break;
    case BTN_TRIANGLE:
        for (float o = -0.4f; o <= 0.4f; o += 0.4f) {
            gfx_line(cx + o, cy - s - 0.5f, cx - s - 0.5f + o, cy + s * 0.8f, mark);
            gfx_line(cx - s - 0.5f + o, cy + s * 0.8f + o, cx + s + 0.5f, cy + s * 0.8f + o, mark);
            gfx_line(cx + s + 0.5f + o, cy + s * 0.8f, cx + o, cy - s - 0.5f, mark);
        }
        break;
    case BTN_SQUARE:
        gfx_frame(cx - s, cy - s, s * 2, s * 2, 1.2f, mark);
        break;
    }
}

/* --- text --- */

void font_init(int need_jpn)
{
    widths_forget();
    intraFontInit();
    s_font = intraFontLoad("flash0:/font/ltn0.pgf", INTRAFONT_CACHE_LARGE | INTRAFONT_CACHE_ALL);
    s_bold = intraFontLoad("flash0:/font/ltn4.pgf", INTRAFONT_CACHE_LARGE | INTRAFONT_CACHE_ALL);
    if (!s_bold)
        s_bold = s_font;
    if (need_jpn)
        s_jpn = intraFontLoad("flash0:/font/jpn0.pgf", INTRAFONT_STRING_UTF8);

    intraFont *fonts[] = { s_font, s_bold };
    for (int i = 0; i < 2; i++) {
        if (!fonts[i])
            continue;
        intraFontSetEncoding(fonts[i], INTRAFONT_STRING_UTF8);
        if (s_jpn)
            intraFontSetAltFont(fonts[i], s_jpn);
    }
}

void font_ensure_cjk(void)
{
    if (s_jpn || !s_font)
        return;
    s_jpn = intraFontLoad("flash0:/font/jpn0.pgf", INTRAFONT_STRING_UTF8);
    if (!s_jpn)
        return;
    widths_forget(); /* with the extra font some letters get other widths */
    intraFontSetAltFont(s_font, s_jpn);
    if (s_bold != s_font)
        intraFontSetAltFont(s_bold, s_jpn);
}

void font_term(void)
{
    widths_forget();
    if (s_bold && s_bold != s_font)
        intraFontUnload(s_bold);
    if (s_font)
        intraFontUnload(s_font);
    if (s_jpn)
        intraFontUnload(s_jpn);
    s_font = s_bold = s_jpn = NULL;
    intraFontShutdown();
}

static intraFont *pick(int bold)
{
    return bold ? s_bold : s_font;
}

/* Czech letters the system fonts lack (č, ř, ť …) are printed as the plain letter and the mark is
 * drawn on top, see diacritic.h. `buf` gets the plain text; returns the number of marks (0 = print
 * `s` as it is). */
#define PLAIN_MAX 512
static int plain_text(const char *s, char *buf, DiacMark *marks)
{
    if (!diac_maybe(s))
        return 0;
    int n = diac_split(s, buf, PLAIN_MAX, marks, DIAC_MAX_MARKS);
    return n > 0 ? n : 0;
}

/* The width of a text is asked for again every frame (menus, the tab bar, the hints), and measuring goes
 * glyph by glyph through the font. The last results are kept, so a repeated question is answered by
 * looking at the text once. The answer is exactly the one the font gave. */
#define WIDTH_CACHE 128
#define WIDTH_KEY 48 /* longer texts are just measured */

typedef struct {
    float size, width;
    u32 hash;
    int bold;
    char text[WIDTH_KEY]; /* "" = unused */
} WidthEnt;

static WidthEnt s_widths[WIDTH_CACHE];

static void widths_forget(void)
{
    memset(s_widths, 0, sizeof(s_widths));
}

float text_width(float size, int bold, const char *s)
{
    intraFont *f = pick(bold);
    if (!f)
        return 0;

    WidthEnt *e = NULL;
    u32 hash = 2166136261u;
    int n = 0;
    while (s[n] && n < WIDTH_KEY) {
        hash = (hash ^ (u8)s[n]) * 16777619u;
        n++;
    }
    if (n < WIDTH_KEY && n > 0) { /* short enough to keep */
        union { float f; u32 u; } sz = { size };
        hash = (hash ^ sz.u ^ (u32)bold * 0x9E3779B1u) * 16777619u;
        e = &s_widths[(hash >> 8) % WIDTH_CACHE];
        if (e->hash == hash && e->size == size && e->bold == bold && e->text[0] && !strcmp(e->text, s))
            return e->width;
    }

    intraFontSetStyle(f, size, 0xFFFFFFFF, 0, 0.0f, 0);
    if (s_jpn)
        intraFontSetStyle(s_jpn, size, 0xFFFFFFFF, 0, 0.0f, 0);
    char buf[PLAIN_MAX];
    DiacMark marks[DIAC_MAX_MARKS];
    const char *measured = plain_text(s, buf, marks) ? buf : s;
    float w = intraFontMeasureText(f, measured);
    if (e) {
        e->hash = hash;
        e->size = size;
        e->bold = bold;
        e->width = w;
        memcpy(e->text, s, n + 1);
    }
    return w;
}

float text_fit(float size, float min_size, int bold, float max_w, const char *s)
{
    while (size > min_size && text_width(size, bold, s) > max_w)
        size -= 0.02f;
    return size;
}

void text_ellipsis(char *out, int out_size, const char *s, float size, float max_w)
{
    snprintf(out, out_size, "%s", s);
    if (text_width(size, 0, out) <= max_w)
        return;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", s);
    int n = strlen(buf);
    do {
        while (n > 0 && ((u8)buf[--n] & 0xC0) == 0x80) /* ett helt UTF-8-tecken */
            ;
        buf[n] = 0;
        snprintf(out, out_size, "%s...", buf);
    } while (n > 0 && text_width(size, 0, out) > max_w);
}

int text_wrap(const char *s, float size, float max_w, char lines[][96], int max_lines)
{
    int n = 0;
    while (*s == ' ')
        s++;
    while (*s && n < max_lines) {
        if (n == max_lines - 1 && text_width(size, 0, s) > max_w) { /* the last line takes the rest */
            text_ellipsis(lines[n++], 96, s, size, max_w);
            break;
        }
        char buf[96];
        int i = 0, last_space = -1;
        while (s[i]) {
            int l = (u8)s[i] < 0x80 ? 1 : (u8)s[i] < 0xE0 ? 2 : (u8)s[i] < 0xF0 ? 3 : 4;
            for (int k = 1; k < l; k++) /* a cut-off character at the end of the text: never read past the zero */
                if (!s[i + k]) {
                    l = k;
                    break;
                }
            if (i + l >= (int)sizeof(buf))
                break;
            memcpy(buf, s, i + l);
            buf[i + l] = 0;
            if (i > 0 && text_width(size, 0, buf) > max_w)
                break;
            if (s[i] == ' ')
                last_space = i;
            i += l;
        }
        int cut = i;
        if (s[i] && last_space > 0)
            cut = last_space; /* the word doesn't fit: break at the last space instead */
        snprintf(lines[n++], 96, "%.*s", cut, s);
        s += cut;
        while (*s == ' ')
            s++;
    }
    return n;
}

void text_draw(float x, float y, float size, int bold, u32 color, int align, const char *s)
{
    intraFont *f = pick(bold);
    if (!f)
        return;
    intraFontSetStyle(f, size, color, 0, 0.0f, 0);
    if (s_jpn) /* reservtypsnittet ärver inte stilen av sig självt */
        intraFontSetStyle(s_jpn, size, color, 0, 0.0f, 0);
    char buf[PLAIN_MAX];
    DiacMark marks[DIAC_MAX_MARKS];
    int n_marks = plain_text(s, buf, marks);
    if (n_marks)
        s = buf;
    if (align != ALIGN_LEFT) {
        float w = intraFontMeasureText(f, s);
        x -= align == ALIGN_CENTER ? w / 2 : w;
    }
    intraFontPrint(f, x, y, s);
    s_bound = NULL; /* intraFont har bundit sin egen textur */
    s_filter = -1;
    /* intraFont slår på djuptestet efter sig – vi har ingen djupbuffert */
    sceGuDisable(GU_DEPTH_TEST);

    for (int i = 0; i < n_marks; i++) { /* the marks of č, ř, ť … */
        int pos = marks[i].pos;
        char one[2] = { buf[pos], 0 };
        buf[pos] = 0; /* the text before the letter gives where it starts */
        float lx = x + intraFontMeasureText(f, buf);
        buf[pos] = one[0];
        DiacVert tv[DIAC_MAX_VERTS];
        int n = diac_mesh(&marks[i], lx, intraFontMeasureText(f, one), y, size, bold, tv, DIAC_MAX_VERTS);
        if (n <= 0)
            continue;
        CVert *v = sceGuGetMemory(n * sizeof(CVert));
        u32 rgb = color & 0x00FFFFFF;
        float alpha = (float)(color >> 24);
        for (int k = 0; k < n; k++) /* soft edges: the alpha fades out towards the sides */
            v[k] = (CVert){ rgb | ((u32)(alpha * tv[k].a + 0.5f) << 24), tv[k].x, tv[k].y, 0 };
        sceGuDisable(GU_TEXTURE_2D);
        sceGuDrawArray(GU_TRIANGLES, CVERT_FMT, n, 0, v);
    }
}

void gfx_blend_add(int on)
{
    if (on)
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF);
    else
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
}

/* A line through the points xy[2n] with the given width, and the colour colors[i] at point i, with smooth
 * edges: the GPU has no line smoothing of its own, so the line is a strip whose colour fades out over the
 * last pixel on each side (a ramp one pixel wide, centred on the true edge - which is what the coverage of a
 * pixel is for a straight edge). The ends fade the same way. A line thinner than a pixel is a lower, narrow
 * ridge instead, so it fades rather than disappears. At a corner the two sides meet in a mitre (never
 * longer than twice the half width, so a very sharp turn does not make a spike).
 *
 * It is drawn as three strips - the left ramp, the solid middle, the right ramp - of the same vertices. */
#define STROKE_MAX 128

static u32 stroke_alpha(u32 c, float f)
{
    return COLOR_ALPHA(c, (u32)(((c >> 24) & 255) * f + 0.5f));
}

void gfx_stroke(const float *xy, const u32 *colors, int n, float width)
{
    if (n < 2)
        return;
    if (n > STROKE_MAX)
        n = STROKE_MAX;
    /* unit directions and lengths of the n - 1 segments (a segment without length takes its neighbour's) */
    float dx[STROKE_MAX], dy[STROKE_MAX], ln[STROKE_MAX];
    int valid = -1;
    for (int i = 0; i < n - 1; i++) {
        float ex = xy[i * 2 + 2] - xy[i * 2], ey = xy[i * 2 + 3] - xy[i * 2 + 1];
        ln[i] = sqrtf(ex * ex + ey * ey);
        if (ln[i] > 1e-4f) {
            dx[i] = ex / ln[i], dy[i] = ey / ln[i];
            if (valid < 0)
                valid = i;
        }
    }
    if (valid < 0)
        return;
    for (int i = 0; i < n - 1; i++) {
        if (ln[i] > 1e-4f)
            continue;
        int j = i > 0 ? i - 1 : valid; /* the one before it, already filled in */
        dx[i] = dx[j], dy[i] = dy[j];
    }
    const float hw = fmaxf(width, 1.0f) / 2, peak = fminf(width, 1.0f);
    const float off[4] = { -(hw + 0.5f), -(hw - 0.5f), hw - 0.5f, hw + 0.5f };
    const float cap0 = fminf(0.5f, ln[0] * 0.5f), cap1 = fminf(0.5f, ln[n - 2] * 0.5f);
    int m = n + 2; /* a cross-section before the start, one at every point (the first and last pulled in by half a pixel), one after the end */
    CVert *v = sceGuGetMemory(m * 6 * sizeof(CVert));
    CVert *strip[3] = { v, v + 2 * m, v + 4 * m };
    for (int j = 0; j < m; j++) {
        int i = j < 1 ? 0 : j > n ? n - 1 : j - 1; /* the point this section belongs to */
        float px = xy[i * 2], py = xy[i * 2 + 1], f = 1;
        float nx, ny;
        if (i == 0 || i == n - 1) {
            int s = i == 0 ? 0 : n - 2; /* the only segment at this end */
            nx = -dy[s], ny = dx[s];
            if (j <= 1) { /* start: the cap is half a pixel before the point, the first full section half a pixel after it */
                float k = j == 0 ? -cap0 : cap0;
                px += dx[s] * k, py += dy[s] * k;
                f = j == 0 ? 0 : 1;
            } else {
                float k = j == n ? -cap1 : cap1;
                px += dx[s] * k, py += dy[s] * k;
                f = j == n ? 1 : 0;
            }
        } else {
            float ax = -dy[i - 1], ay = dx[i - 1], bx = -dy[i], by = dx[i];
            float mx = ax + bx, my = ay + by, ml = sqrtf(mx * mx + my * my);
            if (ml < 1e-4f) { /* turns right back on itself */
                nx = ax, ny = ay;
            } else {
                mx /= ml, my /= ml;
                float k = 1.0f / fmaxf(mx * ax + my * ay, 0.5f);
                nx = mx * k, ny = my * k;
            }
        }
        u32 edge = stroke_alpha(colors[i], 0), mid = stroke_alpha(colors[i], f * peak);
        CVert c[4];
        for (int k = 0; k < 4; k++)
            c[k] = (CVert){ k == 0 || k == 3 ? edge : mid, px + nx * off[k], py + ny * off[k], 0 };
        for (int s = 0; s < 3; s++) {
            strip[s][j * 2] = c[s];
            strip[s][j * 2 + 1] = c[s + 1];
        }
    }
    sceGuDisable(GU_TEXTURE_2D);
    for (int s = 0; s < 3; s++)
        sceGuDrawArray(GU_TRIANGLE_STRIP, CVERT_FMT, m * 2, 0, strip[s]);
}

void gfx_dot(float cx, float cy, float r, u32 color, int soft)
{
    TVert *v = sceGuGetMemory(2 * sizeof(TVert));
    v[0] = (TVert){ 0, 0, color, cx - r, cy - r, 0 };
    v[1] = (TVert){ SHAPE_SIZE, SHAPE_SIZE, color, cx + r, cy + r, 0 };
    bind(soft ? s_soft : s_disc);
    sceGuDrawArray(GU_SPRITES, TVERT_FMT, 2, 0, v);
}

void gfx_clip(float x, float y, float w, float h)
{
    sceGuScissor((int)x, (int)y, (int)w, (int)h);
}

void gfx_clip_reset(void)
{
    sceGuScissor(0, 0, SCREEN_W, SCREEN_H);
}

void gfx_quad(const float *xy, const u32 *colors)
{
    CVert *v = sceGuGetMemory(4 * sizeof(CVert));
    for (int i = 0; i < 4; i++)
        v[i] = (CVert){ colors[i], xy[i * 2], xy[i * 2 + 1], 0 };
    sceGuDisable(GU_TEXTURE_2D);
    sceGuDrawArray(GU_TRIANGLE_STRIP, CVERT_FMT, 4, 0, v);
}
