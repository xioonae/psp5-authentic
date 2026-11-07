/* psp5 – made by Mille Weber */
/* The Music screen: the songs on the left as the file manager lists files, the player on the right (see musicui.h). */
#include <pspkernel.h>
#include <pspctrl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "musicui.h"
#include "lang.h"
#include "loader.h"
#include "look.h"
#include "popup.h"
#include "osd.h"
#include "sound.h"
#include "system.h"

/* layout (480 x 272): the list as in the file manager (rows of 24, the same frame and the same faint lines), the player in the panel at its right */
#define LIST_X     24.0f
#define LIST_W     266.0f
#define LIST_TOP   62.0f
#define ROW_H      24.0f
#define ROWS       7
#define HEAD_Y     52.0f   /* the line above the list: where it is */
#define PANEL_X    304.0f
#define PANEL_W    152.0f
#define PANEL_Y    44.0f
#define PANEL_H    198.0f
#define COVER_S    70.0f
#define VOLUME_MAX 30      /* the PSP's volume goes from 0 to 30, as in Settings */
#define SEEK_STEP  10.0f   /* seconds a press of ← → on the bar jumps */

#define TEXT_MAIN RGBA(238, 238, 242, 255)
#define TEXT_DIM  RGBA(160, 162, 172, 255)
#define LIGHT     RGBA(236, 236, 240, 255)
#define DARK      RGBA(20, 20, 26, 255)

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float approach(float cur, float target, float speed, float dt) { return cur + (target - cur) * (1.0f - expf(-speed * dt)); }
static u32 fade(u32 c, float a) { return COLOR_ALPHA(c, (int)(((c >> 24) & 0xFF) * clampf(a, 0, 1))); }

/* ---------------------------------------------------------------------- */
/* the folders */

void musicui_init(MusicUi *m)
{
    memset(m, 0, sizeof(*m));
    m->cover_gen = -1;
}

static void free_list(MusicUi *m)
{
    free(m->e);
    m->e = NULL;
    m->n = 0;
}

void musicui_free(MusicUi *m)
{
    free_list(m);
    tex_free(m->cover);
    m->cover = NULL;
}

/* the folder `dir` is the one that is open, with the lines `e` (n of them, or -1: it could not be read; the list is ours from now on) */
static void adopt_dir(MusicUi *m, const char *dir, MusicEntry *e, int n)
{
    free_list(m);
    snprintf(m->dir, sizeof(m->dir), "%s", dir);
    m->e = e;
    m->n = n;
    m->ok = n >= 0;
    if (m->n < 0)
        m->n = 0;
    m->sel = 0;
    m->scroll = 0;
    m->name_t = 0;
}

/* lists the folder `dir` (a folder that cannot be read is an empty list) */
static void load_dir(MusicUi *m, const char *dir)
{
    MusicEntry *e = NULL;
    int n = music_list_dir(dir, &e);
    adopt_dir(m, dir, e, n);
}

static int same_lines(const MusicEntry *a, int na, const MusicEntry *b, int nb)
{
    if (na != nb || na <= 0)
        return 0;
    for (int i = 0; i < na; i++)
        if (a[i].dir != b[i].dir || strcmp(a[i].name, b[i].name))
            return 0;
    return 1;
}

/* A folder whose lines are those of the folder it is in, or those of the top of its storage, is not a folder inside this one: the PSP answered with the wrong folder (the path of a card whose
 * folder entry points to the top of the Memory Stick gets the top of the Memory Stick for any name below it, however deep). Entering it would only make the path longer every time. */
static int answers_with_another_folder(const MusicUi *m, const MusicEntry *e, int n)
{
    if (same_lines(e, n, m->e, m->n))
        return 1;
    const char *colon = strchr(m->dir, ':');
    if (!colon || colon - m->dir > 8)
        return 0;
    char top[16];
    snprintf(top, sizeof(top), "%.*s/", (int)(colon - m->dir) + 1, m->dir);
    MusicEntry *t = NULL;
    int nt = music_list_dir(top, &t);
    int same = nt > 0 && same_lines(e, n, t, nt);
    free(t);
    return same;
}

static int dir_is_under_a_root(const MusicUi *m, const char *dir)
{
    for (int i = 0; i < m->nroots; i++)
        if (!strncmp(dir, m->roots[i], strlen(m->roots[i])))
            return 1;
    return 0;
}

