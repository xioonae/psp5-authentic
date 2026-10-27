/* psp5 – made by Mille Weber */
/* Filhanteraren, i samma stil som inställningarna: temats bakgrund, en lista med mapparna
 * först och en tunn ljus ram runt det valda, och en liten alternativmeny (△, eller ✕ på en fil).
 * Kopiering, flytt och radering körs i en egen tråd med förloppsstapel och går att avbryta.
 * Namn skrivs med PSP:ns eget tangentbord. Bara ms0: och ef0: (PSP Go) – aldrig flash0. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <psputility.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "files.h"
#include "controls.h"
#include "gfx.h"
#include "lang.h"
#include "sound.h"
#include "system.h"
#include "theme.h"
#include "osd.h"
#include "popup.h"

#define LIST_X   24.0f
#define LIST_W   432.0f
#define LIST_TOP 62.0f
#define ROW_H    24.0f
#define ROWS     7
#define MENU_W      200.0f
#define MENU_ITEM_H POPUP_ITEM
#define MENU_EDGE   POPUP_EDGE
#define MAX_PATH 512
#define MAX_CLIP 64          /* shortcut: högst 64 markerade objekt åt gången – höj om det inte räcker */
#define COPY_BUF (128 * 1024)

#define TEXT_MAIN RGBA(238, 238, 242, 255)
#define TEXT_DIM  RGBA(160, 162, 172, 255)
#define LIGHT     RGBA(236, 236, 240, 255)
#define DARK      RGBA(20, 20, 26, 255)

enum { KIND_FILE, KIND_DIR, KIND_DEVICE };

typedef struct {
    char name[256];  /* filnamnet (enhet: "ms0:/") */
    char shown[96];  /* namnet kortat för listan */
    char size[24];   /* storlek (enhet: ledigt utrymme) */
    u64 bytes;
    u8 kind, marked;
} FileEntry;

/* --- listan --- */
static char s_path[MAX_PATH]; /* "" = enhetslistan, annars t.ex. "ms0:/PSP/GAME" */
static FileEntry *s_items;
static int s_count, s_sel, s_top;
static float s_scroll, s_time, s_open;
static int s_repeat_dir;
static float s_repeat_timer;
static char s_toast[96];
static float s_toast_time;
static u64 s_free, s_total;   /* enheten som visas */
static const char *s_devs[2];
static int s_ndev;

/* --- urklipp --- */
static char *s_clip[MAX_CLIP];
static int s_clip_n, s_clip_move;

/* --- meny --- */
enum { M_NONE, M_OPTIONS, M_DELETE };
enum { A_PASTE, A_COPY, A_MOVE, A_RENAME, A_DELETE, A_NEWDIR, A_DELETE_YES, A_CANCEL };
static int s_menu, s_menu_sel, s_menu_n, s_menu_act[8];
static float s_menu_anim;
static int s_menu_ask;       /* the open (or fading) popup is the delete question */
static char s_ask_lines[4][96]; /* the question, wrapped */
static int s_ask_n;
static char s_ask_sub[24];   /* size of the one file that goes, else empty */

/* --- arbetstråden --- */
enum { OP_COPY, OP_MOVE, OP_DELETE };
enum { R_OK, R_FAILED, R_NO_SPACE, R_CANCELLED };
static struct {
    SceUID thread;
    volatile int running, cancel, result;
    int op, n;
    char *src[MAX_CLIP];
    char dest[MAX_PATH];
    volatile u64 done, total; /* byte (kopiera/flytta) eller objekt (radera) */
    char current[128];
    unsigned char *buf;
} W = { .thread = -1 };

