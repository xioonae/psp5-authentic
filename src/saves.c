/* psp5 – made by Mille Weber */
/* Manage Saved Data (see saves.h). Three parts:
 *  - a reading thread: lists PSP/SAVEDATA, then fills in each save (PARAM.SFO, size, which game it belongs to);
 *  - a delete thread with a progress panel that can be cancelled;
 *  - the screen: the list on the left (like the file manager), the details of the highlighted save on the right.
 *
 * Which game a save belongs to: PARAM.SFO of a save has no game id, but the name of the save folder starts with it
 * ("ULUS10041DATA00" belongs to the game ULUS10041). That is tried first. Else the TITLE of the save (the title of the
 * game that wrote it) is compared with the titles of the games and apps, and for homebrew the name of the app's own
 * folder is compared with the name of the save folder. A save that matches none is shown with the TITLE it holds
 * and "(not installed)".
 *
 * Deleting is careful on purpose: deletable() is the only place that decides what may be deleted, and it only says
 * yes to a folder directly in PSP/SAVEDATA - never ARK's own folder (ARK_01234 holds ARK's settings and the
 * VBOOT.PBP that starts the custom firmware). A save loses its PARAM.SFO first, so a delete that is cancelled or fails
 * half way leaves something the XMB no longer shows as a save, instead of a broken one. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "saves.h"
#include "controls.h"
#include "gameops.h"
#include "image.h"
#include "lang.h"
#include "osd.h"
#include "popup.h"
#include "sfo.h"
#include "sound.h"
#include "system.h"
#include "theme.h"

#define SAVE_ROOT  "PSP/SAVEDATA"
#define PATH_LEN   160
#define MAX_SFO    (32 * 1024)
#define MAX_ICON   (256 * 1024)
#define ICON_W     144 /* ICON0.PNG of a save */
#define ICON_H     80

/* the same look as the file manager's list, with the information screen's details on the right */
#define LIST_X   24.0f
#define LIST_W   252.0f
#define LIST_TOP 62.0f
#define ROW_H    24.0f
#define ROW_ICON_X (LIST_X + 8) /* the left edge of a row's circle, check mark or padlock */
#define SORT_X     (LIST_X + 9) /* where the Sort button's text starts; its frame (9 px to the left) and the Sort popup start at LIST_X, like a row's highlight */
#define ROWS     7
#define PANEL_X  292.0f
#define PANEL_W  164.0f
#define PIC_W    128.0f
#define PIC_H    71.0f
#define HINTS_MAX_W 432.0f /* the widest the row of hints may be */

#define TEXT_MAIN RGBA(238, 238, 242, 255)
#define TEXT_DIM  RGBA(160, 162, 172, 255)
#define LIGHT     RGBA(236, 236, 240, 255)
#define DARK      RGBA(20, 20, 26, 255)
#define DANGER    RGBA(255, 128, 118, 255)

typedef struct {
    char dev[6];     /* "ms0:/" or "ef0:/" */
    char dir[64];    /* the folder's name in PSP/SAVEDATA */
    char game[96];   /* the game or app it belongs to: the matching game's title, else the TITLE of PARAM.SFO */
    char title[96];  /* SAVEDATA_TITLE */
    char detail[160]; /* SAVEDATA_DETAIL, its lines joined */
    unsigned long long bytes;
    u32 entries;     /* files, sub folders and the folder itself (the progress of a delete) */
    u32 time;        /* when it was changed, packed like Entry.file_time */
    u8 matched;      /* `game` is the title of a game or app in the list */
    u8 locked;       /* must never be deleted (known as soon as the folder is listed) */
    volatile u8 ready; /* the reading thread is done with this one (set last) */
    /* from here on only the main thread writes */
    u8 marked, laid;
    char row_main[64], row_sub[64], row_size[16];
    float sub_x;
} Save;

/* --- the reading thread --- */
static struct {
    SceUID thread;
    volatile int quit, listed, done;
    Save *volatile items; /* published once, when `listed` is set; the thread then fills in the entries */
    volatile int count;
    const EntryList *games;
} T = { .thread = -1 };

/* --- the delete thread --- */
static struct {
    SceUID thread;
    volatile int running, cancel, result;
    volatile u32 done;
    u32 total;
    char current[64];
    int n;
    char (*paths)[PATH_LEN];
    int *index; /* of each path in the list */
} W = { .thread = -1 };

enum { SORT_DATE, SORT_SIZE, SORT_NAME, SORT_COUNT };

static struct {
    float open, time, scroll;
    int sel, top;
    int sort;
    int sort_focus;       /* the Sort button under the title is selected, not a row (↑ from the first row) */
    float sort_anim;      /* 0..1: how selected it looks */
    int menu, menu_sel;   /* the sorting popup: open, and the way chosen in it */
    float menu_anim;
    int seen;             /* entries checked for characters that need the CJK font */
    int repeat_dir;
    float repeat_timer;
    char toast[96];
    float toast_time;
    unsigned long long free_b[2], total_b[2]; /* ms0:, ef0: */
    /* the question before a delete */
    int ask, ask_sel, ask_n, ask_nl;
    float ask_anim;
    char ask_lines[4][96], ask_size[64];
    /* the details on the right */
    int pan_for, pan_ready, pan_name_n, pan_desc_n;
    char pan_name[2][96], pan_title[96], pan_desc[3][96], pan_date[24], pan_lock[96];
    /* the picture of the highlighted save */
    Texture *pic;
    int pic_sel;          /* the row the timer below counts for, -1 = none yet */
    int pic_loaded;       /* the picture of that row has been looked for (pic is NULL if it has none) */
    float pic_dwell, pic_alpha;
} S;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static u32 fade(u32 c, float a) { return COLOR_ALPHA(c, (int)(((c >> 24) & 0xFF) * clampf(a, 0, 1))); }

/* `text` on one line of at most `max_w` pixels: the largest size from max_size down to min_size that fits, and if
 * even the smallest does not, the end is cut with "...". Returns the size. */
static float fit_line(char *out, int out_size, const char *text, float max_size, float min_size, float max_w)
{
    float size = text_fit(max_size, min_size, 0, max_w, text);
    if (text_width(size, 0, text) > max_w)
        text_ellipsis(out, out_size, text, size, max_w);
    else
        snprintf(out, out_size, "%s", text);
    return size;
}

