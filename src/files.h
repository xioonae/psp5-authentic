/* psp5 – made by Mille Weber */
#ifndef FILES_H
#define FILES_H

#include "ui.h"

/* Filhanteraren (Inställningar → Minne): bläddra på minneskortet (och internminnet
 * på PSP Go), kopiera, flytta, byt namn, radera och skapa mappar. Rör aldrig flash0. */
void files_open(void);
/* 1 = stäng filhanteraren */
int files_update(const UiInput *in, float dt);
void files_draw(void);
/* Avbryter en pågående åtgärd och frigör allt */
void files_close(void);
/* 1 = a job is running or a popup is open: SELECT does not open the power popup then (main.c, power.c). */
int files_modal(void);

#endif