static int s_osk_done;

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float smooth(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
static u32 fade(u32 c, float a) { return COLOR_ALPHA(c, (int)(((c >> 24) & 0xFF) * clampf(a, 0, 1))); }

static void toast(const char *msg)
{
    snprintf(s_toast, sizeof(s_toast), "%s", msg);
    s_toast_time = 2.4f;
}

/* ---------------------------------------------------------------------- */
/* sökvägar och enheter */

static void join(char *out, int size, const char *dir, const char *name)
{
    int n = strlen(dir);
    snprintf(out, size, n && dir[n - 1] == '/' ? "%s%s" : "%s/%s", dir, name);
}

static const char *base_name(const char *path)
{
    const char *s = strrchr(path, '/');
    return s && s[1] ? s + 1 : path;
}

/* "ms0:/PSP/GAME" → "ms0:/PSP", "ms0:/PSP" → "ms0:/" */
static void parent_of(char *out, int size, const char *path)
{
    snprintf(out, size, "%s", path);
    char *s = strrchr(out, '/');
    if (s && s > out && s[-1] == ':')
        s[1] = 0;
    else if (s)
        *s = 0;
}

static int same_device(const char *a, const char *b) { return !strncmp(a, b, 4); }

/* path är dir eller ligger i den */
static int is_inside(const char *path, const char *dir)
{
    int n = strlen(dir);
    return !strncmp(path, dir, n) && (path[n] == 0 || path[n] == '/');
}

static int dev_present(const char *root)
{
    SceUID d = sceIoDopen(root);
    if (d < 0)
        return 0;
    sceIoDclose(d);
    return 1;
}

/* Ledigt och totalt utrymme på enheten som path ligger på */
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

static void fmt_size(char *out, int size, u64 b)
{
    if (b >= 1024ull * 1024 * 1024)
        snprintf(out, size, "%.1f GB", (float)(b / (1024.0f * 1024.0f)) / 1024.0f);
    else if (b >= 1024 * 1024)
        snprintf(out, size, "%.1f MB", (float)b / (1024.0f * 1024.0f));
    else if (b >= 1024)
        snprintf(out, size, "%u KB", (unsigned)(b / 1024));
    else
        snprintf(out, size, "%u B", (unsigned)b);
}

static int valid_name(const char *s)
{
    return s[0] && strcmp(s, ".") && strcmp(s, "..") && !strpbrk(s, "\\/:*?\"<>|");
}

/* Läser en mapp (utan . och ..). Antal, <0 = gick inte att öppna. *out frigörs av anroparen. */
static int read_dir(const char *path, FileEntry **out)
{
    *out = NULL;
    SceUID d = sceIoDopen(path);
    if (d < 0)
        return -1;
    FileEntry *list = NULL;
    int n = 0, cap = 0;
    SceIoDirent ent;
    memset(&ent, 0, sizeof(ent));
    while (sceIoDread(d, &ent) > 0) {
        if (strcmp(ent.d_name, ".") && strcmp(ent.d_name, "..")) {
            if (n == cap) {
                cap = cap ? cap * 2 : 32;
                FileEntry *grown = realloc(list, cap * sizeof(*list));
                if (!grown)
                    break;
                list = grown;
            }
            FileEntry *e = &list[n++];
            memset(e, 0, sizeof(*e));
            snprintf(e->name, sizeof(e->name), "%s", ent.d_name);
            e->kind = FIO_S_ISDIR(ent.d_stat.st_mode) ? KIND_DIR : KIND_FILE;
            e->bytes = ent.d_stat.st_size;
        }
        memset(&ent, 0, sizeof(ent));
    }
    sceIoDclose(d);
    *out = list;
    return n;
}

/* ---------------------------------------------------------------------- */
/* arbetstråden: kopiera, flytta, radera */

/* A name for `name` in `dir` that is not taken: the name itself, else "name (2)", "name (3)" ... 1 = `out` is
 * free to write to, 0 = every name up to (999) is taken (then nothing may be written: it would overwrite). */
static int unique_dest(char *out, int size, const char *dir, const char *name, int is_dir)
{
    join(out, size, dir, name);
    const char *dot = is_dir ? NULL : strrchr(name, '.');
    if (dot == name)
        dot = NULL;
    int stem = dot ? (int)(dot - name) : (int)strlen(name);
    SceIoStat st;
    for (int k = 2; sceIoGetstat(out, &st) >= 0; k++) {
        if (k >= 1000)
            return 0;
        char alt[300];
        snprintf(alt, sizeof(alt), "%.*s (%d)%s", stem, name, k, dot ? dot : "");
        join(out, size, dir, alt);
    }
    return 1;
}

static void tree_size(const char *path, int is_dir, u64 bytes, u64 *total_bytes, u64 *items)
{
    ++*items;
    if (!is_dir) {
        *total_bytes += bytes;
        return;
    }
    FileEntry *list;
    int n = read_dir(path, &list);
    for (int i = 0; i < n; i++) {
        char child[MAX_PATH];
        join(child, sizeof(child), path, list[i].name);
        tree_size(child, list[i].kind == KIND_DIR, list[i].bytes, total_bytes, items);
    }
    free(list);
}

static int copy_file(const char *src, const char *dst)
{
    SceUID in = sceIoOpen(src, PSP_O_RDONLY, 0);
    if (in < 0)
        return R_FAILED;
    SceUID out = sceIoOpen(dst, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (out < 0) {
        sceIoClose(in);
        return R_FAILED;
    }
    int r = R_OK;
    for (;;) {
        if (W.cancel) {
            r = R_CANCELLED;
            break;
        }
        int got = sceIoRead(in, W.buf, COPY_BUF);
        if (got < 0) {
            r = R_FAILED;
            break;
        }
        if (got == 0)
            break;
        if (sceIoWrite(out, W.buf, got) != got) {
            r = R_FAILED;
            break;
        }
        W.done += got;
    }
    sceIoClose(in);
    sceIoClose(out);
    if (r != R_OK)
        sceIoRemove(dst); /* ingen halv fil kvar */
    return r;
}

static int copy_tree(const char *src, const char *dst, int is_dir)
{
    if (!is_dir)
        return copy_file(src, dst);
    if (sceIoMkdir(dst, 0777) < 0)
        return R_FAILED;
    FileEntry *list;
    int n = read_dir(src, &list), r = n < 0 ? R_FAILED : R_OK;
    for (int i = 0; i < n && r == R_OK; i++) {
        char s[MAX_PATH], d[MAX_PATH];
        join(s, sizeof(s), src, list[i].name);
        join(d, sizeof(d), dst, list[i].name);
        r = copy_tree(s, d, list[i].kind == KIND_DIR);
    }
    free(list);
    return r;
}

/* flags: DT_COUNT = counted in the progress bar (a delete), otherwise part of a move.
 * DT_FORCE = go on even if the user has cancelled: the clean-up of a half-made copy has to be finished. */
enum { DT_COUNT = 1, DT_FORCE = 2 };

static int delete_tree(const char *path, int is_dir, int flags)
{
    if (W.cancel && !(flags & DT_FORCE))
        return R_CANCELLED;
    int r = R_OK;
    if (is_dir) {
        FileEntry *list;
        int n = read_dir(path, &list);
        for (int i = 0; i < n && r == R_OK; i++) {
            char child[MAX_PATH];
            join(child, sizeof(child), path, list[i].name);
            r = delete_tree(child, list[i].kind == KIND_DIR, flags);
        }
        free(list);
        if (r == R_OK && sceIoRmdir(path) < 0)
            r = R_FAILED;
    } else if (sceIoRemove(path) < 0) {
        r = R_FAILED;
    }
    if (flags & DT_COUNT)
        W.done++;
    return r;
}

static int worker(SceSize args, void *argp)
{
    int is_dir[MAX_CLIP], r = R_OK;
    u64 bytes = 0, items = 0;
    for (int i = 0; i < W.n; i++) {
        SceIoStat st;
        memset(&st, 0, sizeof(st));
        if (sceIoGetstat(W.src[i], &st) < 0) {
            is_dir[i] = -1; /* finns inte längre */
            continue;
        }
        is_dir[i] = FIO_S_ISDIR(st.st_mode) != 0;
        /* en flytt på samma enhet är bara ett namnbyte – inget att kopiera */
        if (W.op != OP_MOVE || !same_device(W.src[i], W.dest))
            tree_size(W.src[i], is_dir[i], st.st_size, &bytes, &items);
    }
    W.total = W.op == OP_DELETE ? items : bytes;
    u64 free_b, total_b;
    if (W.op != OP_DELETE && bytes && dev_space(W.dest, &free_b, &total_b) && bytes + 1024 * 1024 > free_b)
        r = R_NO_SPACE;

    for (int i = 0; i < W.n && r == R_OK; i++) {
        if (is_dir[i] < 0)
            continue;
        snprintf(W.current, sizeof(W.current), "%s", base_name(W.src[i]));
        if (W.op == OP_DELETE) {
            r = delete_tree(W.src[i], is_dir[i], DT_COUNT);
            continue;
        }
        char parent[MAX_PATH], dst[MAX_PATH];
        parent_of(parent, sizeof(parent), W.src[i]);
        if (W.op == OP_MOVE && !strcmp(parent, W.dest))
            continue; /* redan här */
        if (is_dir[i] && is_inside(W.dest, W.src[i])) {
            r = R_FAILED; /* en mapp in i sig själv */
            break;
        }
        if (!unique_dest(dst, sizeof(dst), W.dest, base_name(W.src[i]), is_dir[i])) {
            r = R_FAILED; /* alla namn är upptagna: skriv inte över något */
            break;
        }
        if (W.op == OP_MOVE && same_device(W.src[i], W.dest) && sceIoRename(W.src[i], dst) >= 0)
            continue;
        r = copy_tree(W.src[i], dst, is_dir[i]);
        if (r != R_OK)
            delete_tree(dst, is_dir[i], DT_FORCE); /* städa bort det som hann kopieras – också efter ○ */
        else if (W.op == OP_MOVE)
            r = delete_tree(W.src[i], is_dir[i], 0);
    }
    W.result = r;
    W.running = 0;
    return 0;
}

static void start_op(int op, char **paths, int n, const char *dest)
{
    W.op = op;
    W.n = 0;
    for (int i = 0; i < n; i++)
        if ((W.src[W.n] = strdup(paths[i])))
            W.n++;
    snprintf(W.dest, sizeof(W.dest), "%s", dest ? dest : "");
    W.done = W.total = 0;
    W.current[0] = 0;
    W.cancel = 0;
    W.result = R_OK;
    W.buf = op == OP_DELETE ? NULL : malloc(COPY_BUF);
    W.running = 1;
    W.thread = (op == OP_DELETE || W.buf)
                   ? sceKernelCreateThread("psp5_files", worker, 0x28, 0x10000, PSP_THREAD_ATTR_USER, NULL)
                   : -1;
    if (W.thread >= 0 && sceKernelStartThread(W.thread, 0, NULL) < 0) {
        sceKernelDeleteThread(W.thread);
        W.thread = -1;
    }
    if (W.thread < 0) { /* ingen tråd: ingenting har ändrats */
        W.running = 0;
        for (int i = 0; i < W.n; i++)
            free(W.src[i]);
        free(W.buf);
        W.buf = NULL;
        toast(L(S_F_FAILED));
        sound_play(SND_ERROR);
    }
}

static void stop_op(void)
{
    if (W.thread < 0)
        return;
    W.cancel = 1;
    sceKernelWaitThreadEnd(W.thread, NULL);
    sceKernelDeleteThread(W.thread);
    W.thread = -1;
    for (int i = 0; i < W.n; i++)
        free(W.src[i]);
    W.n = 0;
    free(W.buf);
    W.buf = NULL;
}

/* ---------------------------------------------------------------------- */
/* listan */

static int cmp_entry(const void *a, const void *b)
{
    const FileEntry *x = a, *y = b;
    if (x->kind != y->kind)
        return x->kind == KIND_DIR ? -1 : 1;
    return strcasecmp(x->name, y->name);
}

static int marked_count(void)
{
    int n = 0;
    for (int i = 0; i < s_count; i++)
        n += s_items[i].marked;
    return n;
}

/* Läser in s_path och markerar `select` (namn), eller annars raden `fallback` */
static void load(const char *select, int fallback)
{
    free(s_items);
    s_items = NULL;
    s_count = 0;
    s_free = s_total = 0;
    if (!s_path[0]) {
        s_items = calloc(s_ndev, sizeof(*s_items));
        for (int i = 0; s_items && i < s_ndev; i++) {
            FileEntry *e = &s_items[i];
            snprintf(e->name, sizeof(e->name), "%s", s_devs[i]);
            snprintf(e->shown, sizeof(e->shown), "%s", L(s_devs[i][0] == 'e' ? S_F_INTERNAL : S_F_MS));
            u64 f, t;
            if (dev_space(e->name, &f, &t)) {
                char num[24];
                fmt_size(num, sizeof(num), f);
                snprintf(e->size, sizeof(e->size), L(S_F_FREE), num);
            }
            e->kind = KIND_DEVICE;
            s_count++;
        }
    } else {
        int n = read_dir(s_path, &s_items);
        s_count = n < 0 ? 0 : n;
        if (s_count > 1) /* (an empty folder has no list: qsort must not get a null pointer) */
            qsort(s_items, s_count, sizeof(*s_items), cmp_entry);
        for (int i = 0; i < s_count; i++) {
            FileEntry *e = &s_items[i];
            text_ellipsis(e->shown, sizeof(e->shown), e->name, 0.58f, LIST_W - 130);
            if (e->kind == KIND_FILE)
                fmt_size(e->size, sizeof(e->size), e->bytes);
        }
        dev_space(s_path, &s_free, &s_total);
    }
    s_sel = fallback < s_count ? fallback : s_count - 1;
    if (s_sel < 0)
        s_sel = 0;
    for (int i = 0; select && i < s_count; i++)
        if (!strcasecmp(s_items[i].name, select))
            s_sel = i;
    s_top = s_sel - ROWS / 2;
    if (s_top > s_count - ROWS)
        s_top = s_count - ROWS;
    if (s_top < 0)
        s_top = 0;
    s_scroll = s_top;
}

static void enter(const char *path, const char *select)
{
    snprintf(s_path, sizeof(s_path), "%s", path);
    load(select, 0);
}

/* ○: en nivå upp. 0 = redan överst (stäng). */
static int go_up(void)
{
    if (!s_path[0])
        return 0;
    int at_root = s_path[strlen(s_path) - 1] == '/';
    if (at_root) {
        if (s_ndev < 2)
            return 0;
        char dev[8];
        snprintf(dev, sizeof(dev), "%s", s_path);
        enter("", dev);
        return 1;
    }
    char parent[MAX_PATH], child[256];
    snprintf(child, sizeof(child), "%s", base_name(s_path));
    parent_of(parent, sizeof(parent), s_path);
    enter(parent, child);
    return 1;
}

/* Markerade objekt, eller det valda om inget är markerat. Sökvägarna frigörs av anroparen. */
static int targets(char **out)
{
    int n = 0, any = marked_count();
    for (int i = 0; i < s_count && n < MAX_CLIP; i++) {
        if (any ? !s_items[i].marked : i != s_sel)
            continue;
        char p[MAX_PATH];
        join(p, sizeof(p), s_path, s_items[i].name);
        if ((out[n] = strdup(p)))
            n++;
    }
    return n;
}

static void clear_clip(void)
{
    for (int i = 0; i < s_clip_n; i++)
        free(s_clip[i]);
    s_clip_n = 0;
}

/* ---------------------------------------------------------------------- */
/* PSP:ns tangentbord */

static void to_utf16(const char *s, unsigned short *o, int n)
{
    int k = 0;
    while (*s && k < n - 1) {
        unsigned c = (u8)*s++;
        if (c >= 0xE0 && s[0] && s[1]) {
            c = ((c & 0x0F) << 12) | ((s[0] & 0x3F) << 6) | (s[1] & 0x3F);
            s += 2;
        } else if (c >= 0xC0 && s[0]) {
            c = ((c & 0x1F) << 6) | (s[0] & 0x3F);
            s++;
        }
        o[k++] = c;
    }
    o[k] = 0;
}

static void to_utf8(const unsigned short *s, char *o, int n)
{
    int k = 0;
    for (; *s; s++) {
        unsigned c = *s;
        int len = c < 0x80 ? 1 : c < 0x800 ? 2 : 3;
        if (k + len >= n)
            break;
        if (len == 1) {
            o[k++] = c;
        } else if (len == 2) {
            o[k++] = 0xC0 | (c >> 6);
            o[k++] = 0x80 | (c & 0x3F);
        } else {
            o[k++] = 0xE0 | (c >> 12);
            o[k++] = 0x80 | ((c >> 6) & 0x3F);
            o[k++] = 0x80 | (c & 0x3F);
        }
    }
    o[k] = 0;
}

static void osk_step(void)
{
    switch (sceUtilityOskGetStatus()) {
    case PSP_UTILITY_DIALOG_VISIBLE: sceUtilityOskUpdate(1); break;
    case PSP_UTILITY_DIALOG_QUIT:    sceUtilityOskShutdownStart(); break;
    case PSP_UTILITY_DIALOG_NONE:    s_osk_done = 1; break;
    }
}

static void draw_scene(void);

/* Visar tangentbordet med `init` ifyllt. 1 = användaren bekräftade (texten i out). */
static int osk_input(const char *desc, const char *init, char *out, int out_size)
{
    static unsigned short desc16[64], in16[256], out16[256];
    to_utf16(desc, desc16, 64);
    to_utf16(init, in16, 256);
    memset(out16, 0, sizeof(out16));
    SceUtilityOskData data;
    memset(&data, 0, sizeof(data));
    data.language = PSP_UTILITY_OSK_LANGUAGE_DEFAULT;
    data.lines = 1;
    data.unk_24 = 1;
    data.inputtype = PSP_UTILITY_OSK_INPUTTYPE_ALL;
    data.desc = desc16;
    data.intext = in16;
    data.outtextlength = 256;
    data.outtextlimit = 200;
    data.outtext = out16;
    SceUtilityOskParams params;
    memset(&params, 0, sizeof(params));
    params.base.size = sizeof(params);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &params.base.language);
    sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_BUTTON_SWAP, &params.base.buttonSwap);
    params.base.graphicsThread = 17;
    params.base.accessThread = 19;
    params.base.fontThread = 18;
    params.base.soundThread = 16;
    params.datacount = 1;
    params.data = &data;
    if (sceUtilityOskInitStart(&params) < 0)
        return 0;
    s_osk_done = 0;
    while (!s_osk_done) {
        gfx_begin(RGBA(20, 20, 24, 255));
        draw_scene();
        gfx_end_with(osk_step);
    }
    if (data.result == PSP_UTILITY_OSK_RESULT_CANCELLED)
        return 0;
    if (out16[0])
        to_utf8(out16, out, out_size);
    else
        snprintf(out, out_size, "%s", init);
    while (out[0] && out[strlen(out) - 1] == ' ') /* FAT tar ändå bort mellanslag på slutet */
        out[strlen(out) - 1] = 0;
    return 1;
}

