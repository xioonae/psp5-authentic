#include <math.h>
#include <string.h>
#include "icons.h"

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

static float len2(float x, float y)
{
    return sqrtf(x * x + y * y);
}

/* --- signed distances in pixels (negative inside the shape) --- */

/* box around the origin with half sizes hx, hy and rounded corners of radius r */
static float sd_rbox(float px, float py, float hx, float hy, float r)
{
    float qx = fabsf(px) - (hx - r), qy = fabsf(py) - (hy - r);
    return len2(fmaxf(qx, 0), fmaxf(qy, 0)) + fminf(fmaxf(qx, qy), 0) - r;
}

/* a line from a to b with flat ends and the half width hw */
static float sd_line(float px, float py, float ax, float ay, float bx, float by, float hw)
{
    float dx = bx - ax, dy = by - ay, len = len2(dx, dy);
    dx /= len, dy /= len;
    float qx = px - (ax + bx) / 2, qy = py - (ay + by) / 2;
    return sd_rbox(qx * dx + qy * dy, -qx * dy + qy * dx, len / 2, hw, 0);
}

/* polygon v[2 * n] */
static float sd_poly(const float *v, int n, float px, float py)
{
    float d = (px - v[0]) * (px - v[0]) + (py - v[1]) * (py - v[1]);
    float s = 1;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        float ex = v[j * 2] - v[i * 2], ey = v[j * 2 + 1] - v[i * 2 + 1];
        float wx = px - v[i * 2], wy = py - v[i * 2 + 1];
        float h = clampf((wx * ex + wy * ey) / (ex * ex + ey * ey), 0, 1);
        float bx = wx - ex * h, by = wy - ey * h;
        d = fminf(d, bx * bx + by * by);
        int c1 = py >= v[i * 2 + 1], c2 = py < v[j * 2 + 1], c3 = ex * wy > ey * wx;
        if ((c1 && c2 && c3) || (!c1 && !c2 && !c3))
            s = -s;
    }
    return s * sqrtf(d);
}

/* --- the cog: 8 teeth, the first one 22.5 degrees from the vertical, as it has always been --- */

#define TEETH 8
#define GEAR_PTS (TEETH * 4)

static void gear_polygon(float r, float *v)
{
    const float root = r * 0.72f;            /* between the teeth */
    const float top = 8.0f * 3.14159265f / 180, base = 16.0f * 3.14159265f / 180; /* half angles */
    for (int k = 0; k < TEETH; k++) {
        float c = (k + 0.5f) * 2.0f * 3.14159265f / TEETH;
        const float a[4] = { c - base, c - top, c + top, c + base };
        const float rr[4] = { root, r, r, root };
        for (int i = 0; i < 4; i++) {
            v[(k * 4 + i) * 2] = cosf(a[i]) * rr[i];
            v[(k * 4 + i) * 2 + 1] = sinf(a[i]) * rr[i];
        }
    }
}

void icon_box(int kind, float cx, float cy, float w, float h, float r, IconBox *b)
{
    int sized = kind == ICON_RRECT || kind == ICON_TRI_R || kind == ICON_TRI_L; /* these have their own w * h */
    float ex = sized ? w : r * 2, ey = sized ? h : kind == ICON_SPEAKER ? r * 4 : r * 2;
    b->tw = (int)ceilf(ex) + 4; /* two pixels of margin on every side */
    b->th = (int)ceilf(ey) + 4;
    float fx = cx - b->tw / 2.0f, fy = cy - b->th / 2.0f;
    float x0 = floorf(fx), y0 = floorf(fy);
    int qx = (int)floorf((fx - x0) * 2 + 0.5f), qy = (int)floorf((fy - y0) * 2 + 0.5f); /* 0, 1 or 2 halves */
    if (qx == 2)
        qx = 0, x0 += 1;
    if (qy == 2)
        qy = 0, y0 += 1;
    b->x = (int)x0;
    b->y = (int)y0;
    b->phase_x = qx;
    b->phase_y = qy;
    b->cx = b->tw / 2.0f + qx * 0.5f;
    b->cy = b->th / 2.0f + qy * 0.5f;
}

/* The folder, drawn like the folder picture it was made after (a folder icon with a wide tab and big soft corners): the picture is `h` pixels
 * high and 1.38 times as wide. The tab sticks up 16 % of the height above the body and is 45 % of the width wide; the body's corners are 17 %
 * of the height, the tab's top left corner (the folder's own top left corner) 31 % and its top right corner 23 %: a big soft shoulder
 * that runs down into the top edge of the body. The numbers were found by fitting this shape to the picture (to within a quarter of a
 * pixel at 23 pixels high). */
