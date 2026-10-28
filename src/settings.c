/* psp5 – made by Mille Weber */
/* Inställningar i samma anda som PS5:s: mörk yta med ett strålkastarljus,
 * kategorier till vänster, alternativ till höger med tunna skiljelinjer
 * och en tunn ljus ram runt det valda. All grafik ritas här. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "settings.h"
#include "arkcfg.h"
#include "plugins.h"
#include "system.h"
#include "sound.h"
#include "lang.h"
#include "config.h"
#include "controls.h"
#include "theme.h"
#include "osd.h"
#include "popup.h"

#define TITLE_Y     34.0f
#define LEFT_X      24.0f  /* the same sides as the lists of the file manager and Manage Saved Data: 24 px from the edges of the screen */
#define LEFT_W      136.0f
#define RIGHT_X     172.0f /* 12 px after the left column */
#define RIGHT_W     284.0f /* to 456 */
#define ROW_H       26.0f
#define LIST_TOP    58.0f
#define LIST_BOTTOM 236.0f
#define MAX_ITEMS   (MAX_PLUGINS + 4)
#define PSP5_VERSION "1.2.0"

#define TEXT_MAIN RGBA(238, 238, 242, 255)
#define TEXT_DIM  RGBA(160, 162, 172, 255)
#define LIGHT     RGBA(236, 236, 240, 255)

typedef enum { IT_TOGGLE, IT_CHOICE, IT_ACTION, IT_INFO, IT_THEME } ItemType;

typedef struct {
    const char *label;
    ItemType type;
    unsigned char *value;        /* TOGGLE/CHOICE */
    const char *const *opts;     /* CHOICE */
    int nopts;
    int action;                  /* ACTION: SA_*, THEME: temats index */
    char info[48];               /* INFO-värde / undertext */
    int plugin;                  /* >0: plugin nr (1-baserat) */
    int own;                     /* psp5:s egen inställning (psp5.cfg) */
    int immediate;               /* the system has it, not a file of ours: nothing to save (the PSP's button assignment) */
    int (*glyph)(int opt);       /* CHOICE: a button symbol (BTN_*) to draw for value `opt`, or -1: the values that are buttons */
    void (*set)(int opt);        /* CHOICE / TOGGLE: how the value is set when it is more than storing it (the buttons that swap, the PSP's own settings), else NULL */
} Item;

typedef struct {
    const char *name;
    const char *desc;
    Item items[MAX_ITEMS];
    int count;
} Category;

enum { CAT_PSP5, CAT_THEME, CAT_CONTROLS, CAT_DISPLAY, CAT_NETWORK, CAT_PERF, CAT_SYSTEM, CAT_PLUGINS, CAT_POWER, CAT_ABOUT, CAT_COUNT };

static Category s_cats[CAT_COUNT];
static unsigned char s_plugin_on[MAX_PLUGINS];
/* Radbrytningen mäter texten många gånger – gör den bara när texten byts, inte varje bildruta */
static const char *s_wrapped; /* texten som s_desc håller, NULL = inget */
static int s_wrapped_n;

static struct {
    int cat, item[CAT_COUNT];
    int cat_top;        /* the first category shown (more categories than fit: the list scrolls) */
    float cat_scroll;
    int focus;          /* 0 = kategorier, 1 = alternativ */
    float scroll, open, time;
    int cfw_dirty, plugins_dirty, own_dirty;
    int repeat_dir;
    float repeat_timer;
    int stick_x;        /* the analog stick sideways as it was last time: -1, 0 or 1 */
    /* the popup of a choice (✕ on it): the list of values, the one chosen, and the first one shown when there are more than fit */
    int pop, pop_cat, pop_item, pop_sel, pop_top;
    float pop_anim, pop_scroll;
    char toast[96];     /* a message that is shown for a moment (settings_notice) */
    float toast_time;
} S;

#define VIS_CATS 6 /* the categories shown at once in the left column */
#define POP_ROWS 8 /* the most values a popup shows at once; a longer list scrolls (the 14 regions, the 11 languages) */

/* valtexter byggs om när språket ändras */
static const char *const OPT_CLOCK[] = { "Auto", "133 MHz", "222 MHz", "333 MHz", "383 MHz", "403 MHz", "423 MHz", "443 MHz" };
static const char *OPT_HIGHMEM[5], *OPT_MSCACHE[4], *OPT_INFERNO[3], *OPT_LOGOS[4], *OPT_PICS[4];
static const char *OPT_UMDREG[4], *OPT_VSHREG[14], *OPT_LANG[LANG_COUNT];
static const char *OPT_BUTTON[CTL_COUNT];                                     /* the PSP's setting, then ✕ ○ □ △ (each is shown with its symbol, too) */
static const char *OPT_EQ[5];                                                 /* the equalizer: off and the XMB's four */
static const char *OPT_PSP_CONFIRM[2];                                       /* the PSP's own confirm button: Cross or Circle */
static const char *const OPT_START_SELECT[] = { "START", "SELECT" };          /* the options button */
static const char *const OPT_SELECT_START[] = { "SELECT", "START" };          /* the power menu button: the same setting, the other way round */

static void build_options(void)
{
    const char *off = L(S_OFF), *all = L(S_ALL), *def = L(S_STANDARD);
    const char *highmem[] = { off, def, L(S_AUTO), "+16 MiB", "Max" };
    const char *mscache[] = { off, "4 KiB", "8 KiB", "16 KiB" };
    const char *inferno[] = { off, "LRU", "RR" };
    const char *logos[] = { off, all, L(S_LOGOS_GAME), L(S_LOGOS_BOOT) };
    const char *pics[] = { off, all, "PIC0", "PIC1" };
    const char *umd[] = { def, L(S_R_JAPAN), L(S_R_AMERICA), L(S_R_EUROPE) };
    const char *vsh[] = { def, L(S_R_JAPAN), L(S_R_AMERICA), L(S_R_EUROPE), L(S_R_KOREA), L(S_R_UK),
                          L(S_R_LATAM), L(S_R_AUSTRALIA), L(S_R_HONGKONG), L(S_R_TAIWAN), L(S_R_RUSSIA),
                          L(S_R_CHINA), "Debug I", "Debug II" };
    memcpy(OPT_HIGHMEM, highmem, sizeof(highmem));
    memcpy(OPT_MSCACHE, mscache, sizeof(mscache));
    memcpy(OPT_INFERNO, inferno, sizeof(inferno));
    memcpy(OPT_LOGOS, logos, sizeof(logos));
    memcpy(OPT_PICS, pics, sizeof(pics));
    memcpy(OPT_UMDREG, umd, sizeof(umd));
    memcpy(OPT_VSHREG, vsh, sizeof(vsh));
    for (int i = 0; i < LANG_COUNT; i++)
        OPT_LANG[i] = lang_name(i);
    OPT_EQ[0] = off;
    OPT_EQ[1] = L(S_EQ_HEAVY);
    OPT_EQ[2] = L(S_EQ_POPS);
    OPT_EQ[3] = L(S_EQ_JAZZ);
    OPT_EQ[4] = L(S_EQ_UNIQUE);
    OPT_BUTTON[CTL_PSP] = L(S_CTL_PSP_SETTING);
    OPT_BUTTON[CTL_CROSS] = OPT_PSP_CONFIRM[0] = L(S_BTN_CROSS);
    OPT_BUTTON[CTL_CIRCLE] = OPT_PSP_CONFIRM[1] = L(S_BTN_CIRCLE);
    OPT_BUTTON[CTL_SQUARE] = L(S_BTN_SQUARE);
    OPT_BUTTON[CTL_TRIANGLE] = L(S_BTN_TRIANGLE);
}

