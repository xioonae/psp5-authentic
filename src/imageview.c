/* psp5 authentic – the image viewer (see imageview.h) */
#include <pspkernel.h>
#include <pspctrl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "imageview.h"
#include "image.h"
#include "gfx.h"
#include "lang.h"
#include "sound.h"

#define OVERLAY_SHOW 3.0f /* seconds the overlay is there after a press */
#define OVERLAY_FADE 0.5f /* ... the last of them are its going away */

struct ImageView {
    char dir[256];
    const MusicEntry *files;
    int count, index;
    Texture *tex;      /* the picture that is shown (made to fit the screen) */
    Texture *old;      /* the one before it, while the new one comes up */
    float fade;        /* 0..1: how much of the new picture there is */
    int failed;        /* the picture that was asked for cannot be read */
    float overlay;     /* seconds left of the overlay */
    float time;
    /* the thread that reads pictures: the last one asked for wins, and the result goes through one slot that the thread does not touch again before it has been emptied */
    SceUID thread;
    volatile int quit;
    volatile int want;    /* serial of the request, 0 = none yet */
    char path[300];       /* the request (written before `want` is) */
    volatile int ready;   /* the slot is full: `img`, `ok` and `handled` are the thread's result */
    volatile int ok;
    volatile int handled;
    Image img;
    int serial;
};

static int reader(SceSize args, void *argp)
{
    ImageView *v = *(ImageView **)argp;
    int last = 0;
    while (!v->quit) {
        int s = v->want;
        if (s == last) {
            sceKernelDelayThread(8000);
            continue;
        }
        char path[300];
        snprintf(path, sizeof(path), "%s", v->path);
        __sync_synchronize();
        Image img = { 0, 0, NULL };
        int ok = img_load_fit(path, SCREEN_W, SCREEN_H, &img);
        last = s;
        if (v->want != s) { /* another one was asked for meanwhile: this one is not wanted */
            img_free(&img);
            continue;
        }
        while (v->ready && !v->quit)
            sceKernelDelayThread(4000);
        if (v->quit) {
            img_free(&img);
            break;
        }
        v->img = img;
        v->ok = ok;
        v->handled = s;
        __sync_synchronize();
        v->ready = 1;
    }
    return 0;
}

static void request(ImageView *v)
{
    v->failed = 0;
    snprintf(v->path, sizeof(v->path), "%s/%s", v->dir, v->files[v->index].name);
    __sync_synchronize();
    v->want = ++v->serial;
}

ImageView *imageview_open(const char *dir, const MusicEntry *files, int count, int index)
{
    if (count <= 0 || index < 0 || index >= count)
        return NULL;
    ImageView *v = calloc(1, sizeof(*v));
    if (!v)
        return NULL;
    snprintf(v->dir, sizeof(v->dir), "%s", dir);
    v->files = files;
    v->count = count;
    v->index = index;
    v->overlay = OVERLAY_SHOW;
    v->fade = 1;
    v->thread = sceKernelCreateThread("psp5_image", reader, 0x20, 0x4000, 0, NULL);
    if (v->thread < 0) {
        free(v);
        return NULL;
    }
    ImageView *self = v;
    request(v);
    sceKernelStartThread(v->thread, sizeof(self), &self);
    return v;
}

void imageview_close(ImageView *v)
{
    if (!v)
        return;
    v->quit = 1;
    sceKernelWaitThreadEnd(v->thread, NULL);
    sceKernelDeleteThread(v->thread);
    img_free(&v->img);
    tex_free(v->tex);
    tex_free(v->old);
    free(v);
}

static void go(ImageView *v, int by)
{
    v->index = (v->index + by + v->count) % v->count;
    request(v);
    sound_play(SND_MOVE);
}

