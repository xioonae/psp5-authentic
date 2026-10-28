/* psp5 – made by Mille Weber */
/* The information screen (see info.h). It reads the files when it is opened, so nothing is stored in the
 * game list for it, and it never writes anything. The size of an EBOOT game's whole folder takes a while
 * to count on a big folder, so that row says "..." until the count (gameops.c) is ready. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "info.h"
#include "gameops.h"
#include "iso.h"
#include "lang.h"
#include "lastplayed.h"
#include "loader.h"
#include "osd.h"
#include "sfo.h"
#include "sound.h"
#include "system.h"
#include "theme.h"
#include "umd.h"

/* the same layout as the settings */
#define TITLE_Y     34.0f
#define LEFT_X      24.0f  /* the same sides as the lists of the file manager and Manage Saved Data: 24 px from the edges of the screen */
#define LEFT_W      136.0f
#define RIGHT_X     172.0f /* 12 px after the left column */
#define RIGHT_W     284.0f /* to 456 */
#define ROW_H       26.0f
#define LIST_TOP    58.0f
#define LIST_BOTTOM 236.0f

#define MAX_ROWS    80
#define MAX_SFO     (64 * 1024)
#define PBP_MAGIC   0x50425000
#define PBP_SECTIONS 8

#define TEXT_MAIN RGBA(238, 238, 242, 255)
#define TEXT_DIM  RGBA(160, 162, 172, 255)

typedef struct {
    char label[48];
    char value[160];
    char shown[176]; /* the value as it is drawn (cut to fit) */
    float ls, vs;    /* text sizes of label and value */
    u8 header;       /* a caption, not a row you can select */
    u8 left_cut;     /* a path: when it is too long the start is cut, not the end */
    u8 pending;      /* the value is not known yet (folder count) */
} Row;

static Row s_rows[MAX_ROWS];
static int s_n;
static struct {
    float open, time, scroll;
    int sel;
    int index;            /* in the game list */
    int measure_row;      /* row waiting for the folder count, or -1 */
    int repeat_dir;
    float repeat_timer;
    Entry e;
} S;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static u32 fade(u32 c, float a) { return COLOR_ALPHA(c, (int)(((c >> 24) & 0xFF) * clampf(a, 0, 1))); }
static float approach(float cur, float target, float speed, float dt)
{
    float t = 1.0f - expf(-speed * dt);
    return cur + (target - cur) * t;
}

/* ---------------------------------------------------------------------- */
/* collecting the rows */

static Row *add_row(const char *label, const char *value)
{
    if (s_n >= MAX_ROWS)
        return NULL;
    Row *r = &s_rows[s_n++];
    memset(r, 0, sizeof(*r));
    snprintf(r->label, sizeof(r->label), "%s", label);
    snprintf(r->value, sizeof(r->value), "%s", value);
    return r;
}

static void add_header(const char *text)
{
    Row *r = add_row(text, "");
    if (r)
        r->header = 1;
}

static void add_rowf(const char *label, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
static void add_rowf(const char *label, const char *fmt, ...)
{
    char value[160];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(value, sizeof(value), fmt, ap);
    va_end(ap);
    add_row(label, value);
}

static u32 le32(const u8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24); }

/* n bytes from `offset` in a file; the number read, or <0 */
static int read_at(const char *path, u32 offset, void *buf, u32 n)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return -1;
    sceIoLseek32(fd, offset, PSP_SEEK_SET);
    int got = sceIoRead(fd, buf, n);
    sceIoClose(fd);
    return got;
}

static int dev_space(const char *path, u64 *free_b, u64 *total_b)
{
    char dev[8];
    snprintf(dev, sizeof(dev), "%.4s", path);
    struct { u32 max_clusters, free_clusters, max_sectors, sector_size, sectors_per_cluster; } info;
    void *p = &info;
    if (sceIoDevctl(dev, 0x02425818, &p, sizeof(p), NULL, 0) < 0)
        return 0;
    u64 cluster = (u64)info.sector_size * info.sectors_per_cluster;
    *free_b = (u64)info.free_clusters * cluster;
    *total_b = (u64)info.max_clusters * cluster;
    return 1;
}