/* ---------------------------------------------------------------------- */
/* menyn */

static void add_act(int act) { s_menu_act[s_menu_n++] = act; }

static void ask_build(void);

static void open_menu(int kind)
{
    s_menu = kind;
    s_menu_ask = kind == M_DELETE;
    s_menu_n = 0;
    s_menu_sel = 0;
    if (kind == M_OPTIONS) {
        if (s_clip_n)
            add_act(A_PASTE);
        if (s_count) {
            add_act(A_COPY);
            add_act(A_MOVE);
            if (marked_count() <= 1)
                add_act(A_RENAME);
            add_act(A_DELETE);
        }
        add_act(A_NEWDIR);
    } else {
        add_act(A_DELETE_YES);
        add_act(A_CANCEL);
        s_menu_sel = 1; /* Avbryt är förvalt – inget raderas av misstag */
        ask_build();
    }
    sound_play(SND_SELECT);
}

static const char *act_label(int act, char *buf, int size)
{
    switch (act) {
    case A_PASTE:      snprintf(buf, size, "%s (%d)", L(S_F_PASTE), s_clip_n); return buf;
    case A_COPY:       return L(S_F_COPY);
    case A_MOVE:       return L(S_F_MOVE);
    case A_RENAME:     return L(S_F_RENAME);
    case A_DELETE:
    case A_DELETE_YES: return L(S_F_DELETE);
    case A_NEWDIR:     return L(S_F_NEWDIR);
    }
    return L(S_F_CANCEL);
}