static void folder_dims(int w, int h, int *tab_w, int *tab_h, int *r, int *rl, int *rr)
{
    *tab_w = (int)floorf(w * 0.45f + 0.5f);
    *tab_h = (int)floorf(h * 0.16f + 0.5f);
    *r = (int)floorf(h * 0.17f + 0.5f);
    *rl = (int)floorf(h * 0.31f + 0.5f);
    *rr = (int)floorf(h * 0.23f + 0.5f);
    if (*tab_h < 3)
        *tab_h = 3;
    if (*r < 2)
        *r = 2;
    if (*rl > *tab_w / 2) /* (each of the two corners of the tab is at most half of it: the flat top between them is never negative) */
        *rl = *tab_w / 2;
    if (*rr > *tab_w / 2)
        *rr = *tab_w / 2;
    if (*rl < *r)
        *rl = *r;
    if (*rr < *r)
        *rr = *r;
}

void icon_folder_pic_at(int x, int y, int h, FolderPic *p)
{
    p->h = h;
    p->w = (int)floorf(h * 1.38f + 0.5f);
    p->x = x;
    p->y = y;
    folder_dims(p->w, p->h, &p->tab_w, &p->tab_h, &p->r, &p->tab_rl, &p->tab_rr);
}

void icon_folder_pic(int l, int t, int w, int h, FolderPic *p)
{
    int ph = (int)floorf(h * 0.66f + 0.5f);
    int pw = (int)floorf(ph * 1.38f + 0.5f);
    /* centred, on whole pixels (the odd pixel goes to the left, as with the Library picture) */
    icon_folder_pic_at(l + (w - pw + 1) / 2, t + (h - ph + 1) / 2, ph, p);
}

/* The Library picture, in whole pixels from its top left corner. The design is 83 x 77 units: six squares (19 on a pitch of 25) and the
 * controller (56 x 41) with its top left corner at (27, 36); `u` pixels per unit, every edge rounded to a whole pixel. The right half of
 * the controller is the mirror image of its left half, so it is exactly the same on both sides whatever the size. */
typedef struct {
    float w, h;           /* the picture */
    float sq, pitch, cr;  /* a square, the distance between two, its corner radius */
    float rim;            /* the controller's rim: the squares stay this far from it */
    float part[5][5];     /* the controller: x, y, w, h, corner radius of the body, the two shoulders and the two grips */
    float arch[5];        /* the arch between the grips, cut out of the controller */
} LibGeo;

static void library_geo(int h, LibGeo *g)
{
    const float u = h / 77.0f;
    #define PXL(v) floorf((v) * u + 0.5f)
    const float cw = PXL(56), ox = PXL(27), oy = h - PXL(41); /* the controller's width, its left edge, its top (it ends at the bottom) */
    g->h = (float)h;
    g->sq = PXL(19);
    g->pitch = PXL(25);
    g->cr = fminf(PXL(3), g->sq / 2);
    g->rim = fmaxf(1, PXL(4));
    /* the body and the arch are centred; the left shoulder and grip are made, the right ones are their mirror images */
    static const float left[2][5] = { { 3, 0, 23, 16, 8 }, { 0, 14, 20, 27, 10 } }; /* shoulder, grip (design units, from the controller) */
    static const float body[5] = { 0, 5, 56, 27, 12 }, arch[5] = { 0, 28, 0, 20, 8 }; /* (the arch's own x and width come from the grips) */
    float *p = g->part[0];
    p[0] = ox; p[1] = oy + PXL(body[1]); p[2] = cw; p[3] = PXL(body[1] + body[3]) - PXL(body[1]); p[4] = fminf(PXL(body[4]), fminf(p[2], p[3]) / 2);
    for (int i = 0; i < 2; i++) {
        const float *q = left[i];
        float l = PXL(q[0]), r = PXL(q[0] + q[2]), t = PXL(q[1]), b = PXL(q[1] + q[3]);
        float rad = fminf(PXL(q[4]), fminf(r - l, b - t) / 2);
        float *a = g->part[1 + i * 2], *c = g->part[2 + i * 2];
        a[0] = ox + l; a[1] = oy + t; a[2] = r - l; a[3] = b - t; a[4] = rad;
        c[0] = ox + cw - r; c[1] = a[1]; c[2] = a[2]; c[3] = a[3]; c[4] = rad; /* mirrored: the same size, the same distance from the right edge */
    }
    /* the arch is what is left between the grips, so it is exactly in the middle (and the same width as the gap between the grips) */
    float gap_l = g->part[3][0] + g->part[3][2], gap_r = g->part[4][0]; /* the inner edges of the grips */
    float at = PXL(arch[1]), ab = PXL(arch[1] + arch[3]), aw = gap_r - gap_l;
    g->arch[0] = gap_l; g->arch[1] = oy + at; g->arch[2] = aw; g->arch[3] = ab - at; g->arch[4] = fminf(PXL(arch[4]), aw / 2);
    g->w = fmaxf(ox + cw, 2 * g->pitch + g->sq);
    #undef PXL
}