static void toast(const char *msg)
{
    snprintf(S.toast, sizeof(S.toast), "%s", msg);
    S.toast_time = 2.8f;
}

static void save_path(char *out, int size, const Save *s)
{
    snprintf(out, size, "%s%s/%s", s->dev, SAVE_ROOT, s->dir);
}

static int dev_index(const char *dev) { return dev[0] == 'e'; }

/* ---------------------------------------------------------------------- */
/* what may be deleted, which game a save belongs to */

/* Only a folder directly in PSP/SAVEDATA of the memory card or the internal storage, and never ARK's own. */
static int deletable(const char *dev, const char *dir)
{
    if (strcmp(dev, "ms0:/") && strcmp(dev, "ef0:/"))
        return 0;
    size_t n = strlen(dir);
    if (n == 0 || n >= sizeof(((Save *)0)->dir))
        return 0;
    if (dir[strspn(dir, ".")] == 0) /* ".", ".." and the like */
        return 0;
    if (strpbrk(dir, "/\\:"))
        return 0;
    if (!strncasecmp(dir, "ARK_", 4))
        return 0;
    return 1;
}

/* The name of the folder an app lives in: "ms0:/PSP/GAME/Foo/EBOOT.PBP" -> "Foo". "" if there is none. */
static void app_folder(const Entry *e, char *out, int size)
{
    out[0] = 0;
    const char *last = strrchr(e->path, '/');
    if (!last || last == e->path)
        return;
    const char *start = last;
    while (start > e->path && start[-1] != '/')
        start--;
    snprintf(out, size, "%.*s", (int)(last - start), start);
}

/* Index in the game list of the game or app a save belongs to, or -1 */
static int match_game(const EntryList *games, const char *dir, const char *title)
{
    if (!games)
        return -1;
    for (int i = 0; i < games->count; i++) { /* the game id the folder name starts with */
        const char *id = games->items[i].game_id;
        size_t n = strlen(id);
        if (n >= 8 && !strncasecmp(dir, id, n))
            return i;
    }
    for (int i = 0; i < games->count && title[0]; i++)
        if (!strcasecmp(games->items[i].title, title))
            return i;
    for (int i = 0; i < games->count; i++) {
        if (games->items[i].type != ENTRY_HOMEBREW)
            continue;
        char folder[64];
        app_folder(&games->items[i], folder, sizeof(folder));
        if (folder[0] && !strcasecmp(folder, dir))
            return i;
    }
    return -1;
}

/* ---------------------------------------------------------------------- */
/* the reading thread */

/* One line of text from PARAM.SFO: line breaks become " / ", other control characters spaces, the ends are trimmed */
static void tidy(char *s, int size)
{
    char tmp[256];
    int n = 0, pending = 0;
    for (const char *p = s; *p && n < (int)sizeof(tmp) - 5; p++) {
        u8 c = (u8)*p;
        if (c == '\r')
            continue;
        if (c == '\n') {
            pending = 1;
            continue;
        }
        if (c < 0x20)
            c = ' ';
        if (n == 0 && c == ' ')
            continue;
        if (pending && n > 0) {
            tmp[n++] = ' ';
            tmp[n++] = '/';
            tmp[n++] = ' ';
        }
        pending = 0;
        tmp[n++] = (char)c;
    }
    while (n > 0 && tmp[n - 1] == ' ')
        n--;
    tmp[n] = 0;
    snprintf(s, size, "%s", tmp);
}

/* A whole small file, or NULL */
static u8 *read_file(const char *path, int max, int *size)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return NULL;
    SceIoStat st;
    u8 *buf = NULL;
    int got = -1;
    if (sceIoGetstat(path, &st) >= 0 && st.st_size > 0 && st.st_size <= max && (buf = malloc(st.st_size))) {
        got = sceIoRead(fd, buf, st.st_size);
        if (got != (int)st.st_size) {
            free(buf);
            buf = NULL;
        }
    }
    sceIoClose(fd);
    if (buf)
        *size = got;
    return buf;
}

static void fill(Save *s)
{
    char path[PATH_LEN + 16];
    save_path(path, PATH_LEN, s);
    size_t base = strlen(path);
    snprintf(path + base, sizeof(path) - base, "/PARAM.SFO");
    int size = 0;
    u8 *sfo = read_file(path, MAX_SFO, &size);
    char tmp[96] = "";
    if (sfo) {
        sfo_get_string(sfo, size, "TITLE", tmp, sizeof(tmp));
        sfo_get_string(sfo, size, "SAVEDATA_TITLE", s->title, sizeof(s->title));
        sfo_get_string(sfo, size, "SAVEDATA_DETAIL", s->detail, sizeof(s->detail));
        free(sfo);
    }
    tidy(tmp, sizeof(tmp));
    tidy(s->title, sizeof(s->title));
    tidy(s->detail, sizeof(s->detail));
    path[base] = 0;
    gameops_tree_stats(path, &s->bytes, &s->entries, &T.quit);

    int m = match_game(T.games, s->dir, tmp);
    if (m >= 0) {
        snprintf(s->game, sizeof(s->game), "%s", T.games->items[m].title);
        s->matched = 1;
    } else {
        snprintf(s->game, sizeof(s->game), "%s", tmp);
    }
    s->ready = 1;
}

static int cmp_date(const void *a, const void *b)
{
    const Save *x = a, *y = b;
    if (x->time != y->time)
        return x->time < y->time ? 1 : -1; /* newest first */
    return strcasecmp(x->dir, y->dir);
}

static int cmp_size(const void *a, const void *b)
{
    const Save *x = a, *y = b;
    if (x->bytes != y->bytes)
        return x->bytes < y->bytes ? 1 : -1; /* biggest first */
    return cmp_date(a, b);
}

static int cmp_name(const void *a, const void *b)
{
    const Save *x = a, *y = b;
    int r = strcasecmp(x->game[0] ? x->game : x->dir, y->game[0] ? y->game : y->dir);
    return r ? r : cmp_date(a, b);
}

