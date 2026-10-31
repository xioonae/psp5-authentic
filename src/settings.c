/* psp5 – made by Mille Weber */
/* Settings in the spirit of the PS5's: a dark surface with a spotlight and one list at a time (as in the file manager: thin dividers,
 * a thin light frame round the chosen row, a thin bar at the right when the list is longer than the screen). The first list has the
 * categories; a category is a list of settings, and System is a list of pages of its own. ○ goes back one list. All graphics are drawn here. */
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
#include "power.h"
#include "popup.h"

#define TITLE_Y     34.0f
#define LIST_X      24.0f  /* the same sides as the list of the file manager: 24 px from the edges of the screen */
#define LIST_W      432.0f
#define ROW_H       26.0f
#define LIST_ROWS        7      /* rows shown at once; a longer list scrolls (as in the file manager and in Manage Saved Data) */
#define LIST_TOP    58.0f
#define ICON_W      26.0f  /* the room of the icon in front of the label (the categories only) */
#define MAX_ITEMS   (MAX_PLUGINS + 4)
#define PSP5_VERSION "1.2.0"
#define APP_NAME     "psp5 authentic"

#define TEXT_MAIN RGBA(238, 238, 242, 255)
#define TEXT_DIM  RGBA(160, 162, 172, 255)
#define LIGHT     RGBA(236, 236, 240, 255)

typedef enum { IT_TOGGLE, IT_CHOICE, IT_ACTION, IT_INFO, IT_THEME, IT_PAGE, IT_LANG } ItemType;

/* the icons of the first list (drawn here, no image files) */
enum { ICO_NONE, ICO_APP, ICO_NETWORK, ICO_SYSTEM, ICO_STORAGE, ICO_SCREEN, ICO_CONTROLS };

typedef struct {
    const char *label;
    ItemType type;
    unsigned char *value;        /* TOGGLE/CHOICE */
    const char *const *opts;     /* CHOICE */
    int nopts;
    int action;                  /* ACTION: SA_*, THEME: temats index, LANG: the language, PAGE: the page it opens */
    int icon;                    /* PAGE: ICO_* in front of the label (0 = none) */
    char info[48];               /* INFO-värde / undertext */
    int plugin;                  /* >0: plugin nr (1-baserat) */
    int own;                     /* psp5:s egen inställning (psp5.cfg) */
    int immediate;               /* the system has it, not a file of ours: nothing to save (the PSP's button assignment) */
    int (*glyph)(int opt);       /* CHOICE: a button symbol (BTN_*) to draw for value `opt`, or -1: the values that are buttons */
    void (*set)(int opt);        /* CHOICE / TOGGLE: how the value is set when it is more than storing it (the buttons that swap, the PSP's own settings), else NULL */
} Item;

typedef struct {
    const char *name;
    Item items[MAX_ITEMS];
    int count;
} Category; /* a page: a list of rows */

/* The pages. PG_ROOT is the first list; the six rows of it open the pages after it; the rows of PG_SYSTEM open the six after that. */
enum { PG_ROOT, PG_PSP5, PG_NETWORK, PG_SYSTEM, PG_STORAGE, PG_SCREEN, PG_CONTROLS, PG_INFO, PG_APPEAR, PG_LANG, PG_ARK, PG_PERF, PG_PLUGINS, PG_COUNT };
static const signed char PARENT[PG_COUNT] = { -1, PG_ROOT, PG_ROOT, PG_ROOT, PG_ROOT, PG_ROOT, PG_ROOT, PG_SYSTEM, PG_SYSTEM, PG_SYSTEM, PG_SYSTEM, PG_SYSTEM, PG_SYSTEM };