void musicui_open(MusicUi *m)
{
    char keep[256];
    int had = m->level == 1 && m->dir[0];
    snprintf(keep, sizeof(keep), "%s", m->dir);
    int keep_sel = m->sel;
    m->nroots = music_roots(m->roots);
    m->warn_t = 0;
    m->focus = 0;
    m->popup = 0;
    m->popup_anim = 0;
    m->pbtn = MU_PLAY;
    free_list(m);
    if (had && dir_is_under_a_root(m, keep)) { /* where the screen was left */
        load_dir(m, keep);
        if (m->ok) {
            m->level = 1;
            m->sel = keep_sel < m->n ? keep_sel : 0;
            m->scroll = m->sel > ROWS - 1 ? (float)(m->sel - (ROWS - 1)) : 0;
            return;
        }
    }
    m->depth = 0;
    m->sel = 0;
    m->scroll = 0;
    if (m->nroots == 1) { /* the one MUSIC folder there is: straight into it */
        load_dir(m, m->roots[0]);
        m->level = 1;
    } else {
        m->level = 0;
        m->dir[0] = 0;
        m->n = m->nroots;
        m->ok = 1;
    }
}

/* the number of lines that the list has now */
static int lines(const MusicUi *m)
{
    return m->level == 0 ? m->nroots : m->n;
}

/* ○: up one folder; false when there is no folder above (the screen is to be closed) */
static int go_up(MusicUi *m)
{
    if (m->level == 0)
        return 0;
    if (m->depth > 0) {
        char *slash = strrchr(m->dir, '/');
        int sel = m->sel_stack[m->depth - 1];
        if (slash)
            *slash = 0;
        char parent[256];
        snprintf(parent, sizeof(parent), "%s", m->dir);
        m->depth--;
        load_dir(m, parent);
        m->sel = sel < m->n ? sel : 0;
        m->scroll = m->sel > ROWS - 1 ? (float)(m->sel - (ROWS - 1)) : 0;
        return 1;
    }
    if (m->nroots >= 2) { /* back to the storages, on the one that was open */
        int which = 0;
        for (int i = 0; i < m->nroots; i++)
            if (!strncmp(m->dir, m->roots[i], strlen(m->roots[i])))
                which = i;
        free_list(m);
        m->level = 0;
        m->dir[0] = 0;
        m->n = m->nroots;
        m->sel = which;
        m->scroll = which > ROWS - 1 ? (float)(which - (ROWS - 1)) : 0;
        return 1;
    }
    return 0;
}

/* ✕ on a line of the list */
static void activate(MusicUi *m)
{
    if (m->level == 0) {
        if (m->sel < 0 || m->sel >= m->nroots)
            return;
        char root[16];
        snprintf(root, sizeof(root), "%s", m->roots[m->sel]);
        m->sel_stack[0] = m->sel; /* (not needed to go back: ○ finds the storage by its name) */
        load_dir(m, root);
        m->level = 1;
        m->depth = 0;
        sound_play(SND_SELECT);
        return;
    }
    if (m->sel < 0 || m->sel >= m->n)
        return;
    const MusicEntry *e = &m->e[m->sel];
    if (e->dir) {
        if (m->depth >= 15)
            return;
        char sub[256];
        if (snprintf(sub, sizeof(sub), "%s/%s", m->dir, e->name) >= (int)sizeof(sub))
            return;
        MusicEntry *ne = NULL;
        int nn = music_list_dir(sub, &ne);
        if (nn > 0 && answers_with_another_folder(m, ne, nn)) {
            free(ne);
            m->warn_t = 4;
            sound_play(SND_ERROR);
            return;
        }
        m->sel_stack[m->depth++] = m->sel;
        adopt_dir(m, sub, ne, nn);
        sound_play(SND_SELECT);
    } else {
        music_play_folder(m->dir, m->e, m->n, m->sel);
        sound_play(SND_SELECT);
    }
}

/* ---------------------------------------------------------------------- */
/* input */

/* a direction with repeat: ±1 = left / right, ±2 = up / down. A new press moves at once, a held one repeats. */
static int step(MusicUi *m, u32 held, int lx, int ly, float dt)
{
    int dir = 0;
    if ((held & PSP_CTRL_LEFT) || lx < 40) dir = -1;
    if ((held & PSP_CTRL_RIGHT) || lx > 215) dir = 1;
    if ((held & PSP_CTRL_UP) || ly < 40) dir = -2;
    if ((held & PSP_CTRL_DOWN) || ly > 215) dir = 2;
    if (!dir) {
        m->rep_dir = 0;
        return 0;
    }
    if (dir != m->rep_dir) {
        m->rep_dir = dir;
        m->rep_timer = 0.32f;
        m->rep_held = 0;
        return dir;
    }
    m->rep_held += dt;
    m->rep_timer -= dt;
    if (m->rep_timer <= 0) {
        m->rep_timer += m->rep_held > 1.2f ? 0.045f : 0.085f;
        return dir;
    }
    return 0;
}

/* The rows of the "..." popup: Shuffle, Repeat and the Equalizer (when the system can say it: the same values, the same words and the same calls as Settings > Screen and Sound). ✕ on a row opens a
 * popup with its values, like Settings does for a choice. */