/* ARK:s runlevel i PLUGINS.TXT → läsbar text */
static const char *runlevel_name(const char *rl)
{
    if (!strcasecmp(rl, "always") || !strcasecmp(rl, "all")) return L(S_ALL);
    if (!strcasecmp(rl, "game")) return L(S_TAB_GAMES);
    if (!strcasecmp(rl, "umd") || !strcasecmp(rl, "psp")) return L(S_RL_PSP);
    if (!strcasecmp(rl, "homebrew")) return L(S_RL_APPS);
    if (!strcasecmp(rl, "pops") || !strcasecmp(rl, "psx") || !strcasecmp(rl, "ps1")) return "PS1";
    if (!strcasecmp(rl, "vsh") || !strcasecmp(rl, "xmb")) return "XMB";
    if (!strcasecmp(rl, "launcher")) return "Launcher";
    return rl; /* spel-ID */
}

#define N(a) (int)(sizeof(a) / sizeof((a)[0]))

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float approach(float cur, float target, float speed, float dt)
{
    return cur + (target - cur) * (1.0f - expf(-speed * dt));
}
static u32 fade(u32 c, float a)
{
    return COLOR_ALPHA(c, (int)(((c >> 24) & 0xFF) * clampf(a, 0, 1)));
}

/* --- menyn --- */

static Item *add(Category *c, const char *label, ItemType type)
{
    Item *it = &c->items[c->count++];
    memset(it, 0, sizeof(*it));
    it->label = label;
    it->type = type;
    return it;
}

static void toggle(Category *c, const char *label, unsigned char *v)
{
    add(c, label, IT_TOGGLE)->value = v;
}

static void choice(Category *c, const char *label, unsigned char *v, const char *const *opts, int n)
{
    Item *it = add(c, label, IT_CHOICE);
    it->value = v;
    it->opts = opts;
    it->nopts = n;
    if (*v >= n)
        *v = 0;
}

static void action(Category *c, const char *label, int act)
{
    add(c, label, IT_ACTION)->action = act;
}

static void info(Category *c, const char *label, const char *value)
{
    snprintf(add(c, label, IT_INFO)->info, sizeof(((Item *)0)->info), "%s", value);
}

/* --- the Controls category: values that are buttons --- */

static unsigned char s_psp_circle; /* the PSP's own setting as the system says it: 1 = ○ confirms (read when the menu is built) */

static int glyph_of_ctl(int ctl)
{
    return ctl == CTL_CROSS ? BTN_CROSS : ctl == CTL_CIRCLE ? BTN_CIRCLE : ctl == CTL_SQUARE ? BTN_SQUARE : ctl == CTL_TRIANGLE ? BTN_TRIANGLE : -1;
}
/* the two values of "PSP button assignment": the symbol of the button that confirms */
static int glyph_psp(int opt) { return opt ? BTN_CIRCLE : BTN_CROSS; }
/* the values of Confirm and Back: the PSP's setting (shown with the symbol of the button it gives that job now), then ✕ ○ □ △ */
static int glyph_confirm(int opt) { return opt == CTL_PSP ? (controls_psp_circle() ? BTN_CIRCLE : BTN_CROSS) : glyph_of_ctl(opt); }
static int glyph_back(int opt) { return opt == CTL_PSP ? (controls_psp_circle() ? BTN_CROSS : BTN_CIRCLE) : glyph_of_ctl(opt); }
/* The PSP's own setting is changed in the system (so the XMB follows) and what the system says afterwards is what counts. */
static void set_psp(int opt)
{
    s_psp_circle = sys_set_confirm_is_circle(opt) ? 1 : 0;
    controls_psp_assignment(s_psp_circle);
}
static void set_confirm(int opt) { controls_set_confirm(opt); }
static void set_back(int opt) { controls_set_back(opt); }

/* --- Display and Sound: the PSP's own settings (the XMB's too), through the Impose service (system.c) --- */

#define VOLUME_MAX 30 /* the PSP's volume goes from 0 to 30 (the volume bar of osd.c takes the same) */
#define BRIGHT_MAX 100 /* a brightness the PSP reports above this is not believed (the row is left out) */

static unsigned char s_backlight, s_eq, s_avls; /* the row values: an index in the list of times, the equalizer's mode, 0 / 1 */
static unsigned char s_volume, s_bright;        /* the volume (0 .. 30, the list is the numbers), an index in the list of brightness levels */
static char s_vol_text[VOLUME_MAX + 1][4];
static const char *s_vol_opts[VOLUME_MAX + 1];
static int s_br_levels[6], s_br_n;              /* the PSP's brightness levels in the list: 0 to 3 (shown as 1 to 4) and its own, when it is none of those */
static char s_br_text[6][12];
static const char *s_br_opts[6];
static Item *s_br_item;                         /* its row, whose list changes when the PSP has a level that is not in it */
static int s_bl_secs[8], s_bl_n;                /* the times in the list, in seconds */
static char s_bl_text[8][20];
static const char *s_bl_opts[8];
static Item *s_bl_item;                         /* its row, whose list changes when the PSP has a time that is not in it */

static void bl_label(int secs, char *out, int size)
{
    if (secs >= 60 && secs % 60 == 0)
        snprintf(out, size, "%d %s", secs / 60, L(S_UNIT_MIN));
    else
        snprintf(out, size, "%d %s", secs, L(S_UNIT_SEC));
}

/* The list: 30 s, 1, 2, 5, 10 and 15 min, with the PSP's own value in it, in its place, when it is none of those: the row never says
 * anything else than what the PSP has. `now` is what the PSP says (seconds). */
