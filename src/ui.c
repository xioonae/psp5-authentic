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
#include "musicui.h"
#include "imageview.h"
#include "videoplay.h"
#include "system.h"
#include "theme.h"
#include "osd.h"
#include "gameops.h"
#include "gamesize.h"
#include "home.h"
#include "lastplayed.h"
#include "power.h"
#include "playtime.h"
#include "popup.h"
#include "look.h"
#include "config.h"
#include "datetime.h"
#include "testrun.h"

/* layout (480×272) */
#define MARGIN     24.0f
#define TITLE_SIZE 0.66f   /* the size of a game's name (Games and Library), of Play / Open and of the time played: one size for all of them */
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
#define SORT_SIZE  0.56f   /* the text of the Library's Sort button, and the space between it and the edge of its frame */
#define SORT_PAD   9.0f
#define GRID_ROWS  3       /* hela rader som syns */
#define GRID_BOTTOM 230.0f /* rutnätet klipps här, ovanför titeln */

#define BG_DELAY   0.18f /* stå still så här länge innan bakgrunden laddas */
#define BG_FADE    3.5f  /* övertoningshastighet (1/s) */
#define BG_DIP     8.0f  /* the dip to black while the background under the pictures is swapped (1/s: 0.125 s down, 0.125 s up) */
#define DIM_SPEED 4.0f /* how fast the background goes dark behind an opened card (Library, Music) and bright again: 1 / 4 s */
#define DIM_ALPHA_MUSIC 150   /* how dark the open Media screens (Music, Videos, Images) are: the colour (8, 8, 12) at this strength of 255 (the same as the Library) */
#define DIM_ALPHA_LIBRARY 150 /* ... and the open Library (its background is made of fine lines on black, which a strong darkening nearly takes away: 150 is what the maintainer chose) */
#define LAUNCH_LEN 0.45f

#define CLEAR_COLOR RGBA(10, 10, 14, 255)
#define TEXT_MAIN   RGBA(242, 242, 246, 255)
#define TEXT_DIM    RGBA(165, 168, 178, 255)
#define LIGHT       RGBA(238, 238, 242, 255)
#define DARK        RGBA(18, 18, 22, 255)

static const StrId TAB_NAMES[TAB_COUNT] = { S_TAB_GAMES, S_TAB_MEDIA, S_TAB_LIBRARY, S_TAB_MUSIC };

/* the name of a tab in the top bar: the screen of the Media cards has the name of the card that opened it (Music, Videos or Images) */
static StrId tab_name(const Ui *ui, int t)
{
    if (t == TAB_MUSIC)
        return ui->mscreen == 1 ? S_TAB_VIDEOS : ui->mscreen == 2 ? S_TAB_IMAGES : S_TAB_MUSIC;
    return TAB_NAMES[t];
}

/* the list of the open Media screen */
static MusicUi *screen_mu(Ui *ui)
{
    return ui->mscreen == 1 ? &ui->vu : ui->mscreen == 2 ? &ui->iu : &ui->mu;
}

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

/* The tabs the top bar shows, left to right: Games and Media; inside the Library (or Music) only its name, where Games would be
 * (it is not a tab: update_top_bar does not let it be selected) */
static int top_tabs(const Ui *ui, int out[2])
{
    if (ui->tab == TAB_LIBRARY || ui->tab == TAB_MUSIC) {
        out[0] = ui->tab;
        return 1;
    }
    out[0] = TAB_GAMES;
    out[1] = TAB_MEDIA;
    return 2;
}

/* a screen that a card opens (the Library, Music): the top bar has only its name */
static int is_screen(int tab)
{
    return tab == TAB_LIBRARY || tab == TAB_MUSIC;
}

/* The Library is sorted by name, by when a game was added (its file time) or by size, either way round – the disc always first */
_Static_assert(LIB_SORT_COUNT == CONFIG_LIB_SORTS, "config.h must know every way to sort the Library");
static const EntryList *s_sort_list;
static int cmp_disc(const Entry *x, const Entry *y)
{
    if ((x->type == ENTRY_UMD) != (y->type == ENTRY_UMD))
        return x->type == ENTRY_UMD ? -1 : 1;
    return 0;
}
static int cmp_name(const void *a, const void *b)
{
    const Entry *x = &s_sort_list->items[*(const int *)a], *y = &s_sort_list->items[*(const int *)b];
    int d = cmp_disc(x, y);
    if (d)
        return d;
    int c = strcasecmp(x->title, y->title);
    return c ? c : *(const int *)a - *(const int *)b;
}
static int cmp_name_za(const void *a, const void *b)
{
    const Entry *x = &s_sort_list->items[*(const int *)a], *y = &s_sort_list->items[*(const int *)b];
    int d = cmp_disc(x, y);
    if (d)
        return d;
    int c = strcasecmp(y->title, x->title);
    return c ? c : *(const int *)a - *(const int *)b;
}
static int cmp_added_new(const void *a, const void *b)
{
    const Entry *x = &s_sort_list->items[*(const int *)a], *y = &s_sort_list->items[*(const int *)b];
    int d = cmp_disc(x, y);
    if (d)
        return d;
    if (x->file_time != y->file_time)
        return x->file_time > y->file_time ? -1 : 1;
    return cmp_name(a, b);
}
static int cmp_added_old(const void *a, const void *b)
{
    const Entry *x = &s_sort_list->items[*(const int *)a], *y = &s_sort_list->items[*(const int *)b];
    int d = cmp_disc(x, y);
    if (d)
        return d;
    if (x->file_time != y->file_time)
        return x->file_time < y->file_time ? -1 : 1;
    return cmp_name(a, b);
}
static int cmp_size_big(const void *a, const void *b)
{
    const Entry *x = &s_sort_list->items[*(const int *)a], *y = &s_sort_list->items[*(const int *)b];
    int d = cmp_disc(x, y);
    if (d)
        return d;
    u64 sx = gamesize_of(x), sy = gamesize_of(y);
    if (sx != sy)
        return sx > sy ? -1 : 1;
    return cmp_name(a, b);
}
static int cmp_size_small(const void *a, const void *b)
{
    const Entry *x = &s_sort_list->items[*(const int *)a], *y = &s_sort_list->items[*(const int *)b];
    int d = cmp_disc(x, y);
    if (d)
        return d;
    u64 sx = gamesize_of(x), sy = gamesize_of(y);
    if (sx != sy)
        return sx < sy ? -1 : 1;
    return cmp_name(a, b);
}
static int (*const s_game_cmp[LIB_SORT_COUNT])(const void *, const void *) = {
    cmp_name, cmp_name_za, cmp_added_new, cmp_added_old, cmp_size_big, cmp_size_small
};

/* The folders in the order of the Sort button: by name (the folders are numbered in the order of their names; Z-A turns them round), by the newest or the
 * oldest game in them, or by the size of their games together */
