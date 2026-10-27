#ifndef ICONS_H
#define ICONS_H

/* Smooth icon shapes: the button symbols, the cog and rounded rectangles of the small key caps.
 *
 * They are rendered on the CPU into a small alpha image of exactly the size they are drawn at,
 * with the edges anti-aliased from the real geometry, so they are as smooth as the glyphs of the
 * font. gfx.c keeps the images in a cache and draws them pixel for pixel, tinted with the colour.
 * (Shrinking a big round texture, as gfx_circle does, only looks at a few of its pixels and gives
 * jagged edges at this size.) */

enum {
    ICON_DISC,     /* filled circle, radius r */
    ICON_RRECT,    /* rounded rectangle w * h with corner radius r */
    /* the symbols below are drawn inside a button of radius r, in the same size of image as ICON_DISC */
    ICON_CIRCLE,   /* ring */
    ICON_CROSS,
    ICON_TRIANGLE,
    ICON_SQUARE,   /* outline of a square */
    ICON_GEAR,     /* cog with a hole, outer radius r */
    ICON_RRING,    /* rounded rectangle w * h, only a thin ring just inside the edge (0.19 r thick) */
    ICON_BOLT,     /* the charging bolt, 2 r high (about r wide) */
    ICON_TRI_R,    /* triangle w * h pointing right: the back edge on the left, the tip in the middle of the right */
    ICON_TRI_L,    /* the same pointing left */
    ICON_NOTE,     /* the music note (two notes with a beam) in a square of side 2 r, all in one piece */
    ICON_SPEAKER,  /* the loudspeaker: box and cone in one piece, 2 r wide and 4 r high (r = 4.5 for the usual size) */
    ICON_RFRAME,   /* rounded rectangle w * h, a ring of the fixed thickness ICON_FRAME_T just inside the edge */
    ICON_RFRAME_S, /* the same with the thinner ICON_FRAME_T_S - the ring around the selected card of the library grid */
    ICON_LIBRARY,  /* the picture of the Library card: 3 x 2 squares with a game controller in front, w * h whole pixels (see icon_library_width) */
    ICON_FOLDER,   /* the picture of a folder card: a tab and a body in one piece, w * h whole pixels (see icon_folder_pic), every edge on a pixel line */
    ICON_WIFI,     /* the Wi-Fi sign: a small square with three quarter circle arcs round it, soft edged, in a square of side 2 r (r = 7 for the usual size) */
    ICON_LOGO      /* the logo of the boot screen: three solid shapes drawn from their outline, w * h pixels (see icon_logo_width), in one image */
};

#define ICON_FRAME_T   2.5f /* thickness of ICON_RFRAME in pixels (about what the big card's ring always had) */
#define ICON_FRAME_T_S 2.0f /* thickness of ICON_RFRAME_S in pixels (about what the small grid cards' ring always had) */

/* Where an icon goes: the size of its image, the whole-pixel position of the image's top left
 * corner on the screen, and where the centre of the shape is inside the image. The centre is
 * snapped to half a pixel, so only a few different images are ever needed (`phase_x`/`phase_y`,
 * 0 or 1, tell which, for the cache). */
typedef struct {
    int tw, th;
    int x, y;
    float cx, cy;
    int phase_x, phase_y;
} IconBox;

/* The Library picture is `h` pixels high; this is how wide it is. Everything in it is on whole pixels (the squares all the same
 * size and the same distance apart, the controller exactly the same on its left and right), so the picture is made for this one
 * size: the caller asks for it with the whole-pixel w, h and places its top left corner on a whole pixel. */
int icon_library_width(int h);

/* The Library picture of height `h` as a coverage image (w * h bytes, w = icon_library_width(h)). The heights the Library card
 * goes through are stored ready-made (libicon_data.inc, made by tools/gen_library_icons.py) because calculating one takes far
 * too long while the card is growing; any other height is calculated (icon_render with ICON_LIBRARY gives the same image). */
void icon_library_image(int h, unsigned char *out);
/* 1 if the picture of height `h` is stored ready-made (for the host tests: the card's heights must all be) */
int icon_library_baked(int h);

