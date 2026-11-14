/* psp5 authentic – the text viewer and editor of the File Manager (see textedit.h) */
#include <pspkernel.h>
#include <pspctrl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "textedit.h"
#include "textbuf.h"
#include "kbd.h"
#include "controls.h"
#include "gfx.h"
#include "lang.h"
#include "sound.h"
#include "theme.h"
#include "osd.h"
#include "popup.h"
#include "look.h"

#define LEFT     24.0f
#define WIDTH    432.0f
#define AREA_TOP 60.0f
#define AREA_BOT 240.0f
#define RH       14.0f   /* the height of a row of text */
#define PAD      1.0f    /* above and below the rows of a line */
#define TE_SIZE  0.52f
#define PAGE_LINES 8     /* the lines L and R move the choice by in the editor */
#define MENU_W   200.0f
#define MAX_PATH 512
#define WRAP_CACHE 24

#define TEXT_MAIN RGBA(238, 238, 242, 255)
#define TEXT_DIM  RGBA(160, 162, 172, 255)

enum { M_NONE, M_OPTIONS, M_ASK };
enum { A_SAVE, A_NEW_ABOVE, A_COPY, A_PASTE, A_DELETE, A_UNDO, A_GOTO, A_DONT_SAVE, A_CANCEL };

typedef struct {
    int line, n, cap;
    int *br; /* br[0 .. n]: where each row of the line starts (bytes), br[n] = the length */
} WrapEnt;

struct TextEdit {
    TextBuf *tb;
    char path[MAX_PATH];
    char name[256];
    char dir[MAX_PATH];
    int can_edit;             /* the file may be changed (else it is only read: the same screen, the keys that change something do nothing) */
    int sel;                  /* the chosen line (edit) */
    int top_line, top_row;    /* the first row that is shown */
    float off;                /* pixels the text still has to slide (animated scrolling) */
    float time, open_anim;
    int gutter;               /* the width of the line numbers */
    float text_w;             /* the room of a row of text */
    int rep_dir;
    float rep_timer, rep_held; /* a held direction repeats, faster the longer it is held */
    int menu;
    int menu_sel, menu_n, menu_act[10];
    float menu_anim;
    char ask_lines[3][96];
    int ask_n;
    char toast[96];
    float toast_t;
    int saved_any;
    WrapEnt wc[WRAP_CACHE];
    int wc_next;
};