int icon_library_width(int h)
{
    LibGeo g;
    library_geo(h, &g);
    return (int)g.w;
}

#ifdef LIB_NO_BAKED /* (tools/gen_library_icons.py builds this file without the stored images, to make them) */
#define LIB_BAKED_FIRST 1
#define LIB_BAKED_LAST 0
static const unsigned short lib_baked_start[1] = { 0 };
static const unsigned char lib_baked[1] = { 0 };
#else
#include "libicon_data.inc"
#endif

int icon_library_baked(int h)
{
    return h >= LIB_BAKED_FIRST && h <= LIB_BAKED_LAST;
}

void icon_library_image(int h, unsigned char *out)
{
    int w = icon_library_width(h);
    if (h < LIB_BAKED_FIRST || h > LIB_BAKED_LAST) {
        icon_render(ICON_LIBRARY, (float)w, (float)h, 0, w / 2.0f, h / 2.0f, w, h, out);
        return;
    }
    const unsigned char *p = lib_baked + lib_baked_start[h - LIB_BAKED_FIRST];
    for (int n = 0; n < w * h; p += 2) { /* pairs: coverage, run */
        memset(out + n, p[0], p[1]);
        n += p[1];
    }
}

typedef struct {
    int kind;
    float w, h, r;
    LibGeo lib;           /* ICON_LIBRARY */
    float sw, s, sq, tri[6];
    float bolt[12];
    float gear[GEAR_PTS * 2];
    float note[8], beam[8]; /* ICON_NOTE: lx, ly, rx, ry, top_l, top_r, head radius, stem width; the beam polygon */
    float cone[8];          /* ICON_SPEAKER: the cone polygon */
    float u;                /* ICON_SPEAKER: pixels per design unit */
    float fold[5];          /* ICON_FOLDER: the tab's width, the tab's height, the body's corner radius, the tab's two top corner radii (the picture is w * h) */
} Shape;

/* The music note, as it was always drawn from separate pieces, but as one shape: two heads, two stems
 * and the beam between the tops of the stems. The size of the note is `s` (a square of that side). */
static void note_shape(float s, Shape *p)
{
    float rad = 0.2f * s, sw = fmaxf(0.09f * s, 1.2f);
    float lx = -0.28f * s, rx = 0.32f * s, ly = 0.32f * s, ry = 0.24f * s;
    float top_l = -0.42f * s, top_r = -0.5f * s, beam = 0.17f * s;
    const float n[8] = { lx, ly, rx, ry, top_l, top_r, rad, sw };
    const float b[8] = { lx + rad - sw, top_l, rx + rad, top_r, rx + rad, top_r + beam, lx + rad - sw, top_l + beam };
    memcpy(p->note, n, sizeof(n));
    memcpy(p->beam, b, sizeof(b));
}

static float sd_note(const Shape *p, float px, float py)
{
    const float *n = p->note;
    float d = fminf(len2(px - n[0], py - n[1]) - n[6], len2(px - n[2], py - n[3]) - n[6]); /* the two heads */
    float sw = n[7];
    d = fminf(d, sd_rbox(px - (n[0] + n[6] - sw / 2), py - (n[4] + n[1]) / 2, sw / 2, (n[1] - n[4]) / 2, 0));
    d = fminf(d, sd_rbox(px - (n[2] + n[6] - sw / 2), py - (n[5] + n[3]) / 2, sw / 2, (n[3] - n[5]) / 2, 0));
    return fminf(d, sd_poly(p->beam, 4, px, py));
}

/* The loudspeaker: a box (4 x 6 in design units) and a cone that widens from 6 to 15 units high. The
 * centre is the middle of the 9 unit wide shape. */
