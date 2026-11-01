/* psp5 – made by Mille Weber */
/* Sizes of the games and apps (see gamesize.h). */
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gamesize.h"
#include "gameops.h"

typedef struct {
    u64 key;           /* a hash of the entry's path, 0 = the slot is free */
    u64 bytes;
    u32 fallback;      /* the size of the main file (Entry.file_size), until the folder has been counted */
    u8 dev;            /* 0 = ms0:, 1 = ef0: */
    volatile int done; /* 1 once `bytes` is the size of the folder */
} Slot;

typedef struct {
    u32 slot;
    char folder[256];
} Job;

static struct {
    Slot *slots;
    u32 mask; /* the table has mask + 1 slots, a power of two */
    Job *jobs;
    int njobs;
    SceUID thread;
    volatile int cancel, busy, version;
} G = { .thread = -1 };

static u64 hash_path(const char *s)
{
    u64 h = 1469598103934665603ull; /* FNV-1a */
    for (; *s; s++)
        h = (h ^ (u8)*s) * 1099511628211ull;
    return h ? h : 1;
}

static Slot *find(u64 key)
{
    if (!G.slots)
        return NULL;
    for (u32 i = (u32)key & G.mask, n = 0; n <= G.mask; i = (i + 1) & G.mask, n++) {
        if (!G.slots[i].key)
            return NULL;
        if (G.slots[i].key == key)
            return &G.slots[i];
    }
    return NULL;
}

static int count_thread(SceSize args, void *argp)
{
    for (int i = 0; i < G.njobs && !G.cancel; i++) {
        unsigned long long bytes;
        u32 items;
        gameops_tree_stats(G.jobs[i].folder, &bytes, &items, &G.cancel);
        if (G.cancel)
            break;
        Slot *s = &G.slots[G.jobs[i].slot];
        s->bytes = bytes;
        __sync_synchronize(); /* (the size is written before the flag that says it is there) */
        s->done = 1;
        G.version++;
    }
    G.busy = 0;
    return 0;
}

void gamesize_stop(void)
{
    if (G.thread >= 0) {
        G.cancel = 1;
        sceKernelWaitThreadEnd(G.thread, NULL);
        sceKernelDeleteThread(G.thread);
        G.thread = -1;
    }
    G.busy = 0;
}

void gamesize_start(const EntryList *list)
{
    gamesize_stop();
    free(G.slots);
    free(G.jobs);
    G.slots = NULL;
    G.jobs = NULL;
    G.njobs = 0;
    G.cancel = 0;
    G.version++;
    u32 size = 16;
    while (size < (u32)list->count * 2)
        size <<= 1;
    G.slots = calloc(size, sizeof(Slot));
    G.jobs = malloc((size_t)(list->count ? list->count : 1) * sizeof(Job));
    if (!G.slots || !G.jobs) {
        free(G.slots);
        free(G.jobs);
        G.slots = NULL;
        G.jobs = NULL;
        return;
    }
    G.mask = size - 1;
    for (int i = 0; i < list->count; i++) {
        const Entry *e = &list->items[i];
        if (e->type == ENTRY_UMD)
            continue;
        u64 key = hash_path(e->path);
        u32 at = (u32)key & G.mask;
        while (G.slots[at].key && G.slots[at].key != key)
            at = (at + 1) & G.mask;
        Slot *s = &G.slots[at];
        s->key = key;
        s->fallback = e->file_size;
        s->dev = e->path[0] == 'e';
        char folder[256];
        int is_dir = 0;
        if (e->type != ENTRY_ISO && gameops_target(e, folder, sizeof(folder), &is_dir) && is_dir) {
            G.jobs[G.njobs].slot = at;
            snprintf(G.jobs[G.njobs].folder, sizeof(G.jobs[0].folder), "%s", folder);
            G.njobs++;
        } else {
            s->bytes = e->file_size; /* an ISO: its file */
            s->done = 1;
        }
    }
    if (!G.njobs)
        return;
    G.busy = 1;
    G.thread = sceKernelCreateThread("psp5_gamesize", count_thread, 0x30, 0x8000, PSP_THREAD_ATTR_USER, NULL);
    if (G.thread >= 0 && sceKernelStartThread(G.thread, 0, NULL) < 0) {
        sceKernelDeleteThread(G.thread);
        G.thread = -1;
    }
    if (G.thread < 0) /* no thread: the sizes stay the EBOOT's */
        G.busy = 0;
}

unsigned long long gamesize_of(const Entry *e)
{
    Slot *s = e->type == ENTRY_UMD ? NULL : find(hash_path(e->path));
    return s && s->done ? s->bytes : e->file_size;
}

int gamesize_known(const Entry *e)
{
    Slot *s = e->type == ENTRY_UMD ? NULL : find(hash_path(e->path));
    return s && s->done;
}

int gamesize_version(void)
{
    return G.version;
}

int gamesize_busy(void)
{
    return G.busy;
}

unsigned long long gamesize_total(int dev)
{
    unsigned long long total = 0;
    for (u32 i = 0; G.slots && i <= G.mask; i++)
        if (G.slots[i].key && (dev < 0 || G.slots[i].dev == dev))
            total += G.slots[i].done ? G.slots[i].bytes : G.slots[i].fallback;
    return total;
}