/* The delete question, worded like the one on the game and app cards: what goes (the name, or "Delete: 3 items"),
 * that it cannot be undone and, for one file, how big it is. */
static void ask_build(void)
{
    int n = marked_count();
    const FileEntry *one = n == 0 && s_count ? &s_items[s_sel] : NULL;
    for (int i = 0; n == 1 && i < s_count; i++)
        if (s_items[i].marked)
            one = &s_items[i];
    char text[256], shown[64];
    s_ask_sub[0] = 0;
    if (one) {
        text_ellipsis(shown, sizeof(shown), one->shown, 0.5f, 120);
        snprintf(text, sizeof(text), L(one->kind == KIND_DIR ? S_DEL_DIR : S_DEL_FILE), shown);
        if (one->kind == KIND_FILE)
            snprintf(s_ask_sub, sizeof(s_ask_sub), "%s", one->size);
    } else {
        snprintf(shown, sizeof(shown), L(S_F_ITEMS), n);
        snprintf(text, sizeof(text), "%s: %s.", L(S_F_DELETE), shown);
    }
    snprintf(text + strlen(text), sizeof(text) - strlen(text), " %s", L(S_DEL_FOREVER));
    s_ask_n = text_wrap(text, 0.5f, POPUP_ASK_W - 24, s_ask_lines, 4);
}