static void speaker_shape(float u, Shape *p)
{
    const float c[8] = { -1.0f * u, -3.0f * u, 4.5f * u, -7.5f * u, 4.5f * u, 7.5f * u, -1.0f * u, 3.0f * u };
    memcpy(p->cone, c, sizeof(c));
    p->u = u;
}

static float sd_speaker(const Shape *p, float px, float py)
{
    float u = p->u;
    return fminf(sd_rbox(px + 2.5f * u, py, 2.0f * u, 3.0f * u, 0), sd_poly(p->cone, 4, px, py));
}

/* The Library picture: the squares, minus the controller and its rim, plus the controller (minus the arch between its grips).
 * (px, py) is measured from the middle of the picture. It is one shape, so where the parts meet nothing is blended twice. */
static float sd_library(const LibGeo *g, float px, float py)
{
    px += g->w / 2;
    py += g->h / 2; /* from the top left corner */
    float squares = 1e9f, ctrl = 1e9f;
    for (int row = 0; row < 2; row++)
        for (int col = 0; col < 3; col++)
            squares = fminf(squares, sd_rbox(px - col * g->pitch - g->sq / 2, py - row * g->pitch - g->sq / 2, g->sq / 2, g->sq / 2, g->cr));
    for (int i = 0; i < 5; i++) {
        const float *q = g->part[i];
        ctrl = fminf(ctrl, sd_rbox(px - q[0] - q[2] / 2, py - q[1] - q[3] / 2, q[2] / 2, q[3] / 2, q[4]));
    }
    const float *a = g->arch;
    ctrl = fmaxf(ctrl, -sd_rbox(px - a[0] - a[2] / 2, py - a[1] - a[3] / 2, a[2] / 2, a[3] / 2, a[4]));
    return fminf(fmaxf(squares, g->rim - ctrl), ctrl);
}

/* A box whose top left and top right corners have the radii rl and rr and whose bottom corners are square (centred on the origin, half
 * sizes hx, hy). Exact as long as rl and rr are at most hx (and at most hy). */
static float sd_rbox_top(float px, float py, float hx, float hy, float rl, float rr)
{
    float r = px > 0 ? (py < 0 ? rr : 0) : (py < 0 ? rl : 0);
    float qx = fabsf(px) - hx + r, qy = fabsf(py) - hy + r;
    return len2(fmaxf(qx, 0), fmaxf(qy, 0)) + fminf(fmaxf(qx, qy), 0) - r;
}

/* The folder: the body (the whole picture below the tab's height) and the tab (a box from the top, with its two top corners rounded,
 * that reaches down into the body far enough to cover the body's top left corner), as one shape, so that where they meet nothing is
 * blended twice. (px, py) is measured from the middle of the picture. */
static float sd_folder(const Shape *p, float px, float py)
{
    float tw = p->fold[0], th = p->fold[1], r = p->fold[2], rl = p->fold[3], rr = p->fold[4], w = p->w, h = p->h;
    float body = sd_rbox(px, py - th / 2, w / 2, (h - th) / 2, r);
    /* (the tab is at least as high as two of its corners, which sd_rbox_top needs; its bottom edge is in the body, on its straight left side) */
    float depth = fminf(fmaxf(2 * fmaxf(rl, rr), th + r + 1), h - r);
    float tab = sd_rbox_top(px + (w - tw) / 2, py + h / 2 - depth / 2, tw / 2, depth / 2, rl, rr);
    return fminf(body, tab);
}

/* The Wi-Fi sign in a box of side 2 r = 14 units: a square of 3 units in the bottom left corner and, round its middle, the upper
 * right quarters of three rings (2 thick, 1.7 between them), the outer one touching the right and the top of the box. The arcs start
 * half a unit to the right of the square's middle and half a unit above it, which is on a pixel line at the usual size, so their ends are
 * straight lines between two pixels. All of it has soft edges like the other icons (icon_render samples the sign 4 x 4 times per pixel),
 * and the square is on whole pixels, so it is sharp. It is one shape, so where the parts meet nothing is blended twice.
 * (px, py) is measured from the middle of the box. */
