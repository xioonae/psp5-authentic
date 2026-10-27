/* psp5 – made by Mille Weber */
/* psp5:s utseende: valt spels bild över hela skärmen, en rad kort högst upp
 * där valt kort är stort med tunn ljus ram, flikar för spel (Home)/bibliotek/musik
 * (biblioteket visar allt i ett rutnät),
 * neutral ljus typografi och mjuka animationer. All grafik ritas här. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <psppower.h>
#include <pspwlan.h>
#include <psprtc.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "ui.h"
#include "sound.h"
#include "lang.h"
#include "music.h"
#include "system.h"
#include "theme.h"
#include "osd.h"
#include "gameops.h"
#include "home.h"
#include "playtime.h"
#include "popup.h"
#include "config.h"
#include "testrun.h"

/* layout (480×272) */
#define MARGIN     24.0f
#define BOTTOM_Y   243.0f  /* the middle of the bottom row: Start and Options in Games, the tag and title in the Library.
                            * The highlight of Start is 16 px from the bottom edge, as far as the top bar's ink is from the top. */
#define ROW_Y      38.0f   /* kortradens överkant */
#define BIG_W      112.0f
#define BIG_H      62.0f
#define SMALL_W    60.0f
#define SMALL_H    33.0f
#define GAP        6.0f
#define SMALL_X    (MARGIN + BIG_W + 10)  /* första lilla kortet */
#define STEP       (SMALL_W + GAP)
#define LABEL_Y    (ROW_Y + SMALL_H + 26) /* titeln under de små korten */

/* biblioteket: rutnät med 5 kolumner */
#define GRID_COLS  5
#define GRID_GAP   8.0f
#define GRID_Y     64.0f   /* första radens överkant */
#define CELL_W     ((SCREEN_W - 2 * MARGIN - (GRID_COLS - 1) * GRID_GAP) / GRID_COLS) /* 80: the grid and every card in it are whole pixels */
#define CELL_H     44.0f   /* (the pictures are 144 x 80, a little wider than this: 20 x 11 is 1.82, they are 1.80) */
#define ROW_STEP   (CELL_H + 11.0f)
#define SELECT_GROW_W 6.0f /* the selected card of the grid is this much wider and ... */
#define SELECT_GROW_H 4.0f /* ... higher, evenly on all sides (whole pixels) */
#define SORT_SIZE  0.56f   /* the text of the Library's Sort button, and the space between it and the edge of its frame */
#define SORT_PAD   9.0f
#define GRID_ROWS  3       /* hela rader som syns */
#define GRID_BOTTOM 230.0f /* rutnätet klipps här, ovanför titeln */

/* musik: låtlista till vänster, "spelas nu" till höger */
#define MLIST_Y    50.0f
#define MROW_H     26.0f
#define MROWS      7
#define PANEL_X    306.0f

#define BG_DELAY   0.18f /* stå still så här länge innan bakgrunden laddas */
#define BG_FADE    3.5f  /* övertoningshastighet (1/s) */
#define LAUNCH_LEN 0.45f
#define SEEK_HOLD  0.35f /* så länge ← / → hålls innan musiken spolar */

#define CLEAR_COLOR RGBA(10, 10, 14, 255)
#define TEXT_MAIN   RGBA(242, 242, 246, 255)
#define TEXT_DIM    RGBA(165, 168, 178, 255)
#define LIGHT       RGBA(238, 238, 242, 255)
#define DARK        RGBA(18, 18, 22, 255)

static const StrId TAB_NAMES[TAB_COUNT] = { S_TAB_GAMES, S_TAB_MUSIC, S_TAB_LIBRARY };