/* Where it is: storage with free space, then the folder and the file name */
static void location_rows(const Entry *e)
{
    int internal = !strncasecmp(e->path, "ef0:", 4) || (!strncasecmp(e->path, "ms0:", 4) && sys_ms_is_ef());
    const char *name = L(internal ? S_F_INTERNAL : S_F_MS);
    u64 free_b, total_b;
    char value[96];
    if (dev_space(e->path, &free_b, &total_b)) {
        char num[24], free_text[48];
        gameops_fmt_size(num, sizeof(num), free_b);
        snprintf(free_text, sizeof(free_text), L(S_F_FREE), num);
        snprintf(value, sizeof(value), "%s, %s", name, free_text);
    } else {
        snprintf(value, sizeof(value), "%s", name);
    }
    add_row(L(S_I_LOCATION), value);

    const char *slash = strrchr(e->path, '/');
    if (slash) {
        char folder[160];
        snprintf(folder, sizeof(folder), "%.*s", (int)(slash - e->path), e->path);
        Row *r = add_row(L(S_I_FOLDER), folder);
        if (r)
            r->left_cut = 1;
        add_row(L(S_I_FILE), slash + 1);
    }
}

/* a packed time (scan_pack_time()) as "2026-10-28 14:05"; no row when the time is not known (0) */
static void time_row(const char *label, u32 t)
{
    if (!t)
        return;
    add_rowf(label, "%04u-%02u-%02u %02u:%02u", (unsigned)((t >> 26) + 1980), (unsigned)((t >> 22) & 15),
             (unsigned)((t >> 17) & 31), (unsigned)((t >> 12) & 31), (unsigned)((t >> 6) & 63));
}

static void modified_row(const Entry *e)
{
    time_row(L(S_I_MODIFIED), e->file_time);
}

/* when psp5 (or the helper plugin) saw the game played last (lastplayed.c); nothing for a game that has no time (a disc without a disc ID has none) */
static void last_played_row(const Entry *e)
{
    time_row(L(S_I_LAST_PLAYED), lastplayed_time(e));
}

/* The few PARAM.SFO entries worth knowing, in this order, each with its own key as the label */
static const char *const SFO_KEYS[] = { "DISC_ID", "DISC_VERSION", "APP_VER", "PSP_SYSTEM_VER" };

static void sfo_rows(const u8 *sfo, u32 size)
{
    int n = sfo_count(sfo, size);
    int header_done = 0;
    for (unsigned k = 0; k < sizeof(SFO_KEYS) / sizeof(SFO_KEYS[0]); k++) {
        for (int i = 0; i < n; i++) {
            char key[48], val[160];
            if (!sfo_entry(sfo, size, i, key, sizeof(key), val, sizeof(val)) || strcmp(key, SFO_KEYS[k]))
                continue;
            if (!header_done) {
                add_header("PARAM.SFO");
                header_done = 1;
            }
            add_row(key, val);
            break;
        }
    }
}

