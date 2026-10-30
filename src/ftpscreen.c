/* psp5-authentic – the screen of the FTP server (see ftpscreen.h) */
#include <math.h>
#include <pspctrl.h>
#include <stdio.h>
#include <string.h>
#include "ftpscreen.h"
#include "ftp.h"
#include "netlink.h"
#include "gameops.h"
#include "controls.h"
#include "gfx.h"
#include "lang.h"
#include "popup.h"
#include "sound.h"
#include "theme.h"

#define LIGHT RGBA(238, 238, 242, 255)
#define GREY  RGBA(160, 162, 172, 255)
#define GREEN RGBA(120, 220, 150, 255)
#define VISIBLE 5       /* connections that show at once in the list */
#define LIST_W 280.0f

static FtpScreenPhase s_phase;
static NetConn s_conn[NETLINK_MAX];
static int s_nconn, s_sel, s_top, s_dir;
static StrId s_msg;              /* what FTPS_FAILED says */
static int s_start_wait;         /* frames to show the first picture before the network is started (it takes a moment) */
static float s_switch_wait;      /* the Wi-Fi switch is looked at now and then while it is off */
static FtpStatus s_status;

FtpScreenPhase ftpscreen_phase(void)
{
    return s_phase;
}

static void fail(StrId msg)
{
    ftp_stop();
    netlink_stop();
    s_phase = FTPS_FAILED;
    s_msg = msg;
    sound_play(SND_ERROR);
}

void ftpscreen_open(void)
{
    s_phase = FTPS_START;
    s_nconn = s_sel = s_top = s_dir = 0;
    s_switch_wait = 0;
    s_start_wait = 2;
    memset(&s_status, 0, sizeof s_status);
}

/* the first step: the switch, the libraries, the list of the connections; one connection is joined at once */
static void start(unsigned now_ms)
{
    if (!netlink_switch_on()) {
        s_phase = FTPS_SWITCH_OFF;
        s_switch_wait = 0;
        return;
    }
    if (!netlink_start()) {
        fail(S_WIFI_FAILED);
        return;
    }
    s_nconn = netlink_connections(s_conn, NETLINK_MAX);
    s_sel = s_top = 0;
    if (s_nconn <= 0) {
        s_phase = FTPS_NO_CONN;
    } else if (s_nconn == 1) {
        if (netlink_connect(s_conn[0].id, now_ms))
            s_phase = FTPS_CONNECTING;
        else
            fail(S_FTP_CONN_FAILED);
    } else {
        s_phase = FTPS_CHOOSE;
    }
}

/* -1 = up, 1 = down, 0 = neither (the D-pad or the analog stick, as everywhere) */
static int dir_of(const UiInput *in)
{
    return ((in->held & PSP_CTRL_UP) || in->ly < 40) ? -1 : ((in->held & PSP_CTRL_DOWN) || in->ly > 215) ? 1 : 0;
}

int ftpscreen_update(const UiInput *in, float dt, unsigned now_ms)
{
    if (in->pressed & PSP_CTRL_CIRCLE) {
        sound_play(SND_BACK);
        return 1;
    }
    switch (s_phase) {
    case FTPS_START:
        if (s_start_wait > 0)
            s_start_wait--;
        else
            start(now_ms);
        break;
    case FTPS_SWITCH_OFF:
        s_switch_wait += dt;
        if (s_switch_wait >= 0.5f) {
            s_switch_wait = 0;
            if (netlink_switch_on())
                start(now_ms);
        }
        break;
    case FTPS_CHOOSE: {
        int dir = dir_of(in);
        if (dir && dir != s_dir) {
            int next = s_sel + dir;
            if (next >= 0 && next < s_nconn) {
                s_sel = next;
                sound_play(SND_MOVE);
            }
        }
        s_dir = dir;
        if (s_sel < s_top)
            s_top = s_sel;
        if (s_sel >= s_top + VISIBLE)
            s_top = s_sel - VISIBLE + 1;
        if (in->pressed & PSP_CTRL_CROSS) {
            sound_play(SND_SELECT);
            if (netlink_connect(s_conn[s_sel].id, now_ms))
                s_phase = FTPS_CONNECTING;
            else
                fail(S_FTP_CONN_FAILED);
        }
        break;
    }
    case FTPS_CONNECTING: {
        int st = netlink_state(now_ms);
        if (st == LINK_UP) {
            if (ftp_start(FTP_PORT, netlink_ip_value()))
                s_phase = FTPS_RUNNING;
            else
                fail(S_FTP_START_FAILED);
        } else if (st == LINK_FAILED) {
            fail(S_FTP_CONN_FAILED);
        }
        break;
    }
    case FTPS_RUNNING:
        ftp_step(now_ms);
        ftp_status(&s_status);
        if (netlink_state(now_ms) == LINK_FAILED)
            fail(S_FTP_LOST);
        break;
    case FTPS_NO_CONN:
    case FTPS_FAILED:
        break;
    }
    return 0;
}

