/* psp5 – made by Mille Weber */
/* What is on the storage (see storage.h). */
#include <pspkernel.h>
#include <stdio.h>
#include <string.h>
#include "storage.h"
#include "gameops.h"
#include "gamesize.h"
#include "system.h"

static const char *const ROOT[2] = { "ms0:/", "ef0:/" };
static const char *const MEDIA[] = { "MUSIC", "VIDEO", "PICTURE", "PSP/PHOTO" };

static struct {
    int n, dev[2];          /* the storages that are there, in the order they are shown */
    u64 total[2], free_b[2];    /* by device */
    volatile u64 media[2], saved[2];
    volatile int done;      /* the thread has counted everything */
    SceUID thread;
    volatile int cancel;
} G = { .thread = -1 };

static void count_folder(int dev, const char *name, volatile u64 *into)
{
    char path[64];
    unsigned long long bytes;
    u32 items;
    snprintf(path, sizeof(path), "%s%s", ROOT[dev], name);
    gameops_tree_stats(path, &bytes, &items, &G.cancel);
    *into += bytes;
}

static int count_thread(SceSize args, void *argp)
{
    for (int b = 0; b < G.n && !G.cancel; b++) {
        int d = G.dev[b];
        G.media[d] = G.saved[d] = 0;
        for (unsigned i = 0; i < sizeof(MEDIA) / sizeof(MEDIA[0]) && !G.cancel; i++)
            count_folder(d, MEDIA[i], &G.media[d]);
        count_folder(d, "PSP/SAVEDATA", &G.saved[d]);
    }
    if (!G.cancel)
        G.done = 1;
    return 0;
}

void storage_stop(void)
{
    if (G.thread >= 0) {
        G.cancel = 1;
        sceKernelWaitThreadEnd(G.thread, NULL);
        sceKernelDeleteThread(G.thread);
        G.thread = -1;
    }
}

void storage_start(void)
{
    storage_stop();
    memset(G.total, 0, sizeof(G.total));
    memset(G.free_b, 0, sizeof(G.free_b));
    memset((void *)G.media, 0, sizeof(G.media));
    memset((void *)G.saved, 0, sizeof(G.saved));
    G.n = 0;
    G.done = G.cancel = 0;
    int has[2] = { 0, 0 };
    for (int d = 0; d < 2; d++) {
        unsigned long long free_b, total_b;
        if ((d == 1 && sys_ms_is_ef()) || !gameops_dev_space(ROOT[d], &free_b, &total_b) || !total_b)
            continue;
        G.total[d] = total_b;
        G.free_b[d] = free_b;
        has[d] = 1;
    }
    if (has[1]) /* a PSP Go: its own storage first, then the Memory Stick */
        G.dev[G.n++] = 1;
    if (has[0])
        G.dev[G.n++] = 0;
    if (!G.n) { /* nothing could be asked (a PC, an emulator without a card): the memory card, as nothing */
        G.dev[G.n++] = 0;
        G.done = 1;
        return;
    }
    G.thread = sceKernelCreateThread("psp5_storage", count_thread, 0x30, 0x8000, PSP_THREAD_ATTR_USER, NULL);
    if (G.thread >= 0 && sceKernelStartThread(G.thread, 0, NULL) < 0) {
        sceKernelDeleteThread(G.thread);
        G.thread = -1;
    }
    if (G.thread < 0) /* no thread: the folders are counted here, on the spot (the page waits, but it works) */
        count_thread(0, NULL);
}

int storage_count(void)
{
    return G.n ? G.n : 1;
}

void storage_get(int block, StorageInfo *out)
{
    memset(out, 0, sizeof(*out));
    if (block < 0 || block >= G.n)
        return;
    int d = G.dev[block];
    out->dev = d;
    out->total = G.total[d];
    out->free = G.free_b[d];
    out->games = gamesize_total(d);
    out->media = G.media[d];
    out->saved = G.saved[d];
    out->ready = G.done && !gamesize_busy();
    if (out->ready && out->total > out->free) {
        u64 used = out->total - out->free, known = out->games + out->media + out->saved;
        out->other = used > known ? used - known : 0;
    }
}
