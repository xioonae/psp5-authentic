/* psp5 – made by Mille Weber */
#ifndef PLAYTIME_H
#define PLAYTIME_H

#include "scan.h"

/* How long each game has been played, shown as a small clock with "54h" or "45min" in the Games tab and the Library.
 *
 * psp5 is not running while a game is: it only knows when the game was started and when psp5 is started again. That is exact when psp5 is ARK's
 * launcher, because ARK then starts psp5 again the moment the game ends. (Started from the XMB as a normal app, the PSP goes back to the XMB after
 * the game and psp5 is not started again by itself, so nothing is counted then: it would only be a guess.)
 *
 * The time is kept in <data dir>/playtime.txt, one line per game: the seconds, a space and the game's key (entry_cover_key()). Before a game
 * starts, a marker file (the same one that makes psp5 skip its start-up screen after a game) gets the key and two clocks: the system time since
 * the PSP was started and the real-time clock. When psp5 is started and finds the marker, the time that has gone is added to the game. The
 * smaller of the two clocks is used (the system time may stand still while the PSP sleeps, the real-time clock does not), unless the system clock says
 * less than 10 seconds (starting a game restarts the PSP's software and may start that clock from zero again), and a session of more than a day,
 * or of under 10 seconds, or one that a restart of the PSP has made impossible to measure, is not counted. */

#define PLAYTIME_MIN_SECS 10          /* a shorter session is a game that did not really start */
#define PLAYTIME_MAX_SECS (24 * 3600) /* a longer one is a marker that was left behind */

/* Reads <dir>/playtime.txt (no file = nobody has played yet). Calls before the first one are not needed; call it again to forget what was read. */
void playtime_load(const char *dir);

/* The seconds the game has been played, 0 when nothing is known. */
unsigned playtime_seconds(const Entry *e);

/* Writes the marker file at `path` before the game `e` is started. sys_us / rtc_us: the two clocks now, in microseconds.
 * 1 = written, 0 = the file could not be written. */
int playtime_mark_write(const char *path, const Entry *e, unsigned long long sys_us, unsigned long long rtc_us);

/* At the start of psp5: if the marker file exists it is removed, the time that has gone since it was written is added to its game (and
 * saved) and 1 is returned; 0 when there was no marker. A marker that is empty or damaged is removed and counted as "a game just ended". */
int playtime_mark_take(const char *path, unsigned long long sys_us, unsigned long long rtc_us);

/* The same, and `report` (if it is not NULL) gets one line that says what happened and why: "ULUS10041 played 612 s by the real-time clock:
 * counted, 3725 s in all", "... too short, nothing counted", "no marker: psp5 was not started again by a game" ... for playtime.log. */
int playtime_mark_take_ex(const char *path, unsigned long long sys_us, unsigned long long rtc_us, char *report, int report_size);

/* A short message (under 64 bytes) about the last playtime_mark_take(): "Counted +12min, 3h in all", "Not counted: only 4 s", "No marker: not started by a game" ... */
const char *playtime_last_short(void);

/* Adds one line "<when>  <msg>" to <data dir>/playtime.log (at most 8000 bytes, the oldest lines go first), so that it can be seen afterwards
 * what happened at the start of a game and at the next start of psp5. */
void playtime_log(const char *when, const char *msg);

/* The text of a time for the small clock: the minutes ("45min") under an hour, else the whole hours ("54h"), never both; the units are
 * in the language of the launcher. "<1min" when something has been played but not a minute yet; empty only when nothing has. */
void playtime_text(unsigned secs, char *out, int size);

#endif
