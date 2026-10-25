/* psp5 – made by Mille Weber */
/* Measuring and deleting games and apps (see gameops.h). The delete is careful on purpose:
 *  - gameops_target() is the only place that decides what may be deleted, and it only says yes to a
 *    file in ISO/ or a folder in PSP/GAME/ on ms0:/ef0: - never anything else;
 *  - a game folder loses its EBOOT.PBP first, so a delete that is cancelled or fails half way leaves
 *    something that no longer shows up as a game, instead of a game that crashes when started;
 *  - it runs in a thread with a progress panel and can be cancelled. */
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "gameops.h"
#include "gfx.h"
#include "lang.h"
#include "popup.h"
#include "theme.h"

#define MAX_PATH  512
#define MAX_DEPTH 12 /* deeper than any game folder; also keeps the recursion shallow on the thread's stack */

void gameops_fmt_size(char *out, int size, unsigned long long b)
{
    if (b >= 1024ull * 1024 * 1024)
        snprintf(out, size, "%.1f GB", (float)b / (1024.0f * 1024.0f * 1024.0f));
    else if (b >= 1024ull * 1024)
        snprintf(out, size, "%.1f MB", (float)b / (1024.0f * 1024.0f));
    else if (b >= 1024)
        snprintf(out, size, "%u KB", (unsigned)(b / 1024));
    else
        snprintf(out, size, "%u B", (unsigned)b);
}

/* ---------------------------------------------------------------------- */
/* what may be deleted */

static int has_prefix(const char *s, const char *prefix)
{
    return !strncasecmp(s, prefix, strlen(prefix));
}

/* The part after "ms0:/": no empty, "." or ".." segments (a trailing slash is allowed) */
static int clean_path(const char *s)
{
    if (!*s)
        return 0;
    while (*s) {
        const char *end = strchr(s, '/');
        size_t n = end ? (size_t)(end - s) : strlen(s);
        if (n == 0 || (n == 1 && s[0] == '.') || (n == 2 && s[0] == '.' && s[1] == '.'))
            return 0;
        if (memchr(s, '\\', n) || memchr(s, ':', n))
            return 0;
        s += n;
        if (*s == '/')
            s++;
    }
    return 1;
}

int gameops_target(const Entry *e, char *path, int size, int *is_dir)
{
    if (!e || e->type == ENTRY_UMD)
        return 0;
    const char *p = e->path;
    if (!has_prefix(p, "ms0:/") && !has_prefix(p, "ef0:/"))
        return 0;
    const char *rel = p + 5;
    if (!clean_path(rel))
        return 0;

    if (e->type == ENTRY_ISO) { /* the file itself */
        if (!has_prefix(rel, "ISO/") || !rel[4])
            return 0;
        const char *dot = strrchr(rel, '.');
        if (!dot || (strcasecmp(dot + 1, "iso") && strcasecmp(dot + 1, "cso") && strcasecmp(dot + 1, "zso") &&
                     strcasecmp(dot + 1, "dax") && strcasecmp(dot + 1, "jso")))
            return 0;
        if ((int)strlen(p) >= size)
            return 0;
        strcpy(path, p);
        *is_dir = 0;
        return 1;
    }

    /* an EBOOT game or app: the folder that holds the EBOOT.PBP, one level below PSP/GAME or two (PS1/<game>) */
    if (e->type != ENTRY_HOMEBREW && e->type != ENTRY_PSN && e->type != ENTRY_POPS)
        return 0;
    if (!has_prefix(rel, "PSP/GAME/"))
        return 0;
    const char *sub = rel + 9;
    const char *last = strrchr(sub, '/');
    if (!last || last == sub || strcasecmp(last + 1, "EBOOT.PBP"))
        return 0;
    int segments = 1;
    for (const char *q = sub; q < last; q++)
        if (*q == '/')
            segments++;
    if (segments > 2)
        return 0;
    for (const char *s = sub; s < last;) { /* never psp5's own folder */
        const char *end = memchr(s, '/', last - s);
        size_t n = end ? (size_t)(end - s) : (size_t)(last - s);
        if (n == 4 && !strncasecmp(s, "PSP5", 4))
            return 0;
        s += n + (end ? 1 : 0);
    }
    int len = (int)(last - p);
    if (len >= size)
        return 0;
    memcpy(path, p, len);
    path[len] = 0;
    *is_dir = 1;
    return 1;
}

int gameops_still_exists(const Entry *e)
{
    SceIoStat st;
    return sceIoGetstat(e->path, &st) >= 0;
}

/* ---------------------------------------------------------------------- */
/* folders */

typedef struct {
    char name[256];
    u64 size;
    u8 dir;
} DirItem;

/* The entries of a folder (without . and ..), read completely before anything is done to them. The
 * number, or -1 if it can't be opened. *out is freed by the caller. */
