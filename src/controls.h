/* psp5 – made by Mille Weber */
#ifndef CONTROLS_H
#define CONTROLS_H

#include <pspctrl.h>
#include "gfx.h"

/* The buttons of psp5. Every screen asks for the same few things and nothing else:
 *   the D-pad and the analog stick   navigate
 *   PSP_CTRL_CROSS                   confirm
 *   PSP_CTRL_CIRCLE                  back
 *   PSP_CTRL_START                   options
 *   PSP_CTRL_SELECT                  the power menu
 *   PSP_CTRL_LTRIGGER / RTRIGGER     switch tabs (in a long list: a page up / down)
 *   PSP_CTRL_SQUARE / TRIANGLE       what a screen has besides (mark, select all, pause the music)
 * These are the names the screens use; they are *not* the buttons that are pressed. The pad is turned into them by controls_map() before
 * any screen sees it, so a screen never has to ask which button the player has chosen: PSP_CTRL_CROSS in a screen is whichever button
 * confirms. By default that is ✕ (○ when the PSP's own setting says so), ○ goes back, START opens the options and SELECT the power
 * menu. Settings -> Controls gives them to other buttons. */

/* the values of g_ctl_confirm and g_ctl_back: the button, or the PSP's own setting */
enum { CTL_PSP, CTL_CROSS, CTL_CIRCLE, CTL_SQUARE, CTL_TRIANGLE, CTL_COUNT };

extern unsigned char g_ctl_confirm; /* the button that confirms: CTL_PSP = ✕, or ○ when the PSP is set to that (the default) */
extern unsigned char g_ctl_back;    /* the button that goes back: CTL_PSP = ○, or ✕ when the PSP is set to that (the default) */
extern unsigned char g_ctl_options; /* 0 = START opens the options and SELECT the power menu (the default), 1 = the other way round */

/* The PSP's own setting (XMB: which of ✕ and ○ confirms): 1 = ○ confirms, 0 = ✕. Told at the start and when Settings changes it. */
void controls_psp_assignment(int circle_confirms);
int controls_psp_circle(void);

/* The physical button (CTL_CROSS .. CTL_TRIANGLE) that confirms / goes back now: the PSP's setting and a clash between the two settled. */
int controls_confirm_button(void);
int controls_back_button(void);

/* Give a button a job, and keep the pair apart: when confirm and back would be the same button the other one takes the button that this
 * one had (they swap). */
void controls_set_confirm(int ctl);
void controls_set_back(int ctl);

/* The pad as the screens see it: `physical` is the bits of sceCtrl, the result the same bits with the names above (the stick and the
 * D-pad, L, R, HOME ... are not touched). Used on both `held` and `pressed`, so a button that is held while the setting changes
 * does not "press" anything new. */
u32 controls_map(u32 physical);

/* The symbol (BTN_*) of the physical button behind one of the names above (PSP_CTRL_CROSS, CIRCLE, SQUARE, TRIANGLE, START or
 * SELECT): for the hints and the question marks that tell the player which button to press. -1 for anything else. */
int controls_glyph(u32 name);

/* psp5.cfg: "psp", "cross", "circle", "square", "triangle" */
const char *controls_code(int ctl);
int controls_from_code(const char *s);

#endif
