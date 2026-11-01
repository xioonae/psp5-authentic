/* psp5 – made by Mille Weber */
#ifndef GAMESIZE_H
#define GAMESIZE_H

#include <psptypes.h>
#include "scan.h"

/* How big the games and apps are, for the Library's size sort, the Games and Apps list and Console Storage.
 * An ISO is as big as its file. An EBOOT game or app is the whole folder it lives in, which takes a while to count: it is counted in a
 * thread of its own, one folder after the other, and until a folder has been counted its size is the size of the EBOOT.PBP. */

/* Starts counting every game and app of `list` (what was counted before is forgotten). */
void gamesize_start(const EntryList *list);
/* Stops the thread and waits for it. Safe to call at any time. */
void gamesize_stop(void);
/* The size of a game or app in bytes: counted, or the size of its main file as long as it has not been counted. */
unsigned long long gamesize_of(const Entry *e);
/* 1 when the size of `e` has been counted (an ISO is counted at once). */
int gamesize_known(const Entry *e);
/* Grows by one every time another folder has been counted: a screen that is sorted by size sorts again when it changes. */
int gamesize_version(void);
/* All the games and apps of the card together: dev 0 = ms0:, 1 = ef0:, -1 = both (the sizes that are counted, else the main files'). */
unsigned long long gamesize_total(int dev);
/* 1 while the thread is still counting. */
int gamesize_busy(void);

#endif