static void rename_selected(void)
{
    int idx = s_sel;
    for (int i = 0; i < s_count; i++)
        if (s_items[i].marked)
            idx = i;
    char name[256], from[MAX_PATH], to[MAX_PATH];
    if (!osk_input(L(S_F_RENAME), s_items[idx].name, name, sizeof(name)) || !strcmp(name, s_items[idx].name))
        return;
    if (!valid_name(name)) {
        toast(L(S_F_BAD_NAME));
        sound_play(SND_ERROR);
        return;
    }
    join(from, sizeof(from), s_path, s_items[idx].name);
    join(to, sizeof(to), s_path, name);
    if (sceIoRename(from, to) < 0) {
        toast(L(S_F_FAILED));
        sound_play(SND_ERROR);
        return;
    }
    load(name, s_sel);
}

static void new_folder(void)
{
    char name[256], path[MAX_PATH];
    if (!osk_input(L(S_F_NEWDIR), L(S_F_NEWDIR), name, sizeof(name)))
        return;
    if (!valid_name(name)) {
        toast(L(S_F_BAD_NAME));
        sound_play(SND_ERROR);
        return;
    }
    join(path, sizeof(path), s_path, name);
    if (sceIoMkdir(path, 0777) < 0) {
        toast(L(S_F_FAILED));
        sound_play(SND_ERROR);
        return;
    }
    load(name, s_sel);
}

static void run_act(int act)
{
    char *paths[MAX_CLIP];
    int n;
    s_menu = M_NONE;
    switch (act) {
    case A_COPY:
    case A_MOVE:
        clear_clip();
        s_clip_n = targets(s_clip);
        s_clip_move = act == A_MOVE;
        for (int i = 0; i < s_count; i++)
            s_items[i].marked = 0;
        sound_play(SND_SELECT);
        break;
    case A_PASTE:
        start_op(s_clip_move ? OP_MOVE : OP_COPY, s_clip, s_clip_n, s_path);
        break;
    case A_RENAME:
        rename_selected();
        break;
    case A_NEWDIR:
        new_folder();
        break;
    case A_DELETE:
        open_menu(M_DELETE);
        break;
    case A_DELETE_YES:
        n = targets(paths);
        start_op(OP_DELETE, paths, n, NULL);
        for (int i = 0; i < n; i++)
            free(paths[i]);
        break;
    default:
        sound_play(SND_BACK);
        break;
    }
}

/* ---------------------------------------------------------------------- */
/* styrning */