/* The folders of PSP/SAVEDATA on the card(s), newest first. The number; *out is freed by the caller. */
static int list_folders(Save **out)
{
    Save *list = NULL;
    int n = 0, cap = 0;
    static const char *const devs[2] = { "ms0:/", "ef0:/" };
    for (int d = 0; d < (sys_ms_is_ef() ? 1 : 2); d++) {
        char root[40];
        snprintf(root, sizeof(root), "%s%s", devs[d], SAVE_ROOT);
        SceUID fd = sceIoDopen(root);
        if (fd < 0)
            continue;
        SceIoDirent ent;
        memset(&ent, 0, sizeof(ent));
        while (!T.quit && sceIoDread(fd, &ent) > 0) {
            if (FIO_S_ISDIR(ent.d_stat.st_mode) && strcmp(ent.d_name, ".") && strcmp(ent.d_name, "..") &&
                strlen(ent.d_name) < sizeof(list[0].dir)) {
                if (n == cap) {
                    cap = cap ? cap * 2 : 64;
                    Save *grown = realloc(list, cap * sizeof(*list));
                    if (!grown)
                        break;
                    list = grown;
                }
                Save *s = &list[n++];
                memset(s, 0, sizeof(*s));
                snprintf(s->dev, sizeof(s->dev), "%s", devs[d]);
                snprintf(s->dir, sizeof(s->dir), "%s", ent.d_name);
                s->time = scan_pack_time(&ent.d_stat.sce_st_mtime);
                s->locked = !deletable(s->dev, s->dir); /* known at once: such a folder can never be marked */
            }
            memset(&ent, 0, sizeof(ent));
        }
        sceIoDclose(fd);
    }
    if (n > 1)
        qsort(list, n, sizeof(*list), cmp_date);
    *out = list;
    return n;
}

static int scan_thread(SceSize args, void *argp)
{
    Save *list;
    int n = list_folders(&list);
    T.items = list;
    T.count = n;
    T.listed = 1; /* the list is there; the entries fill in one by one */
    for (int i = 0; i < n && !T.quit; i++)
        fill(&list[i]);
    T.done = 1;
    return 0;
}

static void scan_stop(void)
{
    if (T.thread >= 0) {
        T.quit = 1;
        sceKernelWaitThreadEnd(T.thread, NULL);
        sceKernelDeleteThread(T.thread);
        T.thread = -1;
    }
}

/* ---------------------------------------------------------------------- */
/* the delete thread */

static int worker(SceSize args, void *argp)
{
    int r = DEL_OK;
    for (int i = 0; i < W.n && r == DEL_OK; i++) {
        if (W.cancel) {
            r = DEL_CANCELLED;
            break;
        }
        const char *path = W.paths[i];
        const char *slash = strrchr(path, '/');
        snprintf(W.current, sizeof(W.current), "%s", slash ? slash + 1 : path);
        char sfo[PATH_LEN + 16];
        snprintf(sfo, sizeof(sfo), "%s/PARAM.SFO", path);
        SceIoStat st;
        if (sceIoGetstat(sfo, &st) >= 0) { /* first: whatever happens next, this is no longer a save */
            if (sceIoRemove(sfo) < 0) {
                r = DEL_FAILED;
                break;
            }
            W.done++;
        }
        r = gameops_tree_delete(path, &W.cancel, &W.done, NULL, 0);
    }
    W.result = r;
    W.running = 0;
    return 0;
}

static void refresh_space(void)
{
    static const char *const devs[2] = { "ms0:/", "ef0:/" };
    for (int d = 0; d < 2; d++)
        if (!gameops_dev_space(devs[d], &S.free_b[d], &S.total_b[d]))
            S.free_b[d] = S.total_b[d] = 0;
}

/* The saves a delete would take: the marked ones, or the highlighted one if none is marked */
static int is_target(int i, int any_marked)
{
    const Save *s = &T.items[i];
    return any_marked ? s->marked : i == S.sel;
}

static int marked_count(void)
{
    int n = 0;
    for (int i = 0; T.listed && i < T.count; i++)
        n += T.items[i].marked;
    return n;
}

static void free_pic(void)
{
    tex_free(S.pic);
    S.pic = NULL;
    S.pic_sel = -1; /* the picture of whatever is highlighted now has to be looked for again */
    S.pic_loaded = 0;
    S.pic_alpha = 0;
}

/* After a delete: drop the saves that are gone from the list, unmark the rest */
static void prune(void)
{
    Save *it = T.items;
    int n = 0;
    for (int k = 0; k < W.n; k++) {
        SceIoStat st;
        if (sceIoGetstat(W.paths[k], &st) >= 0)
            continue;
        it[W.index[k]].dir[0] = 0; /* marks it as gone */
    }
    for (int i = 0; i < T.count; i++) {
        if (!it[i].dir[0]) {
            if (i < S.sel)
                S.sel--;
            continue;
        }
        it[i].marked = 0;
        if (n != i)
            it[n] = it[i];
        n++;
    }
    T.count = n;
    if (S.sel >= n)
        S.sel = n ? n - 1 : 0;
    if (S.sel < 0)
        S.sel = 0;
    S.pan_for = -1;
    free_pic();
}

static void finish_delete(void)
{
    int r = W.result;
    sceKernelWaitThreadEnd(W.thread, NULL);
    sceKernelDeleteThread(W.thread);
    W.thread = -1;
    prune();
    free(W.paths);
    free(W.index);
    W.paths = NULL;
    W.index = NULL;
    W.n = 0;
    refresh_space();
    toast(L(r == DEL_OK ? S_DELETED : r == DEL_CANCELLED ? S_F_CANCELLED : S_F_FAILED));
    sound_play(r == DEL_OK ? SND_SELECT : SND_ERROR);
}