static const FolderSet *s_sort_folders;
static const u64 *s_folder_size; /* for every folder, the size of its games together (only for the size sorts) */
static int cmp_folder_name(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}
static int cmp_folder_name_za(const void *a, const void *b)
{
    return *(const int *)b - *(const int *)a;
}
static int cmp_folder_added_new(const void *a, const void *b)
{
    const Folder *x = &s_sort_folders->items[*(const int *)a], *y = &s_sort_folders->items[*(const int *)b];
    if (x->newest != y->newest)
        return x->newest > y->newest ? -1 : 1;
    return cmp_folder_name(a, b);
}
static int cmp_folder_added_old(const void *a, const void *b)
{
    const Folder *x = &s_sort_folders->items[*(const int *)a], *y = &s_sort_folders->items[*(const int *)b];
    if (x->oldest != y->oldest)
        return x->oldest < y->oldest ? -1 : 1;
    return cmp_folder_name(a, b);
}
static int cmp_folder_size_big(const void *a, const void *b)
{
    u64 x = s_folder_size[*(const int *)a], y = s_folder_size[*(const int *)b];
    if (x != y)
        return x > y ? -1 : 1;
    return cmp_folder_name(a, b);
}
static int cmp_folder_size_small(const void *a, const void *b)
{
    u64 x = s_folder_size[*(const int *)a], y = s_folder_size[*(const int *)b];
    if (x != y)
        return x < y ? -1 : 1;
    return cmp_folder_name(a, b);
}
static int (*const s_folder_cmp[LIB_SORT_COUNT])(const void *, const void *) = {
    cmp_folder_name, cmp_folder_name_za, cmp_folder_added_new, cmp_folder_added_old, cmp_folder_size_big, cmp_folder_size_small
};

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
    int n = 0, open = ui->lib_open, view = ui->lib_view, sort = ui->lib_sort;
    u64 *fsize = NULL; /* the size sorts: the size of the games of every folder together */
    if (!t)
        return;
    if (sort < 0 || sort >= LIB_SORT_COUNT)
        sort = LIB_SORT_NAME_AZ;
    if (sort == LIB_SORT_SIZE_BIG || sort == LIB_SORT_SIZE_SMALL) {
        fsize = calloc(fs->count + 1, sizeof(u64));
        if (!fsize) {
            free(t);
            return;
        }
        for (int i = 0; i < list->count; i++) {
            int f = i < fs->of_count ? fs->of[i] : -1;
            if (f >= 0 && f < fs->count)
                fsize[f] += gamesize_of(&list->items[i]);
        }
    }
    if (open < 0 && (view == LIB_VIEW_FOLDERS || view == LIB_VIEW_BOTH)) {
        for (int f = 0; f < fs->count; f++)
            t[n++] = f;
        s_sort_folders = fs;
        s_folder_size = fsize;
        qsort(t, n, sizeof(int), s_folder_cmp[sort]);
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
        qsort(t + first, n - first, sizeof(int), s_game_cmp[sort]);
    }
    if (first && n > first && list->items[t[first]].type == ENTRY_UMD) { /* the disc goes in front of the folders */
        int umd = t[first];
        memmove(t + 1, t, first * sizeof(int));
        t[0] = umd;
    }
    memcpy(v->idx, t, n * sizeof(int));
    v->count = n;
    free(t);
    free(fsize);
}

static void build_views(Ui *ui)
{
    for (int t = 0; t < TAB_COUNT; t++) {
        ui->views[t].idx = malloc((ui->list->count + 3) * sizeof(int)); /* (Home has the Library card on top of the games; Media has its three cards) */
        ui->views[t].count = 0;
    }
    folders_free(&ui->folders);
    folders_build(ui->list, &ui->folders);
    if (ui->lib_open >= 0) { /* the folder that is open is found again by its name; it is gone when its last game is */
        ui->lib_open = folders_find(&ui->folders, ui->lib_open_name);
        if (ui->lib_open < 0)
            ui->lib_open_name[0] = 0;
    }
    if (ui->views[TAB_MEDIA].idx) { /* Media: three cards, Music, Videos and Images */
        ui->views[TAB_MEDIA].idx[0] = HOME_MEDIA_MUSIC;
        ui->views[TAB_MEDIA].idx[1] = HOME_MEDIA_VIDEO;
        ui->views[TAB_MEDIA].idx[2] = HOME_MEDIA_IMAGES;
        ui->views[TAB_MEDIA].count = 3;
    }
    LoaderView *homev = &ui->views[TAB_GAMES];
    if (homev->idx)
        homev->count = home_build(ui->list, homev->idx, ui->list->count + 1); /* Home: the disc, the games opened last and the kept ones, the Library card */
    rebuild_library(ui);
}

/* The Library's sort and view are kept in psp5.cfg: they are the same the next time */
static void remember_library_choice(const Ui *ui)
{
    g_lib_sort = (unsigned char)ui->lib_sort;
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
    if (sort == ui->lib_sort || sort < 0 || sort >= LIB_SORT_COUNT)
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
    /* opened from its card: its background is there already, so it does not come up out of the dark (the Media tab and the Music screen have the same one) */
    ui->lib_calm = ((tab == TAB_LIBRARY && ui->bg_lib == 1) || ((tab == TAB_MEDIA || tab == TAB_MUSIC) && ui->bg_lib == 2)) && ui->bg_dip <= 0;
    sound_play(sound);
    ui->pos = ui->sel[tab] + 0.7f; /* raden glider in från höger */
    ui->still = 0;
    ui->label_for = -2;
    if (tab == TAB_MUSIC)
        musicui_open_kind(screen_mu(ui), ui->mscreen == 1 ? MK_VIDEO : ui->mscreen == 2 ? MK_IMAGES : MK_MUSIC); /* the folders are read when the screen opens */
    if (tab != TAB_MUSIC || ui->mscreen != 0)
        musicui_release_cover(&ui->mu);
    ui->lib_focus = 0;
    ui->btn_focus = 0;
    ui->top_focus = -1; /* a new tab starts on its own first thing */
    if (tab != TAB_MUSIC) /* musikfliken har inga kort – låt ikonerna ligga kvar */
        loader_set_view(view(ui));
}

/* L / R and ✕ on a tab of the top bar: Games and Media. Inside the Library and Music there is nothing to switch to. */
static void switch_tab(Ui *ui, int tab)
{
    if (is_screen(ui->tab) || tab < TAB_GAMES || tab > TAB_MEDIA)
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

/* ✕ on the Music card of Media opens the Music screen, ○ closes it: Media again, on its card */
static void open_music(Ui *ui, int which)
{
    ui->mscreen = which; /* 0 = Music, 1 = Videos, 2 = Images: all three are the screen TAB_MUSIC, with a list of their own */
    set_tab(ui, TAB_MUSIC, SND_SELECT);
}

static void close_music(Ui *ui)
{
    set_tab(ui, TAB_MEDIA, SND_BACK);
}

/* --- titel --- */

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
    if (i < 0 && i != HOME_LIBRARY && !HOME_IS_MEDIA(i))
        return;
    const char *title = i == HOME_LIBRARY ? L(S_TAB_LIBRARY) : i == HOME_MEDIA_MUSIC ? L(S_TAB_MUSIC) : i == HOME_MEDIA_VIDEO ? L(S_TAB_VIDEOS) : i == HOME_MEDIA_IMAGES ? L(S_TAB_IMAGES) : ui->list->items[i].title;
    if (i >= 0)
        max_w -= tag_room(ui->list->items[i].type);
    ui->label_size = TITLE_SIZE; /* as in the Library: a title that does not fit keeps its size and scrolls after a moment */
    ui->label_room = max_w;
    ui->label_t = 0;
    snprintf(ui->label, sizeof(ui->label), "%s", title);
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
    musicui_init(&ui->mu);
    musicui_init(&ui->vu);
    ui->vu.kind = MK_VIDEO;
    musicui_init(&ui->iu);
    ui->iu.kind = MK_IMAGES;
    ui->tab_anim = 1;
    ui->top_focus = -1;
    ui->card_ring = 1;
    ui->lib_sort = g_lib_sort < LIB_SORT_COUNT ? g_lib_sort : LIB_SORT_NAME_AZ; /* the Library comes back as it was left (psp5.cfg) */
    ui->lib_view = g_lib_view < LIB_VIEW_COUNT ? g_lib_view : LIB_VIEW_ALL;
    ui->lib_open = -1;
    home_load(list);
    gamesize_start(list); /* how big the games are (the Library's size sort, Console Storage), counted in the background */
    build_views(ui);
    ui->tab = TAB_GAMES; /* (it always has the Library card) */
    ui->bg_lib = sel_card(ui) == HOME_LIBRARY; /* (nothing to play: the Library card is the first card, and starts on its background) */
    ui->pos = 1.5f; /* korten glider in vid start */
    loader_set_view(view(ui));
}

