/* psp5 – made by Mille Weber */
#ifndef SAVES_H
#define SAVES_H

#include "ui.h"
#include "scan.h"

/* Manage Saved Data (options popup -> Manage Saved Data): every folder in PSP/SAVEDATA on the memory card (and on
 * the PSP Go's internal storage), whichever game or app the popup was opened from. For each save: the game or app
 * it belongs to, the title and description its PARAM.SFO holds, its size and when it was changed. Marked saves can
 * be deleted. Drawn like the file manager (the list) and the information screen (the details).
 *
 * `games` is the game list (to find out which game a save belongs to); it has to stay as it is while the screen is
 * open. The buttons are the names of controls.h (confirm, back, □ mark, △ select all). */
void saves_open(const EntryList *games);
/* Once per frame. 1 = close the screen. */
int saves_update(const UiInput *in, float dt);
void saves_draw(void);
/* Stops the background reading (and a delete in progress) and frees everything. */
void saves_close(void);

#endif
