/* psp5-authentic – the power popup of the SELECT button */
#ifndef POWER_H
#define POWER_H

#include "settings.h"

/* SELECT opens the power popup on every screen: Exit to XMB, Sleep, Shut down, Restart and Recovery menu, in the middle of the screen (Settings has none of them).
 * Games, Library and Music keep their own state for it (ui.c: they have the options popups too) but draw it with power_popup_draw(); every other screen (Settings,
 * Information, Manage Saved Data, the File Manager, the USB screen) uses all of this, asked by main.c (power_step()), which also decides when it may open: not while the
 * screen has a popup of its own, or a job running. While it is open the screen gets no buttons. */

#define POWER_CHOICES 5 /* Exit to XMB, Sleep, Shut down, Restart, Recovery menu */
#define POWER_OPEN_ON 1 /* ... and it opens on the second, Sleep (as it always did: SELECT then ✕ is Sleep; Exit to XMB is one step up) */

/* Closed, at once (before a screen is opened: a popup that was still fading out is not carried over to it). */
void power_reset(void);

/* 1 = it is open (it is 0 again as soon as it is closed, while it fades out). */
int power_is_open(void);

/* Opens it on Sleep (POWER_OPEN_ON); a direction that is held when it opens does not move the choice. */
void power_open(const UiInput *in);

/* Every frame (with no buttons when it is closed, to let it fade out). ↑ ↓ choose, ✕ takes the choice, ○ START and SELECT close it. Returns what was chosen: SA_XMB, SA_SUSPEND,
 * SA_SHUTDOWN, SA_RESTART or SA_RECOVERY, else SA_NONE. The choice is closed at once and its sound has started. */
int power_update(const UiInput *in, float dt);

/* What a screen's loop does every frame, before its own update (main.c): SELECT opens the popup unless `can_open` is 0 (the screen has a popup of its own, or a job running);
 * while it is open, and for the press that opens it and the press that closes it, `*in` is cleared (the screen gets no buttons). Returns what was chosen, else SA_NONE. */
int power_frame(UiInput *in, float dt, int can_open);

/* The popup for the frame that is being drawn: dims the screen and draws the panel (nothing when it is closed and gone). Call it last, before osd_draw(). */
void power_draw(void);

/* Only the panel with its five choices, `sel` chosen, `a` = how much it is there (0..1, already smoothed). ui.c draws its own popup with this too, so that the look is in one place. */
void power_popup_draw(int sel, float a);

#endif