/* The picture of a folder card: a folder (a tab at the top left on a body) that is made for one whole-pixel size, like the Library
 * picture. For the card whose pixels are l, t, w, h (whole numbers) this says where the picture goes and how it is built, and
 * icon_render(ICON_FOLDER, p->w, p->h, 0, ...) gives its image. The folder is about two thirds of the card's height. */
typedef struct {
    int x, y, w, h;   /* the picture, in screen pixels */
    int tab_w;        /* the tab: as wide as this, from the left edge */
    int tab_h;        /* the body's top edge is this far below the picture's top edge (the tab is this high) */
    int r;            /* radius of the body's corners (the bottom two and the top right one) */
    int tab_rl, tab_rr; /* radius of the tab's top left corner (it is also the corner of the whole folder) and of its top right corner */
} FolderPic;
void icon_folder_pic(int l, int t, int w, int h, FolderPic *p);

/* The logo of the boot screen (the PlayStation logo, drawn from the outline of its three shapes: the P with its stem and the two swooshes, no
 * picture) is `h` pixels high; this is how wide it is. icon_render(ICON_LOGO, w, h, 0, cx, cy, tw, th, out) gives its image with w = icon_logo_width(h)
 * and the middle of the logo at (cx, cy): the edges are anti-aliased from the exact area each pixel has inside the outline. Images up to
 * ICON_LOGO_MAX_W wide can be made (a wider one comes out empty). */
#define ICON_LOGO_MAX_W 128
int icon_logo_width(int h);

/* The corners of the logo's shapes, for drawing the outline when there is no room for the image: shape `i` (0 to 2) is put at xy[2n] in the box
 * of w * h pixels whose top left corner is (x, y); returns n (at most ICON_LOGO_MAX_PTS), or 0 when there is no shape i. */
#define ICON_LOGO_MAX_PTS 64
int icon_logo_outline(int i, float x, float y, float w, float h, float *xy);

/* The box for a shape centred on (cx, cy) on the screen. `w`, `h` are only used by ICON_RRECT and
 * the triangles (where they are the size of the triangle). */
void icon_box(int kind, float cx, float cy, float w, float h, float r, IconBox *b);

/* Writes the coverage (0 = outside, 255 = inside) of the shape for every pixel of a tw * th image
 * to out[tw * th]. The centre of the shape is at (cx, cy) measured in pixels from the top left
 * corner of the image. `w` and `h` are only used by ICON_RRECT and the triangles. */
void icon_render(int kind, float w, float h, float r, float cx, float cy, int tw, int th, unsigned char *out);

/* --- rounded rectangles of any size from one small image ---
 * The image has the shape of a square rounded rectangle with a transparent pixel around it and one
 * pixel column and row in the middle that can be stretched. It is cut into up to 9 cells, the four
 * corners drawn pixel for pixel and the rest stretched, so a rectangle w * h on whole pixels comes
 * out exactly as if it had been rendered as a whole. */

#define ICON_NINE_MAX_R 28 /* bigger radii are not worth an image (and use the old drawing) */

/* k: half the size of the image; it is 2k+1 pixels wide and high. */
int icon_nine_k(float r);

/* The image of ICON_RRECT or ICON_RRING for radius r: call icon_render with
 * w = h = 2k-1, the centre at (k+0.5, k+0.5) and tw = th = 2k+1. */

/* One piece: the rectangle x0,y0 - x1,y1 on the screen (relative to the top left corner of the
 * rounded rectangle, one pixel outside it on every side) shows the part u0,v0 - u1,v1 of the image
 * (in pixels). */
typedef struct {
    float x0, y0, x1, y1;
    float u0, v0, u1, v1;
} IconCell;

/* The pieces for a rounded rectangle of w * h whole pixels; returns how many (at most 9). The
 * rectangle itself is x = 1 .. w+1, y = 1 .. h+1 in these coordinates. */
int icon_nine(int w, int h, int k, IconCell *cells);

#endif
