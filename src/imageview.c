/* psp5 authentic – the image viewer (see imageview.h) */
#include <pspkernel.h>
#include <pspctrl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "imageview.h"
#include "image.h"
#include "gfx.h"
#include "lang.h"
#include "sound.h"
#include "system.h"

#define OVERLAY_SHOW 3.0f /* seconds the overlay is there after a press */
#define OVERLAY_FADE 0.5f /* ... the last of them are its going away */

#define CACHE 4 /* pictures kept ready: the one shown, the one before and after it, and the one that is fading out */

typedef struct {
    int index;    /* -1 = free */
    Texture *tex; /* NULL = it could not be read (it is not tried again until it has been dropped) */
} Slot;

struct ImageView {
    char dir[256];
    const MusicEntry *files;
    int count, index;
    Slot cache[CACHE];
    Texture *tex;      /* the picture that is shown (made to fit the screen; it belongs to the cache) */
    Texture *old;      /* the one before it, while the new one comes up */
    float fade;        /* 0..1: how much of the new picture there is */
    int failed;        /* the picture that was asked for cannot be read */
    float overlay;     /* seconds left of the overlay */
    float time;
    /* the thread that reads pictures: it makes the one that is shown ready first, then the one after it and the one before it, so that L and R do not wait. `want` is the one it is to read now (-1 = none),
     * and the result goes through one slot that the thread does not touch again before it has been emptied */
    SceUID thread;
    volatile int quit;
    volatile int want;
    volatile int reading; /* the one the thread is reading */
    volatile int ready;   /* the slot is full: `img`, `ok` and `done` are the thread's result */
    volatile int ok;
    volatile int done;    /* the number of the picture in the slot */
    Image img;
};

/* how far two pictures of the folder are from each other, round the corner (the last one is next to the first) */
static int distance(const ImageView *v, int a, int b)
{
    int d = abs(a - b);
    return d > v->count - d ? v->count - d : d;
}

static Slot *find(ImageView *v, int index)
{
    for (int i = 0; i < CACHE; i++)
        if (v->cache[i].index == index)
            return &v->cache[i];
    return NULL;
}

/* the texture of a picture that has been read goes in the cache; one that is not wanted any more (it is far from the shown one) is left out, and the farthest one makes room */
static void keep(ImageView *v, int index, Texture *t)
{
    if (find(v, index)) { /* (it is there already: the shown ones are not to be freed under the screen) */
        tex_free(t);
        return;
    }
    Slot *s = NULL;
    for (int i = 0; i < CACHE && !s; i++)
        if (v->cache[i].index < 0)
            s = &v->cache[i];
    if (!s) {
        for (int i = 0; i < CACHE; i++) {
            Slot *c = &v->cache[i];
            if (c->tex && (c->tex == v->tex || c->tex == v->old))
                continue; /* (the shown ones stay) */
            if (!s || distance(v, c->index, v->index) > distance(v, s->index, v->index))
                s = c;
        }
        if (!s || distance(v, s->index, v->index) < distance(v, index, v->index)) {
            tex_free(t); /* every one that is there is nearer: this one is not kept */
            return;
        }
    }
    if (s->index >= 0)
        tex_free(s->tex);
    s->index = index;
    s->tex = t;
}

static int is_png(const char *name)
{
    const char *dot = strrchr(name, '.');
    return dot && !strcasecmp(dot, ".png");
}

/* the next one the thread is to read: the shown one, then the one after it, then the one before it, when they are not in the cache */
static void plan(ImageView *v)
{
    int order[3] = { v->index, (v->index + 1) % v->count, (v->index + v->count - 1) % v->count };
    int n = v->count >= 3 ? 3 : v->count;
    int next = -1;
    for (int i = 0; i < n && next < 0; i++)
        if (!find(v, order[i]) && (i == 0 || !is_png(v->files[order[i]].name))) /* (a PNG cannot be stopped half way: it is only read when it is the one that is wanted on the screen) */
            next = order[i];
    v->want = next;
}

/* the picture that was asked for is shown when it is there: it fades in over the one before it */
static void show(ImageView *v)
{
    Slot *s = find(v, v->index);
    if (!s)
        return; /* (the one before stays until it comes) */
    if (s->tex) {
        if (s->tex != v->tex) {
            v->old = v->tex;
            v->tex = s->tex;
            v->fade = v->old ? 0 : 1;
        }
        v->failed = 0;
    } else {
        v->failed = 1; /* it could not be read: the screen says so, in place of the picture */
        v->overlay = OVERLAY_SHOW;
    }
}

static int cancel_read(void *arg)
{
    ImageView *v = arg;
    return v->quit || v->want != v->reading; /* (the one that is read is not the one that is wanted any more) */
}

static int reader(SceSize args, void *argp)
{
    ImageView *v = *(ImageView **)argp;
    while (!v->quit) {
        int idx = v->want;
        if (v->ready || idx < 0) {
            sceKernelDelayThread(8000);
            continue;
        }
        char path[300];
        snprintf(path, sizeof(path), "%s/%s", v->dir, v->files[idx].name);
        Image img = { 0, 0, NULL };
        v->reading = idx;
        int ok = img_load_fit_cancel(path, SCREEN_W, SCREEN_H, &img, cancel_read, v);
        if (v->quit) {
            img_free(&img);
            break;
        }
        if (!ok && v->want != idx) { /* it was stopped, as another one is wanted now (a picture that cannot be read while it is still the wanted one is a failure) */
            img_free(&img);
            continue;
        }
        v->img = img;
        v->ok = ok;
        v->done = idx;
        __sync_synchronize();
        v->ready = 1;
    }
    return 0;
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
    for (int i = 0; i < CACHE; i++)
        v->cache[i].index = -1;
    plan(v);
    v->thread = sceKernelCreateThread("psp5_image", reader, 0x28, 0x4000, 0, NULL); /* (below the thread of the screen: the buttons and the drawing come before a picture is read) */
    if (v->thread < 0) {
        free(v);
        return NULL;
    }
    sys_fast(1);
    ImageView *self = v;
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
    sys_fast(0);
    img_free(&v->img);
    for (int i = 0; i < CACHE; i++)
        tex_free(v->cache[i].tex);
    free(v);
}

static void go(ImageView *v, int by)
{
    v->index = (v->index + by + v->count) % v->count;
    v->failed = 0;
    show(v);
    plan(v);
    sound_play(SND_MOVE);
}

int imageview_update(ImageView *v, u32 pressed, u32 held, int lx, int ly, float dt)
{
    v->time += dt;
    if (v->overlay > 0)
        v->overlay -= dt;
    if (v->fade < 1)
        v->fade = fminf(1, v->fade + dt * 5);
    if (v->fade >= 1)
        v->old = NULL; /* (it stays in the cache) */
    if (v->ready) { /* the thread has finished one */
        __sync_synchronize();
        int idx = v->done;
        if (distance(v, idx, v->index) <= 1) {
            Texture *t = v->ok && v->img.px ? img_to_texture(&v->img, GU_PSM_8888) : NULL;
            keep(v, idx, t);
            if (idx == v->index)
                show(v);
        }
        img_free(&v->img);
        plan(v);
        __sync_synchronize();
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
