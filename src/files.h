/* psp5 – made by Mille Weber */
#ifndef FILES_H
#define FILES_H

#include "ui.h"

/* Filhanteraren (Inställningar → Minne): bläddra på minneskortet (och internminnet
 * på PSP Go), kopiera, flytta, byt namn, radera och skapa mappar. Rör aldrig flash0. */
void files_open(void);
/* The File Manager as a place to choose a plugin: only folders and .prx files are listed, ✕ on a file closes it with that file chosen, ○ goes up and out of the top; copy, move,
 * rename, delete, new folder and marks are not there. files_update() gives 1 when it is over; files_picked() then says what was chosen. */
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

#endif