static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static float smooth(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
/* bildfrekvensoberoende utjämning mot målet */
static float approach(float cur, float target, float speed, float dt)
{
    return lerpf(cur, target, 1.0f - expf(-speed * dt));
}
static u32 with_alpha(u32 c, float a)
{
    return COLOR_ALPHA(c, (int)(((c >> 24) & 0xFF) * clampf(a, 0, 1)));
}

static u32 mix_color(u32 a, u32 b, float t)
{
    u32 c = 0;
    for (int s = 0; s < 32; s += 8)
        c |= (u32)(lerpf((float)((a >> s) & 0xFF), (float)((b >> s) & 0xFF), t) + 0.5f) << s;
    return c;
}

/* --- flikar --- */

static const LoaderView *view(const Ui *ui)
{
    return &ui->views[ui->tab];
}

/* The card the selection is on: an index in the game list, HOME_LIBRARY for the Library card of Games, -1 for none */
static int sel_card(const Ui *ui)
{
    const LoaderView *v = view(ui);
    return v->count ? v->idx[ui->sel[ui->tab]] : -1;
}

int ui_selected(const Ui *ui)
{
    int i = sel_card(ui);
    return i >= 0 ? i : -1; /* (the Library card is not a game) */
}

/* The tabs the top bar shows, left to right: Games and Music; inside the Library only the word "Library", where Games would be
 * (it is not a tab: update_top_bar does not let it be selected) */
static int top_tabs(const Ui *ui, int out[2])
{
    if (ui->tab == TAB_LIBRARY) {
        out[0] = TAB_LIBRARY;
        return 1;
    }
    out[0] = TAB_GAMES;
    out[1] = TAB_MUSIC;
    return 2;
}

/* biblioteket sorteras efter namn eller efter när filen lades till – UMD-skivan alltid först */
static const EntryList *s_sort_list;
static int cmp_name(const void *a, const void *b)
{
    const Entry *x = &s_sort_list->items[*(const int *)a], *y = &s_sort_list->items[*(const int *)b];
    if ((x->type == ENTRY_UMD) != (y->type == ENTRY_UMD))
        return x->type == ENTRY_UMD ? -1 : 1;
    int c = strcasecmp(x->title, y->title);
    return c ? c : *(const int *)a - *(const int *)b;
}
static int cmp_recent(const void *a, const void *b)
{
    const Entry *x = &s_sort_list->items[*(const int *)a], *y = &s_sort_list->items[*(const int *)b];
    if ((x->type == ENTRY_UMD) != (y->type == ENTRY_UMD))
        return x->type == ENTRY_UMD ? -1 : 1;
    if (x->file_time != y->file_time)
        return x->file_time > y->file_time ? -1 : 1;
    return cmp_name(a, b);
}

/* the folders in the order of the Sort button: by name (the folders are numbered in the order of their names), or the one with the
 * newest game first */
static const FolderSet *s_sort_folders;
static int cmp_folder_name(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}
static int cmp_folder_recent(const void *a, const void *b)
{
    const Folder *x = &s_sort_folders->items[*(const int *)a], *y = &s_sort_folders->items[*(const int *)b];
    if (x->newest != y->newest)
        return x->newest > y->newest ? -1 : 1;
    return cmp_folder_name(a, b);
}

/* What the Library shows, for the view button (lib_view) and the folder that is open (lib_open), in the order of the Sort button:
 *   open folder          its games and apps
 *   ALL                  every game and app
 *   LOOSE                the games and apps that are in no folder
 *   FOLDERS              the folders (as cards)
 *   BOTH                 the UMD disc (it is in no folder, and always first), the folders, the rest of the games and apps that are in no folder
 * It is written into the same array every time (the loader thread reads it): a card is a game's index in the list, or a folder's
 * LIB_FOLDER_ID. */
static void rebuild_library(Ui *ui)
{
    LoaderView *v = &ui->views[TAB_LIBRARY];
    const FolderSet *fs = &ui->folders;
    const EntryList *list = ui->list;
    int *t = v->idx ? malloc((list->count + 1) * sizeof(int)) : NULL;
    int n = 0, open = ui->lib_open, view = ui->lib_view;
    if (!t)
        return;
    if (open < 0 && (view == LIB_VIEW_FOLDERS || view == LIB_VIEW_BOTH)) {
        for (int f = 0; f < fs->count; f++)
            t[n++] = f;
        s_sort_folders = fs;
        qsort(t, n, sizeof(int), ui->lib_sort ? cmp_folder_recent : cmp_folder_name);
        for (int k = 0; k < n; k++)
            t[k] = LIB_FOLDER_ID(t[k]);
    }
    int first = n;
    for (int i = 0; i < list->count; i++) {
        int f = i < fs->of_count ? fs->of[i] : -1;
        int take = open >= 0 ? f == open : view == LIB_VIEW_ALL ? 1 : (view == LIB_VIEW_LOOSE || view == LIB_VIEW_BOTH) && f < 0;
        if (take)
            t[n++] = i;
    }
    if (n - first > 1) {
        s_sort_list = list;
        qsort(t + first, n - first, sizeof(int), ui->lib_sort ? cmp_recent : cmp_name);
    }
    if (first && n > first && list->items[t[first]].type == ENTRY_UMD) { /* the disc goes in front of the folders */
        int umd = t[first];
        memmove(t + 1, t, first * sizeof(int));
        t[0] = umd;
    }
    memcpy(v->idx, t, n * sizeof(int));
    v->count = n;
    free(t);
}

static void build_views(Ui *ui)
{
    for (int t = 0; t < TAB_COUNT; t++) {
        ui->views[t].idx = malloc((ui->list->count + 1) * sizeof(int)); /* (Home has the Library card on top of the games) */
        ui->views[t].count = 0;
    }
    folders_free(&ui->folders);
    folders_build(ui->list, &ui->folders);
    if (ui->lib_open >= 0) { /* the folder that is open is found again by its name; it is gone when its last game is */
        ui->lib_open = folders_find(&ui->folders, ui->lib_open_name);
        if (ui->lib_open < 0)
            ui->lib_open_name[0] = 0;
    }
    LoaderView *homev = &ui->views[TAB_GAMES];
    if (homev->idx)
        homev->count = home_build(ui->list, homev->idx, ui->list->count + 1); /* Home: the disc, the pinned cards, the ones opened last, the Library card */
    rebuild_library(ui);
}

/* The Library's sort and view are kept in psp5.cfg: they are the same the next time */
static void remember_library_choice(const Ui *ui)
{
    g_lib_sort = ui->lib_sort ? 1 : 0;
    g_lib_view = (unsigned char)ui->lib_view;
    config_save(PSP5_DATA_DIR);
}

/* The Library's cards have been made again: the selection goes to `chosen` (a game's index or a folder's id) if it is there, else to
 * the first card. `snap`: the rows jump to it (a new set of cards), else they glide. */
static void library_reselect(Ui *ui, int chosen, int snap)
{
    const LoaderView *v = &ui->views[TAB_LIBRARY];
    ui->sel[TAB_LIBRARY] = 0;
    for (int p = 0; p < v->count; p++)
        if (v->idx[p] == chosen)
            ui->sel[TAB_LIBRARY] = p;
    if (snap) {
        int row = ui->sel[TAB_LIBRARY] / GRID_COLS;
        ui->grid_target = row > GRID_ROWS - 1 ? row - (GRID_ROWS - 1) : 0;
        ui->grid_scroll = (float)ui->grid_target;
    }
    ui->tab_anim = 0.4f;
    ui->still = 0;
    ui->lib_label_for = -2;
    ui->pt_for = -2;
}

/* The Sort popup of the Library: sort again but keep the same card selected */
static void set_sort(Ui *ui, int sort)
{
    if (sort == ui->lib_sort)
        return;
    int chosen = sel_card(ui);
    ui->lib_sort = sort;
    rebuild_library(ui);
    library_reselect(ui, chosen, 0);
    remember_library_choice(ui);
    sound_play(SND_SELECT);
}

/* The view popup of the Library: another set of cards; the same card stays selected if it is still there */
static void set_view(Ui *ui, int view)
{
    if (view == ui->lib_view || view < 0 || view >= LIB_VIEW_COUNT)
        return;
    int chosen = sel_card(ui);
    ui->lib_view = view;
    rebuild_library(ui);
    library_reselect(ui, chosen, 1);
    remember_library_choice(ui);
    sound_play(SND_SELECT);
}

/* ✕ on a folder card: the folder opens, and shows its games and apps in place of the folders */
static void open_folder(Ui *ui, int n)
{
    if (n < 0 || n >= ui->folders.count)
        return;
    ui->lib_open = n;
    snprintf(ui->lib_open_name, sizeof(ui->lib_open_name), "%s", ui->folders.items[n].name);
    rebuild_library(ui);
    library_reselect(ui, -1, 1);
    ui->lib_focus = 0;
    sound_play(SND_SELECT);
}

/* ○ in an open folder: the folders again, on the card of the folder that was open. (Also used when the Library is left.) */
static void leave_folder(Ui *ui)
{
    int id = LIB_FOLDER_ID(ui->lib_open);
    ui->lib_open = -1;
    ui->lib_open_name[0] = 0;
    rebuild_library(ui);
    library_reselect(ui, id, 1);
}

static void close_folder(Ui *ui)
{
    leave_folder(ui);
    ui->lib_focus = 0;
    sound_play(SND_BACK);
}

static void set_tab(Ui *ui, int tab, SoundId sound)
{
    if (tab == ui->tab || tab < 0 || tab >= TAB_COUNT)
        return;
    if (ui->tab == TAB_LIBRARY && ui->lib_open >= 0)
        leave_folder(ui); /* the Library opens on its folders (or all its games) next time */
    ui->tab = tab;
    ui->tab_anim = 0;
    sound_play(sound);
    ui->pos = ui->sel[tab] + 0.7f; /* raden glider in från höger */
    ui->still = 0;
    ui->label_for = -2;
    ui->seek_dir = ui->seeking = 0; /* en hållen pil gäller bara i musikfliken */
    ui->lib_focus = 0;
    ui->btn_focus = 0;
    ui->top_focus = -1; /* a new tab starts on its own first thing */
    if (tab != TAB_MUSIC) /* musikfliken har inga kort – låt ikonerna ligga kvar */
        loader_set_view(view(ui));
}

/* L / R and ✕ on a tab of the top bar: Games and Music. Inside the Library there is nothing to switch to. */
static void switch_tab(Ui *ui, int tab)
{
    if (ui->tab == TAB_LIBRARY || tab < TAB_GAMES || tab > TAB_MUSIC)
        return;
    set_tab(ui, tab, SND_MOVE);
}

/* ✕ on the Library card of Games: the Library opens ... */
static void open_library(Ui *ui)
{
    set_tab(ui, TAB_LIBRARY, SND_SELECT);
}

/* ... and ○ (the button that goes back) closes it: Games again, on the card that was selected (the Library card, or the one
 * that was put on Home from the Library) */
static void close_library(Ui *ui)
{
    set_tab(ui, TAB_GAMES, SND_BACK);
}

/* --- titel --- */

static void utf8_pop(char *s)
{
    int n = strlen(s);
    while (n > 0) {
        n--;
        if (((u8)s[n] & 0xC0) != 0x80)
            break;
    }
    s[n] = 0;
}

static const char *type_name(u32 t);

/* the tag (PSP, APP, ...) in front of the title: its width plus the gap to the title */
static float tag_room(u32 type)
{
    const char *t = type_name(type);
    return t[0] ? text_width(0.5f, 1, t) + 12 + 8 : 0;
}

static void fit_label(Ui *ui)
{
    int i = sel_card(ui);
    float max_w = SCREEN_W - SMALL_X - MARGIN;
    ui->label_for = i;
    ui->label[0] = 0;
    if (i < 0 && i != HOME_LIBRARY)
        return;
    const char *title = i == HOME_LIBRARY ? L(S_TAB_LIBRARY) : ui->list->items[i].title;
    if (i >= 0)
        max_w -= tag_room(ui->list->items[i].type);
    ui->label_size = 0.78f;
    snprintf(ui->label, sizeof(ui->label), "%s", title);
    while (ui->label_size > 0.62f && text_width(ui->label_size, 0, ui->label) > max_w)
        ui->label_size -= 0.04f;
    if (text_width(ui->label_size, 0, ui->label) > max_w) {
        char buf[sizeof(ui->label)];
        snprintf(buf, sizeof(buf), "%s", title);
        do {
            utf8_pop(buf);
            snprintf(ui->label, sizeof(ui->label), "%s...", buf);
        } while (buf[0] && text_width(ui->label_size, 0, ui->label) > max_w);
    }
}

/* --- livscykel --- */

void ui_init(Ui *ui, const EntryList *list, int can_launch)
{
    memset(ui, 0, sizeof(*ui));
    ui->list = list;
    ui->can_launch = can_launch;
    ui->launch = -1;
    ui->bg_index = -1;
    ui->label_for = -2;
    ui->lib_label_for = -2;
    ui->pt_for = -2;
    ui->cover_for = -2;
    ui->tab_anim = 1;
    ui->top_focus = -1;
    ui->card_ring = 1;
    ui->lib_sort = g_lib_sort ? 1 : 0; /* the Library comes back as it was left (psp5.cfg) */
    ui->lib_view = g_lib_view < LIB_VIEW_COUNT ? g_lib_view : LIB_VIEW_ALL;
    ui->lib_open = -1;
    home_load(list);
    build_views(ui);
    ui->tab = TAB_GAMES; /* (it always has the Library card) */
    ui->pos = 1.5f; /* korten glider in vid start */
    loader_set_view(view(ui));
}

/* Spellistans index före → efter att posten `removed` (-1 = ingen) tagits bort och/eller en post lagts först. */
static int remap_index(int i, int removed, int added_front)
{
    if (i == HOME_LIBRARY)
        return i; /* not a game of the list: it stays what it is */
    if (i < 0 || i == removed)
        return -1;
    if (removed >= 0 && i > removed)
        i--;
    return i + added_front;
}

/* keep_place: markeringen på ett borttaget kort står kvar på samma plats (nästa kort), i stället för att hoppa till första */
static void relist(Ui *ui, int removed, int added_front, int keep_place)
{
    int chosen[TAB_COUNT], old_pos[TAB_COUNT];
    int old_sel = ui->sel[ui->tab];
    char folder_key[FOLDER_NAME_MAX] = ""; /* the name of the folder card that is selected in the Library: its number changes with the list */
    for (int t = 0; t < TAB_COUNT; t++) {
        LoaderView *v = &ui->views[t];
        old_pos[t] = ui->sel[t];
        chosen[t] = -1;
        if (t != TAB_MUSIC && v->count && v->idx)
            chosen[t] = remap_index(v->idx[ui->sel[t]], removed, added_front);
        if (t == TAB_LIBRARY && v->count && v->idx && LIB_IS_FOLDER(v->idx[ui->sel[t]]) && LIB_FOLDER_NO(v->idx[ui->sel[t]]) < ui->folders.count)
            snprintf(folder_key, sizeof(folder_key), "%s", ui->folders.items[LIB_FOLDER_NO(v->idx[ui->sel[t]])].name);
        /* står man först i fliken blir en nyinsatt skiva markerad */
        if (added_front && (t == TAB_GAMES || t == TAB_LIBRARY) && ui->sel[t] == 0)
            chosen[t] = 0;
        free(v->idx);
        v->idx = NULL;
        v->count = 0;
    }
    build_views(ui);
    if (folder_key[0]) {
        int f = folders_find(&ui->folders, folder_key);
        chosen[TAB_LIBRARY] = f >= 0 ? LIB_FOLDER_ID(f) : -1;
    }
    for (int t = 0; t < TAB_COUNT; t++) {
        if (t == TAB_MUSIC)
            continue;
        LoaderView *v = &ui->views[t];
        int found = 0;
        ui->sel[t] = 0;
        for (int p = 0; p < v->count; p++)
            if (v->idx[p] == chosen[t]) {
                ui->sel[t] = p;
                found = 1;
            }
        /* the chosen card is not in the tab any more (deleted, or taken off Home): the next card takes its place */
        if (keep_place && !found && v->count)
            ui->sel[t] = old_pos[t] < v->count ? old_pos[t] : v->count - 1;
    }
    if (ui->tab != TAB_MUSIC)
        ui->pos += ui->sel[ui->tab] - old_sel; /* raden står still om samma kort är markerat */
    ui->still = 0;
    ui->bg_index = remap_index(ui->bg_index, removed, added_front);
    ui->label_for = -2;
    ui->lib_label_for = -2;
    ui->pt_for = -2;
    ui->menu_open = 0; /* kortet menyn gällde kan ha flyttats */
    ui->menu_anim = 0;
    if (ui->menu_stage == 1)
        gameops_measure_stop(); /* frågan "radera?" räknade en mapp */
    loader_set_view(view(ui));
}

void ui_relist(Ui *ui, int removed_front, int added_front)
{
    relist(ui, removed_front ? 0 : -1, added_front, 0);
}

void ui_relist_removed(Ui *ui, int index)
{
    int tab = ui->tab, old = ui->sel[tab];
    relist(ui, index, 0, 1);
    if (tab != TAB_MUSIC) {
        int n = ui->views[tab].count;
        ui->sel[tab] = old < n ? old : (n ? n - 1 : 0);
        ui->pos = ui->sel[tab] + 0.3f; /* nästa kort glider in på platsen */
    }
}

/* A deleted card leaves Home too - unless another copy of the game is left */
void ui_forget_entry(Ui *ui, const Entry *e)
{
    char key[80], other[80];
    entry_cover_key(e, key, sizeof(key));
    for (int i = 0; i < ui->list->count; i++) {
        const Entry *o = &ui->list->items[i];
        if (o == e)
            continue;
        entry_cover_key(o, other, sizeof(other));
        if (!strcmp(key, other))
            return;
    }
    home_remove(e);
}

/* Home has changed (home.c): build the tabs again. The selection follows its card; when the card left the tab it
 * stays in place, and the next card slides in. */
static void home_changed(Ui *ui)
{
    int tab = ui->tab, chosen = ui_selected(ui);
    relist(ui, -1, 0, 1);
    if (tab != TAB_MUSIC && ui->views[tab].count && ui_selected(ui) != chosen)
        ui->pos = ui->sel[tab] + 0.3f;
}

/* Remove from Home / Add to Home / Pin to Home / Unpin from Home for the selected card */
static void home_do(Ui *ui, int item)
{
    char text[96];
    if (sel_card(ui) == HOME_LIBRARY) { /* the Library card: pinned or not, it is never removed */
        if (item == MI_PIN)
            home_pin_library();
        else
            home_unpin_library();
        snprintf(text, sizeof(text), "%s", L(item == MI_PIN ? S_HOME_PINNED : S_HOME_UNPINNED));
        home_changed(ui);
        ui_toast(ui, text);
        sound_play(SND_SELECT);
        return;
    }
    int index = ui_selected(ui);
    if (index < 0)
        return;
    const Entry *e = &ui->list->items[index];
    int tab = ui->tab;
    if (item == MI_ADD) {
        home_add(e);
        snprintf(text, sizeof(text), "%s", L(S_HOME_ADDED));
    } else if (item == MI_REMOVE || item == MI_REMOVE_YES) {
        home_remove(e);
        snprintf(text, sizeof(text), "%s", L(S_HOME_REMOVED));
    } else if (item == MI_PIN) {
        if (!home_pin(e)) { /* HOME_PIN_MAX games are pinned: the place after them is for the card played last */
            snprintf(text, sizeof(text), L(S_HOME_PIN_LIMIT), HOME_PIN_MAX);
            ui_toast(ui, text);
            sound_play(SND_ERROR);
            return;
        }
        snprintf(text, sizeof(text), "%s", L(S_HOME_PINNED));
    } else {
        home_unpin(e);
        snprintf(text, sizeof(text), "%s", L(S_HOME_UNPINNED));
    }
    home_changed(ui);
    if (tab == TAB_LIBRARY && (item == MI_ADD || item == MI_PIN)) { /* the Games tab opens on the card that was just put there */
        const LoaderView *h = &ui->views[TAB_GAMES];
        for (int p = 0; p < h->count; p++)
            if (h->idx[p] == index)
                ui->sel[TAB_GAMES] = p;
    }
    ui_toast(ui, text);
    sound_play(SND_SELECT);
}

static int menu_dir_of(const UiInput *in)
{
    return ((in->held & PSP_CTRL_UP) || in->ly < 40) ? -1 : ((in->held & PSP_CTRL_DOWN) || in->ly > 215) ? 1 : 0;
}

/* START: Hantera sparad data, Radera (bara för det som får raderas), Information, Ta bort från Home / Lägg till på Home,
 * Fäst / Lossa. UMD-skivan får bara Hantera sparad data och Information (den ligger alltid först på Home). Sparad data
 * gäller alla spel, så den finns på varje kort. */
static void menu_build(Ui *ui, const UiInput *in)
{
    int index = ui_selected(ui);
    char path[256];
    int is_dir;
    ui->menu_kind = MENU_CARD;
    ui->menu_n = 0;
    ui->menu_sel = 0;
    ui->menu_stage = 0;
    ui->menu_dir = menu_dir_of(in); /* en riktning som redan hålls flyttar inte markeringen */
    if (sel_card(ui) == HOME_LIBRARY) { /* the Library card can only be pinned and unpinned: it is always on Home */
        ui->menu_item[ui->menu_n++] = home_library_pinned() ? MI_UNPIN : MI_PIN;
        return;
    }
    const Entry *e = &ui->list->items[index];
    ui->menu_item[ui->menu_n++] = MI_SAVES;
    if (e->type != ENTRY_UMD && gameops_target(e, path, sizeof(path), &is_dir))
        ui->menu_item[ui->menu_n++] = MI_DELETE;
    ui->menu_item[ui->menu_n++] = MI_INFO;
    if (e->type != ENTRY_UMD) {
        int state = home_state(ui->list, index);
        ui->menu_item[ui->menu_n++] = state & HOME_SHOWN ? MI_REMOVE : MI_ADD;
        ui->menu_item[ui->menu_n++] = state & HOME_PINNED ? MI_UNPIN : MI_PIN;
    }
}

/* The Sort button of the Library: the two ways to sort, the one in use chosen */
static void menu_build_sort(Ui *ui, const UiInput *in)
{
    ui->menu_kind = MENU_SORT;
    ui->menu_n = 2;
    ui->menu_item[0] = MI_SORT_NAME;
    ui->menu_item[1] = MI_SORT_RECENT;
    ui->menu_sel = ui->lib_sort ? 1 : 0;
    ui->menu_stage = 0;
    ui->menu_dir = menu_dir_of(in);
}

/* The view button of the Library: the four views, the one in use chosen */
static void menu_build_view(Ui *ui, const UiInput *in)
{
    ui->menu_kind = MENU_VIEW;
    ui->menu_n = LIB_VIEW_COUNT;
    for (int k = 0; k < LIB_VIEW_COUNT; k++)
        ui->menu_item[k] = (unsigned char)(MI_VIEW_ALL + k);
    ui->menu_sel = ui->lib_view;
    ui->menu_stage = 0;
    ui->menu_dir = menu_dir_of(in);
}

/* Stänger menyn. Innehållet står kvar medan den tonas ut. */
/* SELECT: the power popup, the same four choices in the same order as Settings > Power and USB */
static void menu_build_power(Ui *ui, const UiInput *in)
{
    ui->menu_kind = MENU_POWER;
    ui->menu_n = 4;
    ui->menu_item[0] = MI_SLEEP;
    ui->menu_item[1] = MI_SHUTDOWN;
    ui->menu_item[2] = MI_RESTART;
    ui->menu_item[3] = MI_RECOVERY;
    ui->menu_sel = 0;
    ui->menu_stage = 0;
    ui->menu_dir = menu_dir_of(in);
}

static void menu_close(Ui *ui)
{
    ui->menu_open = 0;
    if (ui->menu_stage == 1)
        gameops_measure_stop();
}

/* Radera → frågan: vad som tas bort och hur stort det är. Avbryt är förvalt, så inget raderas av misstag. */
static void menu_ask_delete(Ui *ui)
{
    const Entry *e = &ui->list->items[ui_selected(ui)];
    char path[256];
    int is_dir;
    if (!gameops_target(e, path, sizeof(path), &is_dir))
        return;
    const char *name = strrchr(path, '/');
    char shown[64], text[256];
    text_ellipsis(shown, sizeof(shown), name ? name + 1 : path, 0.5f, 120);
    snprintf(text, sizeof(text), L(is_dir ? S_DEL_DIR : S_DEL_FILE), shown);
    snprintf(text + strlen(text), sizeof(text) - strlen(text), " %s", L(S_DEL_FOREVER));
    ui->del_nlines = text_wrap(text, 0.5f, POPUP_ASK_W - 24, ui->del_lines, 4);
    ui->del_is_dir = is_dir;
    ui->del_measured = 0;
    ui->ask_remove = 0;
    if (is_dir) { /* mappens storlek räknas i bakgrunden */
        snprintf(ui->del_size, sizeof(ui->del_size), "...");
        gameops_measure_start(path);
    } else {
        gameops_fmt_size(ui->del_size, sizeof(ui->del_size), e->file_size);
        ui->del_measured = 1;
    }
    ui->menu_stage = 1;
    ui->menu_n = 2;
    ui->menu_item[0] = MI_DELETE_YES;
    ui->menu_item[1] = MI_CANCEL;
    ui->menu_sel = 1;
    sound_play(SND_SELECT);
}

/* Remove from Home on a pinned card: the card is pinned and will be unpinned too. Cancel is chosen to begin with. */
static void menu_ask_remove(Ui *ui)
{
    const Entry *e = &ui->list->items[ui_selected(ui)];
    char shown[64], text[256];
    text_ellipsis(shown, sizeof(shown), e->title, 0.5f, 120);
    snprintf(text, sizeof(text), L(S_HOME_REMOVE_ASK), shown);
    ui->del_nlines = text_wrap(text, 0.5f, POPUP_ASK_W - 24, ui->del_lines, 4);
    ui->del_is_dir = 0;
    ui->del_measured = 1;
    ui->del_size[0] = 0;
    ui->ask_remove = 1;
    ui->menu_stage = 1;
    ui->menu_n = 2;
    ui->menu_item[0] = MI_REMOVE_YES;
    ui->menu_item[1] = MI_CANCEL;
    ui->menu_sel = 1;
    sound_play(SND_SELECT);
}

void ui_free(Ui *ui)
{
    tex_free(ui->bg_old);
    tex_free(ui->bg_cur);
    ui->bg_old = ui->bg_cur = NULL;
    tex_free(ui->cover);
    ui->cover = NULL;
    for (int t = 0; t < TAB_COUNT; t++) {
        free(ui->views[t].idx);
        ui->views[t].idx = NULL;
        ui->views[t].count = 0;
    }
    folders_free(&ui->folders);
}

void ui_replay_intro(Ui *ui)
{
    ui->intro = 0;
    if (ui->tab != TAB_MUSIC)
        ui->pos = ui->sel[ui->tab] + 1.5f;
}

void ui_toast(Ui *ui, const char *msg)
{
    snprintf(ui->toast, sizeof(ui->toast), "%s", msg);
    ui->toast_time = 2.8f;
}

/* --- uppdatering --- */

/* Riktning med knapprepetition: ±1 = vänster/höger, ±2 = upp/ner (bara i rutnätet) */
static int step_input(Ui *ui, const UiInput *in, float dt, int vertical)
{
    int dir = 0;
    if ((in->held & PSP_CTRL_LEFT) || in->lx < 40) dir = -1;
    if ((in->held & PSP_CTRL_RIGHT) || in->lx > 215) dir = 1;
    if (vertical && ((in->held & PSP_CTRL_UP) || in->ly < 40)) dir = -2;
    if (vertical && ((in->held & PSP_CTRL_DOWN) || in->ly > 215)) dir = 2;
    if (!dir) {
        ui->repeat_dir = 0;
        return 0;
    }
    if (dir != ui->repeat_dir) { /* ny riktning: flytta direkt */
        ui->repeat_dir = dir;
        ui->repeat_timer = 0.32f;
        ui->repeat_held = 0;
        return dir;
    }
    ui->repeat_held += dt;
    ui->repeat_timer -= dt;
    if (ui->repeat_timer <= 0) {
        ui->repeat_timer += ui->repeat_held > 1.2f ? 0.045f : 0.085f;
        return dir;
    }
    return 0;
}

static void update_background(Ui *ui, float dt)
{
    int sel = g_theme_art ? ui_selected(ui) : -1; /* bilderna avstängda: bara temats yta */
    int idx;
    Texture *tex;
    if (loader_take_bg(&idx, &tex)) {
        if (idx == sel && idx != ui->bg_index) {
            tex_free(ui->bg_old);
            ui->bg_old = ui->bg_cur;
            ui->bg_cur = tex;
            ui->bg_index = idx;
            ui->bg_fade = 0;
        } else {
            tex_free(tex); /* hann flytta vidare */
        }
    }
    if (ui->still >= BG_DELAY && sel >= 0 && ui->bg_index != sel)
        loader_request_bg(sel);
    if (sel < 0 && ui->bg_cur) { /* tom flik: tona ut bilden */
        tex_free(ui->bg_old);
        ui->bg_old = ui->bg_cur;
        ui->bg_cur = NULL;
        ui->bg_index = -1;
        ui->bg_fade = 0;
    }
    ui->bg_fade = clampf(ui->bg_fade + dt * BG_FADE, 0, 1);
    if (ui->bg_fade >= 1 && ui->bg_old) {
        tex_free(ui->bg_old);
        ui->bg_old = NULL;
    }
}

/* Omslaget till låten som spelas: begärs när låten byts och tonas in när det är laddat */
static void update_cover(Ui *ui, float dt)
{
    int cur = music_current();
    if (cur != ui->cover_for) {
        tex_free(ui->cover);
        ui->cover = NULL;
        ui->cover_for = cur;
        ui->cover_fade = 0;
        if (cur >= 0)
            loader_request_cover(cur);
    }
    int idx;
    Texture *tex;
    if (loader_take_cover(&idx, &tex)) {
        if (idx == ui->cover_for && !ui->cover)
            ui->cover = tex;
        else
            tex_free(tex); /* låten hann bytas */
    }
    if (ui->cover)
        ui->cover_fade = clampf(ui->cover_fade + dt * 4, 0, 1);
}

/* Musikfliken: ↑↓ väljer, ✕ spelar (eller pausar vald låt), □ paus, ← → föregående/nästa låt. ↑ from the first song (or
 * from an empty list) selects the top bar. `top` = the top bar has the focus: the buttons are not for the songs now. */
static void update_music(Ui *ui, const UiInput *in, float dt, int top)
{
    int n = music_count();
    UiInput vert = *in; /* ← → ska inte upprepas när de hålls in */
    vert.held &= ~(PSP_CTRL_LEFT | PSP_CTRL_RIGHT);
    vert.lx = 128;
    int move = top ? 0 : step_input(ui, &vert, dt, 1); /* (the top bar reads the repeat state itself) */
    if (move == -2 && ui->repeat_held == 0 && (!n || ui->music_sel == 0)) { /* a new press, not a held button */
        ui->top_focus = TAB_MUSIC;
        sound_play(SND_MOVE);
        return;
    }
    if (move && n) {
        int next = clampf(ui->music_sel + move / 2, 0, n - 1);
        if (next != ui->music_sel) {
            ui->music_sel = next;
            sound_play(SND_MOVE);
        }
    }
    if (ui->intro < 1 || !n)
        return;
    int playing = music_current() >= 0 && music_state() != MUSIC_STOPPED;
    /* ← →: kort tryck byter låt (när knappen släpps), håll för att spola – fortare ju längre */
    int dir = (in->held & PSP_CTRL_RIGHT) ? 1 : (in->held & PSP_CTRL_LEFT) ? -1 : 0;
    if (ui->seek_show > 0)
        ui->seek_show -= dt;
    if (dir && playing) {
        if (dir != ui->seek_dir) {
            ui->seek_dir = dir;
            ui->seek_held = 0;
            ui->seeking = 0;
        }
        ui->seek_held += dt;
        float dur = music_duration();
        if (!ui->seeking && ui->seek_held >= SEEK_HOLD && dur > 0) {
            ui->seeking = 1;
            ui->seek_pos = music_elapsed();
        }
        if (ui->seeking) {
            float speed = ui->seek_held < 1.5f ? 8 : ui->seek_held < 3.0f ? 20 : 45; /* sekunder per sekund */
            /* stannar en sekund före slutet: att släppa där ska inte byta låt */
            ui->seek_pos = clampf(ui->seek_pos + dir * speed * dt, 0, dur > 1 ? dur - 1 : 0);
        }
    } else if (ui->seek_dir) { /* släppt */
        if (ui->seeking && playing) {
            music_seek(ui->seek_pos);
            ui->seek_show = 0.5f; /* tills spelartråden har hoppat */
        } else if (playing) {
            if (ui->seek_dir > 0)
                music_next();
            else
                music_prev();
        }
        ui->seek_dir = 0;
        ui->seeking = 0;
    }
    if ((in->pressed & PSP_CTRL_SQUARE) && playing)
        music_toggle_pause();
    if (in->pressed & PSP_CTRL_CROSS) {
        if (playing && ui->music_sel == music_current())
            music_toggle_pause();
        else
            music_play(ui->music_sel);
    }
    if (ui->music_sel < ui->music_target)
        ui->music_target = ui->music_sel;
    if (ui->music_sel > ui->music_target + MROWS - 1)
        ui->music_target = ui->music_sel - (MROWS - 1);
    ui->music_scroll = approach(ui->music_scroll, ui->music_target, 14.0f, dt);
}

/* START, or ✕ on the Options button: the options popup of the selected card */
static void open_card_menu(Ui *ui, const UiInput *in)
{
    menu_build(ui, in);
    ui->menu_kind = MENU_CARD;
    ui->menu_open = 1;
    sound_play(SND_SELECT);
}

/* SELECT, on any tab: the power popup in the middle of the screen */
static void open_power_menu(Ui *ui, const UiInput *in)
{
    menu_build_power(ui, in);
    ui->menu_open = 1;
    sound_play(SND_SELECT);
}

/* ✕ on the selected card or on the Start button */
static void start_game(Ui *ui)
{
    if (ui->can_launch) {
        ui->launch = 0;
        sound_play(SND_SELECT);
    } else {
        ui_toast(ui, L(S_NO_ARK));
        sound_play(SND_ERROR);
    }
}

/* ✕ on the selected card or on the Start (Open) button: a game starts, the Library card opens the Library, a folder opens */
static void activate_card(Ui *ui)
{
    int card = sel_card(ui);
    if (card == HOME_LIBRARY)
        open_library(ui);
    else if (LIB_IS_FOLDER(card))
        open_folder(ui, LIB_FOLDER_NO(card));
    else
        start_game(ui);
}

/* the text of the Library's Sort button */
static void sort_text(const Ui *ui, char *out, int size)
{
    snprintf(out, size, "%s: %s", L(S_SORT), L(ui->lib_sort ? S_SORT_RECENT : S_SORT_NAME));
}

static const char *view_label(int view)
{
    return L(view == LIB_VIEW_FOLDERS ? S_LIB_IN : view == LIB_VIEW_LOOSE ? S_LIB_OUT : view == LIB_VIEW_BOTH ? S_LIB_BOTH : S_LIB_ALL);
}

/* The row at the top of the Library: the view button at the left (inside an open folder the folder's name instead, which is not a
 * button), then how many cards there are, and the Sort button at the right. Where everything goes depends on the language, so
 * the drawing and the ↑ that picks the nearest button both ask here. The frames of the buttons stick out SORT_PAD past the text. */
typedef struct {
    char view[100];          /* the view button's text with a ":" after it (or the folder's name), shortened to fit */
    float view_size;         /* ... its size, and its width */
    float view_w;
    float view_l, view_r;    /* the left and right edge of its frame: around the text and the number of cards after it ("All: 15") */
    float count_x;           /* where the number of cards goes ... */
    int show_count;          /* ... if there is room for it: the name of the view comes first */
    char sort[96];  /* (Russian "Сортировать по: Недавно добавленные" is 66 bytes) */
    float sort_w;
    float sort_l, sort_r;    /* the same for the Sort button */
} LibHeader;

static void lib_header(const Ui *ui, LibHeader *h)
{
    char num[16];
    snprintf(num, sizeof(num), "%d", view(ui)->count);
    float count_w = text_width(0.6f, 0, num);
    sort_text(ui, h->sort, sizeof(h->sort));
    h->sort_w = text_width(SORT_SIZE, 0, h->sort);
    h->sort_r = SCREEN_W - MARGIN + SORT_PAD;
    h->sort_l = SCREEN_W - MARGIN - h->sort_w - SORT_PAD;
    int open = ui->lib_open >= 0;
    char name[FOLDER_NAME_MAX + 8];
    snprintf(name, sizeof(name), "%s:", open ? ui->lib_open_name : view_label(ui->lib_view)); /* "All: 15", "Racing: 3" */
    float pad = open ? 0 : SORT_PAD; /* (a folder's name has no frame) */
    float gap = fmaxf(3.0f, ceilf(text_width(0.6f, 0, " "))); /* a space between the colon and the number (whole pixels) */
    float room = h->sort_l - 12 - MARGIN - 2 * pad; /* for the text of the view button, with a gap before the Sort button */
    float room_n = room - gap - count_w;            /* ... with the number of cards next to it */
    h->show_count = 1;
    h->view_size = text_fit(0.66f, 0.45f, 0, room_n, name);
    if (text_width(h->view_size, 0, name) > room_n) { /* too long in this language even at the smallest size: the number goes first */
        h->show_count = 0;
        room_n = room;
        h->view_size = text_fit(0.66f, 0.45f, 0, room_n, name);
    }
    if (room_n < 24)
        room_n = 24;
    if (text_width(h->view_size, 0, name) > room_n) /* ... and then the end of the name */
        text_ellipsis(h->view, sizeof(h->view), name, h->view_size, room_n);
    else
        snprintf(h->view, sizeof(h->view), "%s", name);
    h->view_w = text_width(h->view_size, 0, h->view);
    h->view_l = MARGIN - pad;
    h->count_x = MARGIN + h->view_w + gap;
    h->view_r = (h->show_count ? h->count_x + count_w : MARGIN + h->view_w) + pad;
}

enum { LIB_BTN_SORT = 1, LIB_BTN_VIEW = 2 }; /* what Ui.lib_focus can be, and the bits of lib_buttons() */

/* The buttons the Library has now: Sort when there is more than one card to sort, the view button unless a folder is open (and
 * as long as there is anything at all to show) */
static int lib_buttons(const Ui *ui)
{
    return (view(ui)->count >= 2 ? LIB_BTN_SORT : 0) | (ui->lib_open < 0 && ui->list->count > 0 ? LIB_BTN_VIEW : 0);
}

/* ↑ from the first row: the button nearest to the card (by how far the middle of the card is from the button's frame), the view
 * button when they are as near. 0 when the Library has no button. */
static int lib_button_above(const Ui *ui, int pos)
{
    int avail = lib_buttons(ui);
    LibHeader h;
    lib_header(ui, &h);
    float cx = MARGIN + (pos % GRID_COLS) * (CELL_W + GRID_GAP) + CELL_W / 2;
    float dv = cx < h.view_l ? h.view_l - cx : cx > h.view_r ? cx - h.view_r : 0;
    float ds = cx < h.sort_l ? h.sort_l - cx : cx > h.sort_r ? cx - h.sort_r : 0;
    if ((avail & LIB_BTN_VIEW) && (!(avail & LIB_BTN_SORT) || dv <= ds))
        return LIB_BTN_VIEW;
    return avail & LIB_BTN_SORT ? LIB_BTN_SORT : 0;
}

/* ↑ from the first thing of a tab (or from the Sort button, to the cog wheel) selects the top bar */
static void enter_top_bar(Ui *ui, int which)
{
    ui->top_focus = which;
    sound_play(SND_MOVE);
}

/* The top bar has the focus: ← → move over the tabs and the cog wheel, ✕ on a tab opens it (the highlight goes, the
 * tab turns bold as always), ✕ on the cog wheel opens Settings, ↓ goes back down to where ↑ came from (in the Library
 * that is always the Sort button, the first stop on the way up) and ○ goes back to the selected game. */
static int update_top_bar(Ui *ui, const UiInput *in, float dt)
{
    const u32 ok = PSP_CTRL_CROSS, back = PSP_CTRL_CIRCLE; /* (the names of controls.h: whichever buttons the player has chosen) */
    UiInput sides = *in;
    if (fabsf((float)in->lx - 128) >= fabsf((float)in->ly - 128))
        sides.ly = 128; /* the stick is pushed sideways: that is along the bar, not down */
    int move = step_input(ui, &sides, dt, 1);
    if (move == 2 || (in->pressed & back)) {
        ui->top_focus = -1;
        if (move != 2)
            ui->lib_focus = 0; /* ○ is always "back to the game", also when ↑ came from the Sort button */
        sound_play(move == 2 ? SND_MOVE : SND_BACK);
    } else if (move == -1 || move == 1) {
        int items[3], n = 0, at = 0; /* what can be selected: the tabs (not the word "Library"), then the cog wheel */
        if (ui->tab != TAB_LIBRARY)
            n = top_tabs(ui, items);
        items[n++] = TOP_COG;
        for (int k = 0; k < n; k++)
            if (items[k] == ui->top_focus)
                at = k;
        int next = items[(int)clampf((float)(at + move), 0, (float)(n - 1))];
        if (next != ui->top_focus) {
            ui->top_focus = next;
            sound_play(SND_MOVE);
        }
    } else if (in->pressed & ok) {
        if (ui->top_focus == TOP_COG) {
            if (ui->intro >= 1)
                return UI_SETTINGS; /* the cog wheel stays selected for when Settings is closed; main.c plays the sound once
                                     * Settings has been read in */
        } else {
            int tab = ui->top_focus;
            ui->top_focus = -1;
            if (tab != ui->tab)
                switch_tab(ui, tab);
            else
                sound_play(SND_SELECT);
        }
    }
    return UI_NONE;
}

int ui_update(Ui *ui, const UiInput *in, float dt)
{
    ui->time += dt;
    ui->intro = clampf(ui->intro + dt * 2.2f, 0, 1);
    ui->tab_anim = clampf(ui->tab_anim + dt * 4.0f, 0, 1);
    if (ui->toast_time > 0)
        ui->toast_time -= dt;

    if (ui->launch >= 0) {
        ui->launch += dt / LAUNCH_LEN;
        return ui->launch >= 1 ? UI_LAUNCH : UI_NONE;
    }
    /* alternativmenyn: upp/ner väljer, ✕ bekräftar, ○ eller START stänger – resten av gränssnittet får inga knappar */
    ui->menu_anim = clampf(ui->menu_anim + (ui->menu_open ? 7 : -7) * dt, 0, 1);
    static const UiInput none = UI_NO_INPUT;
    if (ui->menu_open) {
        const u32 ok = PSP_CTRL_CROSS;
        u32 back = PSP_CTRL_CIRCLE | PSP_CTRL_START; /* (the options button that opened a popup closes it again) */
        if (ui->menu_kind == MENU_POWER)
            back |= PSP_CTRL_SELECT; /* the button that opened it closes it, like START on the options popup */
        int dir = menu_dir_of(in);
        if (dir && dir != ui->menu_dir) {
            int next = (int)clampf(ui->menu_sel + dir, 0, ui->menu_n - 1);
            if (next != ui->menu_sel)
                sound_play(SND_MOVE);
            ui->menu_sel = next;
        }
        ui->menu_dir = dir;
        if (ui->menu_stage == 1 && ui->del_is_dir && !ui->del_measured) {
            unsigned long long bytes;
            unsigned files;
            if (gameops_measure_poll(&bytes, &files)) {
                char num[24];
                gameops_fmt_size(num, sizeof(num), bytes);
                snprintf(ui->del_size, sizeof(ui->del_size), L(S_I_SIZE_FILES), num, files);
                ui->del_measured = 1;
            }
        }
        if (in->pressed & ok) {
            int item = ui->menu_item[ui->menu_sel];
            if (item == MI_DELETE) {
                menu_ask_delete(ui);
            } else if (item == MI_REMOVE && (home_state(ui->list, ui_selected(ui)) & HOME_PINNED)) {
                menu_ask_remove(ui); /* a pinned card is unpinned too: ask first */
            } else {
                menu_close(ui);
                if (item >= MI_SLEEP && item <= MI_RECOVERY) { /* main.c does it, like the same four in Settings */
                    sound_play(SND_SELECT);
                    sound_wait(400); /* let the sound be heard before anything closes */
                    return item == MI_SLEEP ? UI_SLEEP : item == MI_SHUTDOWN ? UI_SHUTDOWN : item == MI_RESTART ? UI_RESTART : UI_RECOVERY;
                }
                if (item == MI_INFO || item == MI_SAVES) {
                    sound_play(SND_SELECT);
                    return item == MI_INFO ? UI_INFO : UI_SAVES;
                }
                if (item == MI_DELETE_YES) {
                    sound_play(SND_SELECT);
                    return UI_DELETE;
                }
                if (item == MI_SORT_NAME || item == MI_SORT_RECENT)
                    set_sort(ui, item == MI_SORT_RECENT);
                else if (item >= MI_VIEW_ALL && item <= MI_VIEW_BOTH)
                    set_view(ui, item - MI_VIEW_ALL);
                else if (item == MI_CANCEL)
                    sound_play(SND_BACK);
                else
                    home_do(ui, item);
            }
        } else if (in->pressed & back) {
            menu_close(ui);
            sound_play(SND_BACK);
        }
        in = &none;
    }
    ui->ask_anim = clampf(ui->ask_anim + (ui->menu_open && ui->menu_stage == 1 ? 7 : -7) * dt, 0, 1);
    if (!ui->menu_open && (in->pressed & PSP_CTRL_SELECT) && ui->intro >= 1) { /* on every tab, also with the top bar selected */
        open_power_menu(ui, in);
        in = &none;
    }
    if (ui->tab == TAB_LIBRARY) {
        int avail = lib_buttons(ui);
        if (ui->lib_focus == LIB_BTN_SORT && !(avail & LIB_BTN_SORT)) /* nothing to sort: the view button, if there is one */
            ui->lib_focus = avail & LIB_BTN_VIEW ? LIB_BTN_VIEW : 0;
        if (ui->lib_focus == LIB_BTN_VIEW && !(avail & LIB_BTN_VIEW)) /* (a folder was opened) */
            ui->lib_focus = avail & LIB_BTN_SORT ? LIB_BTN_SORT : 0;
        if (!view(ui)->count && (avail & LIB_BTN_VIEW))
            ui->lib_focus = LIB_BTN_VIEW; /* no card in this view: the button that changes it is all there is to select */
    } else {
        ui->lib_focus = 0;
    }
    ui->sort_anim = clampf(ui->sort_anim + (ui->lib_focus == LIB_BTN_SORT ? 8 : -8) * dt, 0, 1);
    ui->sort_frame = clampf(ui->sort_frame + (ui->lib_focus == LIB_BTN_SORT && ui->top_focus < 0 ? 8 : -8) * dt, 0, 1); /* (not while the top bar is selected) */
    ui->view_anim = clampf(ui->view_anim + (ui->lib_focus == LIB_BTN_VIEW ? 8 : -8) * dt, 0, 1);
    ui->view_frame = clampf(ui->view_frame + (ui->lib_focus == LIB_BTN_VIEW && ui->top_focus < 0 ? 8 : -8) * dt, 0, 1);
    int ring_on = ui->top_focus < 0 && !(ui->tab == TAB_LIBRARY && ui->lib_focus) && !ui->btn_focus; /* the card has the focus */
    ui->card_ring = clampf(ui->card_ring + (ring_on ? 8 : -8) * dt, 0, 1);
    if (ui->tab != TAB_GAMES || view(ui)->count == 0)
        ui->btn_focus = 0; /* the buttons belong to a card of the Games tab */
    for (int k = 0; k < 2; k++)
        ui->btn_anim[k] = clampf(ui->btn_anim[k] + (ui->btn_focus == k + 1 ? 8 : -8) * dt, 0, 1);
    if (in->pressed & PSP_CTRL_LTRIGGER)
        switch_tab(ui, ui->tab - 1);
    if (in->pressed & PSP_CTRL_RTRIGGER)
        switch_tab(ui, ui->tab + 1);

    /* the top bar: while a tab or the cog wheel is selected the rest of the screen gets no buttons */
    for (int k = 0; k <= TOP_COG; k++)
        ui->top_anim[k] = clampf(ui->top_anim[k] + (ui->top_focus == k ? 8 : -8) * dt, 0, 1);
    int top = ui->top_focus >= 0;
    if (top) {
        int action = update_top_bar(ui, in, dt);
        if (action != UI_NONE)
            return action;
        in = &none;
    }

    if (ui->tab == TAB_MUSIC)
        update_music(ui, in, dt, top);

    int count = view(ui)->count;
    int *sel = &ui->sel[ui->tab];
    int grid = ui->tab == TAB_LIBRARY;
    int on_sort = grid && ui->lib_focus == LIB_BTN_SORT; /* the Sort button is selected, not a card */
    int on_view = grid && ui->lib_focus == LIB_BTN_VIEW; /* ... or the view button */
    int on_top_btn = on_sort || on_view;
    const u32 ok = PSP_CTRL_CROSS, back_btn = PSP_CTRL_CIRCLE;
    int on_btn = ui->btn_focus; /* Games: 1 = Start, 2 = Options (the card stays selected) */
    if (grid && !on_top_btn && (in->pressed & back_btn) && ui->intro >= 1) { /* ○ closes an open folder, else the Library, also an empty one (on a button at the top it only goes back to the cards) */
        if (ui->lib_open >= 0)
            close_folder(ui);
        else
            close_library(ui);
        return UI_NONE;
    }
    if ((ui->tab == TAB_GAMES || (grid && !on_top_btn)) && (in->pressed & PSP_CTRL_START) && count && ui->intro >= 1 &&
        !(grid && LIB_IS_FOLDER(sel_card(ui)))) /* (a folder has no options) */
        open_card_menu(ui, in);
    UiInput sides = *in;
    if (ui->tab == TAB_GAMES && fabsf((float)in->lx - 128) >= fabsf((float)in->ly - 128))
        sides.ly = 128; /* the stick is pushed sideways: that is the cards, not the buttons below them */
    int move = ui->tab == TAB_MUSIC || top ? 0 : step_input(ui, &sides, dt, grid || ui->tab == TAB_GAMES);
    int fresh = ui->repeat_held == 0; /* a move that is a new press, not a held button repeating: only that enters the top bar */
    if (ui->tab == TAB_GAMES && count) { /* ↓ to Start, ← → between Start and Options, ↑ or ○ back to the card, ↑ from the card to the top bar */
        if (!on_btn) {
            if (move == 2) {
                ui->btn_focus = 1;
                sound_play(SND_MOVE);
            } else if (move == -2 && fresh) {
                enter_top_bar(ui, ui->tab);
            }
        } else if (move == -2) {
            ui->btn_focus = 0;
            sound_play(SND_MOVE);
        } else if (move == -1 && on_btn == 2) {
            ui->btn_focus = 1;
            sound_play(SND_MOVE);
        } else if (move == 1 && on_btn == 1) {
            ui->btn_focus = 2;
            sound_play(SND_MOVE);
        } else if (in->pressed & back_btn) {
            ui->btn_focus = 0;
            sound_play(SND_BACK);
        } else if ((in->pressed & ok) && on_btn == 2 && ui->intro >= 1) {
            open_card_menu(ui, in);
        }
        if (move == 2 || move == -2 || on_btn)
            move = 0; /* up and down never move the selection in Games, and the buttons keep it where it is */
    }
    if (on_top_btn) { /* ↓ or ○ back to the cards, ← → between the two buttons, ↑ to the cog wheel, ✕ opens the popup of the button */
        int avail = lib_buttons(ui);
        if (move == 2 || (in->pressed & back_btn)) {
            if (count) {
                ui->lib_focus = 0;
                sound_play(move == 2 ? SND_MOVE : SND_BACK);
            } else if ((in->pressed & back_btn) && ui->intro >= 1) { /* nothing to go back to: ○ closes the Library */
                close_library(ui);
                return UI_NONE;
            }
        } else if (move == -2) {
            if (fresh)
                enter_top_bar(ui, TOP_COG); /* the button stays what ↓ comes back to */
        } else if (move == -1 && on_sort && (avail & LIB_BTN_VIEW)) {
            ui->lib_focus = LIB_BTN_VIEW;
            sound_play(SND_MOVE);
        } else if (move == 1 && on_view && (avail & LIB_BTN_SORT)) {
            ui->lib_focus = LIB_BTN_SORT;
            sound_play(SND_MOVE);
        } else if ((in->pressed & ok) && ui->intro >= 1) {
            if (on_sort)
                menu_build_sort(ui, in);
            else
                menu_build_view(ui, in);
            ui->menu_open = 1;
            sound_play(SND_SELECT);
        }
        move = 0;
    } else if (grid && move == -2 && *sel < GRID_COLS) { /* ↑ from any card of the first row: to the button nearest above it, the first stop on the way up (when there
                                                          * is none - a folder with a single game - ↑ goes straight to the cog wheel) */
        int btn = lib_button_above(ui, *sel);
        if (btn) {
            ui->lib_focus = btn;
            sound_play(SND_MOVE);
        } else if (fresh) {
            enter_top_bar(ui, TOP_COG); /* (the word "Library" cannot be selected, so this is the only thing up there) */
        }
        move = 0;
    } else if (ui->tab == TAB_GAMES && !count && move == -2 && fresh) { /* an empty tab still reaches the top bar (and so the cog wheel) */
        enter_top_bar(ui, ui->tab);
    }
    if (move && count) {
        int next = *sel + move;
        if (move == 2 || move == -2) {
            next = *sel + (move > 0 ? GRID_COLS : -GRID_COLS);
            /* ner från näst sista raden till en kortare sista rad: sista kortet */
            if (next >= count && *sel / GRID_COLS < (count - 1) / GRID_COLS)
                next = count - 1;
            if (next < 0 || next >= count)
                next = *sel;
        }
        next = clampf(next, 0, count - 1);
        if (next != *sel) {
            *sel = next;
            ui->still = 0;
            sound_play(SND_MOVE);
        }
    }
    ui->still += dt;

    /* ✕ startar – på kortet och på Start-knappen; på Alternativ-knappen öppnar det menyn (ut till XMB går man med HOME) */
    if ((in->pressed & ok) && count && ui->intro >= 1 && !on_top_btn && on_btn != 2)
        activate_card(ui);

    /* rutnätet rullar så att markerad rad alltid syns helt */
    int row = count ? *sel / GRID_COLS : 0;
    if (grid) {
        if (row < ui->grid_target)
            ui->grid_target = row;
        if (row > ui->grid_target + GRID_ROWS - 1)
            ui->grid_target = row - (GRID_ROWS - 1);
        ui->grid_scroll = approach(ui->grid_scroll, ui->grid_target, 14.0f, dt);
    }

    ui->pos = approach(ui->pos, *sel, 13.0f, dt);
    if (fabsf(ui->pos - *sel) < 0.001f)
        ui->pos = *sel;
    if (ui->tab != TAB_MUSIC)
        loader_focus(*sel);
    if (music_needs_cjk())
        font_ensure_cjk(); /* japanska/ryska låttitlar (laddas bara en gång) */
    update_background(ui, dt);
    update_cover(ui, dt);

    if (ui->label_for != sel_card(ui)) {
        fit_label(ui);
        ui->label_anim = 0;
    }
    ui->label_anim = clampf(ui->label_anim + dt * 5.0f, 0, 1);
    return UI_NONE;
}

/* --- ritning --- */

static void draw_background(Ui *ui)
{
    /* en bild som täcker hela skärmen (färdigtonad, eller den gamla helt under den nya) – då behövs ingen yta under */
    if (!ui->bg_cur || (ui->bg_fade < 1 && !ui->bg_old))
        theme_backdrop(theme_cur(), ui->time, 1); /* the theme's surface: shows when a game has no picture (or pictures are off) */

    const u32 tint = RGBA(205, 205, 210, 0);
    int fade = (int)(ui->bg_fade * 255);
    if (ui->bg_old)
        gfx_tex(ui->bg_old, 0, 0, SCREEN_W, SCREEN_H, tint | (u32)(ui->bg_cur ? 255 : 255 - fade) << 24);
    if (ui->bg_cur)
        gfx_tex(ui->bg_cur, 0, 0, SCREEN_W, SCREEN_H, tint | (u32)fade << 24);

    /* A game's picture is darkened at the top (tabs, cards), on the left (text) and at the bottom (buttons) so they stay
     * readable on any picture. The theme's surface is not darkened: it looks the same as on the other screens. The
     * shades follow how much of the picture is shown, so a picture fading in or out takes them with it. */
    float cover = ui->bg_cur ? (ui->bg_old ? 1.0f : ui->bg_fade) : (ui->bg_old ? 1.0f - ui->bg_fade : 0.0f);
    if (cover > 0) {
        gfx_gradient_v(0, 0, SCREEN_W, 120, RGBA(0, 0, 0, (int)(165 * cover)), RGBA(0, 0, 0, 0));
        gfx_gradient_h(0, 0, 300, SCREEN_H, RGBA(0, 0, 0, (int)(120 * cover)), RGBA(0, 0, 0, 0));
        gfx_gradient_v(0, 160, SCREEN_W, SCREEN_H - 160, RGBA(0, 0, 0, 0), RGBA(0, 0, 0, (int)(175 * cover)));
    }
}

/* The battery on whole pixels, as on the PS5: a terminal 2 x 4 on the LEFT, then a body 21 x 12 with a 1 px outline and 2 px between
 * the outline and the bars. There are four bars 3 x 6 with 1 px between them; they are lit from the right, so the charge runs
 * out from the left. While it charges, the bars above the level are switched on one after the other (in half the strength of
 * the real ones) and then start again from the level, and the charging bolt is left of the battery (draw_top_bar puts it there). */
#define BAT_BARS   4
#define BAT_TERM_W 2.0f
#define BAT_BODY_W 21.0f
#define BAT_H      12.0f
#define BAT_W      (BAT_TERM_W + BAT_BODY_W) /* the whole battery */

/* How many of the four bars a level lights: one for anything above 0 up to 25 %, then two (26-50), three (51-75) and four (76-100) */
static int battery_bars(int percent)
{
    if (percent <= 0)
        return 0;
    int n = (percent + 24) / 25;
    return n < 1 ? 1 : n > BAT_BARS ? BAT_BARS : n;
}

static void draw_battery(float x, float y, int percent, int charging, float time, float a)
{
    float body = x + BAT_TERM_W;
    gfx_rect(x, y + 4, BAT_TERM_W, 4, with_alpha(LIGHT, 0.85f * a));
    gfx_round_rect(body, y, BAT_BODY_W, BAT_H, 2.5f, with_alpha(LIGHT, 0.85f * a));
    gfx_round_rect(body + 1, y + 1, BAT_BODY_W - 2, BAT_H - 2, 1.5f, with_alpha(DARK, a));
    if (percent < 0)
        return;
    int lit = battery_bars(percent);
    int step = 0; /* how many bars above the level the charging shows right now */
    if (charging && lit < BAT_BARS) {
        int cycle = BAT_BARS - lit + 1, n = (int)floorf(time / 0.5f);
        step = ((n % cycle) + cycle) % cycle;
    }
    u32 c = percent <= 15 && !charging ? RGBA(255, 95, 85, 255) : LIGHT;
    for (int i = 0; i < lit + step; i++) /* bar 0 is the right one */
        gfx_rect(body + 3 + (BAT_BARS - 1 - i) * 4, y + 3, 3, 6, with_alpha(c, (i < lit ? 1.0f : 0.5f) * a));
}

static void format_time(char *out, int size, float sec)
{
    int t = sec > 0 ? (int)sec : 0;
    snprintf(out, size, "%d:%02d", t / 60, t % 60);
}

#define TOP_MID     20.0f /* the middle of the clock row, where the wheel, the battery and the Wi-Fi sign are centred: the cog wheel's frame goes round it */
#define TOP_FRAME_H 24.0f /* (a tab's frame is centred on its text) */
#define TOP_GAP     12.0f /* between the things on the right: Wi-Fi sign, battery (with its number), clock, cog wheel; the tabs keep it from the first of them, too */
#define BAT_GAP      5.0f /* between the battery and its number, which belong together (the number's own left edge adds a pixel to it) */
#define BOLT_R       6.0f /* the charging bolt is 2 R = 12 px high, as high as the battery ... */
#define BOLT_W       6.0f /* ... and R = 6 px wide, from its left tip to its right one (see bolt_polygon in icons.c) */
#define BOLT_GAP     3.0f /* between the bolt and the battery, which belong together (the bolt is slanted: it looks further from the battery than this) */
#define WIFI_R       7.0f /* the Wi-Fi sign is a square of 2 R = 14 px (a little higher than the battery: three soft arcs need that much room) */
#define COG_R        7.0f /* the cog wheel is a square of 2 R, and its right edge is on the margin like the clock's used to be */

static void draw_top_bar(Ui *ui)
{
    float a = ui->intro;

    /* klockan, batteriet och Wi-Fi ändras sakta – systemet frågas en gång i sekunden, inte varje bild */
    static float polled_at = -10;
    static char clock_text[8];
    static int sys_present, sys_charging, sys_level = -1, sys_wifi;
    if (fabsf(ui->time - polled_at) >= 1.0f) { /* även om tiden har nollställts */
        polled_at = ui->time;
        ScePspDateTime now;
        clock_text[0] = 0;
        if (sceRtcGetCurrentClockLocalTime(&now) >= 0)
            snprintf(clock_text, sizeof(clock_text), "%02d:%02d", now.hour, now.minute);
        int battery = scePowerIsBatteryExist();
        sys_present = battery ? 1 : 0;
        sys_charging = battery && scePowerIsBatteryCharging() > 0;
        sys_level = battery ? scePowerGetBatteryLifePercent() : -1;
        sys_wifi = sceWlanGetSwitchState() > 0; /* the Wi-Fi switch (the PSP has no call for Bluetooth, so the PSP Go shows none) */
    }
    int charging = testrun_charging() >= 0 ? testrun_charging() : sys_charging;
    int level = testrun_battery() >= 0 ? testrun_battery() : sys_level;
    int present = testrun_battery() >= 0 || sys_present;
    int wifi = testrun_wifi() >= 0 ? testrun_wifi() : sys_wifi;

    /* From the right edge to the left, with TOP_GAP between the things: the cog wheel, the clock, the battery with its number and,
     * while it charges, its bolt (these three are close together: BAT_GAP and BOLT_GAP), the Wi-Fi sign. Only what exists is there: no
     * battery in the PSP means no battery, number or bolt, Wi-Fi switched off no sign, and everything on the left moves to the right
     * to close the gap. The battery, the bolt and the sign are on whole pixels, so they look the same whatever the clock says. */
    float gx = SCREEN_W - MARGIN - COG_R, cog = smooth(ui->top_anim[TOP_COG]);
    float clock_right = gx - COG_R - TOP_GAP;
    text_draw(clock_right, 25, 0.8f, 0, with_alpha(TEXT_MAIN, a), ALIGN_RIGHT, clock_text);
    float bar_left = clock_right - text_width(0.8f, 0, clock_text); /* the left end of everything on the right so far */
    if (present) {
        char pct[8] = "";
        if (g_battery_pct && level >= 0)
            snprintf(pct, sizeof(pct), "%d%%", level > 100 ? 100 : level);
        float next = bar_left - TOP_GAP; /* where the battery (or its number) ends */
        if (pct[0])
            next -= text_width(0.8f, 0, pct) + BAT_GAP;
        float bx = floorf(next - BAT_W + 0.5f);
        draw_battery(bx, TOP_MID - BAT_H / 2, level, charging, ui->time, a);
        if (pct[0])
            text_draw(bx + BAT_W + BAT_GAP, 25, 0.8f, 0, with_alpha(TEXT_MAIN, a), ALIGN_LEFT, pct);
        bar_left = bx;
        if (charging) { /* the bolt is left of the battery and belongs to it: the battery stays where it is, everything left of it moves */
            float bolt_right = bx - BOLT_GAP;
            gfx_bolt(bolt_right - BOLT_W / 2, TOP_MID, BOLT_R, with_alpha(LIGHT, a));
            bar_left = bolt_right - BOLT_W;
        }
    }
    if (wifi) {
        float wx = floorf(bar_left - TOP_GAP - 2 * WIFI_R + 0.5f);
        gfx_wifi(wx + WIFI_R, TOP_MID, WIFI_R, with_alpha(LIGHT, a));
        bar_left = wx;
    }

    /* the cog wheel is a button too: selected (↑ from the first thing of a tab) it gets the frame of a chosen entry, like Sort */
    if (cog * a > 0.01f) {
        float fy, fh;
        popup_frame_around(TOP_MID, TOP_FRAME_H, &fy, &fh);
        popup_choice(gx - 15, fy, 30, fh, cog * a);
    }
    gfx_gear(gx, TOP_MID, COG_R, with_alpha(LIGHT, (0.9f + 0.1f * cog) * a), RGBA(14, 14, 18, (int)(255 * a)));

    /* flikarna krymper om de inte får plats (t.ex. långa ryska ord) – mäts bara om när något ändras */
    static float size = 0.8f, gap = 20, sized_room = -1;
    static int sized_tab = -1;
    static const char *sized_lang;
    float room = bar_left - TOP_GAP - MARGIN; /* the last tab's frame (9 px wider than its text) ends 3 px before the first thing on the right */
    int tabs[2], nt = top_tabs(ui, tabs); /* Games and Music, or only Library inside the Library */
    if (room != sized_room || ui->tab != sized_tab || L(TAB_NAMES[0]) != sized_lang) {
        size = 0.8f;
        gap = 20;
        for (;;) {
            float w = 0;
            for (int k = 0; k < nt; k++)
                w += text_width(size, tabs[k] == ui->tab, L(TAB_NAMES[tabs[k]])) + gap;
            if (w - gap <= room || size <= 0.56f)
                break;
            size -= 0.04f;
            gap = 14;
        }
        sized_room = room;
        sized_tab = ui->tab;
        sized_lang = L(TAB_NAMES[0]);
    }
    float x = MARGIN;
    for (int k = 0; k < nt; k++) {
        int t = tabs[k], active = t == ui->tab;
        float tw = text_width(size, active, L(TAB_NAMES[t])), pick = smooth(ui->top_anim[t]);
        if (pick * a > 0.01f) { /* selected with ↑ (not the same as the open tab, which is the bold one) */
            float fy, fh; /* centred on the text, which shrinks in long languages */
            popup_frame_around(popup_label_mid(25, size), TOP_FRAME_H, &fy, &fh);
            popup_choice(x - 9, fy, tw + 18, fh, pick * a);
        }
        text_draw(x, 25, size, active, with_alpha(active ? TEXT_MAIN : mix_color(RGBA(255, 255, 255, 120), TEXT_MAIN, pick), a),
                  ALIGN_LEFT, L(TAB_NAMES[t]));
        x += tw + gap;
    }

    /* musik som spelas syns som en liten not med titeln i de andra flikarna */
    int cur = music_current();
    if (ui->tab != TAB_MUSIC && cur >= 0 && music_state() != MUSIC_STOPPED) {
        float right = bar_left - 40, left = x + 4;
        if (right - left > 40) {
            static char line[100];
            static int line_for = -2, line_gen = -1;
            static float line_w;
            const Track *tr = music_track(cur);
            if (tr && (line_for != cur || line_gen != music_generation() || line_w != right - left)) {
                text_ellipsis(line, sizeof(line), tr->title, 0.5f, right - left - 16);
                line_for = cur;
                line_gen = music_generation();
                line_w = right - left;
            }
            float pulse = music_state() == MUSIC_PLAYING ? 0.75f + 0.25f * sinf(ui->time * 3) : 0.5f;
            float tw = text_width(0.5f, 0, line);
            gfx_note(right - tw - 8, 21, 10, with_alpha(LIGHT, pulse * a));
            text_draw(right, 24.5f, 0.5f, 0, with_alpha(TEXT_DIM, a), ALIGN_RIGHT, line);
        }
    }
}

/* Placering för kortet på avståndet d från markeringen (0 = valt). */
static void card_geometry(float d, float *x, float *w, float *h, float *y)
{
    float t = clampf(fabsf(d), 0, 1);
    *w = lerpf(BIG_W, SMALL_W, t);
    *h = lerpf(BIG_H, SMALL_H, t);
    *y = lerpf(ROW_Y, ROW_Y + 3, t);
    if (d >= 1)
        *x = SMALL_X + (d - 1) * STEP;
    else if (d >= 0)
        *x = lerpf(MARGIN, SMALL_X, d);
    else if (d > -1)
        *x = lerpf(MARGIN, MARGIN - STEP, -d);
    else
        *x = MARGIN - STEP + (d + 1) * STEP;
}

/* grid: a card of the library grid, whose selection ring is a little thinner than the big card's in the row. `focus` (0..1) is how
 * selected the card is (brightness, shadow); `ring` (0..1) is how visible its ring is: the selected card keeps its size and
 * brightness when the focus is somewhere else (the top bar, Start, Sort), only the ring goes. */
static void draw_card(Ui *ui, int list_index, float x, float y, float w, float h, float focus, float ring, float alpha, int grid)
{
    int library = list_index == HOME_LIBRARY; /* the Library card has no icon to load: a card in the theme's colour with its picture */
    int folder = LIB_IS_FOLDER(list_index);   /* ... and so has a folder of the Library: the same colour, a folder with its first letter */
    IconSlot *slot = library || folder ? NULL : loader_icon(list_index);
    float r = w / 12.0f; /* corner radius: 1/12 of the width at every size, which is what art.c cuts the pictures to */
    /* the card's pixels: a pixel is part of it when its middle is (the picture, the ring and the corners all use this box) */
    int l = (int)ceilf(x - 0.5f), t = (int)ceilf(y - 0.5f);
    int rt = (int)ceilf(x + w - 0.5f), b = (int)ceilf(y + h - 0.5f);

    gfx_glow(x + 1, y + 3, w - 2, h - 2, 5 + 4 * focus, RGBA(0, 0, 0, (int)(130 * alpha)));

    float shown = 0;
    if (slot && slot->state == SLOT_READY) {
        slot->shown = clampf(slot->shown + 1.0f / 9.0f, 0, 1); /* ~150 ms intoning */
        shown = slot->shown;
    }
    if (shown < 1) {
        /* platshållare: mörkt kort med initial, andas svagt medan den laddas */
        int loading = !library && !folder && (!slot || slot->state == SLOT_EMPTY || slot->state == SLOT_LOADING);
        float breath = loading ? 0.5f + 0.5f * sinf(ui->time * 4.0f + list_index) : 0;
        float pa = alpha * (1 - shown);
        u32 cc = theme_cur()->card; /* temats färg, andas medan bilden laddas */
        if (library) { /* the same colour as a game without a picture; the picture is one image on the card's whole pixels */
            int tint = (int)lerpf(200, 255, focus); /* as dim as a picture is when it is not selected */
            gfx_round_rect(l, t, rt - l, b - t, r, RGBA(cc & 0xFF, (cc >> 8) & 0xFF, (cc >> 16) & 0xFF, (int)(255 * pa)));
            gfx_library_icon(l, t, rt - l, b - t, RGBA(tint, tint, tint, (int)(255 * pa)));
        } else if (folder) { /* a folder drawn by the launcher, the capital letter of the folder's name cut out of its body */
            int tint = (int)lerpf(200, 255, focus);
            u32 cc_a = RGBA(cc & 0xFF, (cc >> 8) & 0xFF, (cc >> 16) & 0xFF, (int)(255 * pa));
            gfx_round_rect(l, t, rt - l, b - t, r, cc_a);
            FolderPic pic;
            gfx_folder_icon(l, t, rt - l, b - t, RGBA(tint, tint, tint, (int)(255 * pa)), &pic);
            int n = LIB_FOLDER_NO(list_index);
            if (n < ui->folders.count) {
                char initial[FOLDER_INITIAL_MAX];
                folder_initial(ui->folders.items[n].name, initial, sizeof(initial));
                float lx, ly, ls;
                gfx_folder_letter_at(&pic, &lx, &ly, &ls);
                text_draw(lx, ly, ls, 0, cc_a, ALIGN_CENTER, initial);
            }
        } else {
            gfx_round_rect(x, y, w, h, r, RGBA((cc & 0xFF) + (int)(10 * breath), ((cc >> 8) & 0xFF) + (int)(10 * breath),
                                               ((cc >> 16) & 0xFF) + (int)(12 * breath), (int)(235 * pa)));
            const char *t = ui->list->items[list_index].title;
            char initial[2] = { (t[0] >= 'a' && t[0] <= 'z') ? t[0] - 32 : t[0], 0 };
            if ((u8)initial[0] < 0x80 && initial[0])
                text_draw(x + w / 2, y + h / 2 + 8 * (w / BIG_W), 1.1f * (w / BIG_W), 0,
                          RGBA(205, 208, 220, (int)(200 * pa)), ALIGN_CENTER, initial);
        }
    }
    if (shown > 0) {
        int tint = (int)lerpf(200, 255, focus);
        gfx_tex(slot->tex, l, t, rt - l, b - t, RGBA(tint, tint, tint, (int)(255 * alpha * shown)));
    }

    /* valt kort: tunn ljus ram, 2,5 px på det stora kortet och 2 px i rutnätet. Den sitter mitt på kortets kant
     * (1 px utanför, resten innanför) och har samma
     * hörnmitt som kortet, så bilden fyller ramen helt – ingen springa – och bildens kant och avskurna hörn
     * göms under ramen. Kanterna läggs på samma hela pixlar som kortet själv hamnar på. */
    if (ring > 0.01f)
        gfx_edge_ring(l - 1, t - 1, rt - l + 2, b - t + 2, r + 1, grid, RGBA(240, 240, 244, (int)(235 * ring * alpha)));
}

static void draw_row(Ui *ui)
{
    const LoaderView *v = view(ui);
    float launch = ui->launch < 0 ? 0 : smooth(ui->launch);
    float row_alpha = ui->intro * smooth(ui->tab_anim);
    int first = (int)floorf(ui->pos) - 2, last = (int)floorf(ui->pos) + 8;
    if (first < 0) first = 0;
    if (last > v->count - 1) last = v->count - 1;

    /* närmast markeringen ritas sist, så att det stora kortet hamnar överst */
    int nearest = (int)floorf(ui->pos + 0.5f);
    for (int pass = 0; pass < 2; pass++) {
        for (int p = last; p >= first; p--) {
            if ((p == nearest) != (pass == 1))
                continue;
            float d = p - ui->pos;
            float x, y, w, h;
            card_geometry(d, &x, &w, &h, &y);
            float focus = 1 - clampf(fabsf(d), 0, 1);
            float alpha = row_alpha * (d < -0.5f ? clampf(1.5f + d, 0, 1) : 1); /* bleknar ut åt vänster */
            if (p == ui->sel[ui->tab] && launch > 0) {
                /* startanimation: kortet växer mot skärmens mitt */
                float grow = 1 + launch * 1.6f;
                float cx = lerpf(x + w / 2, SCREEN_W / 2, launch), cy = lerpf(y + h / 2, SCREEN_H / 2, launch);
                w *= grow;
                h *= grow;
                x = cx - w / 2;
                y = cy - h / 2;
            } else {
                alpha *= 1 - launch;
            }
            if (x > SCREEN_W || x + w < 0 || alpha <= 0.01f)
                continue;
            draw_card(ui, v->idx[p], x, y, w, h, focus, focus * smooth(ui->card_ring), alpha, 0);
        }
    }
}

static const char *type_name(u32 t)
{
    switch (t) {
    case ENTRY_ISO:      return "PSP";
    case ENTRY_HOMEBREW: return "APP";
    case ENTRY_PSN:      return "PSN";
    case ENTRY_POPS:     return "PS1";
    case ENTRY_UMD:      return "UMD";
    }
    return "";
}

/* The background of a button under the card: dark (t = 0), white when it is selected (t = 1) */
static void button_bg(float x, float y, float w, float h, float t, float fade)
{
    if (t > 0)
        gfx_glow(x, y + 2, w, h, 6, RGBA(0, 0, 0, (int)(110 * fade * t)));
    gfx_round_rect(x, y, w, h, h / 2, with_alpha(mix_color(RGBA(0, 0, 0, 110), COLOR_ALPHA(LIGHT, 242), t), fade));
}

/* The small clock with the time played, a pill like the buttons of the bottom row (the same height and look), at the right edge of the row:
 * the Games tab has it opposite Start and Options, the Library opposite the tag and the title. A white clock face with two hands
 * (all on whole pixels) and the time, "54h" or "45min" (never both). Nothing is drawn for a game that has not been played. */
#define PILL_H   26.0f
#define PILL_PAD 12.0f
#define CLOCK_D  12.0f /* the clock face: 12 px, so that it sits in the middle of the 26 px pill */
#define CLOCK_GAP 6.0f
#define PILL_SIZE 0.66f /* the text, as large as the text of Start */

/* the time played of the game `i` of the list as text; "" = nothing to show. The seconds are looked up once for the game that is selected
 * (the file is searched for it), the text is made every time so that it follows the language (Settings can change it while this is open). */
static const char *play_text(Ui *ui, int i)
{
    if (i < 0)
        return "";
    if (ui->pt_for != i) {
        ui->pt_secs = playtime_seconds(&ui->list->items[i]);
        ui->pt_for = i;
    }
    playtime_text(ui->pt_secs, ui->pt_text, sizeof(ui->pt_text));
    return ui->pt_text;
}

static float play_pill_width(const char *text)
{
    return text[0] ? PILL_PAD + CLOCK_D + CLOCK_GAP + ceilf(text_width(PILL_SIZE, 0, text)) + PILL_PAD : 0;
}

/* draws the pill with its right edge at `right` on the row whose middle is BOTTOM_Y; `fade` 0..1 */
static void draw_play_pill(const char *text, float right, float fade)
{
    float w = play_pill_width(text);
    if (w <= 0 || fade <= 0)
        return;
    float x = floorf(right - w + 0.5f), by = BOTTOM_Y - PILL_H / 2;
    button_bg(x, by, w, PILL_H, 0, fade);
    float cx = x + PILL_PAD, cy = by + (PILL_H - CLOCK_D) / 2; /* the top left of the clock face */
    gfx_circle(cx + CLOCK_D / 2, cy + CLOCK_D / 2, CLOCK_D / 2, with_alpha(LIGHT, fade));
    u32 hands = with_alpha(DARK, fade);
    gfx_rect(cx + CLOCK_D / 2, cy + 2, 1, CLOCK_D / 2 - 1, hands);      /* the long hand: up from the middle */
    gfx_rect(cx + CLOCK_D / 2 - 3, cy + CLOCK_D / 2, 4, 1, hands);      /* the short one: to the left */
    text_draw(cx + CLOCK_D + CLOCK_GAP, by + 17.5f, PILL_SIZE, 0, with_alpha(TEXT_MAIN, fade), ALIGN_LEFT, text);
}

static void draw_details(Ui *ui)
{
    int i = sel_card(ui);
    float fade = ui->intro * smooth(ui->tab_anim) * (ui->launch < 0 ? 1 : 1 - smooth(ui->launch * 2));
    if ((i < 0 && i != HOME_LIBRARY) || fade <= 0)
        return;

    /* titeln bredvid kortet glider in när markeringen byts, med typetiketten (PSP, APP …) framför som på PS5 (the Library card has none) */
    float la = smooth(ui->label_anim);
    float tx = SMALL_X + (1 - la) * 8;
    const char *type = i >= 0 ? type_name(ui->list->items[i].type) : "";
    if (type[0]) {
        float bw = ceilf(text_width(0.5f, 1, type)) + 12; /* (whole pixels) */
        float ty = floorf(LABEL_Y - 7.5f - ui->label_size * 6.8f + 0.5f); /* tagg och versaler har samma mitt, som i Biblioteket */
        gfx_round_rect(tx, ty, bw, 15, 3, with_alpha(LIGHT, 0.92f * la * fade));
        text_draw(tx + bw / 2, ty + 11, 0.5f, 1, with_alpha(DARK, la * fade), ALIGN_CENTER, type);
        tx += bw + 8;
    }
    text_draw(tx, LABEL_Y, ui->label_size, 0, with_alpha(TEXT_MAIN, la * fade), ALIGN_LEFT, ui->label);

    /* Start and Options under the card, on the bottom row. Both have a dark background; the one
     * that is selected (↓ from the card) turns white with dark text. Options is a circle as high as Start, with three dots. */
    const char *go = L(i >= 0 ? S_LAUNCH : S_F_OPEN); /* the Library card is opened, not started */
    float bx = MARGIN, bh = 26, by = BOTTOM_Y - bh / 2;
    float w = ceilf(text_width(0.66f, 0, go)) + 32;
    float t0 = smooth(ui->btn_anim[0]), t1 = smooth(ui->btn_anim[1]);
    button_bg(bx, by, w, bh, t0, fade);
    text_draw(bx + w / 2, by + 17.5f, 0.66f, 0, with_alpha(mix_color(TEXT_MAIN, DARK, t0), fade), ALIGN_CENTER, go);

    float mx = bx + w + 10;
    button_bg(mx, by, bh, bh, t1, fade);
    u32 dots = with_alpha(mix_color(TEXT_MAIN, DARK, t1), fade);
    for (int k = -1; k <= 1; k++)
        gfx_circle(mx + bh / 2 + k * 6, by + bh / 2, 2, dots);

    draw_play_pill(play_text(ui, i), SCREEN_W - MARGIN, fade * la); /* the time played, on the right of the same row */
}

/* Alternativmenyn (START, eller ✕ på Alternativ-knappen): en mörk, fyrkantig panel intill det valda kortet, med valen Hantera sparad data / Radera / Information /
 * Ta bort från Home (eller Lägg till) / Fäst (eller Lossa) – inga knapptips. Det valda valet ramas in som i inställningarna.
 * Radera och Ta bort (ett fäst kort) öppnar en fråga mitt på skärmen (popup_ask), där Avbryt är förvalt. I Biblioteket
 * visar samma panel de två sorteringarna, nere till höger om Sortera-knappen. */
#define MENU_ITEM_H POPUP_ITEM
#define MENU_EDGE   POPUP_EDGE /* panel edge to the frame of a choice, on all four sides */

static const char *menu_label(int item)
{
    switch (item) {
    case MI_SAVES:        return L(S_SAVES);
    case MI_INFO:         return L(S_INFO);
    case MI_REMOVE:
    case MI_REMOVE_YES:   return L(S_HOME_REMOVE);
    case MI_ADD:          return L(S_HOME_ADD);
    case MI_PIN:          return L(S_HOME_PIN);
    case MI_UNPIN:        return L(S_HOME_UNPIN);
    case MI_SORT_NAME:    return L(S_SORT_NAME);
    case MI_SORT_RECENT:  return L(S_SORT_RECENT);
    case MI_VIEW_ALL:
    case MI_VIEW_FOLDERS:
    case MI_VIEW_LOOSE:
    case MI_VIEW_BOTH:    return view_label(item - MI_VIEW_ALL);
    case MI_CANCEL:       return L(S_F_CANCEL);
    case MI_SLEEP:        return L(S_SLEEP);
    case MI_SHUTDOWN:     return L(S_SHUTDOWN);
    case MI_RESTART:      return L(S_RESTART);
    case MI_RECOVERY:     return L(S_RECOVERY);
    }
    return L(S_F_DELETE); /* MI_DELETE, MI_DELETE_YES */
}

static void draw_menu(Ui *ui)
{
    float a = smooth(ui->menu_anim);
    if (a <= 0 || ui->menu_n <= 0)
        return;
    popup_dim(a);
    if (ui->menu_stage == 1) { /* the question, in the middle of the screen */
        const char *labels[2];
        u32 colors[2];
        for (int k = 0; k < ui->menu_n && k < 2; k++) {
            labels[k] = menu_label(ui->menu_item[k]);
            colors[k] = ui->menu_item[k] == MI_DELETE_YES ? POPUP_DANGER : POPUP_TEXT;
        }
        popup_ask(ui->del_lines, ui->del_nlines, lang_needs_cjk() ? 15.0f : 13.0f, ui->ask_remove ? NULL : ui->del_size,
                  labels, colors, ui->menu_n, ui->menu_sel, smooth(ui->ask_anim));
        return;
    }
    float w = 200, h = MENU_EDGE + ui->menu_n * MENU_ITEM_H + MENU_EDGE;
    float x, y;
    if (ui->menu_kind == MENU_VIEW) { /* wide enough for the longest of the four views */
        for (int k = 0; k < ui->menu_n; k++)
            w = fmaxf(w, ceilf(text_width(0.62f, 0, menu_label(ui->menu_item[k]))) + 32);
        w = fminf(w, SCREEN_W - 2 * (MARGIN - 9)); /* (the widest it can be: the screen less 15 px each side) */
    }
    if (ui->menu_kind == MENU_SORT) { /* under the Sort button, whose right edge it shares */
        x = SCREEN_W - MARGIN + 9 - w + (1 - a) * 10;
        y = 66;
    } else if (ui->menu_kind == MENU_VIEW) { /* under the view button, whose left edge it shares */
        x = MARGIN - 9 - (1 - a) * 10;
        y = 66;
    } else if (ui->menu_kind == MENU_POWER) { /* in the middle of the screen, rising into place like the questions */
        x = (SCREEN_W - w) / 2;
        y = (SCREEN_H - h) / 2 + (1 - a) * 10;
    } else { /* till höger om det valda kortet */
        x = MARGIN + BIG_W + 8 - (1 - a) * 10;
        y = ROW_Y;
    }
    popup_panel(x, y, w, h, POPUP_DARK, a);
    for (int k = 0; k < ui->menu_n; k++) {
        int item = ui->menu_item[k];
        float ry = y + MENU_EDGE + k * MENU_ITEM_H;
        const char *label = menu_label(item);
        u32 col = item == MI_DELETE ? POPUP_DANGER : POPUP_TEXT;
        if (k == ui->menu_sel) { /* som valet i inställningarna: svagt ljus fyllning och en ram */
            popup_choice(x + MENU_EDGE, ry, w - 2 * MENU_EDGE, MENU_ITEM_H, a);
        } else {
            col = with_alpha(col, 0.8f);
        }
        text_draw(x + 16, ry + 18, text_fit(0.62f, 0.45f, 0, w - 32, label), 0, with_alpha(col, a), ALIGN_LEFT, label);
    }
}

/* Biblioteket: allt i ett rutnät, rubrik med antal och sortering, vald titel nere till vänster. */
static void draw_library(Ui *ui)
{
    const LoaderView *v = view(ui);
    float launch = ui->launch < 0 ? 0 : smooth(ui->launch);
    float a = ui->intro * smooth(ui->tab_anim);
    float ui_a = a * (1 - smooth(launch * 2));
    float slide = (1 - smooth(ui->tab_anim)) * 10;

    /* (bakgrunden mörkas av draw_dim, före topplisten) */

    /* the row at the top: the view button at the left (inside an open folder the folder's name, which is not a button), then how many
     * cards there are, and the Sort button at the right */
    LibHeader hd;
    lib_header(ui, &hd);
    float view_sel = smooth(ui->view_anim), view_shown = smooth(ui->view_frame);
    float sort_mid = popup_label_mid(53, SORT_SIZE); /* (both buttons have their frame centred on the same line, whatever the size of their text) */
    float head_y = floorf(sort_mid - popup_label_mid(0, hd.view_size) + 0.5f);
    if (ui->lib_open < 0 && view_shown * ui_a > 0.01f) { /* selected with ↑ from a card under it (or ← from the Sort button): the frame of a chosen entry; ✕ opens the popup */
        float fy, fh;
        popup_frame_around(sort_mid, 22, &fy, &fh);
        popup_choice(hd.view_l, fy, hd.view_r - hd.view_l, fh, view_shown * ui_a);
    }
    text_draw(MARGIN, head_y, hd.view_size, 0, with_alpha(TEXT_MAIN, ui_a), ALIGN_LEFT, hd.view);
    char num[16];
    snprintf(num, sizeof(num), "%d", v->count);
    if (hd.show_count)
        text_draw(hd.count_x, head_y, 0.6f, 0, with_alpha(TEXT_DIM, ui_a), ALIGN_LEFT, num); /* (on the baseline of the text before it, not of the Sort button: the label is larger and sits lower) */

    /* the Sort button: selected with ↑ from a card under it, it gets the frame of a chosen entry; ✕ opens the popup. Its frame
     * is gone for as long as the top bar has the focus (sort_frame), so moving along the top bar never brings it back. */
    float on_sort = smooth(ui->sort_anim), sort_shown = smooth(ui->sort_frame);
    if (sort_shown * ui_a > 0.01f) {
        float fy, fh; /* centred on the text, whose baseline is at 53 */
        popup_frame_around(sort_mid, 22, &fy, &fh);
        popup_choice(hd.sort_l, fy, hd.sort_r - hd.sort_l, fh, sort_shown * ui_a);
    }
    text_draw(SCREEN_W - MARGIN, 53, SORT_SIZE, 0, with_alpha(sort_shown > 0.5f ? TEXT_MAIN : TEXT_DIM, ui_a), ALIGN_RIGHT, hd.sort);

    if (!v->count) { /* the view has nothing in it (no folders, or nothing outside them): the button above changes the view */
        text_draw(MARGIN, 120, 0.9f, 0, with_alpha(TEXT_MAIN, ui_a), ALIGN_LEFT, L(ui->lib_view == LIB_VIEW_FOLDERS ? S_LIB_NO_IN : S_LIB_NO_OUT));
        return;
    }

    /* korten – klipps under rubriken */
    int sel = ui->sel[TAB_LIBRARY];
    float focus_anim = smooth(ui->label_anim), card_focus = focus_anim; /* (the selected card stays large when the focus is elsewhere) */
    int first_row = (int)floorf(ui->grid_scroll) - 1, last_row = (int)ceilf(ui->grid_scroll) + GRID_ROWS + 1;
    gfx_clip(0, GRID_Y - 8, SCREEN_W, GRID_BOTTOM - (GRID_Y - 8));
    float sx = 0, sy = 0, sw = 0, shh = 0;
    for (int p = first_row * GRID_COLS; p < (last_row + 1) * GRID_COLS && p < v->count; p++) {
        if (p < 0)
            continue;
        int r = p / GRID_COLS, c = p % GRID_COLS;
        float x = MARGIN + c * (CELL_W + GRID_GAP);
        float y = GRID_Y + (r - ui->grid_scroll) * ROW_STEP + slide;
        if (y > SCREEN_H || y + CELL_H < GRID_Y - 8)
            continue;
        if (p == sel) { /* ritas sist, ovanpå grannarna */
            sx = x; sy = y; sw = CELL_W; shh = CELL_H;
            continue;
        }
        /* en rad som bara syns delvis bleknar */
        float fade = clampf((GRID_BOTTOM - y) / CELL_H, 0, 1) * clampf((y + CELL_H - GRID_Y + 8) / CELL_H, 0, 1);
        draw_card(ui, v->idx[p], x, y, CELL_W, CELL_H, 0, 0, a * (1 - launch) * fade, 1);
    }
    if (sw > 0) {
        float w = sw + SELECT_GROW_W * card_focus, h = shh + SELECT_GROW_H * card_focus; /* (6 x 4 px more at rest: the edges stay on whole pixels) */
        float cx = sx + sw / 2, cy = sy + shh / 2;
        if (launch > 0) { /* startanimation: kortet växer mot mitten */
            gfx_clip_reset();
            float g = 1 + launch * 2.4f;
            w *= g;
            h *= g;
            cx = lerpf(cx, SCREEN_W / 2, launch);
            cy = lerpf(cy, SCREEN_H / 2, launch);
        }
        draw_card(ui, v->idx[sel], cx - w / 2, cy - h / 2, w, h, (0.4f + 0.6f * focus_anim) * (1 - fmaxf(on_sort, view_sel)), (0.4f + 0.6f * focus_anim) * smooth(ui->card_ring), a, 1);
    }
    gfx_clip_reset();

    /* vald titel med typetikett nere till vänster, på samma rad som Start och Alternativ i Spel. A folder has the tag "FOLDER" (in
     * capitals like PSP and APP, in the language of the launcher) and its name. */
    int card = sel_card(ui), i = card;
    const char *type = NULL, *title = NULL;
    char folder_tag[32];
    if (i >= 0) {
        type = type_name(ui->list->items[i].type);
        title = ui->list->items[i].title;
    } else if (LIB_IS_FOLDER(card) && LIB_FOLDER_NO(card) < ui->folders.count) {
        folder_upper(L(S_I_FOLDER), folder_tag, sizeof(folder_tag));
        type = folder_tag;
        title = ui->folders.items[LIB_FOLDER_NO(card)].name;
    }
    if (type) {
        float bw = ceilf(text_width(0.5f, 1, type)) + 12;
        gfx_round_rect(MARGIN, BOTTOM_Y - 8, bw, 15, 3, with_alpha(LIGHT, 0.92f * ui_a));
        text_draw(MARGIN + bw / 2, BOTTOM_Y + 3.5f, 0.5f, 1, with_alpha(DARK, ui_a), ALIGN_CENTER, type);
        float tx = MARGIN + bw + 8;
        const char *played = play_text(ui, i); /* the pill with the time played is on the right of the row: the title stops before it */
        float max_w = SCREEN_W - MARGIN - tx - (played[0] ? play_pill_width(played) + 12 : 0);
        if (ui->lib_label_for != card || ui->lib_label_room != max_w) {
            text_ellipsis(ui->lib_label, sizeof(ui->lib_label), title, 0.66f, max_w);
            ui->lib_label_for = card;
            ui->lib_label_room = max_w;
        }
        draw_play_pill(played, SCREEN_W - MARGIN, ui_a * focus_anim);
        text_draw(tx + (1 - focus_anim) * 6, BOTTOM_Y + 4.5f, 0.66f, 0, with_alpha(TEXT_MAIN, ui_a * focus_anim),
                  ALIGN_LEFT, ui->lib_label);
    }
}

/* Musikfliken: låtlista till vänster, "spelas nu" med förlopp till höger */
typedef struct { int idx; char title[100]; char sub[72]; } RowCache;

static void draw_play_glyph(float cx, float cy, float r, int paused, u32 color)
{
    if (paused) {
        /* two bars of exactly the same whole-pixel width, whatever r and the position are */
        float bw = fmaxf(1, floorf(r * 0.38f + 0.5f)), bh = floorf(r * 1.2f + 0.5f);
        float gap = fmaxf(1, floorf(r * 0.34f + 0.5f)), top = floorf(cy - bh / 2 + 0.5f);
        float left = floorf(cx - (bw * 2 + gap) / 2 + 0.5f);
        gfx_rect(left, top, bw, bh, color);
        gfx_rect(left + bw + gap, top, bw, bh, color);
    } else {
        gfx_triangle(cx - r * 0.42f, cy - r * 0.62f, r * 1.1f, r * 1.24f, 1, color);
    }
}

/* Spolning: två trianglar åt hållet man spolar (▶▶ eller ◀◀) */
static void draw_seek_glyph(float cx, float cy, float r, int dir, u32 color)
{
    for (int k = 0; k < 2; k++) {
        float tip = cx + dir * (k ? r : 0), back = tip - dir * r;
        gfx_triangle(fminf(back, tip), cy - r * 0.7f, r, r * 1.4f, dir, color);
    }
}

/* Tipstexterna säger ms0:/ – på PSP Go:s internminne visas ef0:/ i stället */
static const char *on_root(const char *s)
{
    static char buf[160];
    const char *p = strstr(s, "ms0:");
    if (!p || !strcmp(g_root, "ms0:"))
        return s;
    snprintf(buf, sizeof(buf), "%.*s%s%s", (int)(p - s), s, g_root, p + 4);
    return buf;
}

static void draw_music(Ui *ui)
{
    float a = ui->intro * smooth(ui->tab_anim);
    float slide = (1 - smooth(ui->tab_anim)) * 10;
    int n = music_count(), cur = music_current(), state = music_state();
    /* (bakgrunden mörkas av draw_dim, före topplisten) */

    /* --- spelas nu --- */
    float px = PANEL_X, pw = SCREEN_W - MARGIN - PANEL_X, py = MLIST_Y + slide, ph = MROWS * MROW_H - 2;
    gfx_round_rect(px, py, pw, ph, 10, RGBA(255, 255, 255, (int)(16 * a)));
    text_draw(px + 12, py + 17, 0.5f, 0, with_alpha(TEXT_DIM, a), ALIGN_LEFT, L(S_NOW_PLAYING));
    const Track *t = cur >= 0 ? music_track(cur) : NULL;
    /* omslaget, eller en ruta med färgton från låtens nummer och en egenritad not */
    float as = 64, ax = px + (pw - as) / 2, ay = py + 28;
    unsigned h = (cur >= 0 ? cur : 7) * 2654435761u;
    int r0 = 60 + (h >> 8) % 120, g0 = 60 + (h >> 16) % 110, b0 = 90 + (h >> 24) % 140;
    gfx_glow(ax, ay + 3, as, as, 8, RGBA(0, 0, 0, (int)(120 * a)));
    gfx_round_rect(ax, ay, as, as, 8, RGBA(r0, g0, b0, (int)((t ? 255 : 90) * a)));
    gfx_gradient_v(ax + 4, ay + as * 0.5f, as - 8, as * 0.5f - 4, RGBA(0, 0, 0, 0), RGBA(0, 0, 0, (int)(60 * a)));
    gfx_note(ax + as / 2, ay + as / 2, 30, RGBA(255, 255, 255, (int)((t ? 230 : 120) * a)));
    if (ui->cover && ui->cover_for == cur && cur >= 0) {
        float ca = smooth(ui->cover_fade);
        gfx_tex(ui->cover, ax, ay, as, as, RGBA(255, 255, 255, (int)(255 * a * ca)));
    }

    static char ptitle[100], partist[72];
    static int pfor = -2, pgen = -1;
    static const char *plang;
    if (pfor != cur || pgen != music_generation() || plang != L(S_UNKNOWN_ARTIST)) {
        pgen = music_generation();
        plang = L(S_UNKNOWN_ARTIST);
        text_ellipsis(ptitle, sizeof(ptitle), t ? t->title : L(S_NOTHING_PLAYING), 0.6f, pw - 20);
        text_ellipsis(partist, sizeof(partist), t ? (t->artist[0] ? t->artist : L(S_UNKNOWN_ARTIST)) : "", 0.5f, pw - 20);
        pfor = cur;
    }
    text_draw(px + pw / 2, ay + as + 20, 0.6f, 0, with_alpha(TEXT_MAIN, a), ALIGN_CENTER, ptitle);
    text_draw(px + pw / 2, ay + as + 35, 0.5f, 0, with_alpha(TEXT_DIM, a), ALIGN_CENTER, partist);
    if (t) {
        int seek = ui->seeking || ui->seek_show > 0;
        float el = seek ? ui->seek_pos : music_elapsed(), du = music_duration();
        float bw = pw - 28, bxx = px + 14, by = py + ph - 30;
        gfx_round_rect(bxx, by, bw, 3, 1.5f, RGBA(255, 255, 255, (int)(50 * a)));
        if (du > 0)
            gfx_round_rect(bxx, by, fmaxf(3, bw * clampf(el / du, 0, 1)), 3, 1.5f, with_alpha(theme_cur()->accent, a));
        if (du > 0 && ui->seeking) /* spolning: en knopp på målet */
            gfx_circle(bxx + bw * clampf(el / du, 0, 1), by + 1.5f, 4.5f, with_alpha(LIGHT, a));
        char e[12], d[12];
        format_time(e, sizeof(e), el);
        format_time(d, sizeof(d), du);
        text_draw(bxx, by + 15, 0.45f, 0, with_alpha(TEXT_DIM, a), ALIGN_LEFT, e);
        if (du > 0)
            text_draw(bxx + bw, by + 15, 0.45f, 0, with_alpha(TEXT_DIM, a), ALIGN_RIGHT, d);
        gfx_circle(px + pw / 2, by + 11, 8, with_alpha(LIGHT, 0.95f * a));
        if (ui->seeking)
            draw_seek_glyph(px + pw / 2, by + 11, 5.5f, ui->seek_dir, with_alpha(DARK, a));
        else
            draw_play_glyph(px + pw / 2, by + 11, 6, state == MUSIC_PLAYING, with_alpha(DARK, a));
    }

    if (!n && music_scanning()) {
        text_draw(MARGIN, 120, 0.9f, 0, with_alpha(TEXT_MAIN, a), ALIGN_LEFT, L(S_LOADING_MUSIC));
        for (int i = 0; i < 3; i++) { /* tre punkter som pulserar i tur och ordning */
            float p = 0.5f + 0.5f * sinf(ui->time * 6.0f - i * 0.9f);
            gfx_circle(MARGIN + 4 + i * 12, 140, 2 + p, COLOR_ALPHA(theme_cur()->accent, (int)((90 + 165 * p) * a)));
        }
        return;
    }
    if (!n) {
        text_draw(MARGIN, 120, 0.9f, 0, with_alpha(TEXT_MAIN, a), ALIGN_LEFT, L(S_NO_MUSIC));
        text_draw(MARGIN, 142, 0.62f, 0, with_alpha(TEXT_DIM, a), ALIGN_LEFT, on_root(L(S_HINT_MUSIC)));
        return;
    }

    /* --- låtlistan --- */
    static RowCache cache[MROWS + 3];
    static int cache_gen = -1;
    static const char *cache_lang;
    if (cache_gen != music_generation() || cache_lang != L(S_UNKNOWN_ARTIST)) { /* ny lista eller nytt språk */
        for (int i = 0; i < MROWS + 3; i++)
            cache[i].idx = -1;
        cache_gen = music_generation();
        cache_lang = L(S_UNKNOWN_ARTIST);
    }
    float lw = PANEL_X - 14 - MARGIN;
    gfx_clip(0, MLIST_Y - 2, PANEL_X - 6, MROWS * MROW_H + 2);
    int first = (int)floorf(ui->music_scroll), last = first + MROWS;
    for (int i = first; i <= last && i < n; i++) {
        if (i < 0)
            continue;
        float y = MLIST_Y + (i - ui->music_scroll) * MROW_H + slide;
        RowCache *c = &cache[i % (MROWS + 3)];
        const Track *tr = music_track(i);
        if (!tr)
            continue;
        if (c->idx != i) {
            text_ellipsis(c->title, sizeof(c->title), tr->title, 0.6f, lw - 30);
            text_ellipsis(c->sub, sizeof(c->sub), tr->artist[0] ? tr->artist : L(S_UNKNOWN_ARTIST), 0.46f, lw - 30);
            c->idx = i;
        }
        int selected = i == ui->music_sel;
        if (selected)
            gfx_round_rect(MARGIN - 6, y, lw + 12, MROW_H - 2, 7, with_alpha(LIGHT, 0.93f * a));
        u32 main = selected ? with_alpha(DARK, a) : with_alpha(TEXT_MAIN, a);
        u32 dim = selected ? RGBA(70, 72, 80, (int)(255 * a)) : with_alpha(TEXT_DIM, a);
        if (i == cur && state != MUSIC_STOPPED) {
            /* små staplar som rör sig medan låten spelas */
            for (int k = 0; k < 3; k++) {
                float hgt = state == MUSIC_PLAYING ? 3 + 7 * (0.5f + 0.5f * sinf(ui->time * (7 + k * 2.3f) + k)) : 3;
                hgt = floorf(hgt + 0.5f);
                gfx_rect(MARGIN + 2 + k * 4, y + 18 - hgt, 2, hgt, main);
            }
        }
        text_draw(MARGIN + 20, y + 11.5f, 0.6f, 0, main, ALIGN_LEFT, c->title);
        text_draw(MARGIN + 20, y + 22, 0.46f, 0, dim, ALIGN_LEFT, c->sub);
    }
    gfx_clip_reset();
}

static void draw_toast(Ui *ui)
{
    if (ui->toast_time <= 0)
        return;
    float a = clampf(ui->toast_time / 0.4f, 0, 1) * clampf((2.8f - ui->toast_time) / 0.2f, 0, 1);
    popup_toast(ui->toast, a);
}

/* No game at all (nothing was found on the card): where to put them. Shown under the Library card in Games, and in the empty Library. */
static void draw_empty(Ui *ui)
{
    float a = ui->intro * smooth(ui->tab_anim);
    text_draw(MARGIN, 120, 0.9f, 0, with_alpha(TEXT_MAIN, a), ALIGN_LEFT, L(S_NO_GAMES));
    text_draw(MARGIN, 142, 0.62f, 0, with_alpha(TEXT_DIM, a), ALIGN_LEFT, on_root(L(S_HINT_GAMES)));
    text_draw(MARGIN, 160, 0.62f, 0, with_alpha(TEXT_DIM, a), ALIGN_LEFT, on_root(L(S_HINT_APPS)));
}

/* The Library and Music have the Games background (the selected game's picture, or the theme's surface) under a darkening,
 * the same in both. It lies over the background but under the top bar, so that the tabs, the clock, the battery and the cog
 * wheel are drawn bright in front of it. */
#define DIM_COLOR RGBA(8, 8, 12, 150)
static void draw_dim(Ui *ui)
{
    if (ui->tab == TAB_MUSIC || (ui->tab == TAB_LIBRARY && ui->list->count)) /* (an empty Library is shown as an empty tab) */
        gfx_rect(0, 0, SCREEN_W, SCREEN_H, DIM_COLOR);
}

void ui_draw(Ui *ui)
{
    gfx_begin(CLEAR_COLOR);
    draw_background(ui);
    draw_dim(ui);
    draw_top_bar(ui);
    if (ui->tab == TAB_MUSIC) {
        draw_music(ui);
    } else if (ui->tab == TAB_LIBRARY && ui->list->count) { /* (a view without cards is drawn too: its button is what changes it) */
        draw_library(ui);
    } else if (view(ui)->count) {
        draw_row(ui);
        draw_details(ui);
        if (!ui->list->count)
            draw_empty(ui); /* first start with nothing to play: only the Library card is there, the hint says where games go */
    } else {
        draw_empty(ui);
    }
    draw_toast(ui); /* under the popup: a toast from the last action must not cover the choices or the question */
    if (ui->tab == TAB_GAMES || ui->tab == TAB_LIBRARY)
        draw_menu(ui);
    gameops_draw(); /* förloppet när ett spel raderas (annars ingenting) */
    osd_draw();

    /* in- och uttoning från/till svart */
    float black = 1 - ui->intro;
    if (ui->launch > 0)
        black = fmaxf(black, clampf((ui->launch - 0.35f) / 0.65f, 0, 1));
    if (black > 0)
        gfx_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(255 * black)));
    gfx_end();
}

void ui_draw_scanning(int done, const char *name, float time)
{
    gfx_begin(CLEAR_COLOR);
    theme_backdrop(theme_cur(), time, 1);
    text_draw(MARGIN, 120, 0.9f, 0, TEXT_MAIN, ALIGN_LEFT, L(S_LOADING));

    /* tre punkter som pulserar i tur och ordning */
    for (int i = 0; i < 3; i++) {
        float p = 0.5f + 0.5f * sinf(time * 6.0f - i * 0.9f);
        gfx_circle(MARGIN + 4 + i * 12, 140, 2 + p, COLOR_ALPHA(theme_cur()->accent, (int)(90 + 165 * p)));
    }
    char line[96];
    snprintf(line, sizeof(line), "%d  ·  %.60s", done, name);
    text_draw(MARGIN, 170, 0.6f, 0, TEXT_DIM, ALIGN_LEFT, line);
    gfx_end();
}