#define CLIP_MAX 4000
static char s_clip[CLIP_MAX + 8];  /* the line that was copied (kept from one file to the next) */
static int s_clip_len = -1;
static float s_aw[128];              /* the width of the ASCII characters at TE_SIZE */
static int s_aw_ready;
static TextEdit *s_kbd_t;            /* the editor that has the keyboard open (its screen is what is drawn behind it) */

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float smooth(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
static u32 fade(u32 c, float a) { return COLOR_ALPHA(c, (int)(((c >> 24) & 0xFF) * clampf(a, 0, 1))); }

/* ---------------------------------------------------------------------- */
/* the file */

static int path_exists(const char *path)
{
    SceIoStat st;
    return sceIoGetstat(path, &st) >= 0;
}

static long long file_size(const char *path)
{
    SceIoStat st;
    memset(&st, 0, sizeof(st));
    if (sceIoGetstat(path, &st) < 0)
        return -1;
    return (long long)st.st_size;
}

int textedit_sniff_file(const char *path)
{
    static unsigned char buf[4096];
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return -1;
    int n = sceIoRead(fd, buf, sizeof(buf));
    sceIoClose(fd);
    if (n < 0)
        return -1;
    return tb_sniff(buf, n);
}

/* the file in memory (malloc'd), or 0 and why */
static int read_file(const char *path, unsigned char **out, int *len, StrId *err)
{
    long long size = file_size(path);
    if (size < 0) {
        *err = S_TE_CANT;
        return 0;
    }
    if (size > TB_MAX_FILE) {
        *err = S_TE_TOO_BIG;
        return 0;
    }
    unsigned char *buf = malloc((size_t)size + 1);
    SceUID fd = buf ? sceIoOpen(path, PSP_O_RDONLY, 0) : -1;
    if (fd < 0) {
        free(buf);
        *err = S_TE_CANT;
        return 0;
    }
    int got = 0;
    while (got < size) {
        int r = sceIoRead(fd, buf + got, (int)(size - got) > 65536 ? 65536 : (int)(size - got));
        if (r <= 0)
            break;
        got += r;
    }
    sceIoClose(fd);
    if (got != size) {
        free(buf);
        *err = S_TE_CANT;
        return 0;
    }
    *out = buf;
    *len = got;
    return 1;
}

/* A rename inside one folder, checked: the thing is under the new name afterwards and gone from the old (the PSP's rename may take only the NAME of its second argument and rename the thing
 * where it is, as PPSSPP does: then it is put back). */
static int move_verified(const char *src, const char *dst)
{
    if (sceIoRename(src, dst) < 0)
        return 0;
    if (path_exists(dst) && !path_exists(src))
        return 1;
    const char *slash = strrchr(src, '/');
    const char *base = strrchr(dst, '/');
    base = base ? base + 1 : dst;
    if (slash) {
        char inplace[MAX_PATH];
        snprintf(inplace, sizeof(inplace), "%.*s%s", (int)(slash - src) + 1, src, base);
        if (!path_exists(src) && path_exists(inplace))
            sceIoRename(inplace, src);
    }
    return 0;
}

/* Writes the file under a temporary name, then puts it in place of the old one (which is kept until the new one is there): a failure leaves the old file as it was. */
static int save_file(TextEdit *t)
{
    int len = 0;
    unsigned char *buf = tb_serialize(t->tb, &len);
    if (!buf)
        return 0;
    char tmp[MAX_PATH], old[MAX_PATH];
    snprintf(tmp, sizeof(tmp), "%s.psp5tmp", t->path);
    snprintf(old, sizeof(old), "%s.psp5old", t->path);
    SceUID fd = sceIoOpen(tmp, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd < 0) {
        free(buf);
        return 0;
    }
    int ok = 1;
    for (int off = 0; off < len;) {
        int n = len - off > 32768 ? 32768 : len - off;
        if (sceIoWrite(fd, buf + off, n) != n) {
            ok = 0;
            break;
        }
        off += n;
    }
    sceIoClose(fd);
    free(buf);
    if (!ok || file_size(tmp) != len) {
        sceIoRemove(tmp);
        return 0;
    }
    if (path_exists(old))
        sceIoRemove(old);
    if (!move_verified(t->path, old)) {
        sceIoRemove(tmp);
        return 0;
    }
    if (!move_verified(tmp, t->path)) {
        move_verified(old, t->path); /* (the old file back) */
        sceIoRemove(tmp);
        return 0;
    }
    sceIoRemove(old);
    tb_mark_saved(t->tb);
    t->saved_any = 1;
    return 1;
}

/* ---------------------------------------------------------------------- */
/* the rows of a line */

static float char_w(const char *s, int l)
{
    if (!s_aw_ready) {
        for (int c = 32; c < 127; c++) {
            char b[2] = { (char)c, 0 };
            s_aw[c] = text_width(TE_SIZE, 0, b);
        }
        s_aw_ready = 1;
    }
    unsigned char c = (unsigned char)s[0];
    if (c == '\t')
        return 4 * s_aw[' '];
    if (c < 32 || c == 127)
        return s_aw[' '];
    if (c < 127)
        return s_aw[c];
    if (l == 1)
        return s_aw['?']; /* (a byte that is not valid UTF-8 is shown as ?) */
    char b[8];
    memcpy(b, s, (size_t)l);
    b[l] = 0;
    return text_width(TE_SIZE, 0, b);
}

static void wrap_flush(TextEdit *t)
{
    for (int i = 0; i < WRAP_CACHE; i++)
        t->wc[i].line = -1;
}

static void wrap_add(WrapEnt *e, int at)
{
    if (e->n + 2 > e->cap) {
        int cap = e->cap ? e->cap * 2 : 8;
        int *nb = realloc(e->br, (size_t)cap * sizeof(int));
        if (!nb)
            return;
        e->br = nb;
        e->cap = cap;
    }
    e->br[e->n++] = at;
}

static void wrap_compute(TextEdit *t, WrapEnt *e, int line)
{
    int len;
    const char *p = tb_line(t->tb, line, &len);
    e->line = line;
    e->n = 0;
    if (!e->br) {
        e->br = malloc(8 * sizeof(int));
        if (!e->br) { /* (no memory: the line is one row) */
            e->line = -1;
            return;
        }
        e->cap = 8;
    }
    wrap_add(e, 0);
    float w = 0, W = t->text_w;
    int start = 0, lastsp = -1;
    for (int i = 0; i < len;) {
        int cl = tb_char_len(p + i, len - i);
        float c = char_w(p + i, cl);
        if (p[i] != ' ' && w + c > W && i > start) { /* (a space may hang over the edge: it is the end of the row it follows) */
            int brk = lastsp > start ? lastsp + 1 : i;
            wrap_add(e, brk);
            start = brk;
            w = 0;
            lastsp = -1;
            for (int k = brk; k < i;) {
                int kl = tb_char_len(p + k, len - k);
                w += char_w(p + k, kl);
                if (p[k] == ' ')
                    lastsp = k;
                k += kl;
            }
            continue;
        }
        w += c;
        if (p[i] == ' ')
            lastsp = i;
        i += cl;
    }
    e->br[e->n] = len; /* (wrap_add keeps room for it) */
}

static WrapEnt *wrap_line(TextEdit *t, int line)
{
    for (int i = 0; i < WRAP_CACHE; i++)
        if (t->wc[i].line == line)
            return &t->wc[i];
    WrapEnt *e = &t->wc[t->wc_next];
    t->wc_next = (t->wc_next + 1) % WRAP_CACHE;
    wrap_compute(t, e, line);
    return e;
}

static int rows_of(TextEdit *t, int line) { return wrap_line(t, line)->n; }

static float line_h(TextEdit *t, int line) { return rows_of(t, line) * RH + 2 * PAD; }

/* a row for text_draw: tabs as four spaces, bytes that are no valid UTF-8 as ?, control characters as spaces */
static void row_text(const char *p, int from, int to, char *out, int cap)
{
    int k = 0;
    for (int i = from; i < to && k < cap - 8;) {
        unsigned char c = (unsigned char)p[i];
        int cl = tb_char_len(p + i, to - i);
        if (c == '\t') {
            for (int s = 0; s < 4; s++)
                out[k++] = ' ';
        } else if (c < 32 || c == 127) {
            out[k++] = ' ';
        } else if (c >= 0x80 && cl == 1) {
            out[k++] = '?';
        } else {
            for (int s = 0; s < cl; s++)
                out[k++] = p[i + s];
        }
        i += cl;
    }
    while (k > 0 && out[k - 1] == ' ')
        k--; /* (nothing to draw at the end of a row) */
    out[k] = 0;
}

/* ---------------------------------------------------------------------- */
/* where the text is shown from */

static int digits(int n)
{
    int d = 1;
    while (n >= 10) {
        n /= 10;
        d++;
    }
    return d;
}

static void set_gutter(TextEdit *t)
{
    int g = 14 + 6 * digits(tb_count(t->tb));
    if (g < 30)
        g = 30;
    if (g != t->gutter) {
        t->gutter = g;
        t->text_w = WIDTH - g - 14;
        wrap_flush(t);
    }
}

/* the pixels from the first row (line a, row ar) to the first row of (b, br) when they are near each other (0 when far: no sliding) */
static float row_dist(TextEdit *t, int al, int ar, int bl, int br)
{
    if (al == bl)
        return (br - ar) * RH;
    int lo = al < bl ? al : bl, hi = al < bl ? bl : al;
    if (hi - lo > 40)
        return 0;
    float px = 0;
    for (int l = lo; l < hi; l++)
        px += line_h(t, l);
    px = al < bl ? px - ar * RH + br * RH : -(px - br * RH + ar * RH);
    return px;
}

static void set_top(TextEdit *t, int line, int row)
{
    int ol = t->top_line, orow = t->top_row;
    if (line < 0)
        line = 0;
    if (line >= tb_count(t->tb))
        line = tb_count(t->tb) - 1;
    int rows = rows_of(t, line);
    if (row < 0)
        row = 0;
    if (row >= rows)
        row = rows - 1;
    float d = row_dist(t, ol, orow, line, row);
    t->top_line = line;
    t->top_row = row;
    t->off = clampf(t->off + d, -AREA_BOT, AREA_BOT);
}

/* the pixels from the first shown row to the end of line `s` */
static float px_to_end(TextEdit *t, int top_line, int top_row, int s)
{
    float px = -top_row * RH;
    for (int l = top_line; l <= s; l++)
        px += line_h(t, l);
    return px;
}

/* the chosen line is whole in the shown part, as far as it fits (the text is moved by rows, as little as it can be) */
static void ensure_visible(TextEdit *t)
{
    int s = t->sel;
    if (s < t->top_line || (s == t->top_line && t->top_row > 0)) {
        set_top(t, s, 0);
        return;
    }
    if (s - t->top_line > 60) {
        set_top(t, s - 3, 0);
        return;
    }
    const float avail = AREA_BOT - AREA_TOP - 1;
    int l = t->top_line, row = t->top_row;
    while (px_to_end(t, l, row, s) > avail && !(l == s && row == 0)) {
        row++;
        if (row >= rows_of(t, l)) {
            l++;
            row = 0;
        }
    }
    if (l != t->top_line || row != t->top_row)
        set_top(t, l, row);
}

static void jump_to(TextEdit *t, int line)
{
    int n = tb_count(t->tb);
    line = line < 0 ? 0 : line >= n ? n - 1 : line;
    t->sel = line;
    t->top_line = line > 3 ? line - 3 : 0;
    t->top_row = 0;
    t->off = 0;
    ensure_visible(t);
}

/* ---------------------------------------------------------------------- */
/* opening and closing */

TextEdit *textedit_open(const char *path, StrId *err)
{
    StrId dummy;
    if (!err)
        err = &dummy;
    unsigned char *data = NULL;
    int len = 0;
    if (!read_file(path, &data, &len, err))
        return NULL;
    int e = TB_OK;
    TextBuf *tb = tb_load(data, len, &e); /* (the data are the buffer's now) */
    if (!tb) {
        *err = e == TB_ERR_BIG ? S_TE_TOO_BIG : S_TE_CANT;
        return NULL;
    }
    TextEdit *t = calloc(1, sizeof(*t));
    if (!t) {
        tb_free(tb);
        *err = S_TE_CANT;
        return NULL;
    }
    t->tb = tb;
    snprintf(t->path, sizeof(t->path), "%s", path);
    const char *slash = strrchr(path, '/');
    snprintf(t->name, sizeof(t->name), "%s", slash ? slash + 1 : path);
    snprintf(t->dir, sizeof(t->dir), "%.*s", slash ? (int)(slash - path) : 0, path);
    t->can_edit = tb_editable(tb);
    if (!t->can_edit) { /* it is shown, and it says why it cannot be changed */
        snprintf(t->toast, sizeof(t->toast), "%s", L(S_TE_VIEW_ONLY));
        t->toast_t = 3.5f;
    }
    for (int i = 0; i < tb->data_len; i++)
        if (tb->data[i] >= 0xCA) { /* (letters that the Latin font may not have: the other font, as the game titles get it) */
            font_ensure_cjk();
            break;
        }
    s_aw_ready = 0;
    wrap_flush(t);
    set_gutter(t);
    return t;
}

void textedit_close(TextEdit *t)
{
    if (!t)
        return;
    for (int i = 0; i < WRAP_CACHE; i++)
        free(t->wc[i].br);
    tb_free(t->tb);
    free(t);
}

int textedit_saved(const TextEdit *t) { return t->saved_any; }

/* ---------------------------------------------------------------------- */
/* the menu */

static void toast(TextEdit *t, const char *msg)
{
    snprintf(t->toast, sizeof(t->toast), "%s", msg);
    t->toast_t = 1.8f;
}

static void add_act(TextEdit *t, int act) { t->menu_act[t->menu_n++] = act; }

static void open_menu(TextEdit *t)
{
    t->menu = M_OPTIONS;
    t->menu_n = 0;
    t->menu_sel = 0;
    if (t->can_edit) {
        if (tb_dirty(t->tb))
            add_act(t, A_SAVE);
        add_act(t, A_NEW_ABOVE);
        add_act(t, A_COPY);
        if (s_clip_len >= 0)
            add_act(t, A_PASTE);
        add_act(t, A_DELETE);
        if (tb_can_undo(t->tb))
            add_act(t, A_UNDO);
        add_act(t, A_GOTO);
    } else { /* (a line can be copied out of a file that cannot be changed) */
        add_act(t, A_GOTO);
        add_act(t, A_COPY);
    }
    sound_play(SND_SELECT);
}

static StrId act_label(int act)
{
    switch (act) {
    case A_SAVE:       return S_TE_SAVE;
    case A_NEW_ABOVE:  return S_TE_NEW_ABOVE;
    case A_COPY:       return S_TE_COPY_LINE;
    case A_PASTE:      return S_TE_PASTE_LINE;
    case A_DELETE:     return S_TE_DEL_LINE;
    case A_UNDO:       return S_TE_UNDO;
    case A_GOTO:       return S_TE_GOTO;
    case A_DONT_SAVE:  return S_TE_DONT_SAVE;
    }
    return S_F_CANCEL;
}

/* ---------------------------------------------------------------------- */
/* editing a line */

static void kbd_scene(void)
{
    if (s_kbd_t)
        textedit_draw(s_kbd_t);
    osd_draw();
}

/* the line `line` on the PSP's keyboard. 1 = a new text was confirmed (in out, the start of the line that has a tab in it put back), 0 = closed. `line` -1: a new, empty line. */
static int ask_line(TextEdit *t, int line, char *out, int out_size)
{
    char init[TB_EDIT_MAX + 8] = "", typed[3 * KBD_MAX + 8];
    char lead[TB_EDIT_MAX + 8] = "";
    int lead_len = 0;
    if (line >= 0) {
        int len;
        const char *p = tb_line(t->tb, line, &len);
        lead_len = tb_lead_tabs(p, len);
        if (lead_len > TB_EDIT_MAX)
            lead_len = 0;
        if (len - lead_len > TB_EDIT_MAX)
            len = lead_len + TB_EDIT_MAX;
        memcpy(lead, p, (size_t)lead_len);
        lead[lead_len] = 0;
        memcpy(init, p + lead_len, (size_t)(len - lead_len));
        init[len - lead_len] = 0;
    }
    s_kbd_t = t;
    int ok = kbd_input(L(line >= 0 ? S_TE_EDIT_LINE : S_TE_NEW_BELOW), init, typed, sizeof(typed), TB_EDIT_MAX, KBD_KEEP_SPACES | KBD_ALLOW_EMPTY, kbd_scene);
    s_kbd_t = NULL;
    if (!ok)
        return 0;
    for (char *c = typed; *c; c++)
        if (*c == '\n' || *c == '\r')
            *c = ' '; /* (a line is one line) */
    snprintf(out, out_size, "%s%s", lead, typed);
    return 1;
}

static void edit_line(TextEdit *t)
{
    char text[4 * KBD_MAX];
    if (!ask_line(t, t->sel, text, sizeof(text)))
        return;
    int len;
    const char *p = tb_line(t->tb, t->sel, &len);
    int nl = (int)strlen(text);
    if (nl == len && !memcmp(p, text, (size_t)len))
        return;
    if (!tb_replace(t->tb, t->sel, text, nl)) {
        toast(t, L(S_F_FAILED));
        return;
    }
    wrap_flush(t);
    ensure_visible(t);
}

static void new_line(TextEdit *t, int below)
{
    char text[4 * KBD_MAX];
    if (!ask_line(t, -1, text, sizeof(text)))
        return;
    int at = below ? t->sel + 1 : t->sel;
    if (!tb_insert(t->tb, at, text, (int)strlen(text))) {
        toast(t, L(S_F_FAILED));
        sound_play(SND_ERROR);
        return;
    }
    t->sel = at;
    wrap_flush(t);
    set_gutter(t);
    ensure_visible(t);
}

static void goto_line(TextEdit *t)
{
    char num[16], out[32];
    int cur = t->sel + 1;
    snprintf(num, sizeof(num), "%d", cur);
    s_kbd_t = t;
    int ok = kbd_input(L(S_TE_GOTO), num, out, sizeof(out), 6, 0, kbd_scene);
    s_kbd_t = NULL;
    if (!ok)
        return;
    int v = atoi(out);
    if (v < 1)
        v = 1;
    jump_to(t, v - 1);
}

static void do_save(TextEdit *t, int *closed)
{
    if (save_file(t)) {
        toast(t, L(S_TE_SAVED));
        sound_play(SND_SELECT);
    } else {
        toast(t, L(S_TE_SAVE_FAILED));
        sound_play(SND_ERROR);
        if (closed)
            *closed = 0;
    }
}

/* ○ in the editor: leaves, or asks about the changes. 1 = it is over. */
static int try_leave(TextEdit *t)
{
    if (!tb_dirty(t->tb))
        return 1;
    t->menu = M_ASK;
    t->menu_n = 0;
    add_act(t, A_SAVE);
    add_act(t, A_DONT_SAVE);
    add_act(t, A_CANCEL);
    t->menu_sel = 0;
    t->ask_n = text_wrap(L(S_TE_UNSAVED), 0.5f, POPUP_ASK_W - 24, t->ask_lines, 3);
    sound_play(SND_SELECT);
    return 0;
}

/* an entry of the menu or of the question; 1 = the editor is over */
static int run_act(TextEdit *t, int act)
{
    int was_ask = t->menu == M_ASK;
    t->menu = M_NONE;
    switch (act) {
    case A_SAVE:
        if (was_ask) {
            int closed = 1;
            do_save(t, &closed);
            return closed;
        }
        do_save(t, NULL);
        break;
    case A_DONT_SAVE:
        return 1;
    case A_NEW_ABOVE:
        new_line(t, 0);
        break;
    case A_COPY: {
        int len;
        const char *p = tb_line(t->tb, t->sel, &len);
        if (len > CLIP_MAX)
            len = CLIP_MAX;
        memcpy(s_clip, p, (size_t)len);
        s_clip[len] = 0;
        s_clip_len = len;
        sound_play(SND_SELECT);
        break;
    }
    case A_PASTE:
        if (s_clip_len >= 0 && tb_insert(t->tb, t->sel + 1, s_clip, s_clip_len)) {
            t->sel++;
            wrap_flush(t);
            set_gutter(t);
            ensure_visible(t);
            sound_play(SND_SELECT);
        }
        break;
    case A_DELETE:
        if (tb_delete(t->tb, t->sel)) {
            if (t->sel >= tb_count(t->tb))
                t->sel = tb_count(t->tb) - 1;
            wrap_flush(t);
            set_gutter(t);
            ensure_visible(t);
            sound_play(SND_SELECT);
        }
        break;
    case A_UNDO: {
        int line = tb_undo(t->tb);
        if (line >= 0) {
            t->sel = line;
            wrap_flush(t);
            set_gutter(t);
            ensure_visible(t);
            sound_play(SND_SELECT);
        }
        break;
    }
    case A_GOTO:
        goto_line(t);
        break;
    default:
        sound_play(SND_BACK);
        break;
    }
    return 0;
}

/* ---------------------------------------------------------------------- */
/* the buttons */

static int vertical(TextEdit *t, const UiInput *in, float dt)
{
    int dir = ((in->held & PSP_CTRL_UP) || in->ly < 40) ? -1 : ((in->held & PSP_CTRL_DOWN) || in->ly > 215) ? 1 : 0;
    if (!dir) {
        t->rep_dir = 0;
        return 0;
    }
    if (dir != t->rep_dir) {
        t->rep_dir = dir;
        t->rep_timer = 0.32f;
        t->rep_held = 0;
        return dir;
    }
    t->rep_held += dt;
    t->rep_timer -= dt;
    if (t->rep_timer > 0)
        return 0;
    t->rep_timer += t->rep_held > 2.5f ? 0.02f : t->rep_held > 1.0f ? 0.04f : 0.07f; /* (a long file is crossed in a few seconds) */
    return dir;
}

int textedit_update(TextEdit *t, const UiInput *in, float dt)
{
    t->time += dt;
    t->open_anim = clampf(t->open_anim + dt * 4, 0, 1);
    t->menu_anim = clampf(t->menu_anim + (t->menu ? 7 : -7) * dt, 0, 1);
    if (t->toast_t > 0)
        t->toast_t -= dt;
    t->off *= 1 - clampf(dt * 16, 0, 1); /* the text slides into place */
    if (fabsf(t->off) < 0.3f)
        t->off = 0;
    int move = vertical(t, in, dt);
    if (t->menu) {
        if (move) {
            int next = (int)clampf(t->menu_sel + move, 0, t->menu_n - 1);
            if (next != t->menu_sel)
                sound_play(SND_MOVE);
            t->menu_sel = next;
        }
        if (in->pressed & PSP_CTRL_CROSS)
            return run_act(t, t->menu_act[t->menu_sel]);
        if (in->pressed & (PSP_CTRL_CIRCLE | PSP_CTRL_START)) {
            t->menu = M_NONE;
            sound_play(SND_BACK);
        }
        return 0;
    }
    if (in->pressed & PSP_CTRL_START) {
        open_menu(t);
        return 0;
    }
    if (in->pressed & PSP_CTRL_CIRCLE) {
        if (try_leave(t)) {
            sound_play(SND_BACK);
            return 1;
        }
        return 0;
    }
    int n = tb_count(t->tb);
    if (in->pressed & PSP_CTRL_LTRIGGER)
        move = -PAGE_LINES;
    if (in->pressed & PSP_CTRL_RTRIGGER)
        move = PAGE_LINES;
    if (move) {
        int next = (int)clampf(t->sel + move, 0, n - 1);
        if (next != t->sel) {
            t->sel = next;
            sound_play(SND_MOVE);
            ensure_visible(t);
        }
    }
    if (t->can_edit) { /* (a file that cannot be changed is only read: these do nothing) */
        if (in->pressed & PSP_CTRL_CROSS)
            edit_line(t);
        else if (in->pressed & PSP_CTRL_SQUARE)
            new_line(t, 1);
    }
    return 0;
}

/* ---------------------------------------------------------------------- */
/* drawing */

static void draw_hints(TextEdit *t, float a)
{
    struct { int btn; StrId text; } h[4];
    int n = 0;
    if (t->can_edit) {
        h[n++] = (typeof(h[0])){ controls_glyph(PSP_CTRL_CROSS), S_TE_EDIT_LINE };
        h[n++] = (typeof(h[0])){ controls_glyph(PSP_CTRL_SQUARE), S_TE_NEW_BELOW };
    }
    h[n++] = (typeof(h[0])){ controls_glyph(PSP_CTRL_START), S_OPTIONS };
    h[n++] = (typeof(h[0])){ controls_glyph(PSP_CTRL_CIRCLE), S_BACK };
    float w = -14;
    for (int i = 0; i < n; i++)
        w += gfx_button_width(h[i].btn) + 5 + ceilf(text_width(0.56f, 0, L(h[i].text))) + 14;
    float x = SCREEN_W - 22 - w, y = 252;
    for (int i = 0; i < n; i++) {
        float bw = gfx_button_width(h[i].btn);
        gfx_button(x + bw / 2, y, 6.5f, h[i].btn, look_fade(LOOK_HINT_PLATE, a), look_fade(LOOK_HINT_MARK, a));
        x += bw + 5;
        text_draw(x, y + 4.5f, 0.56f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, L(h[i].text));
        x += ceilf(text_width(0.56f, 0, L(h[i].text))) + 14;
    }
}

static void draw_header(TextEdit *t, float a, float slide)
{
    char name[256];
    text_ellipsis(name, sizeof(name), t->name, 0.8f, WIDTH - 190);
    text_draw(LEFT + 4 + slide, 34, 0.8f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, name);
    if (tb_dirty(t->tb)) /* changed since it was read or written: a small dot after the name */
        gfx_circle(LEFT + 4 + slide + ceilf(text_width(0.8f, 0, name)) + 9, 28, 3.5f, fade(RGBA(236, 236, 240, 255), a));
    const char *p = t->dir; /* the folder, cut from the left; "View - " in front of it when the file cannot be changed */
    const char *mode = t->can_edit ? "" : L(S_TE_VIEW);
    float max = WIDTH - 8 - (mode[0] ? text_width(0.5f, 0, mode) + 24 : 0);
    while (text_width(0.5f, 0, p) > max && strchr(p + 1, '/'))
        p = strchr(p + 1, '/');
    char line[MAX_PATH + 40];
    if (mode[0])
        snprintf(line, sizeof(line), "%s  -  %s%s", mode, p == t->dir ? "" : "...", p);
    else
        snprintf(line, sizeof(line), "%s%s", p == t->dir ? "" : "...", p);
    text_draw(LEFT + 4 + slide, 52, 0.5f, 0, fade(TEXT_DIM, a), ALIGN_LEFT, line);
    int n = tb_count(t->tb), cur = t->sel + 1;
    char pos[64];
    snprintf(pos, sizeof(pos), L(S_TE_LINE), cur, n);
    float right = LEFT + WIDTH;
    text_draw(right, 28, 0.5f, 0, fade(TEXT_DIM, a), ALIGN_RIGHT, pos);
    float bw = 120, f = n > 1 ? (float)(cur - 1) / (float)(n - 1) : 1;
    look_bar(right - bw, 34, bw, 4, bw * clampf(f, 0.02f, 1), a);
}

static void draw_line_block(TextEdit *t, int l, float yb, float a, float slide, int selected)
{
    WrapEnt *w = wrap_line(t, l);
    int len;
    const char *p = tb_line(t->tb, l, &len);
    float h = w->n * RH + 2 * PAD;
    if (selected) {
        gfx_rect(LEFT, yb, WIDTH, h, RGBA(255, 255, 255, (int)(16 * a)));
        gfx_frame(LEFT, yb, WIDTH, h, 1, fade(RGBA(232, 232, 236, 230), a));
    }
    char num[16];
    snprintf(num, sizeof(num), "%d", l + 1);
    text_draw(LEFT + t->gutter - 7 + slide, yb + PAD + 10.5f, 0.42f, 0, fade(TEXT_DIM, a), ALIGN_RIGHT, num);
    char rb[1400];
    for (int r = 0; r < w->n; r++) {
        float ry = yb + PAD + r * RH;
        if (ry + RH < AREA_TOP - 2 || ry > AREA_BOT)
            continue;
        row_text(p, w->br[r], w->br[r + 1], rb, sizeof(rb));
        if (rb[0])
            text_draw(LEFT + t->gutter + 2 + slide, ry + 11, TE_SIZE, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, rb);
    }
}

static void draw_text(TextEdit *t, float a, float slide)
{
    int n = tb_count(t->tb);
    gfx_clip(0, AREA_TOP - 2, SCREEN_W, AREA_BOT - AREA_TOP + 4);
    float y0 = AREA_TOP + t->off - t->top_row * RH;
    int sel_on = !t->menu;
    float yb = y0;
    for (int l = t->top_line; l < n && yb < AREA_BOT + 4; l++) {
        draw_line_block(t, l, yb, a, slide, sel_on && l == t->sel);
        yb += line_h(t, l);
    }
    yb = y0; /* the lines above (they show while the text slides up) */
    for (int l = t->top_line - 1; l >= 0 && yb > AREA_TOP - 4; l--) {
        yb -= line_h(t, l);
        draw_line_block(t, l, yb, a, slide, sel_on && l == t->sel);
    }
    gfx_clip_reset();
    if (n > 12) { /* the scroll bar */
        float h = AREA_BOT - AREA_TOP, th = clampf(h * 12.0f / n, 12, h);
        float ty = AREA_TOP + (h - th) * (float)t->sel / (float)(n - 1);
        gfx_round_rect(LEFT + WIDTH + 6, ty, 3, th, 1.5f, fade(RGBA(255, 255, 255, 70), a));
    }
}

static void draw_menu(TextEdit *t)
{
    float a = smooth(t->menu_anim);
    if (a <= 0 || t->menu_n <= 0)
        return;
    popup_dim(a);
    if (t->menu == M_ASK) {
        const char *labels[3];
        u32 colors[3];
        for (int k = 0; k < t->menu_n && k < 3; k++) {
            labels[k] = L(act_label(t->menu_act[k]));
            colors[k] = t->menu_act[k] == A_DONT_SAVE ? POPUP_DANGER : POPUP_TEXT;
        }
        popup_ask(t->ask_lines, t->ask_n, lang_needs_cjk() ? 15.0f : 13.0f, "", labels, colors, t->menu_n, t->menu_sel, a);
        return;
    }
    float w = MENU_W, h = POPUP_EDGE + t->menu_n * POPUP_ITEM + POPUP_EDGE;
    float x = SCREEN_W - 24 - w - (1 - a) * 10, y = 238 - h;
    popup_panel(x, y, w, h, POPUP_DARK, a);
    for (int k = 0; k < t->menu_n; k++) {
        int act = t->menu_act[k];
        float ry = y + POPUP_EDGE + k * POPUP_ITEM;
        u32 col = act == A_DELETE ? POPUP_DANGER : POPUP_TEXT;
        if (k == t->menu_sel)
            popup_choice(x + POPUP_EDGE, ry, w - 2 * POPUP_EDGE, POPUP_ITEM, a);
        else
            col = fade(col, 0.8f);
        const char *label = L(act_label(act));
        text_draw(x + 16, ry + 18, text_fit(0.62f, 0.45f, 0, w - 32, label), 0, fade(col, a), ALIGN_LEFT, label);
    }
}

void textedit_draw(TextEdit *t)
{
    float a = t->open_anim, slide = (1 - a) * 10;
    theme_backdrop(theme_cur(), t->time, 1);
    draw_header(t, a, slide);
    draw_text(t, a, slide);
    draw_hints(t, a * (1 - smooth(t->menu_anim)));
    draw_menu(t);
    if (t->toast_t > 0)
        popup_toast(t->toast, clampf(t->toast_t / 0.4f, 0, 1));
    if (a < 1)
        gfx_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(255 * (1 - a))));
}
