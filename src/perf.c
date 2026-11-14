/* psp5 authentic – see perf.h */
#include <pspkernel.h>
#include <stdio.h>
#include <string.h>
#include "perf.h"

#define MAX_MARKS 96
#define STALL_S 2.0f /* a gap of more than this is not a frame: the PSP slept, or a game ran, or a screen was waiting for a card */

typedef struct {
    char name[24];
    u64 at;
} Mark;

typedef struct {
    unsigned frames, stalls;
    double sum;
    float max, worst[3]; /* the longest three frames, longest first */
    unsigned bucket[5];  /* <= 18 ms (a frame of 60 a second), <= 34, <= 51, <= 100, longer */
} Stat;

static Mark s_marks[MAX_MARKS];
static int s_nmarks;
static Stat s_stat[PF_COUNT];

static const char *const SCREEN[PF_COUNT] = { "Games", "Media", "Library", "Music", "Settings", "Information", "Saved Data", "File Manager", "FTP" };

void perf_mark(const char *what)
{
    if (s_nmarks >= MAX_MARKS)
        return;
    Mark *m = &s_marks[s_nmarks++];
    snprintf(m->name, sizeof(m->name), "%s", what);
    m->at = sceKernelGetSystemTimeWide();
}

void perf_frame(int screen, float seconds)
{
    if (screen < 0 || screen >= PF_COUNT)
        return;
    Stat *s = &s_stat[screen];
    if (seconds > STALL_S) {
        s->stalls++;
        return;
    }
    s->frames++;
    s->sum += seconds;
    if (seconds > s->max)
        s->max = seconds;
    s->bucket[seconds <= 0.018f ? 0 : seconds <= 0.034f ? 1 : seconds <= 0.051f ? 2 : seconds <= 0.100f ? 3 : 4]++;
    for (int i = 0; i < 3; i++)
        if (seconds > s->worst[i]) {
            for (int j = 2; j > i; j--)
                s->worst[j] = s->worst[j - 1];
            s->worst[i] = seconds;
            break;
        }
}

void perf_dump(const char *dir)
{
    char path[64];
    snprintf(path, sizeof(path), "%s/perf.txt", dir);
    SceIoStat st;
    if (sceIoGetstat(path, &st) < 0)
        return; /* not asked for */
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_TRUNC, 0777);
    if (fd < 0)
        return;
    static char buf[8192]; /* (96 marks of 42 letters and 9 screens of about 100) */
    int n = 0;
#define ADD(...) do { if (n < (int)sizeof(buf) - 200) n += snprintf(buf + n, sizeof(buf) - n, __VA_ARGS__); } while (0)
    ADD("psp5 perf: ms since the first step, ms since the step before\n");
    u64 t0 = s_nmarks ? s_marks[0].at : 0;
    for (int i = 0; i < s_nmarks; i++)
        ADD("%-23s %8.1f %8.1f\n", s_marks[i].name, (double)(s_marks[i].at - t0) / 1000.0, i ? (double)(s_marks[i].at - s_marks[i - 1].at) / 1000.0 : 0.0);
    ADD("\nframes: screen, frames, average ms, longest ms, share <=18 / <=34 / <=51 / <=100 / longer ms, three longest, gaps over 2 s\n");
    for (int i = 0; i < PF_COUNT; i++) {
        const Stat *s = &s_stat[i];
        if (!s->frames && !s->stalls)
            continue;
        double f = s->frames ? s->frames : 1;
        ADD("%-14s %6u %6.1f %6.1f  %3.0f%% %3.0f%% %3.0f%% %3.0f%% %3.0f%%  %.0f %.0f %.0f  %u\n", SCREEN[i], s->frames, s->frames ? s->sum / f * 1000.0 : 0.0, s->max * 1000.0, 100.0 * s->bucket[0] / f, 100.0 * s->bucket[1] / f, 100.0 * s->bucket[2] / f,
            100.0 * s->bucket[3] / f, 100.0 * s->bucket[4] / f, s->worst[0] * 1000.0, s->worst[1] * 1000.0, s->worst[2] * 1000.0, s->stalls);
    }
    ADD("\nmemory now: largest free block %d KB, total free %d KB\n", (int)(sceKernelMaxFreeMemSize() / 1024), (int)(sceKernelTotalFreeMemSize() / 1024));
#undef ADD
    sceIoWrite(fd, buf, n);
    sceIoClose(fd);
}