void ftpscreen_poll(unsigned now_ms)
{
    if (s_phase == FTPS_RUNNING)
        ftp_step(now_ms);
}

void ftpscreen_close(void)
{
    ftp_stop();
    netlink_stop();
}

/* ---- drawing ---------------------------------------------------------------------------------------------------------------------------------------------- */

/* the hint at the bottom right: the symbol of ○ and what it does, in a dark capsule (as on the USB screen and in the File Manager) */
static void hint(const char *label)
{
    int btn = controls_glyph(PSP_CTRL_CIRCLE);
    float bw = gfx_button_width(btn), w = bw + 5 + ceilf(text_width(0.56f, 0, label)), x = SCREEN_W - 22 - w, y = 252; /* (whole pixels) */
    gfx_round_rect(x - 9, y - 10, w + 18, 20, 10, RGBA(0, 0, 0, 110));
    gfx_button(x + bw / 2, y, 6.5f, btn, RGBA(236, 236, 240, 255), RGBA(20, 20, 26, 255));
    text_draw(x + bw + 5, y + 4.5f, 0.56f, 0, RGBA(238, 238, 242, 255), ALIGN_LEFT, label);
}

/* the round sign in the middle at the top: it pulses while it waits, the dot is green when a computer is there */
static void sign(float cy, float t, int green, int still)
{
    float pulse = still ? 0.5f : 0.6f + 0.4f * sinf(t * 3);
    gfx_circle(SCREEN_W / 2, cy, 26, RGBA(255, 255, 255, green ? 40 : (int)(30 * pulse)));
    gfx_circle(SCREEN_W / 2, cy, 9, green ? GREEN : RGBA(236, 236, 240, (int)(255 * pulse)));
}

/* a message under the sign, and under it (smaller, grey) a second text that may need two lines */
static void message(float t, StrId msg, StrId under)
{
    sign(86, t, 0, 1);
    float size = text_fit(0.75f, 0.5f, 0, SCREEN_W - 60, L(msg));
    text_draw(SCREEN_W / 2, 150, size, 0, LIGHT, ALIGN_CENTER, L(msg));
    if (under != S_COUNT) {
        char lines[2][96];
        int n = text_wrap(L(under), 0.56f, SCREEN_W - 80, lines, 2);
        for (int i = 0; i < n; i++)
            text_draw(SCREEN_W / 2, 176 + i * 17, 0.56f, 0, GREY, ALIGN_CENTER, lines[i]);
    }
}

static void draw_list(void)
{
    const char *head = L(S_FTP_CHOOSE);
    text_draw(SCREEN_W / 2, 62, text_fit(0.75f, 0.5f, 0, SCREEN_W - 60, head), 0, LIGHT, ALIGN_CENTER, head);
    int rows = s_nconn < VISIBLE ? s_nconn : VISIBLE;
    float w = LIST_W, h = POPUP_EDGE + rows * POPUP_ITEM + POPUP_EDGE, x = (SCREEN_W - w) / 2, y = 74;
    popup_panel(x, y, w, h, POPUP_DARK, 1);
    for (int k = 0; k < rows; k++) {
        int i = s_top + k;
        float ry = y + POPUP_EDGE + k * POPUP_ITEM;
        u32 col = POPUP_TEXT;
        if (i == s_sel)
            popup_choice(x + POPUP_EDGE, ry, w - 2 * POPUP_EDGE, POPUP_ITEM, 1);
        else
            col = COLOR_ALPHA(col, (int)(((col >> 24) & 0xFF) * 0.8f));
        text_draw(x + 16, ry + 18, text_fit(0.62f, 0.45f, 0, w - 32, s_conn[i].name), 0, col, ALIGN_LEFT, s_conn[i].name);
    }
}