static void bl_build(int now)
{
    static const int base[] = { 30, 60, 120, 300, 600, 900 };
    int n = 0, have = 0;
    for (int i = 0; i < 6; i++) {
        if (!have && now < base[i])
            s_bl_secs[n++] = now, have = 1;
        if (now == base[i])
            have = 1;
        s_bl_secs[n++] = base[i];
    }
    if (!have)
        s_bl_secs[n++] = now;
    s_bl_n = n;
    for (int i = 0; i < n; i++) {
        bl_label(s_bl_secs[i], s_bl_text[i], sizeof(s_bl_text[i]));
        s_bl_opts[i] = s_bl_text[i];
        if (s_bl_secs[i] == now)
            s_backlight = i;
    }
    if (s_bl_item) {
        s_bl_item->opts = s_bl_opts;
        s_bl_item->nopts = n;
    }
}

/* The brightness list: the XMB's four levels, 1 to 4. The PSP counts them from 0 (its brightness button gives 0 to 3, and 4 blacks the
 * screen out), so the list shows the PSP's level plus one and sets the level itself. A level of the PSP's that is none of the four is put in
 * the list after them, so the row never says anything else than what the PSP has. */
#define BRIGHT_LEVELS 4

static void br_build(int now)
{
    int n = 0;
    for (int i = 0; i < BRIGHT_LEVELS; i++)
        s_br_levels[n++] = i;
    if (now >= BRIGHT_LEVELS)
        s_br_levels[n++] = now;
    s_br_n = n;
    for (int i = 0; i < n; i++) {
        snprintf(s_br_text[i], sizeof(s_br_text[i]), "%d", s_br_levels[i] + 1);
        s_br_opts[i] = s_br_text[i];
        if (s_br_levels[i] == now)
            s_bright = i;
    }
    if (s_br_item) {
        s_br_item->opts = s_br_opts;
        s_br_item->nopts = n;
    }
}

static void set_brightness(int idx)
{
    int now = sys_set_brightness(s_br_levels[idx]);
    br_build(now >= 0 && now <= BRIGHT_MAX ? now : s_br_levels[s_bright]);
}
static void set_volume(int level)
{
    int now = sys_set_volume(level);
    if (now >= 0 && now <= VOLUME_MAX)
        s_volume = now;
}

static void set_backlight(int idx)
{
    int want = s_bl_secs[idx];
    int now = sys_set_backlight_off(want);
    bl_build(now >= 0 ? now : s_bl_secs[s_backlight]);
    if (now == want && g_backlight_off != want) {
        g_backlight_off = want; /* the PSP forgets the time at a restart: psp5 keeps it (psp5.cfg) and sets it again at every start */
        S.own_dirty = 1;
    }
}
static void set_eq(int mode)
{
    int now = sys_set_equalizer(mode);
    if (now >= 0 && now <= 4)
        s_eq = now;
}
static void set_avls(int on)
{
    int now = sys_set_avls(on);
    if (now == 0 || now == 1)
        s_avls = now;
}

static const char *model_name(void)
{
    int m = sys_model();
    return m == 0 ? "PSP-1000" : m == 1 ? "PSP-2000" : (m == 2 || m == 3 || m == 5 || m == 6 || m == 7 || m == 8) ? "PSP-3000"
         : m == 4 ? "PSP Go" : m == 10 ? "PSP E1000" : "PSP";
}

/* --- About: what the PSP says about itself (nothing here can be changed) --- */

static void fmt_bytes(char *out, int size, unsigned long long b)
{
    if (b >= 1024ull * 1024 * 1024)
        snprintf(out, size, "%.1f GB", (float)(b / (1024.0f * 1024.0f)) / 1024.0f);
    else if (b >= 1024 * 1024)
        snprintf(out, size, "%.1f MB", (float)b / (1024.0f * 1024.0f));
    else
        snprintf(out, size, "%u KB", (unsigned)(b / 1024));
}

static void about_space(Category *c, const char *label, unsigned long long free_b, unsigned long long total_b)
{
    char f[24], t[24], v[48];
    fmt_bytes(f, sizeof(f), free_b);
    fmt_bytes(t, sizeof(t), total_b);
    snprintf(v, sizeof(v), L(S_ABOUT_FREE_OF), f, t);
    info(c, label, v);
}

/* The rows of About, asked of the PSP now (the battery changes): called when the menu is built and every time About comes into view. */
static void fill_about(Category *c)
{
    SysAbout a;
    char v[48];
    c->count = 0;
    sys_about(&a);
    if (a.nickname[0])
        info(c, L(S_ABOUT_NICKNAME), a.nickname);
    info(c, L(S_ABOUT_MODEL), model_name());
    snprintf(v, sizeof(v), "%u.%u%u", a.devkit >> 24, (a.devkit >> 16) & 0xFF, (a.devkit >> 8) & 0xFF); /* 0x06060110 is 6.61 */
    info(c, L(S_ABOUT_SYSTEM), v);
    if (sys_ark_version()) {
        snprintf(v, sizeof(v), "ARK-%d", sys_ark_version());
        info(c, "CFW", v);
    }
    info(c, "psp5", PSP5_VERSION);
    if (a.has_mac) {
        snprintf(v, sizeof(v), "%02X:%02X:%02X:%02X:%02X:%02X", a.mac[0], a.mac[1], a.mac[2], a.mac[3], a.mac[4], a.mac[5]);
        info(c, L(S_ABOUT_MAC), v);
    }
    if (a.has_ms)
        about_space(c, L(S_F_MS), a.ms_free, a.ms_total);
    if (a.has_ef)
        about_space(c, L(S_F_INTERNAL), a.ef_free, a.ef_total);
    if (a.battery) {
        if (a.percent >= 0) {
            snprintf(v, sizeof(v), a.charging ? "%d %%, %s" : "%d %%", a.percent, L(S_ABOUT_CHARGING));
            info(c, L(S_ABOUT_BATTERY), v);
        }
        if (a.mv > 0) {
            snprintf(v, sizeof(v), "%d.%02d V", a.mv / 1000, (a.mv % 1000) / 10);
            info(c, L(S_ABOUT_VOLTAGE), v);
        }
        if (a.celsius >= 0) {
            snprintf(v, sizeof(v), "%d °C", a.celsius);
            info(c, L(S_ABOUT_TEMP), v);
        }
        if (a.mah_full > 0 && a.mah_left >= 0) {
            snprintf(v, sizeof(v), "%d / %d mAh", a.mah_left, a.mah_full);
            info(c, L(S_ABOUT_CAPACITY), v);
        }
    }
}