enum { ROW_SHUFFLE, ROW_REPEAT, ROW_EQ };
static int more_rows(void) { return sys_equalizer() >= 0 ? 3 : 2; }
static int row_values(int row) { return row == ROW_SHUFFLE ? 2 : row == ROW_REPEAT ? 3 : 5; }
static int row_value(int row)
{
    int v = row == ROW_SHUFFLE ? (music_shuffle() ? 1 : 0) : row == ROW_REPEAT ? music_repeat() : sys_equalizer();
    return (int)clampf((float)v, 0, (float)(row_values(row) - 1));
}
static void row_set(int row, int v)
{
    if (row == ROW_SHUFFLE)
        music_set_shuffle(v);
    else if (row == ROW_REPEAT)
        music_set_repeat(v);
    else
        sys_set_equalizer(v);
}
static const char *row_name(int row) { return L(row == ROW_SHUFFLE ? S_MU_SHUFFLE : row == ROW_REPEAT ? S_MU_REPEAT : S_EQUALIZER); }
static const char *row_text(int row, int v)
{
    static const StrId on_off[2] = { S_OFF, S_ON }, rep[3] = { S_OFF, S_MU_REPEAT_ONE, S_ALL }, eq[5] = { S_OFF, S_EQ_HEAVY, S_EQ_POPS, S_EQ_JAZZ, S_EQ_UNIQUE };
    return L(row == ROW_SHUFFLE ? on_off[v & 1] : row == ROW_REPEAT ? rep[v % 3] : eq[v % 5]);
}

static void popup_open(MusicUi *m, int which)
{
    m->popup = which;
    m->popup_sel = 0;
    m->choice_sel = 0;
    if (which == MU_POPUP_VOLUME)
        osd_show_volume(); /* the launcher's own volume bar, the one the volume buttons show */
    sound_play(SND_SELECT);
}

/* a jump on the bar: 10 seconds either way (not past the end: that would be the next song) */
static void jump(MusicUi *m, int dir)
{
    if (music_state() == MUSIC_STOPPED)
        return;
    float dur = music_duration(), from = m->seek_show > 0 ? m->seek_to : music_elapsed();
    float to = from + dir * SEEK_STEP;
    if (to < 0)
        to = 0;
    if (dur > 1 && to > dur - 1)
        to = dur - 1;
    m->seek_to = to;
    m->seek_show = 0.6f;
    music_seek(to);
}

/* a button of the player was pressed */
static void press_button(MusicUi *m)
{
    switch (m->pbtn) {
    case MU_VOLUME:
        popup_open(m, MU_POPUP_VOLUME);
        break;
    case MU_PREV:
        if (music_state() != MUSIC_STOPPED) {
            music_prev();
            sound_play(SND_SELECT);
        }
        break;
    case MU_PLAY:
        if (music_state() != MUSIC_STOPPED) {
            music_toggle_pause();
            sound_play(SND_SELECT);
        }
        break;
    case MU_NEXT:
        if (music_state() != MUSIC_STOPPED) {
            music_next();
            sound_play(SND_SELECT);
        }
        break;
    case MU_MORE:
        popup_open(m, MU_POPUP_MORE);
        break;
    }
}

/* The popups. Volume: the launcher's volume bar is shown and ← → change the volume (the system's, as the volume buttons do); ✕ ○ or START ends it. "...": ↑ ↓ choose a row, ✕ opens
 * the values of the row (↑ ↓ choose, ✕ takes the value and goes back to the rows, ○ goes back and keeps the old one), ○ or START closes. */
static void update_popup(MusicUi *m, u32 pressed, int mv)
{
    if (m->popup == MU_POPUP_VOLUME) {
        osd_show_volume(); /* (the bar stays while the volume is being changed) */
        if (pressed & (PSP_CTRL_CROSS | PSP_CTRL_CIRCLE | PSP_CTRL_START)) {
            m->popup = 0;
            sound_play(SND_BACK);
            return;
        }
        int v = sys_volume();
        if (v >= 0 && (mv == -1 || mv == 1)) {
            int to = (int)clampf((float)(v + mv), 0, VOLUME_MAX);
            if (to != v) {
                sys_set_volume(to);
                sound_play(SND_MOVE);
            }
        }
        return;
    }
    if (m->popup == MU_POPUP_CHOICE) {
        int n = row_values(m->popup_sel);
        if (mv == -2 || mv == 2) {
            int next = (int)clampf((float)(m->choice_sel + (mv > 0 ? 1 : -1)), 0, (float)(n - 1));
            if (next != m->choice_sel) {
                m->choice_sel = next;
                sound_play(SND_MOVE);
            }
        }
        if (pressed & PSP_CTRL_CROSS) {
            row_set(m->popup_sel, m->choice_sel);
            m->popup = MU_POPUP_MORE;
            sound_play(SND_SELECT);
        } else if (pressed & PSP_CTRL_CIRCLE) {
            m->popup = MU_POPUP_MORE;
            sound_play(SND_BACK);
        } else if (pressed & PSP_CTRL_START) {
            m->popup = 0;
            sound_play(SND_BACK);
        }
        return;
    }
    /* "...": the rows */
    if (pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_START)) {
        m->popup = 0;
        sound_play(SND_BACK);
        return;
    }
    if (mv == -2 || mv == 2) {
        int next = (int)clampf((float)(m->popup_sel + (mv > 0 ? 1 : -1)), 0, (float)(more_rows() - 1));
        if (next != m->popup_sel) {
            m->popup_sel = next;
            sound_play(SND_MOVE);
        }
    } else if (pressed & PSP_CTRL_CROSS) {
        m->choice_sel = row_value(m->popup_sel);
        m->popup = MU_POPUP_CHOICE;
        sound_play(SND_SELECT);
    }
}

