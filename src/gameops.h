/* psp5 – made by Mille Weber */
#ifndef GAMEOPS_H
#define GAMEOPS_H

#include <psptypes.h>
#include "scan.h"

/* What the options popup can do to a game or app on the memory card: measure the folder it lives in (for
 * the information screen and the "delete?" question) and delete it. Both run in a thread of their own,
 * so the interface keeps moving and a big folder never freezes the launcher. */

/* "12.3 MB" - the same sizes the file manager shows */
void gameops_fmt_size(char *out, int size, unsigned long long bytes);

/* --- what a delete would remove ---
 * Fills `path` with the file (ISO/CSO/ZSO/DAX) or the folder (EBOOT games and apps: the folder that holds the
 * EBOOT.PBP) and returns 1; *is_dir tells which. Returns 0 for everything that must never be deleted: the
 * UMD disc, anything outside ms0:/ISO, ef0:/ISO, ms0:/PSP/GAME and ef0:/PSP/GAME, the GAME folder itself
 * and the category folders in it, paths with "..", and psp5's own folder. */
int gameops_target(const Entry *e, char *path, int size, int *is_dir);

/* --- measuring a folder in the background --- */
void gameops_measure_start(const char *folder);
/* 1 when the count is finished (*bytes and *files are then valid), 0 while it is still running */
int gameops_measure_poll(unsigned long long *bytes, unsigned *files);
/* Stops a count that is still running (and waits for the thread). Safe to call at any time. */
void gameops_measure_stop(void);

/* --- any folder, for the saved data screen (saves.c) ---
 * Bytes and entries (files, sub folders and the folder itself) of `folder`; stops early when *stop becomes
 * non-zero. Runs on the caller's thread. */
void gameops_tree_stats(const char *folder, unsigned long long *bytes, u32 *items, volatile int *stop);
/* Free and total bytes of the card `path` is on ("ms0:/..." or "ef0:/..."). 1 = filled in. */
int gameops_dev_space(const char *path, unsigned long long *free_b, unsigned long long *total_b);

/* --- deleting --- */
enum { DEL_RUNNING, DEL_OK, DEL_FAILED, DEL_CANCELLED };

/* Deletes `folder` and everything in it on the caller's thread. Every removed entry is added to *done, `current`
 * (may be NULL) gets the name of the entry being removed, and the delete stops with DEL_CANCELLED once *cancel is
 * non-zero. It decides nothing about what may be deleted: the caller has to have checked that. */
int gameops_tree_delete(const char *folder, volatile int *cancel, volatile u32 *done, char *current, int current_size);

/* Starts deleting the game; the back button (PSP_CTRL_CIRCLE, controls.h) cancels it. Returns 0 if it is not allowed or no thread could
 * be started - nothing has changed then. */
int gameops_delete_start(const Entry *e);
/* Once per frame while deleting: handles the cancel button. DEL_RUNNING, or how it ended. */
int gameops_delete_update(u32 pressed);
/* Waits for the thread and frees it. Call once after gameops_delete_update has returned something else
 * than DEL_RUNNING. */
void gameops_delete_end(void);
/* Is the game's main file (the ISO, or the EBOOT.PBP) still there? After a cancelled or failed delete. */
int gameops_still_exists(const Entry *e);
/* The progress panel, drawn over whatever is on screen; nothing when no delete is running. */
void gameops_draw(void);

#endif