/* Spellistans index före → efter att posten `removed` (-1 = ingen) tagits bort och/eller en post lagts först. */
static int remap_index(int i, int removed, int added_front)
{
    if (i == HOME_LIBRARY || HOME_IS_MEDIA(i))
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
    gamesize_start(ui->list); /* (the game that is gone must not be counted any more) */
    if (tab != TAB_MUSIC) {
        int n = ui->views[tab].count;
        ui->sel[tab] = old < n ? old : (n ? n - 1 : 0);
        ui->pos = ui->sel[tab] + 0.3f; /* nästa kort glider in på platsen */
    }
}

void ui_relist_gone(Ui *ui)
{
    relist(ui, -1, 0, 0);
    for (int t = 0; t < TAB_COUNT; t++)
        if (t != TAB_MUSIC)
            ui->sel[t] = 0;
    ui->bg_index = -1;
    ui->pos = (ui->tab != TAB_MUSIC ? ui->sel[ui->tab] : 0) + 0.3f;
    gamesize_start(ui->list); /* the sizes of what is left, counted again */
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
    home_forget(e);
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

/* Keep in Home for the selected card: it is switched (the popup stays open, the switch in it turns over) */
static void home_do(Ui *ui, int item)
{
    (void)item;
    int index = ui_selected(ui);
    if (index < 0)
        return;
    const Entry *e = &ui->list->items[index];
    int kept = (home_state(ui->list, index) & HOME_KEPT) != 0;
    if (!home_keep(e, !kept)) { /* HOME_KEEP_MAX games are kept already */
        char text[96];
        snprintf(text, sizeof(text), L(S_HOME_KEEP_LIMIT), HOME_KEEP_MAX);
        ui_toast(ui, text);
        sound_play(SND_ERROR);
        return;
    }
    ui->menu_kept = !kept;
    ui->menu_home_dirty = 1; /* Home is made again when the popup closes: the card stays where it is until then */
    sound_play(SND_SELECT);
}

static int menu_dir_of(const UiInput *in)
{
    return ((in->held & PSP_CTRL_UP) || in->ly < 40) ? -1 : ((in->held & PSP_CTRL_DOWN) || in->ly > 215) ? 1 : 0;
}

/* a held ↑ / ↓ in a popup repeats: the first step at once, the next after POPUP_REPEAT_FIRST, then every POPUP_REPEAT seconds, as the lists do */
#define POPUP_REPEAT_FIRST 0.32f
#define POPUP_REPEAT       0.08f
#define POPUP_REPEAT_HELD  1e9f /* (the direction was held when the popup opened) */
static int menu_step(Ui *ui, const UiInput *in, float dt)
{
    int dir = menu_dir_of(in);
    if (!dir) {
        ui->menu_dir = 0;
        return 0;
    }
    if (dir != ui->menu_dir) { /* a new press: it moves at once */
        ui->menu_dir = dir;
        ui->menu_timer = POPUP_REPEAT_FIRST;
        return dir;
    }
    ui->menu_timer -= dt;
    if (ui->menu_timer > 0)
        return 0;
    ui->menu_timer = POPUP_REPEAT;
    return dir;
}

/* START: Delete (only what may be deleted), Information and Keep in Home. The UMD disc has only Information (it is always the first card on Home). The Library card
 * has no popup: it is always on Home, at the end. */
static void menu_build(Ui *ui, const UiInput *in)
{
    int index = ui_selected(ui);
    char path[256];
    int is_dir;
    ui->menu_kind = MENU_CARD;
    ui->menu_n = 0;
    ui->menu_sel = 0;
    ui->menu_stage = 0;
    ui->menu_home_dirty = 0;
    ui->menu_dir = menu_dir_of(in);
    ui->menu_timer = POPUP_REPEAT_HELD; /* (a direction that is held when it opens does not move, and does not repeat until it is let go) */
    if (sel_card(ui) == HOME_LIBRARY || index < 0)
        return;
    const Entry *e = &ui->list->items[index];
    if (e->type != ENTRY_UMD && gameops_target(e, path, sizeof(path), &is_dir))
        ui->menu_item[ui->menu_n++] = MI_DELETE;
    ui->menu_item[ui->menu_n++] = MI_INFO;
    if (e->type != ENTRY_UMD) {
        ui->menu_item[ui->menu_n++] = MI_KEEP;
        ui->menu_kept = (home_state(ui->list, index) & HOME_KEPT) != 0;
    }
}

/* The Sort button of the Library: the three ways to sort, the one in use chosen */
static void menu_build_sort(Ui *ui, const UiInput *in)
{
    ui->menu_kind = MENU_SORT;
    ui->menu_n = LIB_SORT_COUNT;
    for (int k = 0; k < LIB_SORT_COUNT; k++)
        ui->menu_item[k] = (unsigned char)(MI_SORT_NAME_AZ + k);
    ui->menu_sel = ui->lib_sort;
    ui->menu_stage = 0;
    ui->menu_dir = menu_dir_of(in);
    ui->menu_timer = POPUP_REPEAT_HELD; /* (a direction that is held when it opens does not move, and does not repeat until it is let go) */
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
    ui->menu_timer = POPUP_REPEAT_HELD; /* (a direction that is held when it opens does not move, and does not repeat until it is let go) */
}

/* Stänger menyn. Innehållet står kvar medan den tonas ut. */
/* SELECT: the power popup, the same choices in the same order as on the other screens (power.c) */
static void menu_build_power(Ui *ui, const UiInput *in)
{
    ui->menu_kind = MENU_POWER;
    ui->menu_n = POWER_CHOICES;
    ui->menu_item[0] = MI_XMB;
    ui->menu_item[1] = MI_SLEEP;
    ui->menu_item[2] = MI_SHUTDOWN;
    ui->menu_item[3] = MI_RESTART;
    ui->menu_item[4] = MI_RECOVERY;
    ui->menu_sel = POWER_OPEN_ON;
    ui->menu_stage = 0;
    ui->menu_dir = menu_dir_of(in);
    ui->menu_timer = POPUP_REPEAT_HELD; /* (a direction that is held when it opens does not move, and does not repeat until it is let go) */
}

static void menu_close(Ui *ui)
{
    ui->menu_open = 0;
    if (ui->menu_stage == 1)
        gameops_measure_stop();
    if (ui->menu_home_dirty) {
        ui->menu_home_dirty = 0;
        home_changed(ui);
    }
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

void ui_free(Ui *ui)
{
    gamesize_stop();
    tex_free(ui->bg_old);
    tex_free(ui->bg_cur);
    ui->bg_old = ui->bg_cur = NULL;
    if (ui->iv)
        imageview_close(ui->iv);
    if (ui->vp)
        videoplay_close(ui->vp);
    ui->iv = NULL;
    ui->vp = NULL;
    musicui_free(&ui->mu);
    musicui_free(&ui->vu);
    musicui_free(&ui->iu);
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

/* How much of the screen the pictures of the games cover (0 none, 1 all): one that is up covers it, one fading in or out covers it by its alpha */
static float bg_cover(const Ui *ui)
{
    return ui->bg_cur ? (ui->bg_old ? 1.0f : ui->bg_fade) : (ui->bg_old ? 1.0f - ui->bg_fade : 0.0f);
}

static void update_background(Ui *ui, float dt)
{
    int sel = g_theme_art && ui->tab != TAB_LIBRARY ? ui_selected(ui) : -1; /* pictures off, or the Library (which has a background of its own): only the surface */
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
    /* What lies under the pictures: the Library's own background on the Library card of Games and in the Library (so that opening the Library changes
     * nothing behind the cards, and closing it neither), the theme's surface everywhere else. Only one of the two pictures fits in video memory at a
     * time, so they cannot be mixed. When a picture covers the screen the swap cannot be seen and is made at once (before the picture starts to fade
     * out in this very update); else the screen dips to black for a moment while it is made.
     * Leaving the Library card for a game: the Library's background stays under the cards until the game's picture is there, and the picture fades in over it
     * (the picture is the game's own, else its icon blurred: art_load_bg). The theme's surface comes only for a game that has nothing to show - the loader answered
     * with no picture - or when Show Game art is off, and then through the dip. */
    int want = ui->tab == TAB_LIBRARY || (ui->tab == TAB_GAMES && sel_card(ui) == HOME_LIBRARY) ? 1 : 0; /* bg_lib: 1 = the Library's background, 2 = the Media tab's, 0 = the surface */
    if (ui->tab == TAB_MEDIA || ui->tab == TAB_MUSIC)
        want = 2; /* the Media tab and the Music screen have one background of their own (in every theme), never a picture of a game */
    if (!want && ui->bg_lib && ui->tab == TAB_GAMES && sel >= 0 && ui->bg_index != sel) /* (the picture of this game has not been answered yet) */
        want = ui->bg_lib;
    if (ui->tab == TAB_LIBRARY || ui->tab == TAB_MEDIA || ui->tab == TAB_MUSIC) {
        ui->bg_lib = want; /* (these draw their own, whatever lay under before) */
        ui->bg_dip = 0;
    } else if (want == ui->bg_lib) {
        ui->bg_dip = clampf(ui->bg_dip - dt * BG_DIP, 0, 1);
    } else if (bg_cover(ui) >= 1) {
        ui->bg_lib = want;
    } else if (ui->bg_cur) { /* the game's picture is fading in over the Library's background: the swap waits until it covers the screen, and is made then, unseen */
        ui->bg_dip = clampf(ui->bg_dip - dt * BG_DIP, 0, 1);
    } else {
        ui->bg_dip = clampf(ui->bg_dip + dt * BG_DIP, 0, 1);
        if (ui->bg_dip >= 1)
            ui->bg_lib = want;
    }

    ui->bg_fade = clampf(ui->bg_fade + dt * BG_FADE, 0, 1);
    if (ui->bg_fade >= 1 && ui->bg_old) {
        tex_free(ui->bg_old);
        ui->bg_old = NULL;
    }
}

/* The Music screen (musicui.c): its input is whatever the top bar did not take (`in` is the neutral input while the top bar has the focus) */
static void open_viewer(Ui *ui)
{
    const MusicEntry *files;
    int count, index;
    const char *dir = musicui_file(screen_mu(ui), &files, &count, &index);
    if (index < 0 || index >= count)
        return;
    if (ui->mscreen == 2)
        ui->iv = imageview_open(dir, files, count, index);
    else if (ui->mscreen == 1)
        ui->vp = videoplay_open(dir, files, count, index);
    if (!ui->iv && !ui->vp) {
        ui_toast(ui, L(ui->mscreen == 2 ? S_IMG_CANT : S_VID_CANT)); /* (no memory for it) */
        sound_play(SND_ERROR);
    }
}

static void update_music(Ui *ui, const UiInput *in, float dt)
{
    int r = musicui_update(screen_mu(ui), in->pressed, in->held, in->lx, in->ly, dt);
    if (ui->intro < 1)
        return;
    if (r == MUSICUI_OPEN)
        open_viewer(ui);
    else if (r == MUSICUI_CLOSE) {
        close_music(ui);
    } else if (r == MUSICUI_TOP) { /* ↑ from the first line: the top bar (the Music screen has only its name and the cog wheel there) */
        ui->top_focus = TOP_COG;
        sound_play(SND_MOVE);
    }
}

/* START, or ✕ on the Options button: the options popup of the selected card */
static void open_card_menu(Ui *ui, const UiInput *in)
{
    menu_build(ui, in);
    if (ui->menu_n <= 0) /* the Library card has no options */
        return;
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
    else if (card == HOME_MEDIA_MUSIC)
        open_music(ui, 0);
    else if (card == HOME_MEDIA_VIDEO)
        open_music(ui, 1);
    else if (card == HOME_MEDIA_IMAGES)
        open_music(ui, 2);
    else if (LIB_IS_FOLDER(card))
        open_folder(ui, LIB_FOLDER_NO(card));
    else
        start_game(ui);
}

/* the text of the Library's Sort button */
static void sort_text(const Ui *ui, char *out, int size)
{
    static const StrId names[LIB_SORT_COUNT] = { S_SORT_NAME_AZ, S_SORT_NAME_ZA, S_SORT_ADDED_NEW, S_SORT_ADDED_OLD, S_SORT_SIZE_BIG, S_SORT_SIZE_SMALL };
    snprintf(out, size, "%s: %s", L(S_SORT), L(names[ui->lib_sort >= 0 && ui->lib_sort < LIB_SORT_COUNT ? ui->lib_sort : 0]));
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
    float count_w = text_width(SORT_SIZE, 0, num); /* (the view text and its number are as large as the text of the Sort button) */
    sort_text(ui, h->sort, sizeof(h->sort));
    h->sort_w = text_width(SORT_SIZE, 0, h->sort);
    h->sort_r = SCREEN_W - MARGIN + SORT_PAD;
    h->sort_l = SCREEN_W - MARGIN - h->sort_w - SORT_PAD;
    int open = ui->lib_open >= 0;
    char name[FOLDER_NAME_MAX + 8];
    snprintf(name, sizeof(name), "%s:", open ? ui->lib_open_name : view_label(ui->lib_view)); /* "All: 15", "Racing: 3" */
    float pad = open ? 0 : SORT_PAD; /* (a folder's name has no frame) */
    float gap = fmaxf(3.0f, ceilf(text_width(SORT_SIZE, 0, " "))); /* a space between the colon and the number (whole pixels) */
    float room = h->sort_l - 12 - MARGIN - 2 * pad; /* for the text of the view button, with a gap before the Sort button */
    float room_n = room - gap - count_w;            /* ... with the number of cards next to it */
    h->show_count = 1;
    h->view_size = text_fit(SORT_SIZE, 0.45f, 0, room_n, name);
    if (text_width(h->view_size, 0, name) > room_n) { /* too long in this language even at the smallest size: the number goes first */
        h->show_count = 0;
        room_n = room;
        h->view_size = text_fit(SORT_SIZE, 0.45f, 0, room_n, name);
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
        if (!is_screen(ui->tab))
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

/* The size sorts of the Library: the size of a game that lives in a folder is counted in the background (gamesize.c). When more sizes are known the Library is
 * sorted again, the same card stays selected - not more than once a second, so a long count does not keep the cards moving. */
static void sort_by_size_again(Ui *ui, float dt)
{
    ui->gs_wait -= dt;
    if (ui->lib_sort != LIB_SORT_SIZE_BIG && ui->lib_sort != LIB_SORT_SIZE_SMALL) {
        ui->gs_version = gamesize_version();
        return;
    }
    if (ui->gs_version == gamesize_version() || ui->gs_wait > 0)
        return;
    ui->gs_version = gamesize_version();
    ui->gs_wait = 1.0f;
    int chosen = sel_card(ui);
    float tab_anim = ui->tab_anim;
    rebuild_library(ui);
    library_reselect(ui, chosen, 0);
    ui->tab_anim = tab_anim; /* (no slide: the cards only change places) */
}

int ui_update(Ui *ui, const UiInput *in, float dt)
{
    ui->time += dt;
    ui->intro = clampf(ui->intro + dt * 2.2f, 0, 1);
    ui->tab_anim = clampf(ui->tab_anim + dt * 4.0f, 0, 1);
    if (ui->tab_anim >= 1)
        ui->lib_calm = 0; /* (a later change of the Library's cards, a new sort, comes up out of the dark as before) */
    if (ui->tab == TAB_LIBRARY || ui->tab == TAB_MUSIC)
        ui->dim_alpha = ui->tab == TAB_MUSIC ? DIM_ALPHA_MUSIC : DIM_ALPHA_LIBRARY;
    ui->dim = clampf(ui->dim + (ui->tab == TAB_LIBRARY || ui->tab == TAB_MUSIC ? 1 : -1) * dt * DIM_SPEED, 0, 1); /* the background goes dark when a card is opened, and bright again when it is closed */
    if (ui->toast_time > 0)
        ui->toast_time -= dt;
    sort_by_size_again(ui, dt);

    if (ui->iv || ui->vp) { /* the image viewer or the video player has the whole screen and all the buttons (○ closes it: the list of its screen is as it was left) */
        int done = ui->iv ? imageview_update(ui->iv, in->pressed, in->held, in->lx, in->ly, dt) : videoplay_update(ui->vp, in->pressed, in->held, in->lx, in->ly, dt);
        if (done) {
            if (ui->iv)
                imageview_close(ui->iv);
            else
                videoplay_close(ui->vp);
            ui->iv = NULL;
            ui->vp = NULL;
            sound_play(SND_BACK);
        }
        return UI_NONE;
    }
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
        int step = menu_step(ui, in, dt);
        if (step) {
            int next = (int)clampf(ui->menu_sel + step, 0, ui->menu_n - 1);
            if (next != ui->menu_sel)
                sound_play(SND_MOVE);
            ui->menu_sel = next;
        }
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
            } else if (item == MI_KEEP) {
                home_do(ui, item); /* a switch: the popup stays open */
            } else {
                menu_close(ui);
                if (item >= MI_XMB && item <= MI_RECOVERY) { /* main.c does it, as on the other screens */
                    sound_play(SND_SELECT);
                    sound_wait(400); /* let the sound be heard before anything closes */
                    return item == MI_XMB ? UI_XMB : item == MI_SLEEP ? UI_SLEEP : item == MI_SHUTDOWN ? UI_SHUTDOWN : item == MI_RESTART ? UI_RESTART : UI_RECOVERY;
                }
                if (item == MI_INFO) {
                    sound_play(SND_SELECT);
                    return UI_INFO;
                }
                if (item == MI_DELETE_YES) {
                    sound_play(SND_SELECT);
                    return UI_DELETE;
                }
                if (item >= MI_SORT_NAME_AZ && item <= MI_SORT_SIZE_SMALL)
                    set_sort(ui, item - MI_SORT_NAME_AZ);
                else if (item >= MI_VIEW_ALL && item <= MI_VIEW_BOTH)
                    set_view(ui, item - MI_VIEW_ALL);
                else if (item == MI_CANCEL)
                    sound_play(SND_BACK);
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
    if ((ui->tab != TAB_GAMES && ui->tab != TAB_MEDIA) || view(ui)->count == 0)
        ui->btn_focus = 0; /* the buttons belong to a card of the Games and Media tabs */
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
        update_music(ui, in, dt);

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
    int row_tab = ui->tab == TAB_GAMES || ui->tab == TAB_MEDIA; /* a row of big cards with the buttons under the selected one */
    if (row_tab && fabsf((float)in->lx - 128) >= fabsf((float)in->ly - 128))
        sides.ly = 128; /* the stick is pushed sideways: that is the cards, not the buttons below them */
    int move = ui->tab == TAB_MUSIC || top ? 0 : step_input(ui, &sides, dt, grid || row_tab);
    int fresh = ui->repeat_held == 0; /* a move that is a new press, not a held button repeating: only that enters the top bar */
    if (row_tab && count) { /* ↓ to Start, ← → between Start and Options, ↑ or ○ back to the card, ↑ from the card to the top bar */
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
        } else if (move == 1 && on_btn == 1 && sel_card(ui) >= 0) { /* (the Library and Music cards have no options: Open is the only button) */
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
    if (ui->tab != TAB_MUSIC) {
        loader_set_round(ui->tab != TAB_LIBRARY); /* the Library's cards are rectangles, the Games tab's are rounded: the pictures follow */
        loader_focus(*sel);
    }
    if (music_needs_cjk())
        font_ensure_cjk(); /* japanska/ryska låttitlar (laddas bara en gång) */
    update_background(ui, dt);
    if (ui->tab == TAB_MUSIC && ui->mscreen == 0)
        musicui_update_cover(&ui->mu, dt);

    if (ui->label_for != sel_card(ui)) {
        fit_label(ui);
        ui->label_anim = 0;
    }
    ui->label_anim = clampf(ui->label_anim + dt * 5.0f, 0, 1);
    ui->label_t += dt; /* (the title scrolls when it does not fit) */
    ui->lib_label_t += dt;
    return UI_NONE;
}

/* --- ritning --- */

/* A card that is opened (the Library card, the Music card) darkens the whole background behind it, all the way from the top of the screen: the cards of Games and Media have the
 * background at full brightness, the screen that opens from them has it dark (so that the lines of the background do not run across the names), and it goes dark and bright again
 * in a quarter of a second. The top bar is drawn over it (the tabs, the clock, the battery and the cog wheel stay bright). */
static void draw_dim(const Ui *ui)
{
    if (ui->dim > 0)
        gfx_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(8, 8, 12, (int)(ui->dim_alpha * smooth(ui->dim))));
}

static void draw_background(Ui *ui)
{
    if (ui->tab == TAB_MEDIA || ui->tab == TAB_MUSIC) {
        /* The Media tab and the Music screen have a background of their own, drawn in code from the Media picture in the colours of the theme (theme.c). It comes up out of
         * the dark with the cards, unless the tab was opened from one that has it already (Media -> Music and back). */
        theme_backdrop_media(theme_cur(), ui->time);
        if (ui->tab_anim < 1 && !ui->lib_calm)
            gfx_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(255 * (1 - smooth(ui->tab_anim)))));
        return;
    }
    if (ui->tab == TAB_LIBRARY) {
        /* The Library has a background of its own (a ribbon of light on black, drawn in code, in the colours of the theme): never a picture of a game, in every theme.
         * It comes up out of the dark with the cards (tab_anim goes from 0.4 to 1 when the tab is opened). */
        theme_backdrop_library(theme_cur(), ui->time);
        if (ui->tab_anim < 1 && !ui->lib_calm) /* (not when it was opened from its card, which has this background already) */
            gfx_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(255 * (1 - smooth(ui->tab_anim)))));
        return;
    }
    /* en bild som täcker hela skärmen (färdigtonad, eller den gamla helt under den nya) – då behövs ingen yta under */
    if (!ui->bg_cur || (ui->bg_fade < 1 && !ui->bg_old)) {
        if (ui->bg_lib == 2)
            theme_backdrop_media(theme_cur(), ui->time); /* (leaving Media: its background stays until the cards and the surface have changed places) */
        else if (ui->bg_lib)
            theme_backdrop_library(theme_cur(), ui->time); /* the Library card: the Library's own background, so that opening the Library changes nothing behind the cards */
        else
            theme_backdrop(theme_cur(), ui->time, 1); /* the theme's surface: shows when a game has no picture (or pictures are off) */
    }

    const u32 tint = RGBA(205, 205, 210, 0);
    int fade = (int)(ui->bg_fade * 255);
    if (ui->bg_old)
        gfx_tex(ui->bg_old, 0, 0, SCREEN_W, SCREEN_H, tint | (u32)(ui->bg_cur ? 255 : 255 - fade) << 24);
    if (ui->bg_cur)
        gfx_tex(ui->bg_cur, 0, 0, SCREEN_W, SCREEN_H, tint | (u32)fade << 24);

    /* A game's picture is darkened at the top (tabs, cards), on the left (text) and at the bottom (buttons) so they stay
     * readable on any picture. The theme's surface is not darkened: it looks the same as on the other screens. The
     * shades follow how much of the picture is shown, so a picture fading in or out takes them with it. */
    float cover = bg_cover(ui);
    if (cover > 0) {
        gfx_gradient_v(0, 0, SCREEN_W, 120, RGBA(0, 0, 0, (int)(165 * cover)), RGBA(0, 0, 0, 0));
        gfx_gradient_h(0, 0, 300, SCREEN_H, RGBA(0, 0, 0, (int)(120 * cover)), RGBA(0, 0, 0, 0));
        gfx_gradient_v(0, 160, SCREEN_W, SCREEN_H - 160, RGBA(0, 0, 0, 0), RGBA(0, 0, 0, (int)(175 * cover)));
    }
    if (ui->bg_dip > 0) /* the dip to black while the Library's background and the surface change places */
        gfx_rect(0, 0, SCREEN_W, SCREEN_H, RGBA(0, 0, 0, (int)(255 * ui->bg_dip)));
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
    gfx_rect(x, y + 4, BAT_TERM_W, 4, with_alpha(LIGHT, a)); /* (the outline is opaque: as light as the clock and the tag) */
    gfx_round_rect(body, y, BAT_BODY_W, BAT_H, 2.5f, with_alpha(LIGHT, a));
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

#define TOP_MID     20.0f /* the middle of the clock row, where the wheel, the battery and the Wi-Fi sign are centred: the cog wheel's frame goes round it */
#define TOP_FRAME_H 24.0f /* (a tab's frame is centred on its text) */
#define TOP_GAP     12.0f /* between the things on the right: Wi-Fi sign, battery (with its number), clock, cog wheel; the tabs keep it from the first of them, too */
#define BAT_GAP      5.0f /* between the battery and its number, which belong together (the number's own left edge adds a pixel to it) */
#define BOLT_R       6.0f /* the charging bolt is 2 R = 12 px high, as high as the battery ... */
#define BOLT_W       6.0f /* ... and R = 6 px wide, from its left tip to its right one (see bolt_polygon in icons.c) */
#define BOLT_GAP     3.0f /* between the bolt and the battery, which belong together (the bolt is slanted: it looks further from the battery than this) */
#define WIFI_R       7.0f /* the Wi-Fi sign is a square of 2 R = 14 px (a little higher than the battery: three soft arcs need that much room) */
#define COG_R        7.0f /* the cog wheel is a square of 2 R, and its right edge is on the margin like the clock's used to be */
#define COG_BTN_D   TOP_FRAME_H /* the white circle of the selected cog wheel: as high as the frame of the selected tab */

static void button_bg(float x, float y, float w, float h, float t, float fade);

static void draw_top_bar(Ui *ui)
{
    float a = ui->intro;

    /* klockan, batteriet och Wi-Fi ändras sakta – systemet frågas en gång i sekunden, inte varje bild */
    static float polled_at = -10;
    static char clock_text[16]; /* "17:54", or "12:05 PM" with the 12-hour clock */
    static int sys_present, sys_charging, sys_level = -1, sys_wifi;
    if (fabsf(ui->time - polled_at) >= 1.0f) { /* även om tiden har nollställts */
        polled_at = ui->time;
        dt_clock_text(clock_text, sizeof(clock_text)); /* the PSP's clock as psp5 shows it (Settings -> System -> Date and Time) */
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

    /* the cog wheel is a button too: selected (↑ from the first thing of a tab) it is a white circle, as high as the frame of a tab, with a dark wheel in
     * it, like Start and Options under a card */
    if (cog * a > 0.01f)
        button_bg(gx - COG_BTN_D / 2, TOP_MID - COG_BTN_D / 2, COG_BTN_D, COG_BTN_D, cog, a);
    gfx_gear(gx, TOP_MID, COG_R, with_alpha(mix_color(LIGHT, DARK, cog), a), RGBA(14, 14, 18, (int)(255 * a)));

    /* flikarna krymper om de inte får plats (t.ex. långa ryska ord) – mäts bara om när något ändras */
    static float size = 0.8f, gap = 20, sized_room = -1;
    static int sized_tab = -1;
    static const char *sized_lang;
    float room = bar_left - TOP_GAP - MARGIN; /* the last tab's frame (9 px wider than its text) ends 3 px before the first thing on the right */
    int tabs[2], nt = top_tabs(ui, tabs); /* Games and Music, or only Library inside the Library */
    if (room != sized_room || ui->tab + 8 * ui->mscreen != sized_tab || L(TAB_NAMES[0]) != sized_lang) {
        size = 0.8f;
        gap = 20;
        for (;;) {
            float w = 0;
            for (int k = 0; k < nt; k++)
                w += text_width(size, tabs[k] == ui->tab, L(tab_name(ui, tabs[k]))) + gap;
            if (w - gap <= room || size <= 0.56f)
                break;
            size -= 0.04f;
            gap = 14;
        }
        sized_room = room;
        sized_tab = ui->tab + 8 * ui->mscreen;
        sized_lang = L(TAB_NAMES[0]);
    }
    float x = MARGIN;
    for (int k = 0; k < nt; k++) {
        int t = tabs[k], active = t == ui->tab;
        float tw = text_width(size, active, L(tab_name(ui, t))), pick = smooth(ui->top_anim[t]);
        if (pick * a > 0.01f) { /* selected with ↑ (not the same as the open tab, which is the bold one) */
            float fy, fh; /* centred on the text, which shrinks in long languages */
            popup_frame_around(popup_label_mid(25, size), TOP_FRAME_H, &fy, &fh);
            popup_choice(x - 9, fy, tw + 18, fh, pick * a);
        }
        text_draw(x, 25, size, active, with_alpha(active ? TEXT_MAIN : mix_color(RGBA(255, 255, 255, 120), TEXT_MAIN, pick), a),
                  ALIGN_LEFT, L(tab_name(ui, t)));
        x += tw + gap;
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
/* the plain card behind a picture (the card's colour): rounded, or a rectangle when r is 0 */
static void card_plate(float x, float y, float w, float h, float r, u32 color)
{
    if (r > 0)
        gfx_round_rect(x, y, w, h, r, color);
    else
        gfx_rect(x, y, w, h, color);
}

static void draw_card(Ui *ui, int list_index, float x, float y, float w, float h, float focus, float ring, float alpha, int grid)
{
    int library = list_index == HOME_LIBRARY; /* the Library card has no icon to load: a card in the theme's colour with its picture */
    int folder = LIB_IS_FOLDER(list_index);   /* ... and so has a folder of the Library: the same colour, a folder with its first letter */
    int media = HOME_IS_MEDIA(list_index); /* ... and the cards of Media: the same colour, the note of the Music icon, the film frame of Videos, the picture frame of Images */
    IconSlot *slot = library || folder || media ? NULL : loader_icon(list_index);
    float r = grid ? 0 : w / 12.0f; /* corner radius: 1/12 of the width at every size, which is what art.c cuts the pictures to; the Library's cards are rectangles (r 0) */
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
        int loading = !library && !folder && !media && (!slot || slot->state == SLOT_EMPTY || slot->state == SLOT_LOADING);
        float breath = loading ? 0.5f + 0.5f * sinf(ui->time * 4.0f + list_index) : 0;
        float pa = alpha * (1 - shown);
        u32 cc = LOOK_CARD; /* the card's own colour (look.h), breathes while the picture loads */
        if (library) { /* the same colour as a game without a picture; the picture is one image on the card's whole pixels */
            int tint = (int)lerpf(200, 255, focus); /* as dim as a picture is when it is not selected */
            card_plate(l, t, rt - l, b - t, r, RGBA(cc & 0xFF, (cc >> 8) & 0xFF, (cc >> 16) & 0xFF, (int)(255 * pa)));
            gfx_library_icon(l, t, rt - l, b - t, RGBA(tint, tint, tint, (int)(255 * pa)));
        } else if (media) { /* the Music card: the note of the reference on the card's colour */
            int tint = (int)lerpf(200, 255, focus);
            card_plate(grid ? l : x, grid ? t : y, grid ? rt - l : w, grid ? b - t : h, r, RGBA(cc & 0xFF, (cc >> 8) & 0xFF, (cc >> 16) & 0xFF, (int)(255 * pa)));
            u32 tc = RGBA(tint, tint, tint, (int)(255 * pa));
            if (list_index == HOME_MEDIA_VIDEO)
                gfx_video_icon(l, t, rt - l, b - t, tc);
            else if (list_index == HOME_MEDIA_IMAGES)
                gfx_images_icon(l, t, rt - l, b - t, tc);
            else
                gfx_music_icon(l, t, rt - l, b - t, tc);
        } else if (folder) { /* a folder drawn by the launcher, the capital letter of the folder's name cut out of its body */
            int tint = (int)lerpf(200, 255, focus);
            u32 cc_a = RGBA(cc & 0xFF, (cc >> 8) & 0xFF, (cc >> 16) & 0xFF, (int)(255 * pa));
            card_plate(l, t, rt - l, b - t, r, cc_a);
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
            card_plate(grid ? l : x, grid ? t : y, grid ? rt - l : w, grid ? b - t : h, r, RGBA((cc & 0xFF) + (int)(10 * breath), ((cc >> 8) & 0xFF) + (int)(10 * breath),
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
    if (ring > 0.01f) {
        if (grid) /* a rectangle: a straight frame, 2 px thick, one pixel outside the card and one inside it */
            gfx_frame(l - 1, t - 1, rt - l + 2, b - t + 2, 2, RGBA(240, 240, 244, (int)(235 * ring * alpha)));
        else
            gfx_edge_ring(l - 1, t - 1, rt - l + 2, b - t + 2, r + 1, grid, RGBA(240, 240, 244, (int)(235 * ring * alpha)));
    }
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
#define PILL_SIZE TITLE_SIZE /* the text, as large as the text of Start */

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
    if ((i < 0 && i != HOME_LIBRARY && !HOME_IS_MEDIA(i)) || fade <= 0)
        return;

    /* titeln bredvid kortet glider in när markeringen byts, med typetiketten (PSP, APP …) framför som på PS5 (the Library card has none) */
    float la = smooth(ui->label_anim);
    float tx = SMALL_X + (1 - la) * 8;
    const char *type = i >= 0 ? type_name(ui->list->items[i].type) : "";
    if (type[0]) { /* the tag and the title are as large as in the Library (the same sizes, the tag 15 high) */
        float bw = ceilf(text_width(0.5f, 1, type)) + 12; /* (whole pixels) */
        float ty = floorf(LABEL_Y - 12.5f + 0.5f); /* tagg och versaler har samma mitt, som i Biblioteket */
        gfx_round_rect(tx, ty, bw, 15, 3, with_alpha(LIGHT, la * fade)); /* (opaque, like the face of the clock of the time played) */
        text_draw(tx + bw / 2, ty + 11.5f, 0.5f, 1, with_alpha(DARK, la * fade), ALIGN_CENTER, type);
        tx += bw + 8;
    }
    text_draw_marquee(tx, LABEL_Y, ui->label_size, 0, with_alpha(TEXT_MAIN, la * fade), ui->label_room, ui->label, ui->label_t);

    /* Start and Options under the card, on the bottom row. Both have a dark background; the one
     * that is selected (↓ from the card) turns white with dark text. Options is a circle as high as Start, with three dots. */
    const char *go = L(i >= 0 ? S_LAUNCH : S_F_OPEN); /* the Library and Music cards are opened, not started */
    float bx = MARGIN, bh = 26, by = BOTTOM_Y - bh / 2;
    float w = ceilf(text_width(TITLE_SIZE, 0, go)) + 32;
    float t0 = smooth(ui->btn_anim[0]), t1 = smooth(ui->btn_anim[1]);
    button_bg(bx, by, w, bh, t0, fade);
    text_draw(bx + w / 2, by + 17.5f, TITLE_SIZE, 0, with_alpha(mix_color(TEXT_MAIN, DARK, t0), fade), ALIGN_CENTER, go);

    if (i >= 0) { /* (the Library and Music cards have no options: no button for them) */
        float mx = bx + w + 10;
        button_bg(mx, by, bh, bh, t1, fade);
        u32 dots = with_alpha(mix_color(TEXT_MAIN, DARK, t1), fade);
        for (int k = -1; k <= 1; k++)
            gfx_circle(mx + bh / 2 + k * 6, by + bh / 2, 2, dots);
    }

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
    case MI_INFO:         return L(S_INFO);
    case MI_KEEP:         return L(S_HOME_KEEP);
    case MI_SORT_NAME_AZ:    return L(S_SORT_NAME_AZ);
    case MI_SORT_NAME_ZA:    return L(S_SORT_NAME_ZA);
    case MI_SORT_ADDED_NEW:  return L(S_SORT_ADDED_NEW);
    case MI_SORT_ADDED_OLD:  return L(S_SORT_ADDED_OLD);
    case MI_SORT_SIZE_BIG:   return L(S_SORT_SIZE_BIG);
    case MI_SORT_SIZE_SMALL: return L(S_SORT_SIZE_SMALL);
    case MI_VIEW_ALL:
    case MI_VIEW_FOLDERS:
    case MI_VIEW_LOOSE:
    case MI_VIEW_BOTH:    return view_label(item - MI_VIEW_ALL);
    case MI_CANCEL:       return L(S_F_CANCEL);
    } /* (the names of the power popup are in power.c: power_popup_draw() draws it) */
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
        popup_ask(ui->del_lines, ui->del_nlines, lang_needs_cjk() ? 15.0f : 13.0f, ui->del_size,
                  labels, colors, ui->menu_n, ui->menu_sel, smooth(ui->ask_anim));
        return;
    }
    if (ui->menu_kind == MENU_POWER) { /* the same popup as on the other screens (power.c) */
        power_popup_draw(ui->menu_sel, a);
        return;
    }
    float w = 200, h = MENU_EDGE + ui->menu_n * MENU_ITEM_H + MENU_EDGE;
    float x, y;
    if (ui->menu_kind == MENU_VIEW || ui->menu_kind == MENU_SORT) { /* wide enough for the longest choice (the sort also has room for its check mark) */
        for (int k = 0; k < ui->menu_n; k++)
            w = fmaxf(w, ceilf(text_width(0.62f, 0, menu_label(ui->menu_item[k]))) + 32 + (ui->menu_kind == MENU_SORT ? 26 : 0));
        w = fminf(w, SCREEN_W - 2 * (MARGIN - 9)); /* (the widest it can be: the screen less 15 px each side) */
    }
    if (ui->menu_kind == MENU_SORT) { /* under the Sort button, whose right edge it shares */
        x = SCREEN_W - MARGIN + 9 - w + (1 - a) * 10;
        y = 66;
    } else if (ui->menu_kind == MENU_VIEW) { /* under the view button, whose left edge it shares */
        x = MARGIN - 9 - (1 - a) * 10;
        y = 66;
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
        float room = w - 32;
        if (item == MI_KEEP) { /* a switch, as in Settings: 26 x 14, the knob 10 across */
            float tx = x + w - 16 - 26, ty = ry + 7;
            look_switch(tx, ty, ui->menu_kept, a);
            room -= 34;
        } else if (ui->menu_kind == MENU_SORT && k == ui->lib_sort) { /* a check mark for the way the Library is sorted now */
            float cx = x + w - 16 - 6, cy = ry + MENU_ITEM_H / 2;
            float xy[6] = { cx - 5, cy, cx - 1.5f, cy + 3.5f, cx + 5, cy - 4 };
            u32 cc = with_alpha(LIGHT, a);
            u32 col3[3] = { cc, cc, cc };
            gfx_stroke(xy, col3, 3, 1.8f);
            room -= 22;
        }
        text_draw(x + 16, ry + 18, text_fit(0.62f, 0.45f, 0, room, label), 0, with_alpha(col, a), ALIGN_LEFT, label);
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
        text_draw(hd.count_x, head_y, SORT_SIZE, 0, with_alpha(TEXT_MAIN, ui_a), ALIGN_LEFT, num);

    /* the Sort button: selected with ↑ from a card under it, it gets the frame of a chosen entry; ✕ opens the popup. Its frame
     * is gone for as long as the top bar has the focus (sort_frame), so moving along the top bar never brings it back. */
    float on_sort = smooth(ui->sort_anim), sort_shown = smooth(ui->sort_frame);
    if (sort_shown * ui_a > 0.01f) {
        float fy, fh; /* centred on the text, whose baseline is at 53 */
        popup_frame_around(sort_mid, 22, &fy, &fh);
        popup_choice(hd.sort_l, fy, hd.sort_r - hd.sort_l, fh, sort_shown * ui_a);
    }
    text_draw(SCREEN_W - MARGIN, 53, SORT_SIZE, 0, with_alpha(TEXT_MAIN, ui_a), ALIGN_RIGHT, hd.sort); /* (white, like the view text at the left) */

    if (!v->count) { /* the view has nothing in it (no folders, or nothing outside them): the button above changes the view */
        text_draw(MARGIN, 120, 0.9f, 0, with_alpha(TEXT_MAIN, ui_a), ALIGN_LEFT, L(ui->lib_view == LIB_VIEW_FOLDERS ? S_LIB_NO_IN : S_LIB_NO_OUT));
        return;
    }

    /* korten – klipps under rubriken */
    int sel = ui->sel[TAB_LIBRARY];
    float focus_anim = smooth(ui->label_anim);
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
        float w = sw, h = shh; /* the selected card is not bigger than the others: it has the white frame */
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
        gfx_round_rect(MARGIN, BOTTOM_Y - 8, bw, 15, 3, with_alpha(LIGHT, ui_a));
        text_draw(MARGIN + bw / 2, BOTTOM_Y + 3.5f, 0.5f, 1, with_alpha(DARK, ui_a), ALIGN_CENTER, type);
        float tx = MARGIN + bw + 8;
        const char *played = play_text(ui, i); /* the pill with the time played is on the right of the row: the title stops before it */
        float max_w = SCREEN_W - MARGIN - tx - (played[0] ? play_pill_width(played) + 12 : 0);
        if (ui->lib_label_for != card || ui->lib_label_room != max_w) {
            snprintf(ui->lib_label, sizeof(ui->lib_label), "%s", title); /* (a title that is too long scrolls: nothing is shortened) */
            if (ui->lib_label_for != card)
                ui->lib_label_t = 0;
            ui->lib_label_for = card;
            ui->lib_label_room = max_w;
        }
        draw_play_pill(played, SCREEN_W - MARGIN, ui_a * focus_anim);
        text_draw_marquee(tx + (1 - focus_anim) * 6, BOTTOM_Y + 4.5f, TITLE_SIZE, 0, with_alpha(TEXT_MAIN, ui_a * focus_anim), max_w, ui->lib_label, ui->lib_label_t);
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
    musicui_draw(screen_mu(ui), a, slide);
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

int ui_video_open(const Ui *ui)
{
    return ui->vp != NULL;
}

void ui_sleep(Ui *ui, int asleep)
{
    if (ui->vp)
        videoplay_sleep(ui->vp, asleep);
}

void ui_draw(Ui *ui)
{
    if (ui->iv || ui->vp) { /* a picture or a video: nothing else on the screen but the volume bar and the sound button's popup */
        gfx_begin(RGBA(0, 0, 0, 255));
        if (ui->iv)
            imageview_draw(ui->iv);
        else
            videoplay_draw(ui->vp);
        osd_draw();
        gfx_end();
        return;
    }
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
        if (ui->tab == TAB_GAMES && !ui->list->count)
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
        gfx_circle(MARGIN + 4 + i * 12, 140, 2 + p, COLOR_ALPHA(TEXT_MAIN, (int)(90 + 165 * p)));
    }
    char line[96];
    snprintf(line, sizeof(line), "%d  ·  %.60s", done, name);
    text_draw(MARGIN, 170, 0.6f, 0, TEXT_DIM, ALIGN_LEFT, line);
    gfx_end();
}