static void start_delete(void)
{
    int any = marked_count() > 0, count = T.count, n = 0;
    Save *it = T.items;
    W.paths = malloc((size_t)count * sizeof(*W.paths));
    W.index = malloc((size_t)count * sizeof(*W.index));
    W.total = 0;
    for (int i = 0; W.paths && W.index && i < count; i++) {
        if (!is_target(i, any) || !it[i].ready || !deletable(it[i].dev, it[i].dir))
            continue;
        save_path(W.paths[n], PATH_LEN, &it[i]);
        W.index[n++] = i;
        W.total += it[i].entries ? it[i].entries : 1;
    }
    W.n = n;
    W.done = 0;
    W.cancel = 0;
    W.result = DEL_OK;
    W.current[0] = 0;
    W.running = 1;
    if (n > 0) {
        W.thread = sceKernelCreateThread("psp5_saves_del", worker, 0x28, 0x10000, PSP_THREAD_ATTR_USER, NULL);
        if (W.thread >= 0 && sceKernelStartThread(W.thread, 0, NULL) < 0) {
            sceKernelDeleteThread(W.thread);
            W.thread = -1;
        }
    }
    if (W.thread < 0) { /* nothing to delete or no thread: nothing has changed */
        W.running = 0;
        free(W.paths);
        free(W.index);
        W.paths = NULL;
        W.index = NULL;
        W.n = 0;
        toast(L(S_F_FAILED));
        sound_play(SND_ERROR);
    }
}

/* ---------------------------------------------------------------------- */
/* opening, updating */

void saves_open(const EntryList *games)
{
    memset(&S, 0, sizeof(S));
    memset(&T, 0, sizeof(T));
    memset(&W, 0, sizeof(W));
    T.thread = W.thread = -1;
    T.games = games;
    S.pic_sel = S.pan_for = -1;
    refresh_space();
    T.thread = sceKernelCreateThread("psp5_saves", scan_thread, 0x30, 0x10000, PSP_THREAD_ATTR_USER, NULL);
    if (T.thread >= 0 && sceKernelStartThread(T.thread, 0, NULL) < 0) {
        sceKernelDeleteThread(T.thread);
        T.thread = -1;
    }
    if (T.thread < 0) /* no thread: read here, on the spot (the screen waits, but it works) */
        scan_thread(0, NULL);
}

