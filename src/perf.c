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

#define MAX_EVENTS 64
typedef struct {
    char what[20];
    unsigned a, b;
    float ms;
    u64 at; /* when it ended */
} Event;

static Mark s_marks[MAX_MARKS];
static int s_nmarks;
static Event s_events[MAX_EVENTS]; /* the last MAX_EVENTS, a ring */
static unsigned s_nevents;
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

unsigned long long perf_clock(void)
{
    return sceKernelGetSystemTimeWide();
}

int perf_wanted(const char *dir)
{
    static int s_wanted = -1; /* (asked once: the file is made before psp5 is started) */
    if (s_wanted < 0) {
        char path[64];
        SceIoStat st;
        snprintf(path, sizeof(path), "%s/perf.txt", dir);
        s_wanted = sceIoGetstat(path, &st) >= 0;
    }
    return s_wanted;
}

void perf_event(const char *what, unsigned a, unsigned b, unsigned long long start)
{
    if (!start)
        return;
    u64 now = sceKernelGetSystemTimeWide();
    Event *e = &s_events[__sync_fetch_and_add(&s_nevents, 1) % MAX_EVENTS]; /* (two threads may report) */
    snprintf(e->what, sizeof(e->what), "%s", what);
    e->a = a;
    e->b = b;
    e->ms = (float)(now - start) / 1000.0f;
    e->at = now;
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
    static char buf[12288]; /* (96 marks of 42 letters, 9 screens of about 100 and 64 events of about 60) */
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
    if (s_nevents) {
        ADD("\nwork that took time (the last %d: seconds since the first step, what, two numbers, milliseconds):\n", s_nevents < MAX_EVENTS ? (int)s_nevents : MAX_EVENTS);
        unsigned first = s_nevents > MAX_EVENTS ? s_nevents - MAX_EVENTS : 0;
        for (unsigned i = first; i < s_nevents; i++) {
            const Event *e = &s_events[i % MAX_EVENTS];
            ADD("%8.1f  %-19s %9u %9u %8.1f\n", (double)(e->at - t0) / 1e6, e->what, e->a, e->b, (double)e->ms);
        }
    }
    ADD("\nmemory now: largest free block %d KB, total free %d KB\n", (int)(sceKernelMaxFreeMemSize() / 1024), (int)(sceKernelTotalFreeMemSize() / 1024));
#undef ADD
    sceIoWrite(fd, buf, n);
    sceIoClose(fd);
}