static int read_dir(const char *path, DirItem **out)
{
    *out = NULL;
    SceUID d = sceIoDopen(path);
    if (d < 0)
        return -1;
    DirItem *list = NULL;
    int n = 0, cap = 0;
    SceIoDirent ent;
    memset(&ent, 0, sizeof(ent));
    while (sceIoDread(d, &ent) > 0) {
        if (strcmp(ent.d_name, ".") && strcmp(ent.d_name, "..")) {
            if (n == cap) {
                cap = cap ? cap * 2 : 32;
                DirItem *grown = realloc(list, cap * sizeof(*list));
                if (!grown)
                    break;
                list = grown;
            }
            DirItem *it = &list[n++];
            snprintf(it->name, sizeof(it->name), "%s", ent.d_name);
            it->dir = FIO_S_ISDIR(ent.d_stat.st_mode) != 0;
            it->size = it->dir ? 0 : (u64)ent.d_stat.st_size;
        }
        memset(&ent, 0, sizeof(ent)); /* d_private must be zero */
    }
    sceIoDclose(d);
    *out = list;
    return n;
}

static void join(char *out, int size, const char *dir, const char *name)
{
    snprintf(out, size, "%s/%s", dir, name);
}

/* ---------------------------------------------------------------------- */
/* measuring */

enum { M_IDLE, M_RUN, M_DONE };

static struct {
    SceUID thread;
    volatile int state, cancel;
    unsigned long long bytes;
    unsigned files;
    char path[MAX_PATH];
} M = { .thread = -1 };

static void measure_walk(const char *path, int depth)
{
    if (depth > MAX_DEPTH || M.cancel)
        return;
    DirItem *list;
    int n = read_dir(path, &list);
    for (int i = 0; i < n && !M.cancel; i++) {
        if (list[i].dir) {
            char child[MAX_PATH];
            join(child, sizeof(child), path, list[i].name);
            measure_walk(child, depth + 1);
        } else {
            M.bytes += list[i].size;
            M.files++;
        }
    }
    free(list);
}

static int measure_thread(SceSize args, void *argp)
{
    measure_walk(M.path, 0);
    M.state = M_DONE;
    return 0;
}

void gameops_measure_stop(void)
{
    if (M.thread >= 0) {
        M.cancel = 1;
        sceKernelWaitThreadEnd(M.thread, NULL);
        sceKernelDeleteThread(M.thread);
        M.thread = -1;
    }
    M.state = M_IDLE;
}

void gameops_measure_start(const char *folder)
{
    gameops_measure_stop();
    M.cancel = 0;
    M.bytes = 0;
    M.files = 0;
    snprintf(M.path, sizeof(M.path), "%s", folder);
    M.state = M_RUN;
    M.thread = sceKernelCreateThread("psp5_measure", measure_thread, 0x30, 0x8000, PSP_THREAD_ATTR_USER, NULL);
    if (M.thread >= 0 && sceKernelStartThread(M.thread, 0, NULL) < 0) {
        sceKernelDeleteThread(M.thread);
        M.thread = -1;
    }
    if (M.thread < 0) { /* no thread: count here, on the spot (a small price on a very rare failure) */
        measure_thread(0, NULL);
    }
}

int gameops_measure_poll(unsigned long long *bytes, unsigned *files)
{
    if (M.state != M_DONE)
        return 0;
    if (M.thread >= 0) { /* it has finished: collect the thread */
        sceKernelWaitThreadEnd(M.thread, NULL);
        sceKernelDeleteThread(M.thread);
        M.thread = -1;
    }
    *bytes = M.bytes;
    *files = M.files;
    return 1;
}

/* Bytes and entries (files, sub folders and the folder itself) of a folder, counted on the caller's thread.
 * Stops early when *stop turns non-zero. */
static void stats_walk(const char *path, int depth, unsigned long long *bytes, u32 *items, volatile int *stop)
{
    if (depth > MAX_DEPTH)
        return;
    ++*items;
    DirItem *list;
    int n = read_dir(path, &list);
    for (int i = 0; i < n && !*stop; i++) {
        if (list[i].dir) {
            char child[MAX_PATH];
            join(child, sizeof(child), path, list[i].name);
            stats_walk(child, depth + 1, bytes, items, stop);
        } else {
            *bytes += list[i].size;
            ++*items;
        }
    }
    free(list);
}

void gameops_tree_stats(const char *folder, unsigned long long *bytes, u32 *items, volatile int *stop)
{
    *bytes = 0;
    *items = 0;
    stats_walk(folder, 0, bytes, items, stop);
}

int gameops_dev_space(const char *path, unsigned long long *free_b, unsigned long long *total_b)
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

/* ---------------------------------------------------------------------- */
/* deleting */

static struct {
    SceUID thread;
    volatile int running, cancel, result;
    char path[MAX_PATH];
    int is_dir, back_is_cross;
    u32 back;
    volatile u32 done, total;
    char current[128];
} D = { .thread = -1, .result = DEL_OK };

static void count_items(const char *path, int depth, u32 *n)
{
    if (depth > MAX_DEPTH)
        return;
    DirItem *list;
    int count = read_dir(path, &list);
    for (int i = 0; i < count; i++) {
        if (list[i].dir) {
            char child[MAX_PATH];
            join(child, sizeof(child), path, list[i].name);
            count_items(child, depth + 1, n); /* counts the sub folder itself */
        } else {
            ++*n;
        }
    }
    free(list);
    ++*n; /* this folder itself */
}