static int vertical(const UiInput *in, float dt)
{
    int dir = ((in->held & PSP_CTRL_UP) || in->ly < 40) ? -1 : ((in->held & PSP_CTRL_DOWN) || in->ly > 215) ? 1 : 0;
    if (!dir) {
        s_repeat_dir = 0;
        return 0;
    }
    if (dir != s_repeat_dir) {
        s_repeat_dir = dir;
        s_repeat_timer = 0.32f;
        return dir;
    }
    s_repeat_timer -= dt;
    if (s_repeat_timer > 0)
        return 0;
    s_repeat_timer += 0.07f;
    return dir;
}

void files_open(void)
{
    s_ndev = 0;
    if (dev_present("ms0:/"))
        s_devs[s_ndev++] = "ms0:/";
    if (sys_model() == 4 && dev_present("ef0:/"))
        s_devs[s_ndev++] = "ef0:/";
    s_open = s_menu_anim = s_toast_time = 0;
    s_menu = M_NONE;
    s_repeat_dir = 0;
    enter(s_ndev == 1 ? s_devs[0] : "", NULL);
}

static void finish_op(void)
{
    int op = W.op, r = W.result;
    stop_op();
    if (op == OP_MOVE && r == R_OK)
        clear_clip();
    static const StrId msg[] = { S_F_DONE, S_F_FAILED, S_F_NO_SPACE, S_F_CANCELLED };
    toast(L(msg[r]));
    sound_play(r == R_OK ? SND_SELECT : SND_ERROR);
    char keep[256] = "";
    if (s_count)
        snprintf(keep, sizeof(keep), "%s", s_items[s_sel].name);
    load(keep, s_sel); /* raderat: samma rad */
}

int files_update(const UiInput *in, float dt)
{
    s_time += dt;
    s_open = clampf(s_open + dt * 4, 0, 1);
    s_menu_anim = clampf(s_menu_anim + (s_menu ? 7 : -7) * dt, 0, 1);
    if (s_toast_time > 0)
        s_toast_time -= dt;

    if (W.thread >= 0) { /* åtgärd pågår: bara ○ (avbryt) */
        if (!W.running)
            finish_op();
        else if (in->pressed & PSP_CTRL_CIRCLE)
            W.cancel = 1;
        return 0;
    }

    int move = vertical(in, dt);
    if (s_menu) {
        if (move) {
            int next = (int)clampf(s_menu_sel + move, 0, s_menu_n - 1);
            if (next != s_menu_sel)
                sound_play(SND_MOVE);
            s_menu_sel = next;
        }
        if (in->pressed & PSP_CTRL_CROSS)
            run_act(s_menu_act[s_menu_sel]);
        else if (in->pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_START)) { /* back, or the options button that opened it */
            s_menu = M_NONE;
            sound_play(SND_BACK);
        }
        return 0;
    }

    if (in->pressed & PSP_CTRL_LTRIGGER)
        move = -ROWS;
    if (in->pressed & PSP_CTRL_RTRIGGER)
        move = ROWS;
    if (move && s_count) {
        int next = (int)clampf(s_sel + move, 0, s_count - 1);
        if (next != s_sel) {
            s_sel = next;
            sound_play(SND_MOVE);
        }
    }
    if (s_sel < s_top)
        s_top = s_sel;
    if (s_sel > s_top + ROWS - 1)
        s_top = s_sel - (ROWS - 1);
    s_scroll += (s_top - s_scroll) * clampf(dt * 16, 0, 1);

    FileEntry *e = s_count ? &s_items[s_sel] : NULL;
    if (in->pressed & PSP_CTRL_CROSS) {
        if (e && e->kind != KIND_FILE) {
            char path[MAX_PATH];
            join(path, sizeof(path), s_path, e->name);
            enter(e->kind == KIND_DEVICE ? e->name : path, NULL);
            sound_play(SND_SELECT);
        } else if (s_path[0]) {
            open_menu(M_OPTIONS); /* ✕ på en fil: vad ska göras med den */
        }
    } else if ((in->pressed & PSP_CTRL_SQUARE) && e && e->kind != KIND_DEVICE) {
        e->marked ^= 1;
        sound_play(SND_SELECT);
        if (s_sel < s_count - 1)
            s_sel++;
    } else if ((in->pressed & PSP_CTRL_START) && s_path[0]) { /* the options button, as everywhere */
        open_menu(M_OPTIONS);
    } else if (in->pressed & PSP_CTRL_CIRCLE) {
        if (!go_up())
            return 1;
        sound_play(SND_BACK);
    }
    return 0;
}

/* ---------------------------------------------------------------------- */
/* ritning */

static u32 type_color(const char *ext)
{
    static const struct { const char *ext; u32 color; } T[] = {
        { "iso", RGBA(74, 112, 204, 255) }, { "cso", RGBA(74, 112, 204, 255) }, { "zso", RGBA(74, 112, 204, 255) }, { "dax", RGBA(74, 112, 204, 255) }, { "jso", RGBA(74, 112, 204, 255) },
        { "pbp", RGBA(128, 92, 196, 255) }, { "prx", RGBA(128, 92, 196, 255) },
        { "mp3", RGBA(52, 156, 110, 255) }, { "at3", RGBA(52, 156, 110, 255) }, { "wav", RGBA(52, 156, 110, 255) },
        { "png", RGBA(206, 128, 62, 255) }, { "jpg", RGBA(206, 128, 62, 255) }, { "bmp", RGBA(206, 128, 62, 255) },
        { "txt", RGBA(96, 126, 150, 255) }, { "ini", RGBA(96, 126, 150, 255) }, { "cfg", RGBA(96, 126, 150, 255) },
    };
    for (unsigned i = 0; ext && i < sizeof(T) / sizeof(T[0]); i++)
        if (!strcasecmp(ext, T[i].ext))
            return T[i].color;
    return RGBA(104, 106, 116, 255);
}