/* rebuild = 1: menyn byggs om (t.ex. nytt språk) utan att tappa osparade val */
static void build(int rebuild)
{
    build_options();
    memset(s_cats, 0, sizeof(s_cats));
    s_wrapped = NULL; /* nytt språk eller typsnitt: bryt om beskrivningarna */
    Category *c;

    c = &s_cats[CAT_PSP5];
    c->name = "psp5";
    c->desc = L(S_DESC_PSP5);
    toggle(c, L(S_BOOT_PSP5), &g_cfw.launcher);
    toggle(c, L(S_SOUNDS), &g_sound_on);
    c->items[c->count - 1].own = 1;
    toggle(c, L(S_BOOT_ANIM), &g_boot_anim);
    c->items[c->count - 1].own = 1;
    toggle(c, L(S_BATTERY_PCT), &g_battery_pct);
    c->items[c->count - 1].own = 1;
    choice(c, L(S_LANGUAGE), &g_lang_setting, OPT_LANG, LANG_COUNT);
    c->items[c->count - 1].own = 1;
    info(c, L(S_IS_LAUNCHER), L(sys_is_launcher() ? S_YES : S_NO));
    action(c, L(S_CLEAR_CACHE), SA_CLEAR_CACHE);

    c = &s_cats[CAT_THEME];
    c->name = L(S_CAT_THEME);
    c->desc = L(S_DESC_THEME);
    toggle(c, L(S_THEME_ART), &g_theme_art);
    c->items[c->count - 1].own = 1;
    for (int i = 0; i < theme_count() && c->count < MAX_ITEMS; i++) {
        Item *it = add(c, theme_get(i)->name, IT_THEME);
        it->action = i;
        it->own = 1;
    }

    c = &s_cats[CAT_CONTROLS];
    c->name = L(S_CAT_CONTROLS);
    c->desc = L(S_DESC_CONTROLS);
    s_psp_circle = sys_confirm_is_circle() ? 1 : 0;
    controls_psp_assignment(s_psp_circle); /* (what the PSP says may have changed since the start: the XMB, a game) */
    {
        Item *it;
        choice(c, L(S_CTL_PSP_ASSIGN), &s_psp_circle, OPT_PSP_CONFIRM, 2);
        it = &c->items[c->count - 1];
        it->glyph = glyph_psp; it->set = set_psp; it->immediate = 1;
        choice(c, L(S_CTL_CONFIRM), &g_ctl_confirm, OPT_BUTTON, CTL_COUNT);
        it = &c->items[c->count - 1];
        it->glyph = glyph_confirm; it->set = set_confirm; it->own = 1;
        choice(c, L(S_BACK), &g_ctl_back, OPT_BUTTON, CTL_COUNT);
        it = &c->items[c->count - 1];
        it->glyph = glyph_back; it->set = set_back; it->own = 1;
        /* Options and the power menu are one setting seen from both sides: START / SELECT, either way round */
        choice(c, L(S_OPTIONS), &g_ctl_options, OPT_START_SELECT, 2);
        c->items[c->count - 1].own = 1;
        choice(c, L(S_CTL_POWER), &g_ctl_options, OPT_SELECT_START, 2);
        c->items[c->count - 1].own = 1;
    }

    c = &s_cats[CAT_DISPLAY];
    c->name = L(S_CAT_DISPLAY_SOUND);
    c->desc = L(S_DESC_DISPLAY_SOUND);
    s_bl_item = s_br_item = NULL;
    {
        Item *it;
        int bl = sys_backlight_off(), eq = sys_equalizer(), av = sys_avls(), vol = sys_volume(), br = sys_brightness();
        if (br >= 0 && br <= BRIGHT_MAX) { /* (the screen first, then the sound) */
            br_build(br);
            choice(c, L(S_BRIGHTNESS), &s_bright, s_br_opts, s_br_n);
            it = &c->items[c->count - 1];
            it->set = set_brightness; it->immediate = 1;
            s_br_item = it;
        }
        if (bl >= 0) { /* a row is left out when the PSP does not have the setting or cannot be asked (a PC, an emulator) */
            bl_build(bl);
            choice(c, L(S_BACKLIGHT_OFF), &s_backlight, s_bl_opts, s_bl_n);
            it = &c->items[c->count - 1];
            it->set = set_backlight; it->immediate = 1;
            s_bl_item = it;
        }
        if (vol >= 0 && vol <= VOLUME_MAX) {
            for (int i = 0; i <= VOLUME_MAX; i++) {
                snprintf(s_vol_text[i], sizeof(s_vol_text[i]), "%d", i);
                s_vol_opts[i] = s_vol_text[i];
            }
            s_volume = vol;
            choice(c, L(S_VOLUME), &s_volume, s_vol_opts, VOLUME_MAX + 1);
            it = &c->items[c->count - 1];
            it->set = set_volume; it->immediate = 1;
        }
        if (eq >= 0 && eq <= 4) {
            s_eq = eq;
            choice(c, L(S_EQUALIZER), &s_eq, OPT_EQ, 5);
            it = &c->items[c->count - 1];
            it->set = set_eq; it->immediate = 1;
        }
        if (av == 0 || av == 1) {
            s_avls = av;
            toggle(c, L(S_AVLS), &s_avls);
            it = &c->items[c->count - 1];
            it->set = set_avls; it->immediate = 1;
        }
        if (!c->count)
            info(c, L(S_UNAVAILABLE), "");
    }

    c = &s_cats[CAT_NETWORK];
    c->name = L(S_CAT_NETWORK);
    c->desc = L(S_DESC_NETWORK);
    action(c, L(S_WIFI_CONNECTION), SA_WIFI);

    c = &s_cats[CAT_PERF];
    c->name = L(S_CAT_PERF);
    c->desc = L(S_DESC_PERF);
    /* ARK-4 har bara Auto/133/222/333 och på/av för extra minne och minneskortscache;
     * PSP-1000 har varken extra minne eller USB-laddning (som i arkMenu) */
    int ark4 = sys_ark_version() == 4, psp1000 = sys_is_psp1000(), go = sys_model() == 4;
    if (ark4) {
        if (g_cfw.clock_game > CLOCK_333) g_cfw.clock_game = CLOCK_333;
        if (g_cfw.clock_vsh > CLOCK_333) g_cfw.clock_vsh = CLOCK_333;
        if (g_cfw.highmem > 1) g_cfw.highmem = 1;
        if (g_cfw.mscache > 1) g_cfw.mscache = 1;
    }
    int nclock = ark4 ? CLOCK_333 + 1 : N(OPT_CLOCK);
    choice(c, L(S_CPU_GAME), &g_cfw.clock_game, OPT_CLOCK, nclock);
    choice(c, L(S_CPU_XMB), &g_cfw.clock_vsh, OPT_CLOCK, nclock);
    if (!psp1000) {
        if (ark4)
            toggle(c, L(S_EXTRA_MEM), &g_cfw.highmem);
        else
            choice(c, L(S_EXTRA_MEM), &g_cfw.highmem, OPT_HIGHMEM, N(OPT_HIGHMEM));
    }
    if (ark4)
        toggle(c, L(S_MS_CACHE), &g_cfw.mscache);
    else
        choice(c, L(S_MS_CACHE), &g_cfw.mscache, OPT_MSCACHE, N(OPT_MSCACHE));
    choice(c, L(S_ISO_CACHE), &g_cfw.infernocache, OPT_INFERNO, N(OPT_INFERNO));

    c = &s_cats[CAT_SYSTEM];
    c->name = L(S_CAT_SYSTEM);
    if (!psp1000)
        toggle(c, L(S_USB_CHARGE), &g_cfw.usbcharge);
    toggle(c, L(S_WPA2), &g_cfw.wpa2);
    choice(c, L(S_SKIP_LOGOS), &g_cfw.skiplogos, OPT_LOGOS, N(OPT_LOGOS));
    choice(c, L(S_HIDE_PICS), &g_cfw.hidepics, OPT_PICS, N(OPT_PICS));
    toggle(c, L(S_HIDE_MAC), &g_cfw.hidemac);
    toggle(c, L(S_HIDE_DLC), &g_cfw.hidedlc);
    toggle(c, L(S_NO_LED), &g_cfw.noled);
    if (go) {
        /* PSP Go: ingen UMD-läsare, men Pausa spel (som i arkMenu) */
        toggle(c, L(S_NO_PAUSE), &g_cfw.disablepause);
        toggle(c, L(S_HIB_BLOCK), &g_cfw.hibblock);
    } else {
        toggle(c, L(S_NO_UMD), &g_cfw.noumd);
    }
    toggle(c, L(S_NO_ANALOG), &g_cfw.noanalog);
    if (!ark4)
        toggle(c, L(S_VITA_MUTE), &g_cfw.vitamute);
    toggle(c, L(S_QA_FLAGS), &g_cfw.qaflags);
    choice(c, L(S_UMD_REGION), &g_cfw.regionchange, OPT_UMDREG, N(OPT_UMDREG));
    choice(c, L(S_XMB_REGION), &g_cfw.vshregion, OPT_VSHREG, N(OPT_VSHREG));

    c = &s_cats[CAT_PLUGINS];
    c->name = L(S_CAT_PLUGINS);
    c->desc = L(S_DESC_PLUGINS);
    for (int i = 0; i < plugins_count() && c->count < MAX_ITEMS; i++) {
        Plugin *p = plugins_get(i);
        if (!rebuild)
            s_plugin_on[i] = p->on;
        Item *it = add(c, p->name, IT_TOGGLE);
        it->value = &s_plugin_on[i];
        it->plugin = i + 1;
        snprintf(it->info, sizeof(it->info), "%s", runlevel_name(p->where));
    }
    if (!c->count)
        info(c, L(S_NO_PLUGINS), "");

    c = &s_cats[CAT_POWER];
    c->name = L(S_CAT_POWER);
    action(c, L(S_USB_MODE), SA_USB);
    action(c, L(S_FILES), SA_FILES);
    action(c, L(S_TO_XMB), SA_XMB);
    /* the same four, in the same order, as the popup of the SELECT button (ui.c) */
    action(c, L(S_SLEEP), SA_SUSPEND);
    action(c, L(S_SHUTDOWN), SA_SHUTDOWN);
    action(c, L(S_RESTART), SA_RESTART);
    action(c, L(S_RECOVERY), SA_RECOVERY);

    c = &s_cats[CAT_ABOUT];
    c->name = L(S_CAT_ABOUT);
    c->desc = L(S_DESC_ABOUT);
    fill_about(c);
}