static float sd_wifi(const Shape *p, float px, float py)
{
    float u = p->r / 7.0f;
    float x = px + 5.5f * u, y = py - 5.5f * u; /* from the middle of the square */
    float d = sd_rbox(x, y, 1.5f * u, 1.5f * u, 0.3f * u);
    float cut = fmaxf(0.5f * u - x, y + 0.5f * u); /* inside where x >= 0.5 and y <= -0.5: up and to the right of the middle */
    for (int i = 0; i < 3; i++)
        d = fminf(d, fmaxf(fabsf(len2(x, y) - (11.5f - 3.7f * i) * u) - 1.0f * u, cut));
    return d;
}

/* The bolt: a zigzag 2 r high, drawn with the corners in this order (the units are 1/5.2 r). */
static void bolt_polygon(float r, float *v)
{
    static const float b[12] = { 1.6f, -5.2f, -2.6f, 0.9f, -0.2f, 0.9f, -1.6f, 5.2f, 2.6f, -0.9f, 0.2f, -0.9f };
    for (int i = 0; i < 12; i++)
        v[i] = b[i] * r / 5.2f;
}

/* signed distance in pixels from the centre of the shape to the point (px, py), negative inside */
static float shape_distance(const Shape *p, float px, float py)
{
    switch (p->kind) {
    case ICON_DISC:
        return len2(px, py) - p->r;
    case ICON_RRECT:
        return sd_rbox(px, py, p->w / 2, p->h / 2, fminf(p->r, fminf(p->w, p->h) / 2));
    case ICON_CIRCLE:
        return fabsf(len2(px, py) - 0.45f * p->r) - p->sw / 2;
    case ICON_CROSS:
        return fminf(sd_line(px, py, -p->s, -p->s, p->s, p->s, p->sw / 2),
                     sd_line(px, py, p->s, -p->s, -p->s, p->s, p->sw / 2));
    case ICON_TRIANGLE:
        return fabsf(sd_poly(p->tri, 3, px, py)) - p->sw / 2;
    case ICON_SQUARE:
        return fabsf(sd_rbox(px, py, p->sq - p->sw / 2, p->sq - p->sw / 2, 0.4f)) - p->sw / 2;
    case ICON_RRING: {
        float sd = sd_rbox(px, py, p->w / 2, p->h / 2, fminf(p->r, fminf(p->w, p->h) / 2));
        return fmaxf(sd, -sd - 0.19f * p->r); /* inside the edge, 0.19 r thick */
    }
    case ICON_RFRAME:
    case ICON_RFRAME_S: {
        float sd = sd_rbox(px, py, p->w / 2, p->h / 2, fminf(p->r, fminf(p->w, p->h) / 2));
        return fmaxf(sd, -sd - (p->kind == ICON_RFRAME ? ICON_FRAME_T : ICON_FRAME_T_S));
    }
    case ICON_BOLT:
        return sd_poly(p->bolt, 6, px, py);
    case ICON_TRI_R:
    case ICON_TRI_L:
        return sd_poly(p->tri, 3, px, py);
    case ICON_NOTE:
        return sd_note(p, px, py);
    case ICON_SPEAKER:
        return sd_speaker(p, px, py);
    case ICON_LIBRARY:
        return sd_library(&p->lib, px, py);
    case ICON_FOLDER:
        return sd_folder(p, px, py);
    case ICON_WIFI:
        return sd_wifi(p, px, py);
    default: /* ICON_GEAR */
        return fmaxf(sd_poly(p->gear, GEAR_PTS, px, py), -(len2(px, py) - 0.34f * p->r));
    }
}

/* --- the logo of the boot screen ---
 *
 * The outline of the PlayStation logo, as three closed shapes (the P with its stem, the left swoosh, the right swoosh) whose corners are in
 * half pixels of a 2000 x 1544 picture of it (4000 x 3088 units). The corners were traced from a reference picture of the logo (along the edge
 * where it is half transparent, simplified until no corner is more than one pixel of that picture off; tools/trace_logo.py does it): 142 points,
 * about 570 bytes, where the picture itself would be hundreds of kilobytes. Every corner is on the edge of the logo and the edge between two corners is straight,
 * so at the size it is drawn (about 90 px wide) the outline is smooth to the eye. No shape touches another, and each one goes round the same way. */