static void draw_icon(float x, float y, const FileEntry *e, float a)
{
    if (e->kind == KIND_DEVICE) {
        gfx_round_rect(x, y + 3, 18, 13, 2.5f, fade(RGBA(214, 214, 220, 255), a));
        gfx_rect(x + 3, y + 6, 8, 2, fade(RGBA(90, 92, 100, 255), a));
        return;
    }
    if (e->kind == KIND_DIR) {
        gfx_round_rect(x, y + 2, 8, 5, 1.5f, fade(RGBA(168, 170, 180, 255), a));
        gfx_round_rect(x, y + 4, 18, 13, 2.5f, fade(RGBA(222, 222, 228, 255), a));
        return;
    }
    const char *dot = strrchr(e->name, '.');
    const char *ext = dot && dot != e->name && strlen(dot + 1) <= 4 ? dot + 1 : NULL;
    gfx_round_rect(x + 1, y + 1, 16, 18, 2.5f, fade(type_color(ext), a)); /* (the page is centred in the 18 px box on whole pixels) */
    if (ext) {
        char up[5];
        int n = 0;
        for (; ext[n] && n < 3; n++)
            up[n] = ext[n] >= 'a' && ext[n] <= 'z' ? ext[n] - 32 : ext[n];
        up[n] = 0;
        text_draw(x + 9, y + 13.5f, 0.3f, 1, fade(RGBA(255, 255, 255, 235), a), ALIGN_CENTER, up);
    }
}

static void draw_check(float cx, float cy, float a)
{
    gfx_circle(cx, cy, 6, fade(theme_toggle_on(theme_cur()), a));
    float xy[6] = { cx - 3, cy, cx - 1, cy + 2.2f, cx + 3, cy - 2.4f };
    u32 col[3] = { fade(LIGHT, a), fade(LIGHT, a), fade(LIGHT, a) };
    gfx_stroke(xy, col, 3, 1.5f);
}

/* Knapptips nere till höger, som i inställningarna */
static void draw_hints(float a)
{
    struct { int btn; StrId text; } h[4];
    int n = 0;
    h[n++] = (typeof(h[0])){ controls_glyph(PSP_CTRL_CROSS), S_F_OPEN };
    if (s_path[0]) {
        h[n++] = (typeof(h[0])){ controls_glyph(PSP_CTRL_SQUARE), S_F_MARK };
        h[n++] = (typeof(h[0])){ controls_glyph(PSP_CTRL_START), S_OPTIONS };
    }
    h[n++] = (typeof(h[0])){ controls_glyph(PSP_CTRL_CIRCLE), S_BACK };
    float w = -14;
    for (int i = 0; i < n; i++)
        w += gfx_button_width(h[i].btn) + 5 + ceilf(text_width(0.56f, 0, L(h[i].text))) + 14; /* (whole pixels) */
    float x = SCREEN_W - 22 - w, y = 252;
    gfx_round_rect(x - 9, y - 10, w + 18, 20, 10, RGBA(0, 0, 0, (int)(110 * a)));
    for (int i = 0; i < n; i++) {
        float bw = gfx_button_width(h[i].btn);
        gfx_button(x + bw / 2, y, 6.5f, h[i].btn, fade(LIGHT, a), fade(DARK, a));
        x += bw + 5;
        text_draw(x, y + 4.5f, 0.56f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, L(h[i].text));
        x += ceilf(text_width(0.56f, 0, L(h[i].text))) + 14;
    }
}

static void draw_header(float a, float slide)
{
    text_draw(LIST_X + 4 + slide, 34, 0.95f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, L(S_FILES));
    if (s_total) { /* ledigt utrymme och en tunn stapel, som PS5:s lagringsvy */
        char num[24], line[64];
        fmt_size(num, sizeof(num), s_free);
        snprintf(line, sizeof(line), L(S_F_FREE), num);
        float right = LIST_X + LIST_W;
        text_draw(right, 28, 0.5f, 0, fade(TEXT_DIM, a), ALIGN_RIGHT, line);
        float bw = 120, used = 1 - (float)s_free / (float)s_total;
        gfx_round_rect(right - bw, 34, bw, 4, 2, fade(RGBA(255, 255, 255, 40), a));
        gfx_round_rect(right - bw, 34, bw * clampf(used, 0.02f, 1), 4, 2, fade(theme_cur()->accent, a));
    }
    if (s_path[0]) { /* sökvägen, kortad från vänster */
        const char *p = s_path;
        float max = LIST_W - 8;
        while (text_width(0.5f, 0, p) > max && strchr(p + 1, '/'))
            p = strchr(p + 1, '/');
        char line[MAX_PATH + 4];
        snprintf(line, sizeof(line), p == s_path ? "%s" : "...%s", p);
        text_draw(LIST_X + 4 + slide, 52, 0.5f, 0, fade(TEXT_DIM, a), ALIGN_LEFT, line);
    }
}