/* ISO / CSO / ZSO / DAX / JSO */
static void iso_rows(const Entry *e)
{
    u8 head[24];
    memset(head, 0, sizeof(head));
    read_at(e->path, 0, head, sizeof(head));
    u32 magic = le32(head);
    int packed = magic == CSO_MAGIC || magic == ZSO_MAGIC || magic == DAX_MAGIC || magic == JSO_MAGIC;
    char buf[96], num[24];
    snprintf(buf, sizeof(buf), L(S_I_IMAGE),
             magic == CSO_MAGIC ? "CSO" : magic == ZSO_MAGIC ? "ZSO" : magic == DAX_MAGIC ? "DAX" : magic == JSO_MAGIC ? "JSO" : "ISO");
    add_row(L(S_I_TYPE), buf);
    location_rows(e);

    gameops_fmt_size(num, sizeof(num), e->file_size);
    add_row(L(S_I_SIZE), num);
    if (packed) { /* the header has the size the image has when unpacked: 64 bits at 8 in a CSO/ZSO, 32 bits at 4 in a DAX, 32 bits at 12 in a JSO */
        u64 total = magic == DAX_MAGIC ? le32(head + 4) : magic == JSO_MAGIC ? le32(head + 12) : (u64)le32(head + 8) | ((u64)le32(head + 12) << 32);
        if (total) {
            gameops_fmt_size(num, sizeof(num), total);
            add_row(L(S_I_UNCOMPRESSED), num);
            add_rowf(L(S_I_COMPRESSION), "%u %%", (unsigned)((u64)e->file_size * 100 / total));
        }
    }
    modified_row(e);
    last_played_row(e);

    IsoReader r;
    if (iso_open(&r, e->path) < 0)
        return;
    static u8 pvd[2048]; /* the volume descriptor (static: keeps the stack small) */
    if (iso_read(&r, pvd, sizeof(pvd), 16 * 2048) == (int)sizeof(pvd) && !memcmp(pvd + 1, "CD001", 5)) {
        char vol[40];
        snprintf(vol, sizeof(vol), "%.32s", (const char *)pvd + 40);
        for (int i = strlen(vol); i > 0 && vol[i - 1] == ' '; i--)
            vol[i - 1] = 0;
        if (vol[0])
            add_row(L(S_I_VOLUME), vol);
    }

    u32 sfo_size;
    u8 *sfo = iso_load(&r, "PSP_GAME/PARAM.SFO", &sfo_size, MAX_SFO);
    if (sfo) {
        sfo_rows(sfo, sfo_size);
        free(sfo);
    }
    iso_close(&r);
}

/* EBOOT.PBP: homebrew, PSN games, PS1 games */
static void pbp_rows(const Entry *e)
{
    add_row(L(S_I_TYPE), L(e->type == ENTRY_PSN ? S_I_PSN : e->type == ENTRY_POPS ? S_I_POPS : S_I_HOMEBREW));
    location_rows(e);

    SceIoStat st;
    u32 total = e->file_size;
    if (sceIoGetstat(e->path, &st) >= 0)
        total = (u32)st.st_size;
    char num[24];
    gameops_fmt_size(num, sizeof(num), total);
    add_row(L(S_I_SIZE), num);
    Row *r = add_row(L(S_I_CONTENTS), "...");
    if (r) {
        r->pending = 1;
        S.measure_row = s_n - 1;
    }
    modified_row(e);
    last_played_row(e);

    u32 h[2 + PBP_SECTIONS];
    memset(h, 0, sizeof(h));
    if (read_at(e->path, 0, h, 4 * (2 + PBP_SECTIONS)) != (int)(4 * (2 + PBP_SECTIONS)) || h[0] != PBP_MAGIC)
        return;
    u32 sfo_size = h[3] - h[2];
    if (h[3] > h[2] && sfo_size <= MAX_SFO && h[3] <= total) { /* section 0 is the PARAM.SFO */
        u8 *sfo = malloc(sfo_size);
        if (sfo) {
            if (read_at(e->path, h[2], sfo, sfo_size) == (int)sfo_size)
                sfo_rows(sfo, sfo_size);
            free(sfo);
        }
    }
}

/* the disc in the UMD drive */
static void umd_rows(const Entry *e)
{
    add_row(L(S_I_TYPE), L(S_I_UMD));
    add_row(L(S_I_LOCATION), L(S_I_UMD_DRIVE));
    last_played_row(e);
    SceIoStat st;
    if (sceIoGetstat(UMD_ROOT "PSP_GAME/PARAM.SFO", &st) >= 0 && st.st_size > 0 && st.st_size <= MAX_SFO) {
        u8 *sfo = malloc(st.st_size);
        if (sfo) {
            if (read_at(UMD_ROOT "PSP_GAME/PARAM.SFO", 0, sfo, st.st_size) == (int)st.st_size)
                sfo_rows(sfo, st.st_size);
            free(sfo);
        }
    }
}