#define LOGO_UNITS_W 4000
#define LOGO_UNITS_H 3088
#define LOGO_SHAPES 3
static const unsigned char logo_len[LOGO_SHAPES] = { 49, 45, 48 };
static const unsigned short logo_pts[] = {
    1498, 1, 1615, 24, 1835, 78, 2203, 184, 2505, 286, 2621, 336,
    2717, 386, 2803, 440, 2875, 496, 2930, 549, 2980, 609, 3024, 677,
    3050, 727, 3084, 817, 3106, 903, 3124, 1017, 3132, 1121, 3132, 1247,
    3126, 1331, 3114, 1407, 3090, 1493, 3062, 1561, 3024, 1627, 2984, 1677,
    2947, 1712, 2923, 1730, 2871, 1760, 2809, 1782, 2741, 1794, 2649, 1794,
    2575, 1782, 2497, 1760, 2415, 1726, 2406, 1717, 2406, 711, 2402, 671,
    2392, 627, 2382, 599, 2358, 559, 2321, 526, 2277, 508, 2241, 504,
    2217, 510, 2189, 531, 2168, 561, 2154, 601, 2148, 649, 2147, 3088,
    1498, 2881, 1361, 1745, 1368, 1745, 1368, 2119, 1365, 2122, 689, 2364,
    621, 2400, 598, 2427, 594, 2447, 600, 2461, 629, 2484, 655, 2496,
    693, 2506, 769, 2514, 847, 2514, 895, 2510, 967, 2498, 1023, 2484,
    1367, 2360, 1368, 2695, 1365, 2698, 1287, 2712, 1195, 2724, 1013, 2736,
    923, 2736, 781, 2728, 677, 2716, 569, 2698, 477, 2678, 363, 2648,
    233, 2606, 193, 2590, 131, 2558, 87, 2528, 42, 2485, 20, 2453,
    8, 2427, 0, 2395, 0, 2361, 16, 2313, 38, 2281, 65, 2255,
    111, 2222, 179, 2182, 279, 2134, 377, 2094, 2942, 1907, 3067, 1906,
    3147, 1910, 3257, 1920, 3381, 1938, 3479, 1958, 3581, 1984, 3663, 2010,
    3803, 2062, 3859, 2090, 3903, 2118, 3935, 2144, 3958, 2167, 3976, 2191,
    3992, 2223, 4000, 2255, 3998, 2297, 3978, 2345, 3964, 2365, 3939, 2392,
    3881, 2438, 3819, 2476, 3737, 2516, 3655, 2548, 2273, 3043, 2273, 2666,
    3323, 2292, 3381, 2264, 3393, 2256, 3416, 2231, 3422, 2217, 3422, 2207,
    3414, 2191, 3383, 2168, 3341, 2152, 3293, 2144, 3239, 2140, 3147, 2142,
    3069, 2152, 3009, 2166, 2947, 2186, 2273, 2424, 2273, 2032, 2345, 2008,
    2429, 1984, 2611, 1944, 2779, 1918, 2867, 1910,
};

int icon_logo_width(int h)
{
    return (int)((float)h * LOGO_UNITS_W / LOGO_UNITS_H + 0.5f);
}

int icon_logo_outline(int i, float x, float y, float w, float h, float *xy)
{
    if (i < 0 || i >= LOGO_SHAPES)
        return 0;
    const unsigned short *p = logo_pts;
    for (int k = 0; k < i; k++)
        p += 2 * logo_len[k];
    for (int k = 0; k < logo_len[i]; k++) {
        xy[2 * k] = x + p[2 * k] * w / LOGO_UNITS_W;
        xy[2 * k + 1] = y + p[2 * k + 1] * h / LOGO_UNITS_H;
    }
    return logo_len[i];
}

/* One pixel row of the coverage, by the exact area of each pixel inside the outline: every edge that crosses the row adds how much of a pixel
 * is covered to the right of it to `acc` (the method of font-rs), and the running sum along the row is the coverage. `acc` has tw + 2 entries.
 * (x0, y0) - (x1, y1) is in pixels of the image; the row is y .. y + 1. */
