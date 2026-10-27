/* psp5 – made by Mille Weber */
#ifndef INFO_H
#define INFO_H

#include "ui.h"

/* The information screen of a game or app (options popup -> Information): where it is, how big it is (for an
 * EBOOT game the whole folder), when it was changed, the packing of a CSO/ZSO/DAX, and the few PARAM.SFO entries that
 * matter: DISC_ID, DISC_VERSION, APP_VER and PSP_SYSTEM_VER.
 * Drawn like the settings: the picture and name on the left, the list on the right.
 *
 * `list_index` is the game's index in the game list (for its picture). The back button (controls.h) closes it. */
void info_open(const Entry *e, int list_index);
/* Once per frame. 1 = close the screen. */
int info_update(const UiInput *in, float dt);
void info_draw(void);
/* Stops the background count of the folder size. */
void info_close(void);

#endif