/* the player has the focus: ← → over the buttons, ↑ selects the bar (← → then jump), ↓ goes back to the buttons, ✕ presses a button */
static void player_input(MusicUi *m, u32 pressed, int mv)
{
    if (m->pbtn == MU_BAR) {
        if (mv == -1 || mv == 1)
            jump(m, mv);
        else if (mv == 2) {
            m->pbtn = MU_PLAY;
            sound_play(SND_MOVE);
        }
        return;
    }
    if (mv == -1 || mv == 1) {
        int next = (int)clampf((float)(m->pbtn + mv), 0, MU_MORE);
        if (next != m->pbtn) {
            m->pbtn = next;
            sound_play(SND_MOVE);
        }
    } else if (mv == -2) {
        m->pbtn = MU_BAR;
        sound_play(SND_MOVE);
    }
    if (pressed & PSP_CTRL_CROSS)
        press_button(m);
}

int musicui_update(MusicUi *m, u32 pressed, u32 held, int lx, int ly, float dt)
{
    m->time += dt;
    m->name_t += dt;
    if (m->warn_t > 0)
        m->warn_t -= dt;
    if (m->seek_show > 0)
        m->seek_show -= dt;
    m->popup_anim = clampf(m->popup_anim + (m->popup ? 7 : -7) * dt, 0, 1);
    int mv = step(m, held, lx, ly, dt);
    if (m->popup) {
        update_popup(m, pressed, mv);
        return MUSICUI_NONE;
    }
    if (pressed & PSP_CTRL_TRIANGLE) { /* between the list and the player */
        m->focus = !m->focus;
        if (m->focus && m->pbtn == MU_BAR)
            m->pbtn = MU_PLAY;
        sound_play(SND_MOVE);
        return MUSICUI_NONE;
    }
    if (m->focus) { /* the player */
        if (pressed & PSP_CTRL_CIRCLE) {
            m->focus = 0;
            sound_play(SND_BACK);
            return MUSICUI_NONE;
        }
        player_input(m, pressed, mv);
        return MUSICUI_NONE;
    }
    /* the list */
    int n = lines(m);
    if (pressed & PSP_CTRL_CIRCLE)
        return go_up(m) ? MUSICUI_NONE : MUSICUI_CLOSE;
    if (mv == -2 && m->rep_held == 0 && (n == 0 || m->sel == 0)) /* a new press, not a held button */
        return MUSICUI_TOP;
    if ((mv == -2 || mv == 2) && n) {
        int next = (int)clampf((float)(m->sel + (mv > 0 ? 1 : -1)), 0, (float)(n - 1));
        if (next != m->sel) {
            m->sel = next;
            m->name_t = 0;
            sound_play(SND_MOVE);
        }
    }
    if ((pressed & PSP_CTRL_CROSS) && n)
        activate(m);
    float target = m->scroll;
    if (m->sel < (int)target)
        target = (float)m->sel;
    if (m->sel > (int)target + ROWS - 1)
        target = (float)(m->sel - (ROWS - 1));
    m->scroll = approach(m->scroll, target, 14.0f, dt);
    if (fabsf(m->scroll - target) < 0.001f)
        m->scroll = target;
    return MUSICUI_NONE;
}

/* ---------------------------------------------------------------------- */
/* the cover of the song that plays */

void musicui_release_cover(MusicUi *m)
{
    tex_free(m->cover);
    m->cover = NULL;
    m->cover_gen = -1;
}

void musicui_update_cover(MusicUi *m, float dt)
{
    int gen = music_generation();
    Track t;
    int have = music_track(&t);
    if (!have && m->cover_gen != -1) { /* nothing plays: no cover */
        tex_free(m->cover);
        m->cover = NULL;
        m->cover_gen = -1;
    } else if (have && gen != m->cover_gen) {
        tex_free(m->cover);
        m->cover = NULL;
        m->cover_gen = gen;
        m->cover_fade = 0;
        loader_request_cover(gen, t.path, t.album);
    }
    int serial;
    Texture *tex;
    if (loader_take_cover(&serial, &tex)) {
        if (serial == m->cover_gen && !m->cover)
            m->cover = tex;
        else
            tex_free(tex); /* the song changed while it was read */
    }
    if (m->cover)
        m->cover_fade = clampf(m->cover_fade + dt * 4, 0, 1);
}