void settings_open(void)
{
    font_ensure_cjk(); /* språkens egna namn (Русский, 日本語) */
    char path[200];
    snprintf(path, sizeof(path), "%sSETTINGS.TXT", sys_ark_dir());
    arkcfg_load(path);
    plugins_load(sys_ark_dir());
    build(0);
    memset(&S, 0, sizeof(S));
}

int settings_is_visit(int action)
{
    return action == SA_WIFI || action == SA_FILES || action == SA_USB;
}

void settings_notice(const char *msg)
{
    snprintf(S.toast, sizeof(S.toast), "%s", msg);
    S.toast_time = 2.8f;
}

int settings_commit(void)
{
    int changed = S.cfw_dirty || S.plugins_dirty;
    if (S.own_dirty)
        config_save(PSP5_DATA_DIR); /* ljud och språk gäller direkt, ingen omstart */
    if (S.cfw_dirty) {
        char path[200];
        snprintf(path, sizeof(path), "%sSETTINGS.TXT", sys_ark_dir());
        arkcfg_save(path);
    }
    if (S.plugins_dirty) {
        for (int i = 0; i < plugins_count(); i++)
            plugins_get(i)->on = s_plugin_on[i];
        plugins_save();
    }
    S.cfw_dirty = S.plugins_dirty = S.own_dirty = 0;
    return changed;
}

/* --- uppdatering --- */

/* The analog stick as a direction on each axis: -1, 0 or 1. It counts when it is pushed far (as in the other menus:
 * below 40, above 215) and only along the axis it is pushed furthest, so a diagonal push is one move, not two. */
static void stick_dir(const UiInput *in, int *dx, int *dy)
{
    *dx = in->lx < 40 ? -1 : in->lx > 215 ? 1 : 0;
    *dy = in->ly < 40 ? -1 : in->ly > 215 ? 1 : 0;
    if (*dx && *dy) {
        int fx = in->lx > 128 ? in->lx - 128 : 128 - in->lx, fy = in->ly > 128 ? in->ly - 128 : 128 - in->ly;
        if (fx > fy)
            *dy = 0;
        else
            *dx = 0;
    }
}

/* ↑ / ↓ on the D-pad or the stick (`stick` = the stick's up/down); a held direction repeats */
static int vertical(const UiInput *in, int stick, float dt)
{
    int dir = stick;
    if (in->held & PSP_CTRL_UP) dir = -1;
    if (in->held & PSP_CTRL_DOWN) dir = 1;
    if (!dir) {
        S.repeat_dir = 0;
        return 0;
    }
    if (dir != S.repeat_dir) {
        S.repeat_dir = dir;
        S.repeat_timer = 0.32f;
        return dir;
    }
    S.repeat_timer -= dt;
    if (S.repeat_timer <= 0) {
        S.repeat_timer += 0.08f;
        return dir;
    }
    return 0;
}

static void mark_dirty(const Item *it)
{
    if (it->immediate)
        return;
    if (it->own)
        S.own_dirty = 1;
    else if (it->plugin)
        S.plugins_dirty = 1;
    else
        S.cfw_dirty = 1;
}

/* Övre kanten för rad i i den högra listan (innan scroll). */
static float row_y(const Category *c, int i);

