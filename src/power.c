/* psp5-authentic – the power popup of the SELECT button (see power.h) */
#include <pspctrl.h>
#include "power.h"
#include "lang.h"
#include "popup.h"
#include "sound.h"

#define POWER_W 200.0f
#define CHOICES POWER_CHOICES

static const StrId NAMES[CHOICES] = { S_TO_XMB, S_SLEEP, S_SHUTDOWN, S_RESTART, S_RECOVERY };
static const int ACTIONS[CHOICES] = { SA_XMB, SA_SUSPEND, SA_SHUTDOWN, SA_RESTART, SA_RECOVERY };

static int s_open, s_sel, s_dir;
static float s_anim; /* 0..1: how much of it there is (it fades in and out) */

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float smooth(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
static u32 with_alpha(u32 c, float a)
{
    return COLOR_ALPHA(c, (int)(((c >> 24) & 0xFF) * clampf(a, 0, 1)));
}

/* -1 = up, 1 = down, 0 = neither (the D-pad or the analog stick, as everywhere) */
static int dir_of(const UiInput *in)
{
    return ((in->held & PSP_CTRL_UP) || in->ly < 40) ? -1 : ((in->held & PSP_CTRL_DOWN) || in->ly > 215) ? 1 : 0;
}

void power_reset(void)
{
    s_open = 0;
    s_sel = 0;
    s_dir = 0;
    s_anim = 0;
}

int power_is_open(void)
{
    return s_open;
}

void power_open(const UiInput *in)
{
    s_open = 1;
    s_sel = POWER_OPEN_ON;
    s_dir = dir_of(in);
    sound_play(SND_SELECT);
}

int power_update(const UiInput *in, float dt)
{
    s_anim = clampf(s_anim + (s_open ? 7 : -7) * dt, 0, 1);
    if (!s_open)
        return SA_NONE;
    int dir = dir_of(in);
    if (dir && dir != s_dir) {
        int next = (int)clampf(s_sel + dir, 0, CHOICES - 1);
        if (next != s_sel)
            sound_play(SND_MOVE);
        s_sel = next;
    }
    s_dir = dir;
    if (in->pressed & PSP_CTRL_CROSS) {
        s_open = 0;
        sound_play(SND_SELECT);
        return ACTIONS[s_sel];
    }
    if (in->pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_START | PSP_CTRL_SELECT)) { /* back, or the button that opened it (like START on the options popups) */
        s_open = 0;
        sound_play(SND_BACK);
    }
    return SA_NONE;
}

int power_frame(UiInput *in, float dt, int can_open)
{
    static const UiInput none = UI_NO_INPUT;
    int act = SA_NONE, mine = s_open;
    if (mine) {
        act = power_update(in, dt);
    } else {
        power_update(&none, dt); /* (it fades out) */
        if (can_open && (in->pressed & PSP_CTRL_SELECT)) {
            power_open(in);
            mine = 1;
        }
    }
    if (mine)
        *in = none;
    return act;
}

void power_popup_draw(int sel, float a)
{
    float w = POWER_W, h = POPUP_EDGE + CHOICES * POPUP_ITEM + POPUP_EDGE;
    float x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2 + (1 - a) * 10; /* in the middle of the screen, rising into place like the questions */
    popup_panel(x, y, w, h, POPUP_DARK, a);
    for (int k = 0; k < CHOICES; k++) {
        float ry = y + POPUP_EDGE + k * POPUP_ITEM;
        const char *label = L(NAMES[k]);
        u32 col = POPUP_TEXT;
        if (k == sel)
            popup_choice(x + POPUP_EDGE, ry, w - 2 * POPUP_EDGE, POPUP_ITEM, a);
        else
            col = with_alpha(col, 0.8f);
        text_draw(x + 16, ry + 18, text_fit(0.62f, 0.45f, 0, w - 32, label), 0, with_alpha(col, a), ALIGN_LEFT, label);
    }
}

void power_draw(void)
{
    float a = smooth(s_anim);
    if (a <= 0)
        return;
    popup_dim(a);
    power_popup_draw(s_sel, a);
}