/* ---------------------------------------------------------------------- */
/* drawing */

/* m:ss (h:mm:ss from an hour on) */
static void time_text(char *out, int size, float sec)
{
    int s = sec < 0 ? 0 : (int)sec;
    if (s >= 3600)
        snprintf(out, size, "%d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60);
    else
        snprintf(out, size, "%d:%02d", s / 60, s % 60);
}

/* pause (two bars of the same whole-pixel width) or play (a triangle), centred on (cx, cy) */
static void play_glyph(float cx, float cy, float r, int paused, u32 color)
{
    if (paused) {
        float bw = fmaxf(1, floorf(r * 0.38f + 0.5f)), bh = floorf(r * 1.2f + 0.5f);
        float gap = fmaxf(1, floorf(r * 0.34f + 0.5f)), top = floorf(cy - bh / 2 + 0.5f);
        float left = floorf(cx - (bw * 2 + gap) / 2 + 0.5f);
        gfx_rect(left, top, bw, bh, color);
        gfx_rect(left + bw + gap, top, bw, bh, color);
    } else {
        gfx_triangle(cx - r * 0.42f, cy - r * 0.62f, r * 1.1f, r * 1.24f, 1, color);
    }
}

/* the centres of the five buttons in a row `y`, relative to the panel's left edge */
static const float BTN_X[5] = { 19, 47, 76, 105, 133 };
#define BTN_Y     178.0f
#define BTN_R     14.0f  /* the white disc of the chosen button */
#define BAR_Y     152.0f
#define BAR_X     12.0f

/* the player in the box (px, py): `focus` = the player has the focus (its chosen button is a white disc with a dark symbol, as the cog wheel and the buttons of Games are when they are
 * chosen); `hint` = show the △ button at the top right (the Music screen: it moves between the list and the player: dark with a light △ while the list has the focus, the other way
 * round, like the hints of every screen, in the player) */
static void draw_player(MusicUi *m, float px, float py, float a, int focus, int hint, int panel)
{
    Track t;
    int have = music_track(&t) && music_state() != MUSIC_STOPPED;
    int state = music_state();
    gfx_round_rect(px, py, PANEL_W, PANEL_H, 10, RGBA(10, 12, 18, (int)(panel * a)));
    text_draw(px + 12, py + 18, 0.5f, 0, fade(TEXT_DIM, a), ALIGN_LEFT, L(S_TAB_MUSIC));
    if (hint) {
        u32 plate = focus ? LOOK_HINT_PLATE : LOOK_HINT_MARK, mark = focus ? LOOK_HINT_MARK : LOOK_HINT_PLATE;
        gfx_button(px + PANEL_W - 20, py + 14, 6.5f, BTN_TRIANGLE, look_fade(plate, a), look_fade(mark, a));
    }

    /* the cover, or a square with a note on it */
    float cx = px + (PANEL_W - COVER_S) / 2, cy = py + 28;
    gfx_glow(cx, cy + 3, COVER_S, COVER_S, 8, RGBA(0, 0, 0, (int)(110 * a)));
    gfx_round_rect(cx, cy, COVER_S, COVER_S, 6, RGBA(52, 54, 64, (int)((have ? 255 : 150) * a)));
    gfx_note(cx + COVER_S / 2, cy + COVER_S / 2, 30, RGBA(255, 255, 255, (int)((have ? 200 : 100) * a)));
    if (m->cover && have)
        gfx_tex(m->cover, cx, cy, COVER_S, COVER_S, RGBA(255, 255, 255, (int)(255 * a * clampf(m->cover_fade, 0, 1))));

    /* the title and the artist */
    static char ptitle[100], partist[72];
    static int pgen = -2, pstate = -2;
    static const char *plang;
    static float pw;
    if (pgen != music_generation() || pstate != have || plang != L(S_UNKNOWN_ARTIST) || pw != PANEL_W) {
        pgen = music_generation();
        pstate = have;
        plang = L(S_UNKNOWN_ARTIST);
        pw = PANEL_W;
        text_ellipsis(ptitle, sizeof(ptitle), have ? t.title : L(S_NOTHING_PLAYING), 0.6f, PANEL_W - 24);
        text_ellipsis(partist, sizeof(partist), have ? (t.artist[0] ? t.artist : L(S_UNKNOWN_ARTIST)) : "", 0.5f, PANEL_W - 24);
    }
    text_draw(px + 12, py + 122, 0.6f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, ptitle);
    text_draw(px + 12, py + 136, 0.5f, 0, fade(TEXT_DIM, a), ALIGN_LEFT, partist);

    /* the bar, with the time played at its left and the length at its right */
    float el = m->seek_show > 0 ? m->seek_to : music_elapsed(), du = music_duration();
    float bw = PANEL_W - 2 * BAR_X, bx = px + BAR_X, by = py + BAR_Y;
    look_bar(bx, by, bw, 3, have && du > 0 ? fmaxf(3, bw * clampf(el / du, 0, 1)) : 0, a);
    char e[16], d[16];
    time_text(e, sizeof(e), have ? el : 0);
    text_draw(bx, by - 5, 0.42f, 0, fade(TEXT_DIM, a), ALIGN_LEFT, e);
    if (have && du > 0) {
        time_text(d, sizeof(d), du);
        text_draw(bx + bw, by - 5, 0.42f, 0, fade(TEXT_DIM, a), ALIGN_RIGHT, d);
    }
    if (focus && m->pbtn == MU_BAR && have && du > 0) /* the bar is chosen: a dot where the song is now (no frame round it) */
        gfx_circle(bx + bw * clampf(el / du, 0, 1), by + 1.5f, 4.5f, fade(LIGHT, a));

    /* the buttons: volume, previous, play / pause, next, more. Round, with no frame: the chosen one is a white disc with a dark symbol, the others are only their symbols */
    float by2 = py + BTN_Y;
    for (int k = 0; k <= MU_MORE; k++) {
        float bx2 = px + BTN_X[k];
        int chosen = focus && m->pbtn == k;
        if (chosen)
            gfx_circle(bx2, by2, BTN_R, fade(LIGHT, a));
        u32 ic = fade(chosen ? DARK : TEXT_MAIN, a);
        if (k == MU_VOLUME) {
            gfx_speaker_scaled(bx2 - 3.8f, by2, 0.85f, ic);
        } else if (k == MU_PREV) {
            gfx_rect(bx2 - 5, by2 - 4, 2, 8, ic);
            gfx_triangle(bx2 - 3, by2 - 4, 8, 8, -1, ic);
        } else if (k == MU_PLAY) {
            play_glyph(bx2, by2, 5.4f, state == MUSIC_PLAYING, ic);
        } else if (k == MU_NEXT) {
            gfx_triangle(bx2 - 5, by2 - 4, 8, 8, 1, ic);
            gfx_rect(bx2 + 3, by2 - 4, 2, 8, ic);
        } else {
            for (int d = -1; d <= 1; d++)
                gfx_circle(bx2 + d * 5.5f, by2, 1.6f, ic);
        }
    }
}

static void draw_row_icon(float x, float y, const MusicEntry *e, float a)
{
    if (e->dir) { /* the folder of the Library's folder cards, 14 px high, as in the file manager */
        gfx_folder_picture((int)ceilf(x - 0.5f), (int)ceilf(y + 2.5f), 14, fade(RGBA(222, 222, 228, 255), a));
        return;
    }
    const char *dot = strrchr(e->name, '.');
    const char *ext = dot && dot != e->name && strlen(dot + 1) <= 4 ? dot + 1 : NULL;
    gfx_round_rect(x + 1, y + 1, 16, 18, 2.5f, fade(RGBA(52, 156, 110, 255), a)); /* (the green of the sound files of the file manager) */
    if (ext) {
        char up[5];
        int n = 0;
        for (; ext[n] && n < 3; n++)
            up[n] = ext[n] >= 'a' && ext[n] <= 'z' ? ext[n] - 32 : ext[n];
        up[n] = 0;
        text_draw(x + 9, y + 13.5f, 0.3f, 1, fade(RGBA(255, 255, 255, 235), a), ALIGN_CENTER, up);
    }
}

/* the name of a song without its ending, as the title of its line */
static void shown_name(const MusicEntry *e, char *out, int size)
{
    snprintf(out, size, "%s", e->name);
    char *dot = strrchr(out, '.');
    if (!e->dir && dot && dot != out)
        *dot = 0;
}

static void draw_list(MusicUi *m, float a, float slide)
{
    int n = lines(m);
    /* the line above the list: the folder that is open (the storages: nothing) */
    if (m->level == 1) {
        static char head[160];
        snprintf(head, sizeof(head), "%s", m->dir);
        float w = text_width(0.5f, 0, head);
        const char *s = head;
        while (w > LIST_W && *s) { /* a long path loses its beginning */
            s++;
            w = text_width(0.5f, 0, s);
        }
        if (m->warn_t > 0) /* a folder that was not entered: this, in the place of the path */
            text_draw(LIST_X, HEAD_Y, 0.5f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, L(S_MU_BAD_FOLDER));
        else
            text_draw(LIST_X, HEAD_Y, 0.5f, 0, fade(TEXT_DIM, a), ALIGN_LEFT, s);
    }
    if (n == 0) {
        float y = 130;
        text_draw(LIST_X, y, 0.8f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, L(m->level == 0 || m->nroots == 0 ? S_NO_MUSIC : S_F_EMPTY));
        if (m->nroots == 0) { /* where the songs go: the hint says ms0:/, on a PSP Go with its internal storage it says ef0:/ */
            static char hint[160];
            const char *h = L(S_HINT_MUSIC), *p = strstr(h, "ms0:");
            if (p && strcmp(g_root, "ms0:"))
                snprintf(hint, sizeof(hint), "%.*s%s%s", (int)(p - h), h, g_root, p + 4);
            else
                snprintf(hint, sizeof(hint), "%s", h);
            text_draw(LIST_X, y + 20, 0.56f, 0, fade(TEXT_DIM, a), ALIGN_LEFT, hint);
        }
        return;
    }
    int playing_row = -1;
    char cur_dir[256] = "";
    if (music_state() != MUSIC_STOPPED)
        snprintf(cur_dir, sizeof(cur_dir), "%s", music_folder());
    float bottom = LIST_TOP + ROWS * ROW_H;
    gfx_clip(0, LIST_TOP - 2, LIST_X + LIST_W + 12, ROWS * ROW_H + 4);
    for (int i = (int)m->scroll; i < n && i <= (int)m->scroll + ROWS; i++) {
        float y = LIST_TOP + (i - m->scroll) * ROW_H + slide;
        if (y > bottom + slide)
            break;
        int sel = i == m->sel && !m->popup;
        float sa = m->focus ? 0.45f : 1.0f; /* the frame is fainter while the player has the focus */
        if (sel) {
            gfx_rect(LIST_X, y, LIST_W, ROW_H, RGBA(255, 255, 255, (int)(16 * a * sa)));
            gfx_frame(LIST_X, y, LIST_W, ROW_H, 1, fade(RGBA(232, 232, 236, 230), a * sa));
        } else if (i < n - 1 && i + 1 != m->sel) {
            gfx_rect(LIST_X + 4, y + ROW_H - 1, LIST_W - 8, 1, RGBA(255, 255, 255, (int)(22 * a)));
        }
        float x = LIST_X + 8;
        MusicEntry as_entry;
        const char *name;
        char shown[132];
        if (m->level == 0) { /* a storage: the folder picture and the name of the storage */
            memset(&as_entry, 0, sizeof(as_entry));
            as_entry.dir = 1;
            draw_row_icon(x, y + 2, &as_entry, a);
            name = L(!strncmp(m->roots[i], "ef0", 3) ? S_F_INTERNAL : S_F_MS);
            snprintf(shown, sizeof(shown), "%s", name);
        } else {
            draw_row_icon(x, y + 2, &m->e[i], a);
            shown_name(&m->e[i], shown, sizeof(shown));
        }
        float room = LIST_W - 26 - 28;
        if (sel && text_width(0.58f, 0, shown) > room)
            text_draw_marquee(x + 26, y + 16.5f, 0.58f, 0, fade(TEXT_MAIN, a), room, shown, m->name_t);
        else {
            char cut[140];
            text_ellipsis(cut, sizeof(cut), shown, 0.58f, room);
            text_draw(x + 26, y + 16.5f, 0.58f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, cut);
        }
        if (m->level == 1 && !m->e[i].dir && music_is_playing_file(m->dir, m->e[i].name)) {
            playing_row = i;
            float bx = LIST_X + LIST_W - 22;
            int paused = music_state() == MUSIC_PAUSED;
            for (int k = 0; k < 3; k++) { /* small bars that move while the song plays */
                float hgt = paused ? 3 : 3 + 7 * (0.5f + 0.5f * sinf(m->time * (7 + k * 2.3f) + k));
                hgt = floorf(hgt + 0.5f);
                gfx_rect(bx + k * 4, y + 18 - hgt, 2, hgt, fade(TEXT_MAIN, a));
            }
        }
    }
    (void)playing_row;
    gfx_clip_reset();
    if (n > ROWS) { /* scroll bar */
        float h = ROWS * ROW_H, th = clampf(h * ROWS / n, 12, h);
        float ty = LIST_TOP + (h - th) * m->scroll / (n - ROWS);
        gfx_round_rect(LIST_X + LIST_W + 6, ty + slide, 3, th, 1.5f, fade(RGBA(255, 255, 255, 70), a));
    }
}

/* the popups: the same dark panel, shadow and frame as every popup of the launcher (popup.h). The volume has none: it is the launcher's own volume bar (osd.c). */
static void draw_popup(MusicUi *m)
{
    float a = clampf(m->popup_anim, 0, 1);
    if (a <= 0.01f || m->popup == MU_POPUP_VOLUME)
        return;
    popup_dim(a);
    if (m->popup == MU_POPUP_CHOICE) { /* the values of one row, the chosen one framed, as Settings shows the values of a choice */
        int row = m->popup_sel, n = row_values(row);
        float w = 200;
        for (int k = 0; k < n; k++)
            w = fmaxf(w, ceilf(text_width(0.62f, 0, row_text(row, k))) + 32);
        w = 2 * ceilf(fminf(w, SCREEN_W - 40) / 2);
        float h = 2 * POPUP_EDGE + n * POPUP_ITEM, x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2 + (1 - a) * 10;
        popup_panel(x, y, w, h, POPUP_DARK, a);
        for (int k = 0; k < n; k++) {
            float ry = y + POPUP_EDGE + k * POPUP_ITEM;
            u32 col = POPUP_TEXT;
            if (k == m->choice_sel)
                popup_choice(x + POPUP_EDGE, ry, w - 2 * POPUP_EDGE, POPUP_ITEM, a);
            else
                col = fade(col, 0.8f);
            text_draw(x + 16, ry + 18, 0.62f, 0, fade(col, a), ALIGN_LEFT, row_text(row, k));
        }
        return;
    }
    int rows = more_rows();
    float w = 230, h = rows * POPUP_ITEM + 2 * POPUP_EDGE, x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2;
    popup_panel(x, y, w, h, POPUP_DARK, a);
    for (int r = 0; r < rows; r++) {
        float ry = y + POPUP_EDGE + r * POPUP_ITEM;
        if (r == m->popup_sel)
            popup_choice(x + POPUP_EDGE, ry, w - 2 * POPUP_EDGE, POPUP_ITEM, a);
        text_draw(x + 16, ry + 18, 0.62f, 0, fade(POPUP_TEXT, a), ALIGN_LEFT, row_name(r));
        text_draw(x + w - 16, ry + 18, 0.62f, 0, fade(r == m->popup_sel ? POPUP_TEXT : POPUP_TEXT_DIM, a), ALIGN_RIGHT, row_text(r, row_value(r)));
    }
}

/* ---------------------------------------------------------------------- */
/* the player as a popup in the middle of the screen (the PSP's sound button, from any screen) */

/* The popup has a player state of its own (m->mp). While one of its functions runs, the fields of the player are the popup's: they are swapped in and out, so that the screen's player (which
 * is drawn behind the popup and gets neutral input) and the popup never share a held button, a chosen button or an open popup. */
static void swap_player(MusicUi *m)
{
    PlayerState t = { m->pbtn, m->popup, m->popup_sel, m->choice_sel, m->popup_anim, m->rep_dir, m->rep_timer, m->rep_held, m->seek_to, m->seek_show };
    m->pbtn = m->mp.pbtn;
    m->popup = m->mp.popup;
    m->popup_sel = m->mp.popup_sel;
    m->choice_sel = m->mp.choice_sel;
    m->popup_anim = m->mp.popup_anim;
    m->rep_dir = m->mp.rep_dir;
    m->rep_timer = m->mp.rep_timer;
    m->rep_held = m->mp.rep_held;
    m->seek_to = m->mp.seek_to;
    m->seek_show = m->mp.seek_show;
    m->mp = t;
}

void musicui_mini_open(MusicUi *m)
{
    swap_player(m);
    m->pbtn = MU_PLAY;
    m->popup = 0;
    m->popup_anim = 0;
    m->rep_dir = 0;
    swap_player(m);
    m->mini = 1;
    sound_play(SND_SELECT);
}

static int mini_update(MusicUi *m, u32 pressed, u32 held, int lx, int ly, float dt, int note)
{
    if (m->seek_show > 0)
        m->seek_show -= dt;
    m->mini_anim = clampf(m->mini_anim + (m->mini ? 8 : -8) * dt, 0, 1);
    m->popup_anim = clampf(m->popup_anim + (m->popup ? 7 : -7) * dt, 0, 1);
    if (!m->mini)
        return 0;
    musicui_update_cover(m, dt);
    int mv = step(m, held, lx, ly, dt);
    if (m->popup) {
        update_popup(m, pressed, mv);
        return 0;
    }
    if (note || (pressed & PSP_CTRL_CIRCLE)) { /* the sound button again, or ○, closes it */
        m->mini = 0;
        sound_play(SND_BACK);
        return 1;
    }
    player_input(m, pressed, mv);
    return 0;
}

int musicui_mini_update(MusicUi *m, u32 pressed, u32 held, int lx, int ly, float dt, int note)
{
    swap_player(m);
    int r = mini_update(m, pressed, held, lx, ly, dt, note);
    swap_player(m);
    return r;
}

void musicui_mini_draw(MusicUi *m)
{
    float a = clampf(m->mini_anim, 0, 1);
    if (a <= 0.01f)
        return;
    swap_player(m);
    popup_dim(a);
    float px = (SCREEN_W - PANEL_W) / 2, py = (SCREEN_H - PANEL_H) / 2 + (1 - a) * 8;
    draw_player(m, px, py, a, 1, 0, 240); /* only the dark rounded player, nearly solid so that what is behind does not show through (no panel behind it) */
    draw_popup(m);
    swap_player(m);
}

void musicui_draw(MusicUi *m, float a, float slide)
{
    draw_list(m, a, slide);
    draw_player(m, PANEL_X, PANEL_Y + slide, a, m->focus, 1, 150);
    draw_popup(m);
}