int imageview_update(ImageView *v, u32 pressed, u32 held, int lx, int ly, float dt)
{
    v->time += dt;
    if (v->overlay > 0)
        v->overlay -= dt;
    if (v->fade < 1)
        v->fade = fminf(1, v->fade + dt * 5);
    if (v->fade >= 1 && v->old) {
        tex_free(v->old);
        v->old = NULL;
    }
    if (v->ready) { /* the thread has finished one: it is the picture when it is the one asked for last, else it is thrown away */
        __sync_synchronize();
        if (v->handled == v->serial) {
            Texture *t = v->ok && v->img.px ? img_to_texture(&v->img, GU_PSM_8888) : NULL;
            if (t) {
                tex_free(v->old);
                v->old = v->tex;
                v->tex = t;
                v->fade = v->old ? 0 : 1;
                v->failed = 0;
            } else {
                v->failed = 1; /* it could not be read: the screen says so, in place of the picture */
                v->overlay = OVERLAY_SHOW;
            }
        }
        img_free(&v->img);
        v->ready = 0;
    }
    if (pressed & PSP_CTRL_CIRCLE)
        return 1;
    if ((pressed & (PSP_CTRL_LTRIGGER | PSP_CTRL_LEFT)) && v->count > 1)
        go(v, -1);
    else if ((pressed & (PSP_CTRL_RTRIGGER | PSP_CTRL_RIGHT)) && v->count > 1)
        go(v, 1);
    if (pressed)
        v->overlay = OVERLAY_SHOW; /* a press shows the overlay again */
    return 0;
}

/* a key sign of the overlay: a light pill with a dark letter */
static void key_sign(float cx, float cy, const char *label, float a)
{
    gfx_round_rect(cx - 13, cy - 8, 26, 16, 5, RGBA(190, 192, 200, (int)(210 * a)));
    text_draw(cx, cy + 4, 0.5f, 1, RGBA(28, 28, 34, (int)(255 * a)), ALIGN_CENTER, label);
}

/* the picture, as large as the screen lets it be (also a small one, made bigger), in the middle */
static void draw_picture(const Texture *t, float alpha)
{
    if (!t || alpha <= 0)
        return;
    float k = fminf((float)SCREEN_W / t->w, (float)SCREEN_H / t->h);
    float w = floorf(t->w * k + 0.5f), h = floorf(t->h * k + 0.5f);
    gfx_tex(t, floorf((SCREEN_W - w) / 2), floorf((SCREEN_H - h) / 2), w, h, RGBA(255, 255, 255, (int)(255 * alpha)));
}

void imageview_draw(ImageView *v)
{
    if (!v->failed) { /* (a picture that cannot be read is not replaced by the one before it) */
        if (v->old)
            draw_picture(v->old, 1);
        draw_picture(v->tex, v->old ? v->fade : 1);
    }
    if (!v->tex && !v->failed)
        text_draw(SCREEN_W / 2, SCREEN_H / 2 + 5, 0.7f, 0, RGBA(150, 152, 162, 255), ALIGN_CENTER, L(S_LOADING));
    if (v->failed)
        text_draw(SCREEN_W / 2, SCREEN_H / 2 + 5, 0.7f, 0, RGBA(238, 238, 242, 255), ALIGN_CENTER, L(S_IMG_CANT));
    float a = v->overlay >= OVERLAY_FADE ? 1 : v->overlay > 0 ? v->overlay / OVERLAY_FADE : 0;
    if (a <= 0)
        return;
    gfx_gradient_v(0, 0, SCREEN_W, 56, RGBA(0, 0, 0, (int)(150 * a)), RGBA(0, 0, 0, 0)); /* so that the overlay can be read on a light picture */
    char n[24];
    snprintf(n, sizeof(n), "%d/%d", v->index + 1, v->count);
    text_draw(SCREEN_W / 2, 31, 0.8f, 0, RGBA(238, 238, 242, (int)(255 * a)), ALIGN_CENTER, n);
    key_sign(34, 26, "L", a);
    key_sign(SCREEN_W - 34, 26, "R", a);
}
