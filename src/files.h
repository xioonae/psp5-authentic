/* psp5 – made by Mille Weber */
#ifndef FILES_H
#define FILES_H

#include "ui.h"

/* Filhanteraren (Inställningar → Minne): bläddra på minneskortet (och internminnet
 * på PSP Go), kopiera, flytta, byt namn, radera och skapa mappar. Rör aldrig flash0. */
void files_open(void);
/* The File Manager as a place to choose a plugin: only folders and .prx files are listed, ✕ on a file closes it with that file chosen, ○ goes up and out of the top; copy, move,
 * rename, delete, new folder and marks are not there. files_update() gives 1 when it is over; files_picked() then says what was chosen.
 *
 * ✕ on a file opens it: a video or a picture in the video player or the image viewer (L and R go to the one before and after in the folder), a song starts (and the songs of the folder
 * after it), a text file asks View or Edit (textedit.h); a file that is not text says it cannot be opened. The options (copy, move, rename, delete, new folder) are on START. */
void files_open_pick(void);
/* The file that was chosen (its whole path) in `out`; 1 = one was chosen, 0 = it was closed without a choice. */
int files_picked(char *out, int size);
/* 1 = stäng filhanteraren */
int files_update(const UiInput *in, float dt);
void files_draw(void);
/* Avbryter en pågående åtgärd och frigör allt */
void files_close(void);
/* 1 = a job is running or a popup is open: SELECT does not open the power popup then (main.c, power.c). */
int files_modal(void);
/* The PSP goes to sleep (1) or wakes up (0): a video that is open is paused and goes on when it wakes. main.c calls it every frame. */
void files_sleep(int asleep);

#endif