static Category s_cats[PG_COUNT];
static char s_ark_name[48]; /* "ARK-5 Settings" or "ARK-4 Settings": the name of the page that has the settings of ARK itself */
static unsigned char s_plugin_on[MAX_PLUGINS];
static struct {
    int page, item[PG_COUNT]; /* the page shown, and the row chosen on each page (kept when the page is left) */
    int top[PG_COUNT];  /* the first row shown on each page: it moves only when the chosen row would leave the screen */
    float scroll, open, time; /* scroll: the first row shown, on its way to top[page] */
    int cfw_dirty, plugins_dirty, own_dirty;
    int repeat_dir;
    float repeat_timer;
    /* the popup of a choice (✕ on it): the list of values, the one chosen, and the first one shown when there are more than fit */
    int pop, pop_page, pop_item, pop_sel, pop_top;
    float pop_anim, pop_scroll;
    char toast[96];     /* a message that is shown for a moment (settings_notice) */
    float toast_time;
} S;

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

/* --- Console Information: what the PSP says about itself, and where psp5 authentic comes from (nothing here can be changed) --- */

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
    info(c, APP_NAME, PSP5_VERSION);
    info(c, L(S_ABOUT_ORIGINAL), "08Weber"); /* psp5 authentic is a fork of psp5, which Mille Weber (08Weber) made */
    info(c, L(S_ABOUT_FORK), "Xioonae");
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

/* a row that opens another page */
static void page_row(Category *c, int page, int icon)
{
    Item *it = add(c, s_cats[page].name, IT_PAGE);
    it->action = page;
    it->icon = icon;
}

