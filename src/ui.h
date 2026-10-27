/* psp5 – made by Mille Weber */
#ifndef UI_H
#define UI_H

#include "gfx.h"
#include "scan.h"
#include "loader.h"
#include "folders.h"

enum { UI_NONE, UI_LAUNCH, UI_SETTINGS, UI_INFO, UI_DELETE, UI_SAVES,
       UI_SLEEP, UI_SHUTDOWN, UI_RESTART, UI_RECOVERY /* the power popup of the SELECT button */ };
/* Games is Home (home.h): the pinned cards, the ones opened last and, at the far right, the Library card. Music has the music.
 * Library is not a tab of the top bar: it is a screen of its own that the Library card opens (✕) and ○ closes. It shows everything,
 * and while it is open the top bar has only the word "Library" in it, in the place of Games (it cannot be selected). */
enum { TAB_GAMES, TAB_MUSIC, TAB_LIBRARY, TAB_COUNT };
/* what the top bar can have selected (Ui.top_focus): Games or Music, or the cog wheel (inside the Library only the cog wheel) */
enum { TOP_COG = TAB_COUNT };

/* the choices of the options popup (menu_item) */
enum { MI_SAVES, MI_INFO, MI_DELETE, MI_DELETE_YES, MI_CANCEL,
       MI_REMOVE,     /* Remove from Home */
       MI_REMOVE_YES, /* ... confirmed, for a pinned card */
       MI_ADD,        /* Add to Home (Library) */
       MI_PIN, MI_UNPIN,
       MI_SORT_NAME, MI_SORT_RECENT, /* the sort popup of the Library */
       MI_VIEW_ALL, MI_VIEW_FOLDERS, MI_VIEW_LOOSE, MI_VIEW_BOTH, /* the view popup of the Library (LIB_VIEW_*, in this order) */
       MI_SLEEP, MI_SHUTDOWN, MI_RESTART, MI_RECOVERY /* the power popup: the same four as Settings > Power and USB */ };
/* what the popup is for (menu_kind) */
enum { MENU_CARD, MENU_SORT, MENU_POWER, MENU_VIEW };

/* What the Library shows (Ui.lib_view; the view button at the top left of the Library, kept in psp5.cfg):
 *   ALL      every game and app, one after the other, as if there were no folders
 *   FOLDERS  only what is in folders: a card for every folder (the games and apps are inside it)
 *   LOOSE    only what is in no folder
 *   BOTH     a card for every folder and, after them, the games and apps that are in no folder
 * A folder is a Categories Lite / arkMenu category: the last folder before the game's own folder in PSP/GAME, or before the image in
 * ISO (folders.h). */
enum { LIB_VIEW_ALL, LIB_VIEW_FOLDERS, LIB_VIEW_LOOSE, LIB_VIEW_BOTH, LIB_VIEW_COUNT };

/* A card of the Library that is a folder is -10 - n in the Library's LoaderView (n is the number of the folder in Ui.folders). The loader
 * leaves every negative number alone, as it does the Library card of Games. */
#define LIB_FOLDER_ID(n) (-10 - (n))
#define LIB_IS_FOLDER(i) ((i) <= -10)
#define LIB_FOLDER_NO(i) (-10 - (i))

typedef struct {
    u32 pressed; /* nedtryckta denna bildruta */
    u32 held;
    u8 lx, ly;   /* analog spak, 128 = mitten */
} UiInput;

/* "Inga knappar, spaken i mitten". Allt noll är INTE neutralt: spaken står då i ytterläget
 * (vänster/upp), och gränssnittet flyttar markeringen åt det hållet. */
#define UI_NO_INPUT { 0, 0, 128, 128 }