static void draw_list(float a, float slide)
{
    if (!s_count) {
        text_draw(SCREEN_W / 2, 140, 0.66f, 0, fade(TEXT_DIM, a), ALIGN_CENTER, L(S_F_EMPTY));
        return;
    }
    int any = marked_count() > 0;
    float bottom = LIST_TOP + ROWS * ROW_H;
    gfx_clip(0, LIST_TOP - 2, SCREEN_W, ROWS * ROW_H + 4);
    for (int i = (int)s_scroll; i < s_count && i <= (int)s_scroll + ROWS; i++) {
        const FileEntry *e = &s_items[i];
        float y = LIST_TOP + (i - s_scroll) * ROW_H;
        if (y > bottom)
            break;
        int sel = i == s_sel && !s_menu;
        if (e->marked)
            gfx_rect(LIST_X, y, LIST_W, ROW_H, fade(COLOR_ALPHA(theme_cur()->accent, 46), a));
        if (sel) {
            gfx_rect(LIST_X, y, LIST_W, ROW_H, RGBA(255, 255, 255, (int)(16 * a)));
            gfx_frame(LIST_X, y, LIST_W, ROW_H, 1, fade(RGBA(232, 232, 236, 230), a));
        } else if (i < s_count - 1 && i + 1 != s_sel) {
            gfx_rect(LIST_X + 4, y + ROW_H - 1, LIST_W - 8, 1, RGBA(255, 255, 255, (int)(22 * a)));
        }
        float x = LIST_X + 8 + slide;
        if (any) {
            if (e->marked)
                draw_check(x + 6, y + ROW_H / 2, a);
            else
                gfx_ring(x, y + ROW_H / 2 - 6, 12, 12, 6, fade(RGBA(255, 255, 255, 90), a));
            x += 18;
        }
        draw_icon(x, y + 2, e, a);
        text_draw(x + 26, y + 16.5f, 0.58f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, e->shown);
        if (e->size[0])
            text_draw(LIST_X + LIST_W - 10, y + 16, 0.5f, 0, fade(TEXT_DIM, a), ALIGN_RIGHT, e->size);
    }
    gfx_clip_reset();
    if (s_count > ROWS) { /* rullningslist */
        float h = ROWS * ROW_H, th = clampf(h * ROWS / s_count, 12, h);
        float ty = LIST_TOP + (h - th) * s_scroll / (s_count - ROWS);
        gfx_round_rect(LIST_X + LIST_W + 6, ty, 3, th, 1.5f, fade(RGBA(255, 255, 255, 70), a));
    }
}

/* The popup (△, or ✕ on a file) is drawn exactly like the one on the game and app cards (ui.c, draw_menu): a dark
 * square panel, the chosen entry with a faint light fill and a thin frame, no title. The delete question is the
 * centred question panel of popup.c, the same as on the cards and in Manage Saved Data. */
static void draw_menu(void)
{
    float a = smooth(s_menu_anim);
    if (a <= 0 || s_menu_n <= 0)
        return;
    popup_dim(a);
    if (s_menu_ask) {
        const char *labels[8];
        u32 colors[8];
        char lb[8][64];
        for (int k = 0; k < s_menu_n && k < 8; k++) {
            labels[k] = act_label(s_menu_act[k], lb[k], sizeof(lb[k]));
            colors[k] = s_menu_act[k] == A_DELETE_YES ? POPUP_DANGER : POPUP_TEXT;
        }
        popup_ask(s_ask_lines, s_ask_n, lang_needs_cjk() ? 15.0f : 13.0f, s_ask_sub, labels, colors, s_menu_n, s_menu_sel, a);
        return;
    }
    /* the choices sit MENU_EDGE from every side of the panel */
    float w = MENU_W, h = MENU_EDGE + s_menu_n * MENU_ITEM_H + MENU_EDGE;
    float x = SCREEN_W - 24 - w - (1 - a) * 10, y = 238 - h;
    popup_panel(x, y, w, h, POPUP_DARK, a);
    for (int k = 0; k < s_menu_n; k++) {
        int act = s_menu_act[k];
        float ry = y + MENU_EDGE + k * MENU_ITEM_H;
        u32 col = act == A_DELETE ? POPUP_DANGER : POPUP_TEXT;
        if (k == s_menu_sel) { /* faint light fill and a thin frame */
            popup_choice(x + MENU_EDGE, ry, w - 2 * MENU_EDGE, MENU_ITEM_H, a);
        } else {
            col = fade(col, 0.8f);
        }
        char lb[64];
        const char *label = act_label(act, lb, sizeof(lb));
        text_draw(x + 16, ry + 18, text_fit(0.62f, 0.45f, 0, w - 32, label), 0, fade(col, a), ALIGN_LEFT, label);
    }
}

static void draw_progress(void)
{
    if (W.thread < 0)
        return;
    StrId title = W.op == OP_COPY ? S_F_COPYING : W.op == OP_MOVE ? S_F_MOVING : S_F_DELETING;
    float f = W.total ? clampf((float)W.done / (float)W.total, 0, 1) : 0;
    popup_progress(L(title), W.current, f, theme_cur()->accent, controls_glyph(PSP_CTRL_CIRCLE), L(S_F_CANCEL));
}

static void draw_scene(void)
{
    float a = s_open, slide = (1 - a) * 10;
    theme_backdrop(theme_cur(), s_time, 1);
    draw_header(a, slide);
    draw_list(a, slide);
    if (s_clip_n) { /* urklippet nere till vänster */
        char line[64];
        snprintf(line, sizeof(line), "%s: %d", L(s_clip_move ? S_F_MOVE : S_F_COPY), s_clip_n);
        gfx_round_rect(LIST_X, 243, 12, 15, 2, fade(RGBA(214, 214, 220, 255), a));
        gfx_rect(LIST_X + 3, 241, 6, 4, fade(RGBA(150, 152, 160, 255), a));
        text_draw(LIST_X + 18, 255, 0.54f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, line);
    }
    draw_hints(a * (1 - smooth(s_menu_anim)));
    draw_menu();
    draw_progress();
    if (s_toast_time > 0) {
        float ta = clampf(s_toast_time / 0.4f, 0, 1);
        popup_toast(s_toast, ta);
    }
    osd_draw();
    if (a < 1)
        gfx_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(255 * (1 - a))));
}

void files_draw(void)
{
    gfx_begin(RGBA(20, 20, 24, 255));
    draw_scene();
    gfx_end();
}

void files_close(void)
{
    stop_op();
    clear_clip();
    free(s_items);
    s_items = NULL;
    s_count = 0;
}