/* rebuild = 1: the menu is built again (a new language, say) without losing the choices that are not saved yet */
static void build(int rebuild)
{
    build_options();
    memset(s_cats, 0, sizeof(s_cats));
    Category *c;
    int ark = sys_ark_version();
    snprintf(s_ark_name, sizeof(s_ark_name), L(S_CAT_ARK), ark == 4 || ark == 5 ? (ark == 4 ? "ARK-4" : "ARK-5") : "ARK");

    /* the names first: the rows that open a page are named after it */
    s_cats[PG_PSP5].name = APP_NAME;
    s_cats[PG_NETWORK].name = L(S_CAT_NETWORK);
    s_cats[PG_SYSTEM].name = L(S_CAT_SYSTEM);
    s_cats[PG_STORAGE].name = L(S_CAT_STORAGE);
    s_cats[PG_SCREEN].name = L(S_CAT_SCREEN_SOUND);
    s_cats[PG_CONTROLS].name = L(S_CAT_CONTROLS);
    s_cats[PG_INFO].name = L(S_CAT_INFO);
    s_cats[PG_APPEAR].name = L(S_CAT_APPEARANCE);
    s_cats[PG_LANG].name = L(S_LANGUAGE);
    s_cats[PG_ARK].name = s_ark_name;
    s_cats[PG_PERF].name = L(S_CAT_PERF);
    s_cats[PG_PLUGINS].name = L(S_CAT_PLUGINS);

    c = &s_cats[PG_ROOT];
    c->name = L(S_SETTINGS);
    page_row(c, PG_PSP5, ICO_APP);
    page_row(c, PG_NETWORK, ICO_NETWORK);
    page_row(c, PG_SYSTEM, ICO_SYSTEM);
    page_row(c, PG_STORAGE, ICO_STORAGE);
    page_row(c, PG_SCREEN, ICO_SCREEN);
    page_row(c, PG_CONTROLS, ICO_CONTROLS);

    c = &s_cats[PG_PSP5];
    toggle(c, L(S_BOOT_PSP5), &g_cfw.launcher);
    toggle(c, L(S_SOUNDS), &g_sound_on);
    c->items[c->count - 1].own = 1;
    toggle(c, L(S_BOOT_ANIM), &g_boot_anim);
    c->items[c->count - 1].own = 1;
    toggle(c, L(S_BATTERY_PCT), &g_battery_pct);
    c->items[c->count - 1].own = 1;
    info(c, L(S_IS_LAUNCHER), L(sys_is_launcher() ? S_YES : S_NO));
    action(c, L(S_CLEAR_CACHE), SA_CLEAR_CACHE);

    c = &s_cats[PG_NETWORK];
    action(c, L(S_WIFI_CONNECTION), SA_WIFI);
    action(c, L(S_FTP_SERVER), SA_FTP);

    c = &s_cats[PG_SYSTEM];
    page_row(c, PG_INFO, 0);
    page_row(c, PG_APPEAR, 0);
    page_row(c, PG_LANG, 0);
    page_row(c, PG_ARK, 0);
    page_row(c, PG_PERF, 0);
    page_row(c, PG_PLUGINS, 0);

    /* Exit to XMB, Sleep, Shut down, Restart and Recovery menu are not here: they are the power popup of the SELECT button (power.c), on every screen */
    c = &s_cats[PG_STORAGE];
    action(c, L(S_FILES), SA_FILES);
    action(c, L(S_USB_MODE), SA_USB);

    c = &s_cats[PG_SCREEN];
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

    c = &s_cats[PG_CONTROLS];
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

    c = &s_cats[PG_INFO];
    fill_about(c);

    c = &s_cats[PG_APPEAR];
    toggle(c, L(S_THEME_ART), &g_theme_art);
    c->items[c->count - 1].own = 1;
    for (int i = 0; i < theme_count() && c->count < MAX_ITEMS; i++) {
        Item *it = add(c, theme_get(i)->name, IT_THEME);
        it->action = i;
        it->own = 1;
    }

    c = &s_cats[PG_LANG];
    for (int i = 0; i < LANG_COUNT; i++) {
        Item *it = add(c, OPT_LANG[i], IT_LANG);
        it->action = i;
        it->own = 1;
    }

    /* ARK's own settings (SETTINGS.TXT): what the old System category had */
    c = &s_cats[PG_ARK];
    int ark4 = ark == 4, psp1000 = sys_is_psp1000(), go = sys_model() == 4;
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

    c = &s_cats[PG_PERF];
    /* ARK-4 har bara Auto/133/222/333 och på/av för extra minne och minneskortscache;
     * PSP-1000 har varken extra minne eller USB-laddning (som i arkMenu) */
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

    c = &s_cats[PG_PLUGINS];
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

int settings_modal(void)
{
    return S.pop != 0;
}

int settings_is_visit(int action)
{
    return action == SA_WIFI || action == SA_FTP || action == SA_FILES || action == SA_USB;
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
 * below 40, above 215) and only along the axis it is pushed furthest, so a diagonal push is one move, not two. Only up and down do
 * anything in Settings (← and → are not used any more), but a push that is mostly sideways must not move the list. */
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

static float smooth(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }

/* A new value for a choice. */
static void choice_set(Item *it, int v)
{
    if (it->set)
        it->set(v);
    else
        *it->value = v;
    mark_dirty(it);
}

/* A new language changes every text, so the menu is built again (the page, the row and the unsaved choices stay). */
static void set_language(const Item *it)
{
    g_lang_setting = it->action;
    mark_dirty(it);
    lang_apply();
    if (lang_needs_cjk())
        font_ensure_cjk();
    build(1);
}

/* ✕ on a choice opens a popup with all its values (the way the other popups work), the current one chosen. No arrow changes a value
 * any more, so a stray press while moving through the list can't change a setting. */
static void pop_open(int page, int item)
{
    const Item *it = &s_cats[page].items[item];
    int vis = it->nopts < POP_ROWS ? it->nopts : POP_ROWS;
    S.pop = 1;
    S.pop_page = page;
    S.pop_item = item;
    S.pop_sel = *it->value;
    S.pop_top = (int)clampf(S.pop_sel - vis / 2, 0, it->nopts - vis);
    S.pop_scroll = S.pop_top;
}

/* The popup: ↑ ↓ choose (a held direction repeats), ✕ takes the value, ○ closes it and keeps the old one. */
static void pop_update(int move, u32 p)
{
    Item *it = &s_cats[S.pop_page].items[S.pop_item];
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

/* The page comes into view: the chosen row is kept (and is on the list), and the list starts where it was, or just enough further on to show the row. */
static void page_enter(int page)
{
    const Category *c = &s_cats[page];
    int *sel = &S.item[page], *top = &S.top[page];
    if (page == PG_INFO) /* what it shows is asked of the PSP when it comes into view (the battery changes) */
        fill_about(&s_cats[PG_INFO]);
    if (*sel >= c->count)
        *sel = c->count ? c->count - 1 : 0;
    if (*sel < *top)
        *top = *sel;
    if (*sel > *top + LIST_ROWS - 1)
        *top = *sel - (LIST_ROWS - 1);
    S.page = page;
    S.scroll = *top;
}

int settings_update(const UiInput *in, float dt)
{
    S.open = clampf(S.open + dt * 5.0f, 0, 1);
    S.time += dt;
    if (S.toast_time > 0)
        S.toast_time -= dt;

    Category *c = &s_cats[S.page];
    int *sel = &S.item[S.page];
    u32 p = in->pressed;
    int sx, sy;
    stick_dir(in, &sx, &sy);
    int move = vertical(in, sy, dt);

    S.pop_anim = clampf(S.pop_anim + (S.pop ? 7 : -7) * dt, 0, 1);
    if (S.pop) { /* a choice's popup is open: it gets all the buttons (the list behind it goes on scrolling into place) */
        pop_update(move, p);
        S.pop_scroll = approach(S.pop_scroll, S.pop_top, 14.0f, dt);
    } else {
        if (move && c->count) {
            int next = (int)clampf(*sel + move, 0, c->count - 1);
            if (next != *sel)
                sound_play(SND_MOVE);
            *sel = next;
        }
        Item *it = c->count ? &c->items[*sel] : NULL;
        if (it && (p & PSP_CTRL_CROSS) && it->type == IT_PAGE) {
            sound_play(SND_SELECT);
            page_enter(it->action);
            sel = &S.item[S.page];
        } else if (it && (p & PSP_CTRL_CROSS) && it->type == IT_CHOICE) {
            pop_open(S.page, *sel);
            sound_play(SND_SELECT);
        } else if (it && (p & PSP_CTRL_CROSS) && it->type == IT_THEME) {
            if (g_theme != it->action) {
                g_theme = it->action;
                mark_dirty(it);
            }
            sound_play(SND_SELECT);
        } else if (it && (p & PSP_CTRL_CROSS) && it->type == IT_LANG) {
            if (g_lang_setting != it->action)
                set_language(it);
            sound_play(SND_SELECT);
        } else if (it && (p & PSP_CTRL_CROSS) && it->type == IT_TOGGLE) {
            if (it->set)
                it->set(!*it->value);
            else
                *it->value = !*it->value;
            mark_dirty(it);
            sound_play(SND_SELECT);
        } else if (it && (p & PSP_CTRL_CROSS) && it->type == IT_ACTION) {
            sound_play(SND_SELECT);
            sound_wait(400); /* låt ljudet höras innan något stängs */
            return it->action;
        } else if (p & PSP_CTRL_CIRCLE) { /* back: up one list, and out of Settings from the first one */
            sound_play(SND_BACK);
            if (PARENT[S.page] < 0)
                return SA_CLOSE;
            page_enter(PARENT[S.page]);
            sel = &S.item[S.page];
        }
    }

    /* the list moves only when the chosen row would leave the screen (the file manager's way): the first row shown follows it */
    int *top = &S.top[S.page];
    if (*sel < *top)
        *top = *sel;
    if (*sel > *top + LIST_ROWS - 1)
        *top = *sel - (LIST_ROWS - 1);
    S.scroll += (*top - S.scroll) * clampf(dt * 16, 0, 1);
    return SA_NONE;
}

/* --- ritning --- */

/* Temat som visas: det markerade i temalistan (förhandsvisning), annars det valda */
static const Theme *shown_theme(void)
{
    const Category *c = &s_cats[PG_APPEAR];
    const Item *it = &c->items[S.item[PG_APPEAR]];
    if (S.page == PG_APPEAR && S.item[PG_APPEAR] < c->count && it->type == IT_THEME)
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

/* The icon of a category, in a box of 16 x 16 round (cx, cy) */
static void draw_icon(int icon, float cx, float cy, float a)
{
    u32 hi = fade(LIGHT, a), mid = fade(RGBA(186, 188, 198, 255), a), lo = fade(RGBA(120, 122, 134, 255), a), dark = fade(RGBA(38, 40, 48, 255), a);
    switch (icon) {
    case ICO_APP: /* the PlayStation logo of the start screen, 12 high and as wide as it is, in the middle */
        gfx_logo((int)cx - icon_logo_width(12) / 2, (int)cy - 6, 12, hi);
        break;
    case ICO_NETWORK:
        gfx_wifi(cx, cy, 7, hi);
        break;
    case ICO_SYSTEM: /* a cube: three faces, soft edged */
        gfx_cube(cx, cy, 8, hi);
        break;
    case ICO_STORAGE: /* a drive: a can with a lighter lid */
        gfx_round_rect(cx - 6, cy + 1, 12, 7, 3, lo);
        gfx_rect(cx - 6, cy - 4, 12, 8, lo);
        gfx_round_rect(cx - 6, cy - 8, 12, 7, 3, hi);
        break;
    case ICO_SCREEN: /* a screen on its foot */
        gfx_round_rect(cx - 8, cy - 7, 16, 11, 2, hi);
        gfx_rect(cx - 6, cy - 5, 12, 7, dark);
        gfx_rect(cx - 1, cy + 4, 2, 3, mid);
        gfx_rect(cx - 4, cy + 7, 8, 1, mid);
        break;
    case ICO_CONTROLS: /* a controller: the pad, two buttons */
        gfx_round_rect(cx - 8, cy - 4, 16, 9, 4, hi);
        gfx_rect(cx - 6, cy - 1, 5, 3, dark);
        gfx_rect(cx - 5, cy - 2, 3, 5, dark);
        gfx_circle(cx + 2, cy - 1, 1, dark);
        gfx_circle(cx + 5, cy + 1, 1, dark);
        break;
    }
}

static void draw_item(const Item *it, float y, const Theme *shown, float a)
{
    float right = LIST_X + LIST_W - 10, left = LIST_X + 8;
    char value[64] = "";
    if (it->icon) {
        draw_icon(it->icon, LIST_X + 8 + 10, y + ROW_H / 2, a);
        left += ICON_W;
    }
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
    case IT_THEME:
        /* liten förhandsvisning, och en bock för temat som används */
        theme_swatch(theme_get(it->action), right - 40, y + 5, 40, 16, a);
        right -= 48;
        /* fall through */
    case IT_LANG:
        if ((it->type == IT_THEME ? it->action == g_theme : it->action == g_lang_setting)) {
            float cx = right - 6, cy = y + 13;
            float xy[6] = { cx - 5, cy, cx - 1.5f, cy + 3.5f, cx + 5, cy - 4 };
            u32 col[3] = { fade(LIGHT, a), fade(LIGHT, a), fade(LIGHT, a) };
            gfx_stroke(xy, col, 3, 1.8f);
            right -= 16;
        }
        break;
    case IT_ACTION:
    case IT_PAGE:
        break; /* an action, or a page, has no value to show */
    }
    float value_w = 0;
    if (value[0]) {
        float vs = text_fit(0.58f, 0.46f, 0, LIST_W * 0.55f, value);
        value_w = text_width(vs, 0, value) + 12;
        text_draw(right, y + 17, vs, 0, fade(TEXT_DIM, a), ALIGN_RIGHT, value);
    }
    /* långa etiketter (t.ex. tyska, japanska) krymper i stället för att krocka med värdet */
    float ls = text_fit(it->icon ? 0.7f : 0.62f, 0.48f, 0, right - value_w - left, it->label);
    text_draw(left, y + 17, ls, 0, fade(TEXT_MAIN, a), ALIGN_LEFT, it->label);
}

/* The popup of a choice, in the middle of the screen and rising into place like the power popup: the values one under the other, the
 * chosen one framed, and a thin bar at the right when there are more than POP_ROWS. */
static void draw_popup(void)
{
    float a = smooth(S.pop_anim);
    if (a <= 0)
        return;
    const Item *it = &s_cats[S.pop_page].items[S.pop_item];
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
    const Category *c = &s_cats[S.page];

    gfx_begin(RGBA(20, 20, 24, 255));
    const Theme *shown = shown_theme();
    draw_background(shown);
    /* the title: Settings on the first list, else where you are (System > Appearance) */
    {
        char title[96];
        if (S.page == PG_ROOT)
            snprintf(title, sizeof(title), "%s", L(S_SETTINGS));
        else if (PARENT[S.page] == PG_ROOT)
            snprintf(title, sizeof(title), "%s", c->name);
        else
            snprintf(title, sizeof(title), "%s > %s", s_cats[PARENT[S.page]].name, c->name);
        text_draw(LIST_X + 4 + slide, TITLE_Y, text_fit(0.95f, 0.6f, 0, LIST_W - 8, title), 0, fade(TEXT_MAIN, a), ALIGN_LEFT, title);
    }

    /* the rows: as in the file manager, a frame round the chosen one, a thin line between the others, the rows that are partly out of the list cut */
    int sel = S.item[S.page];
    gfx_clip(0, LIST_TOP - 2, SCREEN_W, LIST_ROWS * ROW_H + 4);
    for (int i = (int)S.scroll; i < c->count && i <= (int)S.scroll + LIST_ROWS; i++) {
        float y = LIST_TOP + (i - S.scroll) * ROW_H;
        if (i == sel) {
            gfx_rect(LIST_X, y, LIST_W, ROW_H, RGBA(255, 255, 255, (int)(16 * a)));
            gfx_frame(LIST_X, y, LIST_W, ROW_H, 1, fade(RGBA(232, 232, 236, 230), a));
        } else if (i < c->count - 1 && i + 1 != sel) {
            float x0 = LIST_X + (c->items[i].icon ? 8 + ICON_W : 4);
            gfx_rect(x0, y + ROW_H - 1, LIST_X + LIST_W - 4 - x0, 1, RGBA(255, 255, 255, (int)(26 * a)));
        }
        draw_item(&c->items[i], y, shown, a);
    }
    gfx_clip_reset();
    if (c->count > LIST_ROWS) { /* the bar at the right: where the shown part is in the whole list (the file manager's) */
        float h = LIST_ROWS * ROW_H, th = clampf(h * LIST_ROWS / c->count, 12, h);
        float ty = LIST_TOP + (h - th) * S.scroll / (c->count - LIST_ROWS);
        gfx_round_rect(LIST_X + LIST_W + 6, ty, 3, th, 1.5f, fade(RGBA(255, 255, 255, 70), a));
    }

    draw_popup();
    if (S.toast_time > 0) /* as the toast of the other screens: 2.8 s, fading in and out */
        popup_toast(S.toast, clampf(S.toast_time / 0.4f, 0, 1) * clampf((2.8f - S.toast_time) / 0.2f, 0, 1));
    power_draw(); /* the power popup of SELECT (main.c asks power.c) */
    osd_draw();
    /* in-toning */
    if (a < 1)
        gfx_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(255 * (1 - a))));
    gfx_end();
}