void saves_close(void)
{
    if (W.thread >= 0) {
        W.cancel = 1;
        sceKernelWaitThreadEnd(W.thread, NULL);
        sceKernelDeleteThread(W.thread);
        W.thread = -1;
    }
    free(W.paths);
    free(W.index);
    W.paths = NULL;
    W.index = NULL;
    scan_stop();
    free(T.items);
    T.items = NULL;
    T.count = 0;
    T.listed = 0;
    free_pic();
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

/* Titles and descriptions can be Japanese or Russian: load the font for them when the first such text turns up */
static void watch_new_entries(void)
{
    Save *it = T.items;
    int n = T.listed ? T.count : 0;
    if (S.seen > n)
        S.seen = n;
    while (S.seen < n && it[S.seen].ready) {
        const Save *s = &it[S.seen++];
        int cjk = 0;
        for (const char *p = s->game; *p; p++)
            cjk |= (u8)*p >= 0x80;
        for (const char *p = s->title; *p; p++)
            cjk |= (u8)*p >= 0x80;
        for (const char *p = s->detail; *p; p++)
            cjk |= (u8)*p >= 0x80;
        if (cjk)
            font_ensure_cjk();
    }
}

static void fmt_time(char *out, int size, u32 t)
{
    if (!t) {
        out[0] = 0;
        return;
    }
    snprintf(out, size, "%04u-%02u-%02u %02u:%02u", (unsigned)((t >> 26) + 1980), (unsigned)((t >> 22) & 15),
             (unsigned)((t >> 17) & 31), (unsigned)((t >> 12) & 31), (unsigned)((t >> 6) & 63));
}

/* The row's texts, cut to fit: the game, then (if there is room) the title of the save in a dimmer colour */
static void lay_row(Save *s)
{
    s->laid = 1;
    s->row_main[0] = s->row_sub[0] = s->row_size[0] = 0;
    const char *name = s->ready && s->game[0] ? s->game : s->dir; /* what the thread is still writing is not read */
    if (s->ready)
        gameops_fmt_size(s->row_size, sizeof(s->row_size), s->bytes);
    float avail = LIST_W - 28 - 10 - (s->row_size[0] ? text_width(0.5f, 0, s->row_size) + 8 : 0);
    const char *sub = s->ready && s->title[0] && strcmp(s->title, name) ? s->title : "";
    float full = text_width(0.58f, 0, name);
    float main_max = sub[0] ? avail * 0.62f : avail;
    if (full > main_max)
        text_ellipsis(s->row_main, sizeof(s->row_main), name, 0.58f, main_max);
    else
        snprintf(s->row_main, sizeof(s->row_main), "%s", name);
    s->sub_x = text_width(0.58f, 0, s->row_main) + 8;
    if (sub[0] && avail - s->sub_x > 36)
        text_ellipsis(s->row_sub, sizeof(s->row_sub), sub, 0.5f, avail - s->sub_x);
}

/* The details of the highlighted save */
static void lay_panel(void)
{
    S.pan_for = S.sel;
    S.pan_name_n = S.pan_desc_n = 0;
    S.pan_title[0] = S.pan_date[0] = 0;
    S.pan_ready = 0;
    if (!T.listed || S.sel >= T.count)
        return;
    const Save *s = &T.items[S.sel];
    S.pan_ready = s->ready;
    if (!s->ready)
        return;
    char name[200];
    if (s->matched || !s->game[0])
        snprintf(name, sizeof(name), "%s", s->game[0] ? s->game : s->dir);
    else
        snprintf(name, sizeof(name), L(S_SV_NOT_INST), s->game);
    S.pan_name_n = text_wrap(name, 0.6f, PANEL_W, S.pan_name, 2);
    text_ellipsis(S.pan_title, sizeof(S.pan_title), s->title[0] ? s->title : s->dir, 0.52f, PANEL_W);
    S.pan_desc_n = text_wrap(s->detail, 0.5f, PANEL_W, S.pan_desc, s->locked ? 2 : 3);
    text_ellipsis(S.pan_lock, sizeof(S.pan_lock), L(S_SV_PROTECTED), 0.5f, PANEL_W);
    fmt_time(S.pan_date, sizeof(S.pan_date), s->time);
}

/* The picture (ICON0.PNG) of a save, or NULL */
static Texture *load_pic(const Save *s)
{
    char path[PATH_LEN + 16];
    save_path(path, PATH_LEN, s);
    size_t base = strlen(path);
    snprintf(path + base, sizeof(path) - base, "/ICON0.PNG");
    int size = 0;
    u8 *png = read_file(path, MAX_ICON, &size);
    if (!png)
        return NULL;
    Image img;
    int ok = img_decode_png(png, size, &img);
    free(png);
    if (!ok)
        return NULL;
    Texture *t = NULL;
    Image scaled;
    if (img.w == ICON_W && img.h == ICON_H) {
        t = img_to_texture(&img, GU_PSM_8888);
    } else if (img_resize(&img, ICON_W, ICON_H, &scaled)) {
        t = img_to_texture(&scaled, GU_PSM_8888);
        img_free(&scaled);
    }
    img_free(&img);
    if (t)
        tex_round_corners(t, t->w / 12.0f);
    return t;
}

/* The sorting chosen in the Sort popup; the highlighted save stays highlighted */
static void set_sort(int sort)
{
    if (!T.done || T.count < 2 || sort == S.sort)
        return;
    char dev[6], dir[64];
    snprintf(dev, sizeof(dev), "%s", T.items[S.sel].dev);
    snprintf(dir, sizeof(dir), "%s", T.items[S.sel].dir);
    S.sort = sort;
    qsort(T.items, T.count, sizeof(Save), S.sort == SORT_SIZE ? cmp_size : S.sort == SORT_NAME ? cmp_name : cmp_date);
    for (int i = 0; i < T.count; i++)
        if (!strcmp(T.items[i].dir, dir) && !strcmp(T.items[i].dev, dev))
            S.sel = i;
    S.top = (int)clampf(S.sel - ROWS / 2, 0, T.count > ROWS ? T.count - ROWS : 0);
    S.pan_for = -1;
    free_pic();
    sound_play(SND_SELECT);
}

static int all_marked(void)
{
    int any = 0;
    for (int i = 0; T.listed && i < T.count; i++) {
        if (T.items[i].locked)
            continue;
        if (!T.items[i].marked)
            return 0;
        any = 1;
    }
    return any;
}

static void select_all(void)
{
    int want = !all_marked(), any = 0;
    for (int i = 0; T.listed && i < T.count; i++) {
        if (T.items[i].locked)
            continue;
        T.items[i].marked = want;
        any = 1;
    }
    sound_play(any ? SND_SELECT : SND_ERROR);
}

static void mark_highlighted(void)
{
    if (!T.listed || !T.count)
        return;
    Save *s = &T.items[S.sel];
    if (s->locked) {
        toast(L(S_SV_PROTECTED));
        sound_play(SND_ERROR);
        return;
    }
    s->marked ^= 1;
    sound_play(SND_SELECT);
    if (S.sel < T.count - 1)
        S.sel++;
}

/* ✕: ask before deleting the marked saves (or the highlighted one) */
static void open_ask(void)
{
    if (!T.listed || !T.count)
        return;
    int any = marked_count() > 0, n = 0;
    unsigned long long bytes = 0;
    for (int i = 0; i < T.count; i++) {
        const Save *s = &T.items[i];
        if (!is_target(i, any) || s->locked)
            continue;
        n++;
        bytes += s->bytes;
    }
    if (!T.done || n == 0) { /* still reading, or only system data */
        if (T.done)
            toast(L(S_SV_PROTECTED));
        sound_play(SND_ERROR);
        return;
    }
    char num[24], text[256];
    gameops_fmt_size(num, sizeof(num), bytes);
    snprintf(S.ask_size, sizeof(S.ask_size), L(S_SV_SELECTED), n, num);
    snprintf(text, sizeof(text), "%s %s", L(S_SV_DEL_ASK), L(S_DEL_FOREVER));
    S.ask_nl = text_wrap(text, 0.5f, POPUP_ASK_W - 24, S.ask_lines, 4);
    S.ask_n = n;
    S.ask = 1;
    S.ask_sel = 1; /* Cancel is preselected, so nothing is deleted by mistake */
    sound_play(SND_SELECT);
}

int saves_update(const UiInput *in, float dt)
{
    S.time += dt;
    S.open = clampf(S.open + dt * 4, 0, 1);
    S.ask_anim = clampf(S.ask_anim + (S.ask ? 7 : -7) * dt, 0, 1);
    S.menu_anim = clampf(S.menu_anim + (S.menu ? 7 : -7) * dt, 0, 1);
    S.sort_anim = clampf(S.sort_anim + (S.sort_focus ? 8 : -8) * dt, 0, 1);
    if (S.toast_time > 0)
        S.toast_time -= dt;
    watch_new_entries();

    if (W.thread >= 0) { /* a delete is running: only the back button (cancel) */
        if (!W.running)
            finish_delete();
        else if (in->pressed & PSP_CTRL_CIRCLE)
            W.cancel = 1;
        return 0;
    }

    int move = vertical(in, dt);
    if (S.ask) {
        if (move) {
            int next = (int)clampf(S.ask_sel + move, 0, 1);
            if (next != S.ask_sel)
                sound_play(SND_MOVE);
            S.ask_sel = next;
        }
        if (in->pressed & PSP_CTRL_CROSS) {
            S.ask = 0;
            if (S.ask_sel == 0) {
                sound_play(SND_SELECT);
                start_delete();
            } else {
                sound_play(SND_BACK);
            }
        } else if (in->pressed & PSP_CTRL_CIRCLE) {
            S.ask = 0;
            sound_play(SND_BACK);
        }
        return 0;
    }

    if (S.menu) { /* the Sort popup: ↑ ↓ choose, ✕ sorts, ○ closes (the button stays selected) */
        if (move) {
            int next = (int)clampf(S.menu_sel + move, 0, SORT_COUNT - 1);
            if (next != S.menu_sel)
                sound_play(SND_MOVE);
            S.menu_sel = next;
        }
        if (in->pressed & PSP_CTRL_CROSS) {
            S.menu = 0;
            set_sort(S.menu_sel);
        } else if (in->pressed & PSP_CTRL_CIRCLE) {
            S.menu = 0;
            sound_play(SND_BACK);
        }
        return 0;
    }

    int n = T.listed ? T.count : 0;
    if (S.sort_focus && (!T.done || n < 2))
        S.sort_focus = 0; /* nothing to sort (any more) */
    int on_sort = S.sort_focus;
    if (in->pressed & PSP_CTRL_CIRCLE) {
        sound_play(SND_BACK);
        if (on_sort) { /* ○ on the Sort button goes back to the list */
            S.sort_focus = 0;
            return 0;
        }
        return 1;
    }
    if (on_sort) { /* ↓ back to the list, ✕ opens the popup */
        if (move > 0) {
            S.sort_focus = 0;
            sound_play(SND_MOVE);
        } else if (in->pressed & PSP_CTRL_CROSS) {
            S.menu = 1;
            S.menu_sel = S.sort;
            sound_play(SND_SELECT);
        }
        move = 0;
    } else if (move < 0 && T.done && n >= 2 && S.sel == 0) { /* ↑ from the first row: to the Sort button */
        S.sort_focus = 1;
        sound_play(SND_MOVE);
        move = 0;
    }
    if (move && n) {
        int next = (int)clampf(S.sel + move, 0, n - 1);
        if (next != S.sel) {
            S.sel = next;
            sound_play(SND_MOVE);
        }
    }
    if (n && !on_sort) {
        if (in->pressed & PSP_CTRL_SQUARE)
            mark_highlighted();
        else if (in->pressed & PSP_CTRL_TRIANGLE)
            select_all();
        else if (in->pressed & PSP_CTRL_CROSS)
            open_ask();
    }
    n = T.listed ? T.count : 0; /* a sort or a mark moves the highlight */
    if (S.sel >= n)
        S.sel = n ? n - 1 : 0;
    if (S.sel < S.top)
        S.top = S.sel;
    if (S.sel > S.top + ROWS - 1)
        S.top = S.sel - (ROWS - 1);
    S.scroll += (S.top - S.scroll) * clampf(dt * 16, 0, 1);

    /* the details, and the picture once the highlight has rested for a moment */
    if (n && (S.pan_for != S.sel || S.pan_ready != T.items[S.sel].ready))
        lay_panel();
    if (S.pic_sel != S.sel) {
        free_pic();
        S.pic_sel = S.sel;
        S.pic_dwell = 0;
    }
    S.pic_dwell += dt;
    if (n && !S.pic_loaded && S.pic_dwell > 0.12f && T.items[S.sel].ready) {
        S.pic = load_pic(&T.items[S.sel]);
        S.pic_loaded = 1;
    }
    if (S.pic)
        S.pic_alpha = clampf(S.pic_alpha + dt * 6, 0, 1);
    return 0;
}

/* ---------------------------------------------------------------------- */
/* drawing */

static void draw_check(float cx, float cy, float a)
{
    gfx_circle(cx, cy, 6, fade(theme_toggle_on(theme_cur()), a));
    float xy[6] = { cx - 3, cy, cx - 1, cy + 2.2f, cx + 3, cy - 2.4f };
    u32 col[3] = { fade(LIGHT, a), fade(LIGHT, a), fade(LIGHT, a) };
    gfx_stroke(xy, col, 3, 1.5f);
}

/* a padlock for the saves that can't be deleted */
static void draw_lock(float cx, float cy, float a)
{
    u32 c = fade(RGBA(214, 214, 220, 255), a);
    gfx_ring(cx - 4, cy - 7, 8, 9, 4, c);
    gfx_round_rect(cx - 6, cy - 2, 12, 9, 2, c);
}

static void draw_rows(float a, float slide)
{
    float bottom = LIST_TOP + ROWS * ROW_H;
    Save *it = T.items;
    int n = T.count;
    gfx_clip(0, LIST_TOP - 2, SCREEN_W, ROWS * ROW_H + 4);
    for (int i = (int)S.scroll; i < n && i <= (int)S.scroll + ROWS; i++) {
        Save *s = &it[i];
        float y = LIST_TOP + (i - S.scroll) * ROW_H;
        if (y > bottom)
            break;
        if (!s->laid || (s->ready && !s->row_size[0]))
            lay_row(s);
        int sel = i == S.sel && !S.ask;
        float ka = a * (1 - S.sort_anim); /* the highlight gives way to the Sort button's */
        if (s->marked)
            gfx_rect(LIST_X, y, LIST_W, ROW_H, fade(COLOR_ALPHA(theme_cur()->accent, 46), a));
        if (sel) {
            gfx_rect(LIST_X, y, LIST_W, ROW_H, RGBA(255, 255, 255, (int)(16 * ka)));
            gfx_frame(LIST_X, y, LIST_W, ROW_H, 1, fade(RGBA(232, 232, 236, 230), ka));
        } else if (i < n - 1 && i + 1 != S.sel) {
            gfx_rect(LIST_X + 4, y + ROW_H - 1, LIST_W - 8, 1, RGBA(255, 255, 255, (int)(22 * a)));
        }
        float x = ROW_ICON_X + slide, cy = y + ROW_H / 2;
        if (s->locked)
            draw_lock(x + 6, cy, a);
        else if (s->marked)
            draw_check(x + 6, cy, a);
        else
            gfx_ring(x, cy - 6, 12, 12, 6, fade(RGBA(255, 255, 255, 90), a));
        u32 col = !s->ready ? TEXT_DIM : s->matched ? TEXT_MAIN : RGBA(200, 202, 212, 255); /* no game: a bit dimmer */
        text_draw(x + 20, y + 16.5f, 0.58f, 0, fade(col, a), ALIGN_LEFT, s->row_main);
        if (s->row_sub[0])
            text_draw(x + 20 + s->sub_x, y + 16.5f, 0.5f, 0, fade(TEXT_DIM, a), ALIGN_LEFT, s->row_sub);
        if (s->row_size[0])
            text_draw(LIST_X + LIST_W - 10, y + 16, 0.5f, 0, fade(TEXT_DIM, a), ALIGN_RIGHT, s->row_size);
    }
    gfx_clip_reset();
    if (n > ROWS) { /* scroll bar */
        float h = ROWS * ROW_H, th = clampf(h * ROWS / n, 12, h);
        float ty = LIST_TOP + (h - th) * S.scroll / (n - ROWS);
        gfx_round_rect(LIST_X + LIST_W + 6, ty, 3, th, 1.5f, fade(RGBA(255, 255, 255, 70), a));
    }
}

static void draw_panel(float a, float slide)
{
    float x = PANEL_X + slide, y = LIST_TOP;
    gfx_glow(x + 1, y + 3, PIC_W - 2, PIC_H - 2, 5, RGBA(0, 0, 0, (int)(130 * a)));
    if (S.pic && S.pic_sel == S.sel)
        gfx_tex(S.pic, x, y, PIC_W, PIC_H, RGBA(255, 255, 255, (int)(255 * a * S.pic_alpha)));
    else
        gfx_round_rect(x, y, PIC_W, PIC_H, PIC_W / 12, fade(theme_cur()->card, a));
    if (!T.listed || S.sel >= T.count || !S.pan_ready)
        return;
    const Save *s = &T.items[S.sel];
    float ty = y + PIC_H + 17;
    for (int i = 0; i < S.pan_name_n; i++, ty += 15)
        text_draw(x, ty, 0.6f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, S.pan_name[i]);
    ty += 2;
    text_draw(x, ty, 0.52f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, S.pan_title);
    ty += 14;
    for (int i = 0; i < S.pan_desc_n; i++, ty += 13)
        text_draw(x, ty, 0.5f, 0, fade(TEXT_DIM, a), ALIGN_LEFT, S.pan_desc[i]);
    if (s->locked)
        text_draw(x, 222, 0.5f, 0, fade(DANGER, a), ALIGN_LEFT, S.pan_lock);
    text_draw(x, 236, 0.5f, 0, fade(TEXT_DIM, a), ALIGN_LEFT, S.pan_date);
    char size[24];
    gameops_fmt_size(size, sizeof(size), s->bytes);
    text_draw(PANEL_X + PANEL_W, 236, 0.5f, 0, fade(TEXT_MAIN, a), ALIGN_RIGHT, size);
}

/* The header: the title and the sorting on the left, the free space after deleting (with a thin bar) and what is
 * selected on the right. Everything is cut to fit, so a long translation never runs into its neighbour: the title keeps
 * clear of the free-space text and the bar, the sorting keeps clear of the selection. */
typedef struct {
    char title[96], sort[96], free[64], sel[64];
    float title_size, sort_size, sort_x, sort_w;
    int has_free, has_sel;
    float used; /* 0..1 of the card that would be used after the delete */
} Header;

#define HEADER_RIGHT (LIST_X + LIST_W + 16 + PANEL_W) /* 456: the right edge of the details */
#define SORT_BASELINE 52.0f
#define SORT_FRAME_H  20.0f /* the frame of the Sort button is centred on its text, which shrinks when the label is long */

static StrId sort_name(int sort)
{
    static const StrId names[SORT_COUNT] = { S_SV_UPDATED, S_I_SIZE, S_SORT_NAME };
    return names[sort];
}
#define BAR_W        120.0f

static void header_layout(Header *h)
{
    int d = T.listed && S.sel < T.count ? dev_index(T.items[S.sel].dev) : 0;
    unsigned long long freed = 0, sel_bytes = 0;
    int sel_n = 0;
    for (int i = 0; T.listed && i < T.count; i++) {
        const Save *s = &T.items[i];
        if (!s->marked)
            continue;
        sel_n++;
        sel_bytes += s->bytes;
        if (dev_index(s->dev) == d)
            freed += s->bytes;
    }
    float free_w = 0, sel_w = 0;
    h->has_free = S.total_b[d] != 0;
    if (h->has_free) {
        unsigned long long after = S.free_b[d] + freed;
        if (after > S.total_b[d])
            after = S.total_b[d];
        char num[24];
        gameops_fmt_size(num, sizeof(num), after);
        snprintf(h->free, sizeof(h->free), L(S_SV_FREE_AFTER), num);
        h->used = 1 - (float)after / (float)S.total_b[d];
        free_w = text_width(0.5f, 0, h->free);
    }
    h->has_sel = sel_n > 0;
    if (h->has_sel) {
        char num[24];
        gameops_fmt_size(num, sizeof(num), sel_bytes);
        snprintf(h->sel, sizeof(h->sel), L(S_SV_SELECTED), sel_n, num);
        sel_w = text_width(0.5f, 0, h->sel);
    }

    /* the title: not into the free-space text, not over the bar */
    float title_x = LIST_X + 4, limit = HEADER_RIGHT - (h->has_free ? fmaxf(free_w, BAR_W) : 0) - 12;
    h->title_size = fit_line(h->title, sizeof(h->title), L(S_SAVES), 0.95f, 0.5f, fmaxf(limit - title_x, 80));

    /* the Sort button, "Sort: Last updated": its frame starts where the rows' highlight does (LIST_X), the text 9 px in, so it
     * stands right above the rows' circles; not into the selection */
    char line[96];
    snprintf(line, sizeof(line), "%s: %s", L(S_SORT), L(sort_name(S.sort)));
    h->sort_x = SORT_X;
    limit = h->has_sel ? HEADER_RIGHT - sel_w - 12 : HEADER_RIGHT - BAR_W - 12;
    h->sort_size = fit_line(h->sort, sizeof(h->sort), line, 0.5f, 0.4f, fmaxf(limit - h->sort_x - 9, 60));
    h->sort_w = text_width(h->sort_size, 0, h->sort);
}

static void draw_header(float a, float slide)
{
    Header h;
    header_layout(&h);
    text_draw(LIST_X + 4 + slide, 34, h.title_size, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, h.title);

    /* the Sort button: selected with ↑ from the first row, it gets the frame of a chosen entry (as in the Library); ✕ opens the popup */
    float on = S.sort_anim;
    if (on * a > 0.01f) {
        float fy, fh;
        popup_frame_around(popup_label_mid(SORT_BASELINE, h.sort_size), SORT_FRAME_H, &fy, &fh);
        popup_choice(h.sort_x - 9 + slide, fy, h.sort_w + 18, fh, on * a);
    }
    text_draw(h.sort_x + slide, SORT_BASELINE, h.sort_size, 0, fade(on > 0.5f ? TEXT_MAIN : TEXT_DIM, a), ALIGN_LEFT, h.sort);

    if (h.has_free) { /* as in the file manager */
        text_draw(HEADER_RIGHT, 28, 0.5f, 0, fade(TEXT_DIM, a), ALIGN_RIGHT, h.free);
        gfx_round_rect(HEADER_RIGHT - BAR_W, 34, BAR_W, 4, 2, fade(RGBA(255, 255, 255, 40), a));
        gfx_round_rect(HEADER_RIGHT - BAR_W, 34, BAR_W * clampf(h.used, 0.02f, 1), 4, 2, fade(theme_cur()->accent, a));
    }
    if (h.has_sel)
        text_draw(HEADER_RIGHT, 52, 0.5f, 0, fade(TEXT_MAIN, a), ALIGN_RIGHT, h.sel);
}

/* The four button hints (bottom right, as in the file manager) and the text size they are drawn at: a long
 * translation gets smaller text, and if that is not enough the longest label is cut. Returns the width of the row. */
typedef struct {
    int btn;
    const char *text;
} Hint;

static char s_hint_text[4][48]; /* the labels, when one had to be cut */

static float hints_width(const Hint h[4], float size)
{
    float w = -14;
    for (int i = 0; i < 4; i++)
        w += 18 + ceilf(text_width(size, 0, h[i].text)) + 14; /* (whole pixels) */
    return w;
}

static float hints_layout(Hint h[4], float *size)
{
    h[0] = (Hint){ controls_glyph(PSP_CTRL_CROSS), L(S_F_DELETE) };
    h[1] = (Hint){ controls_glyph(PSP_CTRL_SQUARE), L(S_F_MARK) };
    h[2] = (Hint){ controls_glyph(PSP_CTRL_TRIANGLE), L(all_marked() ? S_SV_DESELECT_ALL : S_SV_SELECT_ALL) };
    h[3] = (Hint){ controls_glyph(PSP_CTRL_CIRCLE), L(S_BACK) };
    for (*size = 0.56f; hints_width(h, *size) > HINTS_MAX_W && *size > 0.44f;)
        *size -= 0.02f;
    for (int guard = 0; guard < 8 && hints_width(h, *size) > HINTS_MAX_W; guard++) {
        int widest = 0; /* cut the longest label by the excess */
        for (int i = 1; i < 4; i++)
            if (text_width(*size, 0, h[i].text) > text_width(*size, 0, h[widest].text))
                widest = i;
        float want = text_width(*size, 0, h[widest].text) - (hints_width(h, *size) - HINTS_MAX_W) - 4;
        text_ellipsis(s_hint_text[widest], sizeof(s_hint_text[widest]), h[widest].text, *size, want > 20 ? want : 20);
        h[widest].text = s_hint_text[widest];
    }
    return hints_width(h, *size);
}

static void draw_hints(float a)
{
    Hint h[4];
    float size, w = hints_layout(h, &size);
    float x = SCREEN_W - 22 - w, y = 252;
    gfx_round_rect(x - 9, y - 10, w + 18, 20, 10, RGBA(0, 0, 0, (int)(110 * a)));
    for (int i = 0; i < 4; i++) {
        gfx_button(x + 6.5f, y, 6.5f, h[i].btn, fade(LIGHT, a), fade(DARK, a));
        x += 18;
        text_draw(x, y + 4.5f, size, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, h[i].text);
        x += ceilf(text_width(size, 0, h[i].text)) + 14;
    }
}

/* The Sort popup, under the Sort button: the same panel and choices as the Library's (ui.c, popup.c) */
#define SORT_MENU_W 200.0f

static void draw_sort_menu(void)
{
    float a = S.menu_anim;
    if (a <= 0)
        return;
    popup_dim(a);
    float w = SORT_MENU_W, h = POPUP_EDGE + SORT_COUNT * POPUP_ITEM + POPUP_EDGE;
    float x = LIST_X - (1 - a) * 10, y = 64;
    popup_panel(x, y, w, h, POPUP_DARK, a);
    for (int k = 0; k < SORT_COUNT; k++) {
        float ry = y + POPUP_EDGE + k * POPUP_ITEM;
        const char *label = L(sort_name(k));
        u32 col = POPUP_TEXT;
        if (k == S.menu_sel)
            popup_choice(x + POPUP_EDGE, ry, w - 2 * POPUP_EDGE, POPUP_ITEM, a);
        else
            col = fade(col, 0.8f);
        text_draw(x + 16, ry + 18, text_fit(0.62f, 0.45f, 0, w - 32, label), 0, fade(col, a), ALIGN_LEFT, label);
    }
}

/* The question before a delete: the same centred dark panel as everywhere else (popup.c) */
static void draw_ask(void)
{
    float a = S.ask_anim;
    if (a <= 0)
        return;
    popup_dim(a);
    const char *labels[2] = { L(S_F_DELETE), L(S_F_CANCEL) };
    const u32 colors[2] = { POPUP_DANGER, POPUP_TEXT };
    popup_ask(S.ask_lines, S.ask_nl, lang_needs_cjk() ? 15.0f : 13.0f, S.ask_size, labels, colors, 2, S.ask_sel, a);
}

static void draw_progress(void)
{
    if (W.thread < 0)
        return;
    float f = W.total ? clampf((float)W.done / (float)W.total, 0, 1) : 0;
    popup_progress(L(S_F_DELETING), W.current, f, theme_cur()->accent, controls_glyph(PSP_CTRL_CIRCLE), L(S_F_CANCEL));
}

void saves_draw(void)
{
    float a = S.open, slide = (1 - a) * 10;
    gfx_begin(RGBA(20, 20, 24, 255));
    theme_backdrop(theme_cur(), S.time, 1);
    draw_header(a, slide);
    if (!T.listed) {
        text_draw(SCREEN_W / 2, 140, 0.66f, 0, fade(TEXT_DIM, a), ALIGN_CENTER, "...");
    } else if (!T.count) {
        text_draw(SCREEN_W / 2, 140, 0.66f, 0, fade(TEXT_DIM, a), ALIGN_CENTER, T.done ? L(S_SV_NONE) : "...");
    } else {
        draw_rows(a, slide);
        draw_panel(a, slide);
    }
    draw_hints(a * (1 - fmaxf(S.ask_anim, S.menu_anim)));
    draw_sort_menu();
    draw_ask();
    draw_progress();
    if (S.toast_time > 0) {
        float ta = clampf(S.toast_time / 0.4f, 0, 1);
        popup_toast(S.toast, ta);
    }
    osd_draw();
    if (a < 1)
        gfx_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(255 * (1 - a))));
    gfx_end();
}