static float smooth(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

/* A new value for a choice. A new language changes every text, so the menu is built again. */
static void choice_set(Item *it, int v)
{
    if (it->set)
        it->set(v);
    else
        *it->value = v;
    mark_dirty(it);
    if (it->value == &g_lang_setting) {
        lang_apply();
        if (lang_needs_cjk())
            font_ensure_cjk();
        build(1);
    }
}

/* ✕ on a choice opens a popup with all its values (the way the other popups work), the current one chosen. No arrow changes a value
 * any more, so a stray press while moving through the list can't change a setting. */
static void pop_open(int cat, int item)
{
    const Item *it = &s_cats[cat].items[item];
    int vis = it->nopts < POP_ROWS ? it->nopts : POP_ROWS;
    S.pop = 1;
    S.pop_cat = cat;
    S.pop_item = item;
    S.pop_sel = *it->value;
    S.pop_top = (int)clampf(S.pop_sel - vis / 2, 0, it->nopts - vis);
    S.pop_scroll = S.pop_top;
}

/* The popup: ↑ ↓ choose (a held direction repeats), ✕ takes the value, ○ closes it and keeps the old one. */
static void pop_update(int move, u32 p)
{
    Item *it = &s_cats[S.pop_cat].items[S.pop_item];
    int vis = it->nopts < POP_ROWS ? it->nopts : POP_ROWS;
    if (move) {
        int next = (int)clampf(S.pop_sel + move, 0, it->nopts - 1);
        if (next != S.pop_sel)
            sound_play(SND_MOVE);
        S.pop_sel = next;
    }
    if (S.pop_sel < S.pop_top)
        S.pop_top = S.pop_sel;
    if (S.pop_sel > S.pop_top + vis - 1)
        S.pop_top = S.pop_sel - (vis - 1);
    if (p & PSP_CTRL_CROSS) {
        S.pop = 0;
        sound_play(SND_SELECT);
        if (S.pop_sel != *it->value)
            choice_set(it, S.pop_sel);
    } else if (p & PSP_CTRL_CIRCLE) {
        S.pop = 0;
        sound_play(SND_BACK);
    }
}

int settings_update(const UiInput *in, float dt)
{
    S.open = clampf(S.open + dt * 5.0f, 0, 1);
    S.time += dt;
    if (S.toast_time > 0)
        S.toast_time -= dt;

    Category *c = &s_cats[S.cat];
    int *sel = &S.item[S.cat];
    u32 p = in->pressed;
    int sx, sy;
    stick_dir(in, &sx, &sy);
    if (sx != S.stick_x) { /* a push of the stick sideways is one press of ← / →, like on the D-pad */
        if (sx < 0)
            p |= PSP_CTRL_LEFT;
        else if (sx > 0)
            p |= PSP_CTRL_RIGHT;
        S.stick_x = sx;
    }
    int move = vertical(in, sy, dt);

    S.pop_anim = clampf(S.pop_anim + (S.pop ? 7 : -7) * dt, 0, 1);
    if (S.pop) { /* a choice's popup is open: it gets all the buttons (the list behind it goes on scrolling into place) */
        pop_update(move, p);
        S.pop_scroll = approach(S.pop_scroll, S.pop_top, 14.0f, dt);
    } else if (S.focus == 0) {
        if (move) {
            int cat = (int)clampf(S.cat + move, 0, CAT_COUNT - 1);
            if (cat != S.cat) {
                sound_play(SND_MOVE);
                if (cat == CAT_ABOUT) { /* what it shows is asked of the PSP when it comes into view (the battery changes) */
                    fill_about(&s_cats[CAT_ABOUT]);
                    if (S.item[CAT_ABOUT] >= s_cats[CAT_ABOUT].count)
                        S.item[CAT_ABOUT] = s_cats[CAT_ABOUT].count - 1;
                }
            }
            S.cat = cat;
        }
        if ((p & (PSP_CTRL_CROSS | PSP_CTRL_RIGHT)) && c->count) {
            S.focus = 1;
            sound_play(SND_SELECT);
        }
        if (p & PSP_CTRL_CIRCLE) { /* back */
            sound_play(SND_BACK);
            return SA_CLOSE;
        }
    } else {
        Item *it = &c->items[*sel];
        if (move) {
            int next = (int)clampf(*sel + move, 0, c->count - 1);
            if (next != *sel)
                sound_play(SND_MOVE);
            *sel = next;
            it = &c->items[*sel];
        }
        if ((p & PSP_CTRL_CROSS) && it->type == IT_CHOICE) {
            pop_open(S.cat, *sel);
            sound_play(SND_SELECT);
        } else if ((p & PSP_CTRL_CROSS) && it->type == IT_THEME) {
            if (g_theme != it->action) {
                g_theme = it->action;
                mark_dirty(it);
            }
            sound_play(SND_SELECT);
        } else if ((p & PSP_CTRL_CROSS) && it->type == IT_TOGGLE) {
            if (it->set)
                it->set(!*it->value);
            else
                *it->value = !*it->value;
            mark_dirty(it);
            sound_play(SND_SELECT);
        } else if ((p & PSP_CTRL_CROSS) && it->type == IT_ACTION) {
            sound_play(SND_SELECT);
            sound_wait(400); /* låt ljudet höras innan något stängs */
            return it->action;
        } else if (p & (PSP_CTRL_CIRCLE | PSP_CTRL_LEFT)) {
            S.focus = 0;
            sound_play(SND_BACK);
        }
    }

    /* håll vald rad synlig */
    float target = 0;
    if (S.focus == 1 && c->count) {
        float y = row_y(c, *sel);
        float visible = LIST_BOTTOM - LIST_TOP - ROW_H;
        if (y > visible)
            target = y - visible;
    }
    S.scroll = approach(S.scroll, target, 14.0f, dt);
    if (S.cat < S.cat_top) /* the left column scrolls when there are more categories than fit */
        S.cat_top = S.cat;
    if (S.cat > S.cat_top + VIS_CATS - 1)
        S.cat_top = S.cat - (VIS_CATS - 1);
    S.cat_scroll = approach(S.cat_scroll, S.cat_top, 14.0f, dt);
    return SA_NONE;
}

/* --- ritning --- */

#define MAX_DESC_LINES 6
static char s_desc[MAX_DESC_LINES][160];

/* Radbryter beskrivningen till högerspaltens bredd. */
/* Längd i byte på UTF-8-tecknet som börjar i p */
static int utf8_len(const char *p)
{
    unsigned char c = (unsigned char)*p;
    return c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
}

/* Radbryter beskrivningen till högerspaltens bredd. Bryter vid mellanslag, och
 * mellan enskilda tecken när ett "ord" inte får plats (japanska har inga mellanslag). */
static int wrap_desc_now(const char *text)
{
    const float max_w = RIGHT_W - 8;
    int lines = 0;
    char line[sizeof(s_desc[0])] = "";
    const char *p = text;
    while (*p && lines < MAX_DESC_LINES) {
        while (*p == ' ')
            p++;
        if (!*p)
            break;
        /* nästa ord */
        const char *end = p;
        while (*end && *end != ' ')
            end += utf8_len(end);
        char test[sizeof(line) * 2];
        snprintf(test, sizeof(test), "%s%s%.*s", line, line[0] ? " " : "", (int)(end - p), p);
        if (text_width(0.56f, 0, test) <= max_w && strlen(test) < sizeof(line)) {
            snprintf(line, sizeof(line), "%s", test);
            p = end;
            continue;
        }
        if (line[0]) { /* ordet får inte plats – ny rad */
            snprintf(s_desc[lines++], sizeof(s_desc[0]), "%s", line);
            line[0] = 0;
            continue;
        }
        /* ordet är bredare än raden: ta så många tecken som får plats */
        int n = 0;
        while (p + n < end) {
            int l = utf8_len(p + n);
            snprintf(test, sizeof(test), "%.*s", n + l, p);
            if (n && (text_width(0.56f, 0, test) > max_w || n + l >= (int)sizeof(line)))
                break;
            n += l;
        }
        snprintf(s_desc[lines++], sizeof(s_desc[0]), "%.*s", n, p);
        p += n;
    }
    if (line[0] && lines < MAX_DESC_LINES)
        snprintf(s_desc[lines++], sizeof(s_desc[0]), "%s", line);
    return lines;
}

static int wrap_desc(const char *text)
{
    if (text != s_wrapped) {
        s_wrapped_n = wrap_desc_now(text);
        s_wrapped = text;
    }
    return s_wrapped_n;
}

static float desc_line_h(void)
{
    return lang_needs_cjk() ? 15.0f : 13.0f;
}

static float desc_height(const Category *c)
{
    return c->desc ? wrap_desc(c->desc) * desc_line_h() + 10 : 0;
}

static float row_y(const Category *c, int i)
{
    return desc_height(c) + i * ROW_H;
}

/* Temat som visas: det markerade i temalistan (förhandsvisning), annars det valda */
static const Theme *shown_theme(void)
{
    const Category *c = &s_cats[CAT_THEME];
    const Item *it = &c->items[S.item[CAT_THEME]];
    if (S.cat == CAT_THEME && S.focus == 1 && S.item[CAT_THEME] < c->count && it->type == IT_THEME)
        return theme_get(it->action);
    return theme_cur();
}

static void draw_background(const Theme *t)
{
    theme_backdrop(t, S.time, 1);
}

static void draw_toggle(float x, float y, int on, u32 on_color, float a)
{
    /* 26 x 14 with a knob 10 across, 2 px from the edge on every side: all on whole pixels */
    gfx_round_rect(x, y, 26, 14, 7, fade(on ? on_color : RGBA(74, 76, 84, 255), a));
    gfx_circle(on ? x + 19 : x + 7, y + 7, 5, fade(on ? LIGHT : RGBA(196, 196, 202, 255), a));
}

static void draw_item(const Item *it, float y, int selected, const Theme *shown, float a)
{
    float right = RIGHT_X + RIGHT_W - 8;
    char value[64] = "";
    switch (it->type) {
    case IT_TOGGLE:
        draw_toggle(right - 26, y + 6, *it->value != 0, theme_toggle_on(shown), a);
        right -= 34;
        snprintf(value, sizeof(value), "%s", it->info); /* plugins: var den laddas */
        break;
    case IT_CHOICE: { /* just the value: ✕ opens a popup with the others */
        snprintf(value, sizeof(value), "%s", it->opts[*it->value]);
        int g = it->glyph ? it->glyph(*it->value) : -1; /* a button: its symbol at the end of the row, the words (if any) before it */
        if (g >= 0) {
            float bw = gfx_button_width(g);
            gfx_button(right - ceilf(bw / 2), y + 13, 6.5f, g, fade(LIGHT, a), fade(RGBA(20, 20, 26, 255), a));
            right -= ceilf(bw) + (value[0] ? 6 : 0);
        }
        break;
    }
    case IT_INFO:
        snprintf(value, sizeof(value), "%s", it->info);
        break;
    case IT_THEME: {
        /* liten förhandsvisning, och en bock för temat som används */
        theme_swatch(theme_get(it->action), right - 40, y + 5, 40, 16, a);
        right -= 48;
        if (it->action == g_theme) {
            float cx = right - 6, cy = y + 13;
            float xy[6] = { cx - 5, cy, cx - 1.5f, cy + 3.5f, cx + 5, cy - 4 };
            u32 col[3] = { fade(LIGHT, a), fade(LIGHT, a), fade(LIGHT, a) };
            gfx_stroke(xy, col, 3, 1.8f);
            right -= 16;
        }
        break;
    }
    case IT_ACTION:
        break; /* an action has no value to show */
    }
    float value_w = 0;
    if (value[0]) {
        float vs = text_fit(0.58f, 0.46f, 0, RIGHT_W * 0.55f, value);
        value_w = text_width(vs, 0, value) + 12;
        text_draw(right, y + 17, vs, 0, fade(TEXT_DIM, a), ALIGN_RIGHT, value);
    }
    /* långa etiketter (t.ex. tyska, japanska) krymper i stället för att krocka med värdet */
    float ls = text_fit(0.62f, 0.48f, 0, right - value_w - (RIGHT_X + 8), it->label);
    text_draw(RIGHT_X + 8, y + 17, ls, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, it->label);
}

/* The popup of a choice, in the middle of the screen and rising into place like the power popup: the values one under the other, the
 * chosen one framed, and a thin bar at the right when there are more than POP_ROWS. */
static void draw_popup(void)
{
    float a = smooth(S.pop_anim);
    if (a <= 0)
        return;
    const Item *it = &s_cats[S.pop_cat].items[S.pop_item];
    int n = it->nopts, vis = n < POP_ROWS ? n : POP_ROWS, bar = n > vis;
    float w = 200;
    for (int k = 0; k < n; k++) {
        int g = it->glyph ? it->glyph(k) : -1;
        w = fmaxf(w, ceilf(text_width(0.62f, 0, it->opts[k])) + 32 + (bar ? 12 : 0) + (g >= 0 ? ceilf(gfx_button_width(g)) + (it->opts[k][0] ? 8 : 0) : 0));
    }
    w = 2 * ceilf(fminf(w, SCREEN_W - 40) / 2); /* (even, so that the panel sits on whole pixels) */
    float h = 2 * POPUP_EDGE + vis * POPUP_ITEM, x = (SCREEN_W - w) / 2, y = (SCREEN_H - h) / 2 + (1 - a) * 10;
    float inner = w - 2 * POPUP_EDGE - (bar ? 12 : 0);
    popup_dim(a);
    popup_panel(x, y, w, h, POPUP_DARK, a);
    gfx_clip(x, y + POPUP_EDGE, w, vis * POPUP_ITEM);
    for (int k = 0; k < n; k++) {
        float ry = y + POPUP_EDGE + (k - S.pop_scroll) * POPUP_ITEM;
        if (ry < y - POPUP_ITEM || ry > y + h)
            continue;
        u32 col = POPUP_TEXT;
        if (k == S.pop_sel)
            popup_choice(x + POPUP_EDGE, ry, inner, POPUP_ITEM, a);
        else
            col = fade(col, 0.8f);
        float tx = x + 16;
        int g = it->glyph ? it->glyph(k) : -1;
        if (g >= 0) { /* a value that is a button: its symbol first, then the words, if there are any */
            float bw = ceilf(gfx_button_width(g));
            gfx_button(tx + bw / 2, ry + 14, 6.5f, g, fade(LIGHT, a), fade(RGBA(20, 20, 26, 255), a * (k == S.pop_sel ? 1.0f : 0.9f)));
            tx += bw + 8;
        }
        if (it->opts[k][0])
            text_draw(tx, ry + 18, text_fit(0.62f, 0.45f, 0, inner - 22 - (tx - (x + 16)), it->opts[k]), 0, fade(col, a), ALIGN_LEFT, it->opts[k]);
    }
    gfx_clip_reset();
    if (bar) { /* where the shown part is in the whole list */
        float track_y = y + POPUP_EDGE + 4, track_h = vis * POPUP_ITEM - 8, bx = x + w - POPUP_EDGE - 6;
        float thumb_h = floorf(fmaxf(12.0f, track_h * vis / n) + 0.5f);
        float thumb_y = track_y + floorf((track_h - thumb_h) * clampf(S.pop_scroll / (n - vis), 0, 1) + 0.5f);
        gfx_round_rect(bx, track_y, 3, track_h, 1.5f, RGBA(255, 255, 255, (int)(28 * a)));
        gfx_round_rect(bx, thumb_y, 3, thumb_h, 1.5f, RGBA(255, 255, 255, (int)(150 * a)));
    }
}

void settings_draw(void)
{
    float a = S.open;
    float slide = (1 - a) * 10;
    const Category *c = &s_cats[S.cat];

    gfx_begin(RGBA(20, 20, 24, 255));
    const Theme *shown = shown_theme();
    draw_background(shown);
    text_draw(LEFT_X + 4 + slide, TITLE_Y, 0.95f, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, L(S_SETTINGS));

    /* kategorier (more than fit: the column scrolls, the rows at its ends fade, and a small arrow says there are more) */
    for (int i = 0; i < CAT_COUNT; i++) {
        float y = LIST_TOP + (i - S.cat_scroll) * 30;
        float edge = clampf((y - (LIST_TOP - ROW_H)) / ROW_H, 0, 1) * clampf((LIST_TOP + VIS_CATS * 30 - y) / ROW_H, 0, 1);
        if (edge <= 0)
            continue;
        float ca = a * edge;
        int selected = i == S.cat;
        float ta = selected ? 1.0f : S.focus ? 0.42f : 0.8f;
        if (selected && S.focus == 0) {
            gfx_rect(LEFT_X, y, LEFT_W, ROW_H, RGBA(255, 255, 255, (int)(16 * ca)));
            gfx_frame(LEFT_X, y, LEFT_W, ROW_H, 1, fade(RGBA(232, 232, 236, 230), ca));
        }
        float cs = text_fit(0.66f, 0.5f, 0, LEFT_W - 16, s_cats[i].name);
        text_draw(LEFT_X + 8 + slide, y + 17.5f, cs, 0, fade(TEXT_MAIN, ca * ta), ALIGN_LEFT, s_cats[i].name);
    }
    for (int dir = -1; dir <= 1; dir += 2) { /* ˄ above and ˅ below, when there are more categories that way */
        if (dir < 0 ? S.cat_top <= 0 : S.cat_top + VIS_CATS >= CAT_COUNT)
            continue;
        float cx = LEFT_X + LEFT_W / 2, cy = dir < 0 ? LIST_TOP - 8 : LIST_TOP + VIS_CATS * 30 + 4;
        float xy[6] = { cx - 5, cy - dir * 2, cx, cy + dir * 2, cx + 5, cy - dir * 2 };
        u32 col[3] = { fade(TEXT_DIM, a), fade(TEXT_DIM, a), fade(TEXT_DIM, a) };
        gfx_stroke(xy, col, 3, 1.6f);
    }

    /* alternativ */
    float top = LIST_TOP - S.scroll;
    if (c->desc) {
        int n = wrap_desc(c->desc);
        for (int l = 0; l < n; l++) {
            float y = top + l * desc_line_h() + 9;
            if (y > LIST_TOP - 4 && y < LIST_BOTTOM)
                text_draw(RIGHT_X + 8 + slide, y, 0.56f, 0, fade(TEXT_DIM, a), ALIGN_LEFT, s_desc[l]);
        }
    }
    for (int i = 0; i < c->count; i++) {
        float y = top + row_y(c, i);
        if (y < LIST_TOP - ROW_H || y > LIST_BOTTOM)
            continue;
        /* tona ut rader som glider ut ur listan */
        float edge = clampf((y - (LIST_TOP - ROW_H * 0.6f)) / (ROW_H * 0.6f), 0, 1) *
                     clampf((LIST_BOTTOM - y) / (ROW_H * 0.6f), 0, 1);
        float ra = a * edge;
        int selected = S.focus == 1 && i == S.item[S.cat];
        if (selected) {
            gfx_rect(RIGHT_X, y, RIGHT_W, ROW_H, RGBA(255, 255, 255, (int)(16 * ra)));
            gfx_frame(RIGHT_X, y, RIGHT_W, ROW_H, 1, fade(RGBA(232, 232, 236, 230), ra));
        } else if (i < c->count - 1 && !(S.focus == 1 && i + 1 == S.item[S.cat])) {
            gfx_rect(RIGHT_X + 4, y + ROW_H - 1, RIGHT_W - 8, 1, RGBA(255, 255, 255, (int)(26 * ra)));
        }
        draw_item(&c->items[i], y, selected, shown, ra);
    }

    draw_popup();
    if (S.toast_time > 0) /* as the toast of the other screens: 2.8 s, fading in and out */
        popup_toast(S.toast, clampf(S.toast_time / 0.4f, 0, 1) * clampf((2.8f - S.toast_time) / 0.2f, 0, 1));
    osd_draw();
    /* in-toning */
    if (a < 1)
        gfx_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(255 * (1 - a))));
    gfx_end();
}
