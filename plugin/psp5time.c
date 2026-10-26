/* psp5-authentic - helper plugin "psp5time": counts the seconds a game is played while the PSP is awake.
 *
 * psp5 is not running while a game is, and the PSP's own clocks restart when a game starts, so psp5 alone cannot tell sleep and
 * switched-off time from play time. This kernel module is loaded by ARK into the game (PLUGINS.TXT: "game" and "pops" lines),
 * so it runs for exactly as long as the game does. One thread wakes once a second and adds the time since the last wake-up, unless it
 * was much longer than a second: then the PSP slept (the thread did not run), and nothing is added (see awake.c).
 *
 * It finds out which game it is in by itself, so it counts a game that was started from the XMB as well as one started by psp5:
 * a homebrew or PS1 game is named by its folder (the EBOOT.PBP that the PSP started), a PSP game by the DISC_ID in its PARAM.SFO. Those
 * are the names psp5 uses in playtime.txt (entry_cover_key() in scan.c; awake.c has the same rules, and the tests compare the two).
 * The seconds are kept in <psp5's data folder>/played/<name>.txt (see awake.h), every 10 seconds, in place and with the same length each
 * time, so a file cut off by a battery that runs out is at worst a few seconds behind. The data folder is the folder this plugin was
 * loaded from (ARK passes the path of the module to module_start). At its next start psp5 adds the seconds in the files to playtime.txt and
 * removes them; it never writes playtime.txt from here, so a game that is switched off cannot damage the file with all the times.
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
#define SFO_TRIES     10    /* one a second: a game on a disc is not readable the moment the plugins are loaded */
#define SFO_PATH      "disc0:/PSP_GAME/PARAM.SFO"
#define SFO_MAX       8192
#define PATH_MAX_LEN  256
#define THREAD_PRIO   0x10
#define THREAD_STACK  0x4000

static char s_module[PATH_MAX_LEN]; /* where this plugin was loaded from, as ARK says it: "ms0:/PSP5/psp5time.prx" */
static unsigned char s_sfo[SFO_MAX];

static int read_file(const char *path, void *buf, int size)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return -1;
    int n = sceIoRead(fd, buf, size);
    sceIoClose(fd);
    return n;
}

/* the whole file in one write: in place when the file is there, else (or the first time) as a new file */
static void flush(const char *path, unsigned secs, int fresh)
{
    char buf[AWAKE_FILE_MAX];
    int n = awake_format(buf, sizeof(buf), secs);
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
    const char *init = sceKernelInitFileName();
    if (awake_is_psp5(init))
        return 0; /* psp5 itself (as the launcher or as an app) is not a game */

    char key[AWAKE_KEY_MAX], dir[PATH_MAX_LEN], played[PATH_MAX_LEN], path[PATH_MAX_LEN];
    int kind = awake_key_from_path(init, key, sizeof(key));
    if (!kind || !awake_dir_of(s_module, dir, sizeof(dir)))
        return 0; /* not a game, or we do not know where psp5's folder is */

    AwakeClock clk;
    awake_start(&clk, sceKernelGetSystemTimeWide(), 0);

    if (kind == 2) { /* a PSP game: its name is in the game (counting has started; the time until the name is known is counted too) */
        int found = 0;
        for (int tries = 0; tries < SFO_TRIES && !found; tries++) {
            sceKernelDelayThread(TICK_US);
            awake_tick(&clk, sceKernelGetSystemTimeWide());
            int n = read_file(SFO_PATH, s_sfo, SFO_MAX);
            if (n > 0 && awake_key_from_sfo(s_sfo, (unsigned)n, key, AWAKE_GAME_ID_MAX))
                found = 1;
        }
        if (!found)
            return 0;
    }
    if (!awake_played_dir(dir, played, sizeof(played)) || !awake_played_path(dir, key, path, sizeof(path)))
        return 0;

    /* seconds that psp5 has not added yet (an earlier time in this game, or a game that was started again by itself) are kept and added to */
    int fresh = 1;
    unsigned flushed = 0; /* the seconds in the file: the next write is 10 s after them */
    {
        char text[AWAKE_FILE_MAX];
        unsigned had = 0;
        int n = read_file(path, text, sizeof(text));
        if (n > 0 && awake_parse(text, n, &had)) {
            awake_add_secs(&clk, had);
            flushed = clk.secs; /* (not more than the largest number a clock holds) */
            fresh = 0;
        }
    }

    int made_dir = 0;
    for (;;) {
        unsigned secs = awake_tick(&clk, sceKernelGetSystemTimeWide());
        if (secs >= flushed + FLUSH_SECS) {
            if (!made_dir) {
                sceIoMkdir(played, 0777); /* it is there already after the first time: the error is only "exists" */
                made_dir = 1;
            }
            flush(path, secs, fresh);
            fresh = 0;
            flushed = secs;
        }
        sceKernelDelayThread(TICK_US);
    }
    return 0;
}

int module_start(SceSize args, void *argp)
{
    s_module[0] = 0;
    if (argp && args > 0) {
        const char *p = (const char *)argp;
        int i = 0;
        while ((SceSize)i < args && i < PATH_MAX_LEN - 1 && p[i]) {
            s_module[i] = p[i];
            i++;
        }
        s_module[i] = 0;
    }
    SceUID th = sceKernelCreateThread("psp5time", worker, THREAD_PRIO, THREAD_STACK, 0, NULL);
    if (th < 0)
        return 1; /* not resident: nothing was started */
    sceKernelStartThread(th, 0, NULL);
    return 0;
}