typedef struct {
    const EntryList *list;
    LoaderView views[TAB_COUNT]; /* Spel (Home; with the Library card as HOME_LIBRARY) / Bibliotek (Musik har egen lista, se music.h) */
    int tab;
    int sel[TAB_COUNT];  /* position i respektive flik */
    float pos;           /* animerad scrollposition i aktiv flik */
    float tab_anim;      /* 0→1 efter flikbyte */
    float time;
    float intro;         /* 0→1 vid start */
    float launch;        /* <0 = inaktiv, annars 0→1 */
    int can_launch;      /* 0 = ARK saknas (t.ex. PPSSPP) */

    /* bakgrund: gammal + ny med övertoning */
    Texture *bg_old, *bg_cur;
    float bg_fade;
    int bg_index;
    float still;         /* sekunder sedan markeringen flyttades */

    /* titeln bredvid valt kort, anpassad till bredden */
    int label_for;
    char label[160];
    float label_size;
    float label_anim;

    /* knapprepetition */
    int repeat_dir;
    float repeat_timer, repeat_held;

    /* biblioteket: rutnät med allt */
    int lib_sort;        /* 0 = namn, 1 = senast tillagda */
    int lib_view;        /* LIB_VIEW_*: what the Library shows (the view button) */
    int lib_open;        /* the folder that is open (its number in `folders`), or -1: the Library itself. Open, it shows that folder's games and apps. */
    char lib_open_name[FOLDER_NAME_MAX]; /* ... and its name, which is how it is found again when the list changes */
    FolderSet folders;   /* the folders of the game list */
    int lib_focus;       /* 0 = a card, 1 = the Sort button at the top right, 2 = the view button at the top left */
    float sort_anim;     /* 0..1: how selected the Sort button looks */
    float sort_frame;    /* 0..1: how visible its frame is (it is gone while the top bar is selected, ↓ brings it back) */
    float view_anim;     /* the same two for the view button */
    float view_frame;
    float card_ring;     /* 0..1: how visible the ring around the selected card is: gone while the Sort button, Start / Options or the top bar is selected (the card itself keeps its size) */
    int btn_focus;       /* Games: 0 = the card, 1 = the Start button under it, 2 = the Options button next to it */
    float btn_anim[2];   /* 0..1: how selected Start / Options look */
    int top_focus;       /* -1 = the top bar is not selected, else TAB_GAMES, TAB_MUSIC or TOP_COG: ↑ from the first thing of a tab gets here */
    float top_anim[TOP_COG + 1]; /* 0..1: how selected each tab / the cog wheel looks */
    float grid_scroll;   /* animerad översta rad */
    int grid_target;     /* raden som ska stå överst */
    char lib_label[160];
    int lib_label_for;
    float lib_label_room; /* the room the title in lib_label was shortened for (less when the small clock with the time played is there) */
    int pt_for;           /* the game whose time played is in pt_secs (-2 = none yet): kept so that the list is not searched every frame */
    unsigned pt_secs;     /* ... its seconds */
    char pt_text[16];     /* ... as "54h" or "45min" (made at every draw, in the language of that moment), empty when there is nothing to show */

    /* musik */
    int music_sel;
    float music_scroll;
    int music_target;
    int seek_dir;        /* ← / → som hålls i musikfliken: -1/1, 0 = ingen */
    int seeking;         /* hållen länge nog: spolar i stället för att byta låt */
    float seek_held, seek_pos;
    float seek_show;     /* visa målet en stund efter att knappen släppts */

    /* omslag till låten som spelas */
    Texture *cover;
    int cover_for;       /* låtens index, -1 = inget */
    float cover_fade;

    /* START i Spel/Bibliotek (eller ✕ på Alternativ-knappen): en liten alternativmeny för valt kort (Hantera sparad data, Radera, Information, Ta bort från
     * Home, Fäst). Samma meny visar sorteringen i Biblioteket (menu_kind), och, mitt på skärmen och från alla flikar med SELECT,
     * Viloläge / Stäng av / Starta om / Återställningsmeny. */
    int menu_open;
    int menu_kind;       /* MENU_CARD / MENU_SORT */
    float menu_anim;     /* 0→1 in-/uttoning (skärmen mörknar) */
    float ask_anim;      /* the question (delete? remove?) is on screen */
    int menu_n, menu_sel;
    unsigned char menu_item[6]; /* MI_* */
    int menu_stage;      /* 0 = valen, 1 = frågan "radera?" / "ta bort?" */
    int menu_dir;        /* -1/1 medan upp/ner hålls i menyn (en flytt per tryck) */
    int del_is_dir;      /* frågan gäller en hel mapp */
    int ask_remove;      /* the question is "remove the pinned card from Home?", not "delete?" */
    int del_measured;    /* mappens storlek är räknad */
    char del_lines[4][96]; /* frågans text, radbruten */
    int del_nlines;
    char del_size[64];   /* storlek (och antal filer) under frågan */

    char toast[96];
    float toast_time;
} Ui;

void ui_init(Ui *ui, const EntryList *list, int can_launch);
void ui_free(Ui *ui);
/* Spellistan har ändrats först (UMD-skivan togs ut och/eller sattes i).
 * Bygger om flikarna och behåller markeringen. Laddaren ska redan vara omstartad. */
void ui_relist(Ui *ui, int removed_front, int added_front);
/* Spel nummer `index` i listan har raderats och tagits bort ur den: bygger om flikarna, markeringen
 * står kvar på samma plats så att nästa kort glider dit. Laddaren ska redan vara omstartad. */
void ui_relist_removed(Ui *ui, int index);
/* Glöm kortet på Home (innan det tas bort ur listan) */
void ui_forget_entry(Ui *ui, const Entry *e);
int ui_update(Ui *ui, const UiInput *in, float dt);
void ui_draw(Ui *ui);
void ui_toast(Ui *ui, const char *msg);
/* Låter korten glida in igen som vid start (efter startskärmen). */
void ui_replay_intro(Ui *ui);
/* Index i spellistan för markerad post, eller -1 (also when a folder of the Library is selected). */
int ui_selected(const Ui *ui);

/* Enkel skärm under första skanningen. */
void ui_draw_scanning(int done, const char *name, float time);

#endif
