/* psp5 – made by Mille Weber */
#ifndef MUSICUI_H
#define MUSICUI_H

#include "gfx.h"
#include "music.h"

/* The Music screen (opened by the Music card of the Media tab). On the left the songs, listed like the file manager lists files: the storages first when both the
 * Memory Stick and the internal storage have a MUSIC folder (else straight into the one that has), then the folders and songs of the folder that is open, a thin frame
 * round the chosen line. ✕ opens a folder or plays a song (and the songs of that folder play one after the other), ○ goes up a folder (and out of the screen from the top),
 * △ moves between the list and the player. On the right the player (the picture "Music Player.png", without the key signs): the cover, the title and the artist, the
 * bar with the time played, and the buttons: volume, previous, play / pause, next and "...". ← → move over the buttons, ↑ selects the bar (← → then jump 10 seconds),
 * ✕ presses a button. "..." opens a list with Shuffle, Repeat and Equalizer (✕ on one opens a popup with its values, as Settings does), the speaker shows the launcher's volume bar and ← → change the volume. */

/* What the player (the buttons, the bar, the popups) is doing: the Music screen has one, the popup of the sound button has another, so that the two never take each other's presses */
typedef struct PlayerState {
    int pbtn;                  /* the button chosen (MU_VOLUME .. MU_MORE), or MU_BAR */
    int popup;                 /* 0 = none, MU_POPUP_MORE, MU_POPUP_CHOICE, MU_POPUP_VOLUME */
    int popup_sel;             /* the row of the "..." popup */
    int choice_sel;            /* the value chosen in the popup of a row */
    float popup_anim;          /* 0..1 */
    int rep_dir;               /* a held direction repeats */
    float rep_timer, rep_held;
    float seek_to, seek_show;  /* the bar: where the last jump went to, shown a moment (the song has not got there yet) */
} PlayerState;

typedef struct MusicUi {
    int level;                 /* 0 = the list of storages, 1 = a folder */
    char roots[2][16];         /* the MUSIC folders that there are */
    int nroots;
    char dir[256];             /* the folder that is open (level 1) */
    MusicEntry *e;             /* its lines (the folders first), n of them */
    int n;
    int ok;                    /* the folder could be read */
    int sel;                   /* the chosen line */
    float scroll;              /* animated top line */
    int sel_stack[16];         /* where the choice was in each folder above this one, for ○ */
    int depth;
    int focus;                 /* 0 = the list, 1 = the player */
    /* the player of the screen: */
    int pbtn;                  /* the button chosen (MU_VOLUME ... MU_MORE), or MU_BAR */
    int popup;                 /* 0 = none, MU_POPUP_MORE (Shuffle, Repeat, Equalizer), MU_POPUP_CHOICE (the values of one of them), MU_POPUP_VOLUME (the launcher's volume bar is shown and ← → change it) */
    int popup_sel;             /* the row of the "..." popup */
    int choice_sel;            /* the value chosen in the popup of a row */
    float popup_anim;          /* 0..1 */
    /* a held direction repeats */
    int rep_dir;
    float rep_timer, rep_held;
    /* the bar: where the last jump went to, shown a moment (the song has not got there yet) */
    float seek_to, seek_show;
    /* the player popup of the sound button: its own state (while it is open the fields above are the screen's) */
    int mini;                  /* it is open */
    float mini_anim;           /* 0..1 */
    PlayerState mp;
    float name_t;              /* seconds since the chosen name was shown, for its scrolling */
    /* the cover of the song that plays */
    Texture *cover;
    int cover_gen;             /* music_generation() it was asked for */
    float cover_fade;
    float time;
} MusicUi;

enum { MU_VOLUME, MU_PREV, MU_PLAY, MU_NEXT, MU_MORE, MU_BAR, MU_BUTTONS };
enum { MU_POPUP_MORE = 1, MU_POPUP_CHOICE, MU_POPUP_VOLUME };
enum { MUSICUI_NONE, MUSICUI_CLOSE, MUSICUI_TOP }; /* what musicui_update asks of the screen around it: leave the Music screen, or select the top bar */

void musicui_init(MusicUi *m);
void musicui_free(MusicUi *m);
/* The Music screen is opened: reads the MUSIC folders (a folder that was open is open again, if it can still be read). */
void musicui_open(MusicUi *m);
/* One step. `pressed` and `held` are the buttons as the screens see them (controls.h), lx / ly the stick. */
int musicui_update(MusicUi *m, u32 pressed, u32 held, int lx, int ly, float dt);
/* The cover is let go (the screen is left: the picture is not needed, and it is asked for again when the screen opens). */
void musicui_release_cover(MusicUi *m);
/* Keeps the cover of the song that plays (call it every frame, also when the screen is not the one shown: nothing is drawn then). */
void musicui_update_cover(MusicUi *m, float dt);
/* The whole screen: `a` = how visible (0..1), `slide` = pixels it has still to slide in. */
void musicui_draw(MusicUi *m, float a, float slide);

/* The player alone, as a popup in the middle of the screen over whatever screen is open: the PSP's sound button opens it (and closes it again), ○ closes it too.
 * It has the player's buttons, bar and popups, and nothing of the list. main.c gives it the buttons while it is open and the screen below gets none. */
void musicui_mini_open(MusicUi *m);
/* One step (also while it fades out); `note` = the sound button was pressed. Returns 1 in the step in which it closes. */
int musicui_mini_update(MusicUi *m, u32 pressed, u32 held, int lx, int ly, float dt, int note);
/* Draws it over the screen (nothing when it is closed); called by osd_draw(), at the end of every frame. */
void musicui_mini_draw(MusicUi *m);

#endif
