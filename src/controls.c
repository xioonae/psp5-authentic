/* psp5 – made by Mille Weber */
/* Which button does what. See controls.h: the screens only know confirm, back, options and the power menu, and this file turns the
 * buttons that are pressed into those. No system calls here, so the PC tests run the real thing. */
#include <string.h>
#include "controls.h"

unsigned char g_ctl_confirm = CTL_PSP;
unsigned char g_ctl_back = CTL_PSP;
unsigned char g_ctl_options = 0;

static int s_psp_circle; /* the PSP's own setting: 1 = ○ confirms */

void controls_psp_assignment(int circle_confirms)
{
    s_psp_circle = circle_confirms ? 1 : 0;
}

int controls_psp_circle(void)
{
    return s_psp_circle;
}

static u32 face_bit(int ctl)
{
    switch (ctl) {
    case CTL_CROSS:    return PSP_CTRL_CROSS;
    case CTL_CIRCLE:   return PSP_CTRL_CIRCLE;
    case CTL_SQUARE:   return PSP_CTRL_SQUARE;
    case CTL_TRIANGLE: return PSP_CTRL_TRIANGLE;
    }
    return 0;
}

/* the first of ○ ✕ □ △ that is not `not_this` */
static int other_than(int not_this)
{
    static const int order[4] = { CTL_CIRCLE, CTL_CROSS, CTL_SQUARE, CTL_TRIANGLE };
    for (int i = 0; i < 4; i++)
        if (order[i] != not_this)
            return order[i];
    return CTL_CROSS;
}

/* The buttons behind the four names: confirm (PSP_CTRL_CROSS), back (CIRCLE), and the two that are left (SQUARE, TRIANGLE). */
typedef struct { int confirm, back, square, triangle; } Faces;

static void psp_pair(int *confirm, int *back)
{
    *confirm = s_psp_circle ? CTL_CIRCLE : CTL_CROSS;
    *back = s_psp_circle ? CTL_CROSS : CTL_CIRCLE;
}

/* what the two settings say, the PSP's setting in the place of CTL_PSP, and a button that both want left as it is */
static void raw_pair(int *c, int *b)
{
    int pc, pb;
    psp_pair(&pc, &pb);
    *c = g_ctl_confirm >= CTL_CROSS && g_ctl_confirm <= CTL_TRIANGLE ? g_ctl_confirm : pc;
    *b = g_ctl_back >= CTL_CROSS && g_ctl_back <= CTL_TRIANGLE ? g_ctl_back : pb;
}

static Faces resolve(void)
{
    Faces f;
    int c, b;
    raw_pair(&c, &b);
    if (c == b) { /* never the same button for both: the one that follows the PSP gives way; if both were chosen, back does */
        if (g_ctl_confirm == CTL_PSP && g_ctl_back != CTL_PSP)
            c = other_than(b);
        else
            b = other_than(c);
    }
    f.confirm = c;
    f.back = b;
    /* □ and △ go to the two buttons that are left, in the order □ △ ✕ ○, so with the usual ✕ and ○ they stay where they are */
    static const int order[4] = { CTL_SQUARE, CTL_TRIANGLE, CTL_CROSS, CTL_CIRCLE };
    int got = 0;
    f.square = f.triangle = CTL_SQUARE;
    for (int i = 0; i < 4 && got < 2; i++) {
        if (order[i] == c || order[i] == b)
            continue;
        if (got++ == 0)
            f.square = order[i];
        else
            f.triangle = order[i];
    }
    return f;
}

int controls_confirm_button(void)
{
    return resolve().confirm;
}

int controls_back_button(void)
{
    return resolve().back;
}

void controls_set_confirm(int ctl)
{
    int oc, ob;
    raw_pair(&oc, &ob);
    g_ctl_confirm = (unsigned char)ctl;
    int c, b;
    raw_pair(&c, &b);
    if (c == b)
        g_ctl_back = (unsigned char)oc; /* swap: back takes the button that confirm had (also when confirm goes back to the PSP's setting) */
}

void controls_set_back(int ctl)
{
    int oc, ob;
    raw_pair(&oc, &ob);
    g_ctl_back = (unsigned char)ctl;
    int c, b;
    raw_pair(&c, &b);
    if (c == b)
        g_ctl_confirm = (unsigned char)ob;
}

u32 controls_map(u32 physical)
{
    const u32 faces = PSP_CTRL_CROSS | PSP_CTRL_CIRCLE | PSP_CTRL_SQUARE | PSP_CTRL_TRIANGLE;
    Faces f = resolve();
    u32 out = physical & ~(faces | PSP_CTRL_START | PSP_CTRL_SELECT);
    if (physical & face_bit(f.confirm))  out |= PSP_CTRL_CROSS;
    if (physical & face_bit(f.back))     out |= PSP_CTRL_CIRCLE;
    if (physical & face_bit(f.square))   out |= PSP_CTRL_SQUARE;
    if (physical & face_bit(f.triangle)) out |= PSP_CTRL_TRIANGLE;
    u32 opt = g_ctl_options ? PSP_CTRL_SELECT : PSP_CTRL_START, pwr = g_ctl_options ? PSP_CTRL_START : PSP_CTRL_SELECT;
    if (physical & opt) out |= PSP_CTRL_START;
    if (physical & pwr) out |= PSP_CTRL_SELECT;
    return out;
}

static int ctl_glyph(int ctl)
{
    switch (ctl) {
    case CTL_CROSS:    return BTN_CROSS;
    case CTL_CIRCLE:   return BTN_CIRCLE;
    case CTL_SQUARE:   return BTN_SQUARE;
    case CTL_TRIANGLE: return BTN_TRIANGLE;
    }
    return -1;
}

int controls_glyph(u32 name)
{
    Faces f = resolve();
    switch (name) {
    case PSP_CTRL_CROSS:    return ctl_glyph(f.confirm);
    case PSP_CTRL_CIRCLE:   return ctl_glyph(f.back);
    case PSP_CTRL_SQUARE:   return ctl_glyph(f.square);
    case PSP_CTRL_TRIANGLE: return ctl_glyph(f.triangle);
    case PSP_CTRL_START:    return g_ctl_options ? BTN_SELECT : BTN_START;
    case PSP_CTRL_SELECT:   return g_ctl_options ? BTN_START : BTN_SELECT;
    }
    return -1;
}

static const char *const CODES[CTL_COUNT] = { "psp", "cross", "circle", "square", "triangle" };

const char *controls_code(int ctl)
{
    return ctl >= 0 && ctl < CTL_COUNT ? CODES[ctl] : CODES[0];
}

int controls_from_code(const char *s)
{
    for (int i = 0; i < CTL_COUNT; i++) {
        size_t n = strlen(CODES[i]);
        if (!strncmp(s, CODES[i], n) && (s[n] == 0 || s[n] == '\n' || s[n] == '\r' || s[n] == ' '))
            return i;
    }
    return CTL_PSP;
}
