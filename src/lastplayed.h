/* psp5-authentic – when each game was played last */
#ifndef LASTPLAYED_H
#define LASTPLAYED_H

#include <psptypes.h>
#include <psprtc.h>
#include "scan.h"

/* When each game and app was played last: shown on the Information screen, and what the Library's "Recently played" sorting uses for the games that
 * are not on Home's list of the cards opened last (ui.c, played_key()).
 *
 * A time is a packed local time, the same kind as Entry.file_time (scan_pack_time(): year since 1980, month, day, hour, minute, second in
 * one number that grows with time). 0 means "not known".
 *
 * Two things write them: psp5 itself when it starts a game (lastplayed_note_launch), and the time of the file that the helper plugin
 * keeps for a game (<data dir>/played/<key>.txt, written again every few seconds while the game runs) when psp5 adds that file to the
 * played times at its next start (playtime.c): that is how a game started from the XMB gets its time. A game is named as in playtime.txt
 * (entry_cover_key()). The times are kept in <data dir>/lastplayed.txt, one line per game: the time, a space and the key. */

/* Reads <dir>/lastplayed.txt (no file = nothing is known). Call it before playtime_load(). */
void lastplayed_load(const char *dir);

/* A date and time as such a time; 0 when it is not one that fits (before 1980 or after 2043). */
u32 lastplayed_pack(const ScePspDateTime *t);

/* The PSP's clock as a packed local time; 0 when it cannot be read or is not a date that fits. */
u32 lastplayed_now(void);

/* The last time the game was played, 0 when it is not known (never, or before psp5 knew). The disc has none. */
u32 lastplayed_time(const Entry *e);

/* The game with this key was played at `time`. Only a later time than the one known is kept; the file is written at once when something
 * changed. A time of 0 is ignored. */
void lastplayed_note(const char *key, u32 time);

/* psp5 starts the game: it was played now (the disc is not kept). */
void lastplayed_note_launch(const Entry *e);

#endif