/* ---------------------------------------------------------------------- */
/* fitting the text to the rows */

static void layout_row(Row *r)
{
    r->ls = text_fit(0.62f, 0.48f, 0, RIGHT_W * 0.5f, r->label);
    float avail = RIGHT_W - 16 - text_width(r->ls, 0, r->label) - 14;
    if (avail < 40)
        avail = 40;
    r->vs = text_fit(0.58f, 0.46f, 0, avail, r->value);
    snprintf(r->shown, sizeof(r->shown), "%s", r->value);
    if (text_width(r->vs, 0, r->value) <= avail)
        return;
    const char *p = r->value;
    if (r->left_cut) { /* "...PSP/GAME/Name": the end of a path is the part that matters */
        while (text_width(r->vs, 0, p) > avail && strchr(p + 1, '/'))
            p = strchr(p + 1, '/');
        if (p != r->value)
            snprintf(r->shown, sizeof(r->shown), "...%s", p);
    }
    if (text_width(r->vs, 0, r->shown) > avail)
        text_ellipsis(r->shown, sizeof(r->shown), r->shown, r->vs, avail);
}

/* ---------------------------------------------------------------------- */
/* opening, updating */

void info_open(const Entry *e, int list_index)
{
    memset(&S, 0, sizeof(S));
    s_n = 0;
    S.e = *e;
    S.index = list_index;
    S.measure_row = -1;

    if (e->type == ENTRY_ISO)
        iso_rows(e);
    else if (e->type == ENTRY_UMD)
        umd_rows(e);
    else
        pbp_rows(e);

    int cjk = 0;
    for (int i = 0; i < s_n; i++)
        for (const char *p = s_rows[i].value; *p; p++)
            cjk |= (u8)*p >= 0x80;
    for (const char *p = e->title; *p; p++)
        cjk |= (u8)*p >= 0x80;
    if (cjk)
        font_ensure_cjk(); /* e.g. a Japanese title in PARAM.SFO */
    for (int i = 0; i < s_n; i++)
        layout_row(&s_rows[i]);
    while (S.sel < s_n && s_rows[S.sel].header)
        S.sel++;

    if (S.measure_row >= 0) {
        char folder[160];
        const char *slash = strrchr(e->path, '/');
        snprintf(folder, sizeof(folder), "%.*s", slash ? (int)(slash - e->path) : 0, e->path);
        gameops_measure_start(folder);
    }
}

void info_close(void)
{
    gameops_measure_stop();
}

static int vertical(const UiInput *in, float dt)
{
    int dir = ((in->held & PSP_CTRL_UP) || in->ly < 40) ? -1 : ((in->held & PSP_CTRL_DOWN) || in->ly > 215) ? 1 : 0;
    if (!dir) {
        S.repeat_dir = 0;
        return 0;
    }
    if (dir != S.repeat_dir) {
        S.repeat_dir = dir;
        S.repeat_timer = 0.32f;
        return dir;
    }
    S.repeat_timer -= dt;
    if (S.repeat_timer > 0)
        return 0;
    S.repeat_timer += 0.07f;
    return dir;
}

int info_update(const UiInput *in, float dt)
{
    S.open = clampf(S.open + dt * 5.0f, 0, 1);
    S.time += dt;

    if (S.measure_row >= 0) { /* the folder count */
        unsigned long long bytes;
        unsigned files;
        if (gameops_measure_poll(&bytes, &files)) {
            char num[24], text[160];
            Row *r = &s_rows[S.measure_row];
            gameops_fmt_size(num, sizeof(num), bytes);
            snprintf(text, sizeof(text), L(S_I_SIZE_FILES), num, files);
            snprintf(r->value, sizeof(r->value), "%s", text);
            r->pending = 0;
            layout_row(r);
            S.measure_row = -1;
        }
    }

    int move = vertical(in, dt);
    if (move) {
        int next = S.sel + move;
        while (next >= 0 && next < s_n && s_rows[next].header)
            next += move; /* the captions can't be selected */
        if (next >= 0 && next < s_n && next != S.sel) {
            S.sel = next;
            sound_play(SND_MOVE);
        }
    }
    if (in->pressed & PSP_CTRL_CIRCLE) {
        sound_play(SND_BACK);
        return 1;
    }

    /* keep the selected row visible, as in the settings */
    float target = 0, y = S.sel * ROW_H, visible = LIST_BOTTOM - LIST_TOP - ROW_H;
    if (y > visible)
        target = y - visible;
    S.scroll = approach(S.scroll, target, 14.0f, dt);
    return 0;
}