/* What a delete reports while it works: the cancel flag it obeys, the number of removed entries, and
 * (optionally) the name of the entry it is at. A game delete and the saved data screen each pass their own. */
typedef struct {
    volatile int *cancel;
    volatile u32 *done;
    char *current;
    int current_size;
} Progress;

static int delete_tree(const char *path, int depth, const Progress *p)
{
    if (depth > MAX_DEPTH)
        return DEL_FAILED; /* never recurse without end */
    DirItem *list;
    int n = read_dir(path, &list);
    int r = n < 0 ? DEL_FAILED : DEL_OK;
    for (int i = 0; i < n && r == DEL_OK; i++) {
        if (*p->cancel) {
            r = DEL_CANCELLED;
            break;
        }
        char child[MAX_PATH];
        join(child, sizeof(child), path, list[i].name);
        if (p->current)
            snprintf(p->current, p->current_size, "%s", list[i].name);
        if (list[i].dir) {
            r = delete_tree(child, depth + 1, p);
        } else if (sceIoRemove(child) < 0) {
            r = DEL_FAILED;
        } else {
            ++*p->done;
        }
    }
    free(list);
    if (r == DEL_OK && sceIoRmdir(path) < 0)
        r = DEL_FAILED;
    if (r == DEL_OK)
        ++*p->done;
    return r;
}

int gameops_tree_delete(const char *folder, volatile int *cancel, volatile u32 *done, char *current, int current_size)
{
    Progress p = { cancel, done, current, current_size };
    return delete_tree(folder, 0, &p);
}

static int delete_thread(SceSize args, void *argp)
{
    int r = DEL_OK;
    SceIoStat st;
    if (sceIoGetstat(D.path, &st) < 0) {
        r = DEL_OK; /* already gone */
    } else if (!D.is_dir) {
        D.total = 1;
        const char *name = strrchr(D.path, '/');
        snprintf(D.current, sizeof(D.current), "%.127s", name ? name + 1 : D.path);
        r = sceIoRemove(D.path) < 0 ? DEL_FAILED : DEL_OK;
        D.done = 1;
    } else {
        u32 items = 0;
        count_items(D.path, 0, &items);
        D.total = items;
        /* the EBOOT.PBP first: whatever happens next, this is no longer listed as a game */
        char eboot[MAX_PATH];
        join(eboot, sizeof(eboot), D.path, "EBOOT.PBP");
        snprintf(D.current, sizeof(D.current), "EBOOT.PBP");
        if (sceIoGetstat(eboot, &st) >= 0) {
            if (sceIoRemove(eboot) < 0)
                r = DEL_FAILED;
            else
                D.done++;
        }
        if (r == DEL_OK) {
            Progress p = { &D.cancel, &D.done, D.current, sizeof(D.current) };
            r = delete_tree(D.path, 0, &p);
        }
    }
    D.result = r;
    D.running = 0;
    return 0;
}

int gameops_delete_start(const Entry *e, u32 back, int back_is_cross)
{
    if (D.thread >= 0 || D.running)
        return 0;
    char path[MAX_PATH];
    int is_dir;
    if (!gameops_target(e, path, sizeof(path), &is_dir))
        return 0;
    gameops_measure_stop(); /* nothing else works on the folder */
    snprintf(D.path, sizeof(D.path), "%s", path);
    D.is_dir = is_dir;
    D.back = back;
    D.back_is_cross = back_is_cross;
    D.done = D.total = 0;
    D.current[0] = 0;
    D.cancel = 0;
    D.result = DEL_OK;
    D.running = 1;
    D.thread = sceKernelCreateThread("psp5_delete", delete_thread, 0x28, 0x10000, PSP_THREAD_ATTR_USER, NULL);
    if (D.thread >= 0 && sceKernelStartThread(D.thread, 0, NULL) < 0) {
        sceKernelDeleteThread(D.thread);
        D.thread = -1;
    }
    if (D.thread < 0) { /* no thread: nothing has changed */
        D.running = 0;
        return 0;
    }
    return 1;
}

int gameops_delete_update(u32 pressed)
{
    if (D.running) {
        if (pressed & D.back)
            D.cancel = 1;
        return DEL_RUNNING;
    }
    return D.result;
}

void gameops_delete_end(void)
{
    if (D.thread >= 0) {
        D.cancel = 1;
        sceKernelWaitThreadEnd(D.thread, NULL);
        sceKernelDeleteThread(D.thread);
        D.thread = -1;
    }
}

/* the same panel as the file manager's (popup.c) */
void gameops_draw(void)
{
    if (D.thread < 0)
        return;
    float f = D.total ? (float)D.done / (float)D.total : 0;
    popup_progress(L(S_F_DELETING), D.current, f, theme_cur()->accent, D.back_is_cross ? BTN_CROSS : BTN_CIRCLE, L(S_F_CANCEL));
}
