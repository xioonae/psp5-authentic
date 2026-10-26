/* psp5-authentic - helper plugin "psp5time": counts the seconds a game is played while the PSP is awake.
 *
 * psp5 is not running while a game is, and the PSP's own clocks restart when a game starts, so psp5 alone cannot tell sleep and
 * switched-off time from play time. This kernel module is loaded by ARK into the game (PLUGINS.TXT: "game" and "pops" lines),
 * so it runs for exactly as long as the game does. One thread wakes once a second and adds the time since the last wake-up, unless it
 * was much longer than a second: then the PSP slept (the thread did not run), and nothing is added (see awake.c).
 *
 * It only counts for a game that psp5 started: psp5 leaves the file ms0:/PSP5/ingame ("the marker") before it starts a game. The
 * plugin reads the line "rtc N" of it (which game this is) and keeps the seconds in ms0:/PSP5/ingame.awake (see awake.h), every 10 seconds,
 * in place and with the same length each time, so a file cut off by a battery that runs out is at worst a few seconds behind. At its next
 * start psp5 reads that file, uses the seconds in it and removes it. Without a marker the plugin does nothing at all.
 *
 * No libc: only the system calls below and awake.c. */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <pspinit.h>
#include "../src/awake.h"

PSP_MODULE_INFO("psp5time", 0x1007, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

#define TICK_US       1000000
#define FLUSH_SECS    10    /* a game that is shorter is not counted by psp5 anyway */
#define FIND_TRIES    10    /* one a second: the memory stick is there from the start, but a game may not have a marker */
#define THREAD_PRIO   0x10
#define THREAD_STACK  0x4000

/* ms0: is the memory stick; on a PSP Go with the games on the internal storage the marker may be on ef0: */
static const char *const MARKER[] = { "ms0:/PSP5/ingame", "ef0:/PSP5/ingame" };
#define N_MARKER 2

static int read_file(const char *path, char *buf, int size)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return -1;
    int n = sceIoRead(fd, buf, size);
    sceIoClose(fd);
    return n;
}

static void put(char *dst, const char *src, int size)
{
    int i = 0;
    while (src[i] && i < size - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

/* the whole file in one write: in place when the file is there, else (or the first time) as a new file */
static void flush(const char *path, const char *token, unsigned secs, int fresh)
{
    char buf[AWAKE_FILE_MAX];
    int n = awake_format(buf, sizeof(buf), token, secs);
    if (n <= 0)
        return;
    SceUID fd = -1;
    if (!fresh)
        fd = sceIoOpen(path, PSP_O_WRONLY, 0777);
    if (fd < 0)
        fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0)
        return;
    sceIoWrite(fd, buf, n);
    sceIoClose(fd);
}

static int worker(SceSize args, void *argp)
{
    if (awake_is_psp5(sceKernelInitFileName()))
        return 0; /* psp5 itself (as the launcher or as an app): the marker that is there is the one of the game that has just ended */

    AwakeClock clk;
    awake_start(&clk, sceKernelGetSystemTimeWide(), 0);

    /* which game is this? (counting has started; what is found later is added to it) */
    char token[AWAKE_TOKEN_MAX], mpath[32], apath[48];
    int found = 0;
    for (int tries = 0; tries < FIND_TRIES && !found; tries++) {
        sceKernelDelayThread(TICK_US);
        awake_tick(&clk, sceKernelGetSystemTimeWide());
        for (int m = 0; m < N_MARKER && !found; m++) {
            char text[128];
            int n = read_file(MARKER[m], text, sizeof(text));
            if (n > 0 && awake_token(text, n, token, sizeof(token))) {
                put(mpath, MARKER[m], sizeof(mpath));
                found = 1;
            }
        }
    }
    if (!found)
        return 0; /* not started by psp5: nothing to do */

    int len = 0;
    while (mpath[len])
        len++;
    put(apath, mpath, sizeof(apath));
    put(apath + len, ".awake", (int)sizeof(apath) - len);

    /* a game that was started again by itself (a reset) goes on from what it had */
    {
        char text[AWAKE_FILE_MAX], t2[AWAKE_TOKEN_MAX];
        unsigned had = 0;
        int n = read_file(apath, text, sizeof(text));
        if (n > 0 && awake_parse(text, n, t2, sizeof(t2), &had)) {
            int same = 1;
            for (int i = 0; i < AWAKE_TOKEN_MAX && (t2[i] || token[i]); i++)
                if (t2[i] != token[i]) {
                    same = 0;
                    break;
                }
            if (same)
                awake_add_secs(&clk, had);
        }
    }

    unsigned flushed = 0;
    int fresh = 1;
    for (;;) {
        unsigned secs = awake_tick(&clk, sceKernelGetSystemTimeWide());
        if (secs >= flushed + FLUSH_SECS) {
            flush(apath, token, secs, fresh);
            fresh = 0;
            flushed = secs;
        }
        sceKernelDelayThread(TICK_US);
    }
    return 0;
}

int module_start(SceSize args, void *argp)
{
    SceUID th = sceKernelCreateThread("psp5time", worker, THREAD_PRIO, THREAD_STACK, 0, NULL);
    if (th < 0)
        return 1; /* not resident: nothing was started */
    sceKernelStartThread(th, 0, NULL);
    return 0;
}