/* ---------------------------------------------------------------------- */
/* drawing */

static void draw_left(float a, float slide)
{
    float x = LEFT_X + slide, y = LIST_TOP, w = LEFT_W, h = floorf(w * 80 / 144 + 0.5f); /* (the shape of a game's ICON0: 144 x 80) */
    IconSlot *slot = loader_icon(S.index);
    gfx_glow(x + 1, y + 3, w - 2, h - 2, 5, RGBA(0, 0, 0, (int)(130 * a)));
    if (slot && slot->state == SLOT_READY && slot->tex)
        gfx_tex(slot->tex, x, y, w, h, RGBA(255, 255, 255, (int)(255 * a)));
    else
        gfx_round_rect(x, y, w, h, w / 12, fade(theme_cur()->card, a));

    char lines[3][96];
    int n = text_wrap(S.e.title, 0.6f, w, lines, 3);
    float ty = y + h + 24;
    for (int i = 0; i < n; i++)
        text_draw(x, ty + i * 15, 0.6f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, lines[i]);
    if (S.e.type != ENTRY_HOMEBREW && S.e.game_id[0])
        text_draw(x, ty + n * 15 + 6, 0.56f, 0, fade(TEXT_DIM, a), ALIGN_LEFT, S.e.game_id);
}

static void draw_row(const Row *r, float y, int selected, int last, int next_selected, float a)
{
    if (r->header) {
        text_draw(RIGHT_X + 8, y + 17, 0.52f, 0, fade(TEXT_DIM, a * 0.8f), ALIGN_LEFT, r->label);
        return;
    }
    if (selected) {
        gfx_rect(RIGHT_X, y, RIGHT_W, ROW_H, RGBA(255, 255, 255, (int)(16 * a)));
        gfx_frame(RIGHT_X, y, RIGHT_W, ROW_H, 1, fade(RGBA(232, 232, 236, 230), a));
    } else if (!last && !next_selected) {
        gfx_rect(RIGHT_X + 4, y + ROW_H - 1, RIGHT_W - 8, 1, RGBA(255, 255, 255, (int)(26 * a)));
    }
    text_draw(RIGHT_X + 8, y + 17, r->ls, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, r->label);
    text_draw(RIGHT_X + RIGHT_W - 8, y + 17, r->vs, 0, fade(TEXT_DIM, a), ALIGN_RIGHT, r->shown);
}

void info_draw(void)
{
    float a = S.open, slide = (1 - a) * 10;
    gfx_begin(RGBA(20, 20, 24, 255));
    theme_backdrop(theme_cur(), S.time, 1);
    text_draw(LEFT_X + 4 + slide, TITLE_Y, 0.95f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, L(S_INFO));
    draw_left(a, slide);

    float top = LIST_TOP - S.scroll;
    for (int i = 0; i < s_n; i++) {
        float y = top + i * ROW_H;
        if (y < LIST_TOP - ROW_H || y > LIST_BOTTOM)
            continue;
        /* rows sliding out of the list fade away */
        float edge = clampf((y - (LIST_TOP - ROW_H * 0.6f)) / (ROW_H * 0.6f), 0, 1) *
                     clampf((LIST_BOTTOM - y) / (ROW_H * 0.6f), 0, 1);
        draw_row(&s_rows[i], y, i == S.sel, i == s_n - 1, i + 1 == S.sel, a * edge);
    }
    osd_draw();
    if (a < 1)
        gfx_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(255 * (1 - a))));
    gfx_end();
}
