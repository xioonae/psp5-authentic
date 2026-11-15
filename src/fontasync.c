/* psp5 authentic – the Japanese font loaded in a thread (see gfx.c, font_ensure_cjk_later) */
#include <pspkernel.h>
#include <intraFont.h>
#include "gfx.h"
#include "perf.h"

static SceUID s_thread = -1;
static intraFont *volatile s_loaded;
static volatile int s_state; /* 0 nothing, 1 loading, 2 loaded (fontasync_poll takes it), 3 it could not be loaded (not tried again) */

static int load_thread(SceSize args, void *argp)
{
    unsigned long long t0 = PERF_START();
    intraFont *f = intraFontLoad("flash0:/font/jpn0.pgf", INTRAFONT_STRING_UTF8);
    PERF_END("font: jpn, thread", 0, f != NULL, t0);
    s_loaded = f;
    __sync_synchronize();
    s_state = f ? 2 : 3;
    return 0;
}

static void join(void)
{
    if (s_thread >= 0) {
        sceKernelWaitThreadEnd(s_thread, NULL);
        sceKernelDeleteThread(s_thread);
        s_thread = -1;
    }
}

void fontasync_start(void)
{
    if (s_state)
        return;
    s_state = 1;
    SceUID th = sceKernelCreateThread("psp5_cjk_font", load_thread, 0x30, 0x4000, PSP_THREAD_ATTR_USER, NULL);
    if (th >= 0 && sceKernelStartThread(th, 0, NULL) >= 0) {
        s_thread = th;
        return;
    }
    if (th >= 0)
        sceKernelDeleteThread(th);
    s_state = 0;
    font_ensure_cjk(); /* no thread: here */
}

/* once a frame (gfx_begin): a font that the thread has loaded is put in use */
void fontasync_poll(void)
{
    if (s_state < 2)
        return;
    join();
    if (s_state == 2) {
        if (!font_attach_cjk(s_loaded) && s_loaded)
            intraFontUnload(s_loaded); /* (it was loaded here meanwhile) */
        s_loaded = NULL;
        s_state = 0;
    }
}

/* font_ensure_cjk(): wait for the thread when it is at it, and take what it loaded */
void fontasync_wait(void)
{
    if (s_state == 1) {
        join();
        s_state = s_loaded ? 2 : 3;
    }
    if (s_state == 2)
        fontasync_poll();
}

/* font_unload(): drop what the thread loaded */
void fontasync_cancel(void)
{
    join();
    if (s_loaded)
        intraFontUnload(s_loaded);
    s_loaded = NULL;
    s_state = 0;
}