static void logo_edge(float *acc, int tw, int y, float x0, float y0, float x1, float y1)
{
    if (y0 == y1)
        return;
    float dir = 1;
    if (y0 > y1) {
        float t;
        dir = -1;
        t = x0, x0 = x1, x1 = t;
        t = y0, y0 = y1, y1 = t;
    }
    if (y1 <= (float)y || y0 >= (float)(y + 1))
        return;
    float dxdy = (x1 - x0) / (y1 - y0);
    float ya = fmaxf(y0, (float)y), yb = fminf(y1, (float)(y + 1));
    float xa = clampf(x0 + (ya - y0) * dxdy, 0, (float)tw), xb = clampf(x0 + (yb - y0) * dxdy, 0, (float)tw);
    float d = (yb - ya) * dir;
    float xl = fminf(xa, xb), xr = fmaxf(xa, xb);
    float xlf = floorf(xl);
    int il = (int)xlf, ir = (int)ceilf(xr);
    if (ir <= il + 1) { /* inside one pixel (or on the line between two) */
        float xm = 0.5f * (xa + xb) - xlf;
        acc[il] += d - d * xm;
        acc[il + 1] += d * xm;
        return;
    }
    float s = 1.0f / (xr - xl);
    float fl = xl - xlf;
    float a0 = 0.5f * s * (1 - fl) * (1 - fl);
    float fr = xr - (float)ir + 1;
    float am = 0.5f * s * fr * fr;
    acc[il] += d * a0;
    if (ir == il + 2) {
        acc[il + 1] += d * (1 - a0 - am);
    } else {
        float a1 = s * (1.5f - fl);
        acc[il + 1] += d * (a1 - a0);
        for (int x = il + 2; x < ir - 1; x++)
            acc[x] += d * s;
        float a2 = a1 + (float)(ir - il - 3) * s;
        acc[ir - 1] += d * (1 - a2 - am);
    }
    acc[ir] += d * am;
}

static void logo_render(float w, float h, float cx, float cy, int tw, int th, unsigned char *out)
{
    memset(out, 0, (size_t)tw * th);
    if (tw > ICON_LOGO_MAX_W)
        return;
    float acc[ICON_LOGO_MAX_W + 2];
    float px[LOGO_SHAPES][2 * ICON_LOGO_MAX_PTS];
    int n[LOGO_SHAPES];
    for (int i = 0; i < LOGO_SHAPES; i++)
        n[i] = icon_logo_outline(i, cx - w / 2, cy - h / 2, w, h, px[i]);
    for (int y = 0; y < th; y++) {
        memset(acc, 0, sizeof(float) * (tw + 2));
        for (int i = 0; i < LOGO_SHAPES; i++)
            for (int k = 0, j = n[i] - 1; k < n[i]; j = k++)
                logo_edge(acc, tw, y, px[i][2 * j], px[i][2 * j + 1], px[i][2 * k], px[i][2 * k + 1]);
        float sum = 0;
        for (int x = 0; x < tw; x++) {
            sum += acc[x];
            out[y * tw + x] = (unsigned char)(clampf(fabsf(sum), 0, 1) * 255.0f + 0.5f);
        }
    }
}