static void draw_running(float t)
{
    const FtpStatus *s = &s_status;
    char line[128], num[32], num2[32];
    sign(78, t, s->clients > 0, 0);
    text_draw(SCREEN_W / 2, 134, 0.95f, 0, LIGHT, ALIGN_CENTER, netlink_ip());
    snprintf(line, sizeof line, "%s %u", L(S_FTP_PORT), (unsigned)ftp_port());
    text_draw(SCREEN_W / 2, 154, 0.56f, 0, GREY, ALIGN_CENTER, line);
    text_draw(SCREEN_W / 2, 170, 0.5f, 0, RGBA(160, 162, 172, 190), ALIGN_CENTER, L(S_FTP_OPEN));
    if (s->activity == FTP_IDLE) {
        if (s->clients > 0)
            snprintf(line, sizeof line, "%s: %d", L(S_FTP_CLIENTS), s->clients);
        else
            snprintf(line, sizeof line, "%s", L(S_FTP_WAITING));
        text_draw(SCREEN_W / 2, 204, text_fit(0.75f, 0.5f, 0, SCREEN_W - 60, line), 0, LIGHT, ALIGN_CENTER, line);
        return;
    }
    const char *what = L(s->activity == FTP_SENDING ? S_FTP_SENDING : S_FTP_RECEIVING);
    text_draw(SCREEN_W / 2, 198, 0.7f, 0, LIGHT, ALIGN_CENTER, what);
    text_ellipsis(line, sizeof line, s->name, 0.56f, 340);
    text_draw(SCREEN_W / 2, 215, 0.56f, 0, GREY, ALIGN_CENTER, line);
    float bw = 260, bx = (SCREEN_W - bw) / 2;
    gfx_round_rect(bx, 223, bw, 4, 2, RGBA(255, 255, 255, 40));
    gameops_fmt_size(num, sizeof num, s->done);
    if (s->total > 0) { /* a file that goes out: how far it has come */
        float f = (float)((double)s->done / (double)s->total);
        f = f < 0 ? 0 : f > 1 ? 1 : f;
        if (f > 0)
            gfx_round_rect(bx, 223, bw * f < 4 ? 4 : bw * f, 4, 2, GREEN);
        gameops_fmt_size(num2, sizeof num2, s->total);
        snprintf(line, sizeof line, "%s / %s", num, num2);
    } else { /* a file that comes in: the size is not known */
        gfx_round_rect(bx, 223, bw, 4, 2, RGBA(120, 220, 150, 110));
        snprintf(line, sizeof line, "%s", num);
    }
    text_draw(SCREEN_W / 2, 241, 0.5f, 0, GREY, ALIGN_CENTER, line);
}

void ftpscreen_draw(float t)
{
    gfx_begin(RGBA(20, 20, 24, 255));
    theme_backdrop(theme_cur(), t, 1); /* as the settings and USB Mode */
    text_draw(28, 34, 0.95f, 0, LIGHT, ALIGN_LEFT, L(S_FTP_SERVER));
    switch (s_phase) {
    case FTPS_START:
        sign(86, t, 0, 0);
        break;
    case FTPS_SWITCH_OFF:
        message(t, S_FTP_WIFI_OFF, S_COUNT);
        break;
    case FTPS_NO_CONN:
        message(t, S_FTP_NO_CONN, S_FTP_NO_CONN_HINT);
        break;
    case FTPS_CHOOSE:
        draw_list();
        break;
    case FTPS_CONNECTING:
        sign(86, t, 0, 0);
        text_draw(SCREEN_W / 2, 150, text_fit(0.75f, 0.5f, 0, SCREEN_W - 60, L(S_FTP_CONNECTING)), 0, LIGHT, ALIGN_CENTER, L(S_FTP_CONNECTING));
        break;
    case FTPS_RUNNING:
        draw_running(t);
        break;
    case FTPS_FAILED:
        message(t, s_msg, S_COUNT);
        break;
    }
    hint(L(s_phase == FTPS_RUNNING ? S_FTP_STOP : s_phase == FTPS_CONNECTING ? S_F_CANCEL : S_BACK));
}
