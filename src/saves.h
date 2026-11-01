/* psp5 – made by Mille Weber */
#ifndef SAVES_H
#define SAVES_H

#include "ui.h"
#include "scan.h"

/* Manage Saved Data (Settings > Storage > Console Storage > Saved Data): every folder in PSP/SAVEDATA on the memory card (and on
 * the PSP Go's internal storage), whichever game or app the popup was opened from. For each save: the game or app
 * it belongs to, the title and description its PARAM.SFO holds, its size and when it was changed. Marked saves can
 * be deleted. Drawn like the file manager (the list) and the information screen (the details).
 *
 * `games` is the game list (to find out which game a save belongs to); it has to stay as it is while the screen is
 * open. The buttons are the names of controls.h (confirm, back, □ mark, △ select all). */
void saves_open(const EntryList *games);
/* The same, only the saves of one storage (0 = ms0:, 1 = ef0:); -1 = both */
void saves_open_on(const EntryList *games, int dev);
/* Games and Apps: the games and apps of the game list (those on one storage if dev is 0 or 1, else all), to sort, mark and delete like the saved data.
 * The list has to stay as it is while the screen is open. The games that were deleted are in saves_gone_*() after it, until the next time a screen is opened:
 * their places in the game list, the last place first, for the caller to remove them one by one (the UMD disc is never in the screen). */
void saves_open_games(const EntryList *games, int dev);
int saves_gone_count(void);
int saves_gone_entry(int i);
/* Once per frame. 1 = close the screen. */
int saves_update(const UiInput *in, float dt);
void saves_draw(void);
/* Stops the background reading (and a delete in progress) and frees everything. */
void saves_close(void);
/* 1 = a delete is running or a popup is open: SELECT does not open the power popup then (main.c, power.c). */
int saves_modal(void);

#endif
