/* psp5 authentic – the video player (see videoplay.h) */
#include <pspkernel.h>
#include <pspctrl.h>
#include <psppower.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "videoplay.h"
#include "video.h"
#include "videomem.h"
#include "gfx.h"
#include "lang.h"
#include "look.h"
#include "popup.h"

#define OVERLAY_SHOW 3.0f
#define OVERLAY_FADE 0.5f
#define SEEK_STEP 10.0f      /* seconds a press of ← or → jumps */
#define SEEK_WAIT 0.4f       /* seconds after the last press before the video jumps (the bar shows where it will go) */
#define SAVE_EVERY 15.0f     /* seconds between two times the place is written down */
#define BAR_X0 84.0f
#define BAR_X1 396.0f
#define BAR_Y 221.0f

#ifdef VIDEO_DEBUG
#define FAIL(v) do { (v)->failed = 1; vlog("failed at line %d\n", __LINE__); } while (0)
#else
#define FAIL(v) ((v)->failed = 1)
#endif

struct VideoPlayer {
    const MusicEntry *files;
    char dir[256];
    int count, index;
    Video *vid;
    int failed;          /* this video cannot be played */
    int ask;             /* the question "continue from ..." is on screen */
    int ask_sel;         /* 0 = continue, 1 = from the beginning */
    float resume_at;
    int len_s;           /* the length in whole seconds, what the memory knows a video by */
    int paused, ended;
    float overlay;
    float time, since_save;
    int rep_dir;
    float rep_hold;
    int seek_pending;
    float seek_target, seek_wait;
    int was_paused;      /* the state before the PSP went to sleep */
    float ask_anim;
};