void icon_render(int kind, float w, float h, float r, float cx, float cy, int tw, int th, unsigned char *out)
{
    if (kind == ICON_LOGO) {
        logo_render(w, h, cx, cy, tw, th, out);
        return;
    }
    Shape sh = { .kind = kind, .w = w, .h = h, .r = r };
    sh.sw = fmaxf(1.5f, 0.23f * r); /* line width of the symbols */
    sh.s = 0.45f * r;               /* half size of the symbols */
    /* Horizontal and vertical edges are moved onto whole pixels (by at most half a pixel), so that
     * the square and the base of the triangle are crisp; only the slanted lines and curves need
     * smoothing. The distances are measured from the centre, which is at (cx, cy) in the image. */
    float fx = cx - floorf(cx), fy = cy - floorf(cy);
    sh.sq = sh.s; /* half size of the square, outside edge */
    if (fabsf(fx - fy) < 0.01f) /* the same on both axes, otherwise it could not stay a square */
        sh.sq = floorf(sh.s - fx + 0.5f) + fx;
    float base = floorf(cy + 0.8f * sh.s + sh.sw / 2 + 0.5f) - cy - sh.sw / 2; /* the base of the triangle: outer edge on a pixel line */
    const float tri[6] = { 0, -(sh.s + 0.5f), -(sh.s + 0.5f), base, sh.s + 0.5f, base };
    memcpy(sh.tri, tri, sizeof(tri));
    if (kind == ICON_GEAR)
        gear_polygon(r, sh.gear);
    if (kind == ICON_BOLT)
        bolt_polygon(r, sh.bolt);
    if (kind == ICON_NOTE)
        note_shape(2 * r, &sh);
    if (kind == ICON_SPEAKER)
        speaker_shape(r / 4.5f, &sh);
    if (kind == ICON_LIBRARY)
        library_geo((int)(h + 0.5f), &sh.lib);
    if (kind == ICON_FOLDER) {
        int tab_w, tab_h, rad, rl, rr;
        folder_dims((int)(w + 0.5f), (int)(h + 0.5f), &tab_w, &tab_h, &rad, &rl, &rr);
        sh.fold[0] = (float)tab_w;
        sh.fold[1] = (float)tab_h;
        sh.fold[2] = (float)rad;
        sh.fold[3] = (float)rl;
        sh.fold[4] = (float)rr;
    }
    if (kind == ICON_TRI_R || kind == ICON_TRI_L) { /* the tip is in the middle of the far side */
        float d = kind == ICON_TRI_R ? 1.0f : -1.0f;
        const float t[6] = { -d * w / 2, -h / 2, d * w / 2, 0, -d * w / 2, h / 2 };
        memcpy(sh.tri, t, sizeof(t));
    }

    /* The coverage of a pixel follows from the distance of its centre to the edge (a one pixel wide ramp),
     * which is exact for straight edges and good for curves that are not too tight. Small round shapes
     * (a few pixels across) are tighter than that, so they are sampled 4 x 4 times per pixel instead.
     * So are the bolt, the triangles, the note, the speaker and the Wi-Fi sign, whose sharp tips and thin stems and arcs are tighter still. */
    float feature = kind == ICON_DISC ? r
                  : kind == ICON_RRECT || kind == ICON_RRING || kind == ICON_RFRAME || kind == ICON_RFRAME_S
                      ? fminf(r, fminf(w, h) / 2) : 99;
    int fine = kind == ICON_BOLT || kind == ICON_TRI_R || kind == ICON_TRI_L || kind == ICON_NOTE || kind == ICON_SPEAKER || kind == ICON_WIFI;
    if (kind == ICON_LIBRARY && h < 32)
        fine = 1; /* its corners are a pixel or two across at the small size */
    if (kind == ICON_FOLDER && h < 24)
        fine = 1; /* the same for the folder */
    int ss = feature < 2.5f || fine ? 4 : 1;
    for (int y = 0; y < th; y++) {
        for (int x = 0; x < tw; x++) {
            float cov;
            if (ss == 1) {
                cov = clampf(0.5f - shape_distance(&sh, x + 0.5f - cx, y + 0.5f - cy), 0, 1);
            } else {
                cov = 0;
                for (int j = 0; j < ss; j++)
                    for (int i = 0; i < ss; i++) /* each sample covers 1/4 pixel: a ramp 1/4 pixel wide */
                        cov += clampf(0.5f - 4 * shape_distance(&sh, x + (i + 0.5f) / ss - cx, y + (j + 0.5f) / ss - cy), 0, 1);
                cov /= ss * ss;
            }
            out[y * tw + x] = (unsigned char)(cov * 255.0f + 0.5f);
        }
    }
}

int icon_nine_k(float r)
{
    return (int)ceilf(r) + 2; /* the corner arc ends at 1 + r, and the pixel after it must be straight too */
}

/* Cuts one direction (n pixels) into at most three parts: the start, the stretched middle, the end.
 * The parts of the start and the end are as wide as the image allows (k), but never more than half
 * of what there is, so that they do not overlap in a small rectangle. */
static void nine_axis(int n, int k, float *a0, float *a1, float *b0, float *b1)
{
    int d = 2 * k + 1;
    int total = n + 2; /* with the transparent pixel on each side */
    int first = total / 2 < k ? total / 2 : k;
    int last = total - first < k ? total - first : k;
    a0[0] = 0, a1[0] = (float)first, b0[0] = 0, b1[0] = (float)first;
    a0[1] = (float)first, a1[1] = (float)(total - last), b0[1] = (float)k, b1[1] = (float)(k + 1);
    a0[2] = (float)(total - last), a1[2] = (float)total, b0[2] = (float)(d - last), b1[2] = (float)d;
}

int icon_nine(int w, int h, int k, IconCell *cells)
{
    float xa0[3], xa1[3], xb0[3], xb1[3], ya0[3], ya1[3], yb0[3], yb1[3];
    nine_axis(w, k, xa0, xa1, xb0, xb1);
    nine_axis(h, k, ya0, ya1, yb0, yb1);
    int n = 0;
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            if (xa1[col] <= xa0[col] || ya1[row] <= ya0[row])
                continue; /* the middle part of a small rectangle has no size */
            cells[n++] = (IconCell){ xa0[col], ya0[row], xa1[col], ya1[row], xb0[col], yb0[row], xb1[col], yb1[row] };
        }
    }
    return n;
}