static void say_time(char *out, int size, float sec)
{
    int s = sec < 0 ? 0 : (int)sec;
    if (s >= 3600)
        snprintf(out, size, "%d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
    else
        snprintf(out, size, "%d:%02d", s / 60, s % 60);
}

static const char *current_name(const VideoPlayer *v)
{
    return v->files[v->index].name;
}

/* writes down where the video is, when it is somewhere in the middle (and forgets it when it was seen to the end) */
static void remember(VideoPlayer *v)
{
    if (!v->vid || v->failed || v->ask || v->len_s <= 0)
        return;
    if (v->ended)
        videomem_forget(current_name(v), v->len_s);
    else
        videomem_set(current_name(v), v->len_s, video_time(v->vid));
}

static void drop(VideoPlayer *v)
{
    if (v->vid)
        video_close(v->vid);
    v->vid = NULL;
}

static void load_current(VideoPlayer *v)
{
    drop(v);
    v->failed = v->ask = v->ended = v->paused = v->seek_pending = 0;
    v->since_save = 0;
    v->overlay = OVERLAY_SHOW;
    char path[300];
    snprintf(path, sizeof(path), "%s/%s", v->dir, current_name(v));
    v->vid = video_open(path);
    if (!v->vid) {
#ifdef VIDEO_DEBUG
        vlog("open failed: %s\n", path);
#endif
        FAIL(v);
        return;
    }
    v->len_s = (int)(video_duration(v->vid) + 0.5f);
    float at = 0;
    if (v->len_s > 0 && videomem_get(current_name(v), v->len_s, &at) && at > 0) { /* it was left in the middle: ask */
        v->ask = 1;
        v->ask_sel = 0;
        v->resume_at = at;
        return;
    }
    if (!video_play(v->vid, 0))
        FAIL(v);
}

VideoPlayer *videoplay_open(const char *dir, const MusicEntry *files, int count, int index)
{
    if (count <= 0 || index < 0 || index >= count)
        return NULL;
    VideoPlayer *v = calloc(1, sizeof(*v));
    if (!v)
        return NULL;
    snprintf(v->dir, sizeof(v->dir), "%s", dir);
    v->files = files;
    v->count = count;
    v->index = index;
    music_stop(); /* the sound is the video's now */
    load_current(v);
    return v;
}

void videoplay_close(VideoPlayer *v)
{
    if (!v)
        return;
    remember(v);
    drop(v);
    free(v);
}

void videoplay_sleep(VideoPlayer *v, int asleep)
{
    if (!v->vid)
        return;
    if (asleep) {
        v->was_paused = v->paused;
        video_pause(v->vid, 1);
    } else {
        video_pause(v->vid, v->was_paused || v->ask);
    }
}

static void go(VideoPlayer *v, int by)
{
    if (v->count < 2)
        return;
    remember(v);
    v->index = (v->index + by + v->count) % v->count;
    load_current(v);
}

static void nudge(VideoPlayer *v, int dir)
{
    float now = v->seek_pending ? v->seek_target : video_time(v->vid);
    float dur = video_duration(v->vid);
    now += dir * SEEK_STEP;
    if (now < 0)
        now = 0;
    if (dur > 1 && now > dur - 1)
        now = dur - 1;
    v->seek_target = now;
    v->seek_pending = 1;
    v->seek_wait = SEEK_WAIT;
}

int videoplay_update(VideoPlayer *v, u32 pressed, u32 held, int lx, int ly, float dt)
{
    v->time += dt;
    if (v->overlay > 0)
        v->overlay -= dt;
    v->ask_anim += ((v->ask ? 1.0f : 0.0f) - v->ask_anim) * fminf(1, dt * 12);
    if (pressed)
        v->overlay = OVERLAY_SHOW;
    if (pressed & PSP_CTRL_CIRCLE)
        return 1;
    if (pressed & PSP_CTRL_LTRIGGER)
        go(v, -1);
    else if (pressed & PSP_CTRL_RTRIGGER)
        go(v, 1);
    if (v->failed || !v->vid)
        return 0;
    if (v->ask) { /* the question: continue where it was left, or from the beginning */
        if (pressed & (PSP_CTRL_UP | PSP_CTRL_DOWN))
            v->ask_sel = !v->ask_sel;
        if (pressed & PSP_CTRL_CROSS) {
            if (v->ask_sel == 1)
                videomem_forget(current_name(v), v->len_s); /* from the beginning: the memory of the place is gone */
            v->ask = 0;
            if (!video_play(v->vid, v->ask_sel == 0 ? v->resume_at : 0))
                FAIL(v);
        }
        return 0;
    }
    if (pressed & PSP_CTRL_CROSS) {
        if (v->ended) { /* played to the end: ✕ plays it again */
            v->ended = 0;
            v->paused = 0;
            video_pause(v->vid, 0);
            if (!video_play(v->vid, 0))
                FAIL(v);
        } else {
            v->paused = !v->paused;
            video_pause(v->vid, v->paused);
        }
    }
    int dir = (held & PSP_CTRL_RIGHT) ? 1 : (held & PSP_CTRL_LEFT) ? -1 : 0;
    if (dir != v->rep_dir) {
        v->rep_dir = dir;
        v->rep_hold = 0;
        if (dir)
            nudge(v, dir);
    } else if (dir) {
        v->rep_hold += dt;
        if (v->rep_hold > 0.4f) {
            v->rep_hold -= 0.15f;
            nudge(v, dir);
        }
    }
    if (dir)
        v->overlay = OVERLAY_SHOW;
    if (v->seek_pending && !dir) {
        v->seek_wait -= dt;
        if (v->seek_wait <= 0) {
            v->seek_pending = 0;
            v->ended = 0;
            if (!video_play(v->vid, v->seek_target)) {
#ifdef VIDEO_DEBUG
                vlog("the jump to %.2f failed\n", v->seek_target);
#endif
                FAIL(v);
            }
            video_pause(v->vid, v->paused);
        }
    }
    if (!v->paused && !v->ask)
        scePowerTick(PSP_POWER_TICK_ALL); /* no button is pressed while a film is watched: the PSP must not dim the screen or go to sleep meanwhile */
    video_update(v->vid, dt);
    if (video_failed(v->vid)) {
#ifdef VIDEO_DEBUG
        vlog("the engine gave up at %.2f\n", video_time(v->vid));
#endif
        FAIL(v);
    }
    if (video_ended(v->vid) && !v->ended) {
        v->ended = 1;
        v->overlay = OVERLAY_SHOW;
        videomem_forget(current_name(v), v->len_s);
    }
    v->since_save += dt;
    if (v->since_save >= SAVE_EVERY && !v->paused && !v->ended) {
        v->since_save = 0;
        remember(v);
    }
    return 0;
}

/* a key sign of the overlay: a light pill with a dark letter */
static void key_sign(float cx, float cy, const char *label, float a)
{
    gfx_round_rect(cx - 13, cy - 8, 26, 16, 5, RGBA(190, 192, 200, (int)(210 * a)));
    text_draw(cx, cy + 4, 0.5f, 1, RGBA(28, 28, 34, (int)(255 * a)), ALIGN_CENTER, label);
}

static void draw_picture(const Texture *t)
{
    if (!t)
        return;
    float k = fminf((float)SCREEN_W / t->w, (float)SCREEN_H / t->h);
    float w = floorf(t->w * k + 0.5f), h = floorf(t->h * k + 0.5f);
    gfx_tex(t, floorf((SCREEN_W - w) / 2), floorf((SCREEN_H - h) / 2), w, h, RGBA(255, 255, 255, 255));
}

static void draw_overlay(const VideoPlayer *v, float a)
{
    gfx_gradient_v(0, 0, SCREEN_W, 56, RGBA(0, 0, 0, (int)(150 * a)), RGBA(0, 0, 0, 0));
    gfx_gradient_v(0, SCREEN_H - 100, SCREEN_W, 100, RGBA(0, 0, 0, 0), RGBA(0, 0, 0, (int)(170 * a)));
    char n[24];
    snprintf(n, sizeof(n), "%d/%d", v->index + 1, v->count);
    text_draw(SCREEN_W / 2, 31, 0.8f, 0, RGBA(238, 238, 242, (int)(255 * a)), ALIGN_CENTER, n);
    if (v->count > 1) {
        key_sign(34, 26, "L", a);
        key_sign(SCREEN_W - 34, 26, "R", a);
    }
    if (!v->vid || v->failed)
        return;
    float dur = video_duration(v->vid), now = v->seek_pending ? v->seek_target : video_time(v->vid);
    float share = dur > 0 ? fminf(1, fmaxf(0, now / dur)) : 0, bw = BAR_X1 - BAR_X0;
    char t[16];
    say_time(t, sizeof(t), now);
    text_draw(32, BAR_Y + 5, 0.62f, 0, RGBA(238, 238, 242, (int)(255 * a)), ALIGN_LEFT, t);
    if (dur > 0) {
        say_time(t, sizeof(t), dur);
        text_draw(SCREEN_W - 32, BAR_Y + 5, 0.62f, 0, RGBA(238, 238, 242, (int)(255 * a)), ALIGN_RIGHT, t);
    }
    look_bar(BAR_X0, BAR_Y - 1.5f, bw, 3, bw * share, a);
    float kx = BAR_X0 + bw * share;
    gfx_circle(kx, BAR_Y, 13, RGBA(236, 236, 240, (int)(255 * a)));
    u32 dark = RGBA(20, 20, 26, (int)(255 * a));
    if (v->paused || v->ended) {
        gfx_triangle(kx - 3.5f, BAR_Y - 5.5f, 9, 11, 1, dark);
    } else {
        gfx_rect(kx - 4, BAR_Y - 5, 3, 10, dark);
        gfx_rect(kx + 1, BAR_Y - 5, 3, 10, dark);
    }
}

static void draw_question(const VideoPlayer *v, float a)
{
    char first[64], t[16];
    say_time(t, sizeof(t), v->resume_at);
    snprintf(first, sizeof(first), L(S_VID_RESUME), t);
    const char *rows[2] = { first, L(S_VID_RESTART) };
    float w = 200;
    for (int k = 0; k < 2; k++)
        w = fmaxf(w, ceilf(text_width(0.62f, 0, rows[k])) + 32);
    w = 2 * ceilf(fminf(w, SCREEN_W - 40) / 2);
    float h = 2 * POPUP_EDGE + 2 * POPUP_ITEM, x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2 + (1 - a) * 10;
    popup_dim(a);
    popup_panel(x, y, w, h, POPUP_DARK, a);
    for (int k = 0; k < 2; k++) {
        float ry = y + POPUP_EDGE + k * POPUP_ITEM;
        u32 col = POPUP_TEXT;
        if (k == v->ask_sel)
            popup_choice(x + POPUP_EDGE, ry, w - 2 * POPUP_EDGE, POPUP_ITEM, a);
        else
            col = COLOR_ALPHA(col, 204);
        text_draw(x + 16, ry + 18, 0.62f, 0, COLOR_ALPHA(col, (int)(((col >> 24) & 0xFF) * a)), ALIGN_LEFT, rows[k]);
    }
}

void videoplay_draw(VideoPlayer *v)
{
    if (v->vid && !v->failed)
        draw_picture(video_picture(v->vid));
    if (v->failed)
        text_draw(SCREEN_W / 2, SCREEN_H / 2 + 5, 0.7f, 0, RGBA(238, 238, 242, 255), ALIGN_CENTER, L(S_VID_CANT));
    float a = v->overlay >= OVERLAY_FADE ? 1 : v->overlay > 0 ? v->overlay / OVERLAY_FADE : 0;
    if (v->failed || v->ask)
        a = fmaxf(a, v->failed ? 1 : 0);
    if (a > 0)
        draw_overlay(v, a);
    if (v->ask_anim > 0.02f)
        draw_question(v, v->ask_anim);
}
