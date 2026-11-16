/* psp5 – made by Mille Weber */
#ifndef PLAYTIME_H
#define PLAYTIME_H

#include "scan.h"

/* How long each game has been played, shown as a small clock with "54h" or "45min" in the Games tab and the Library.
 *
 * psp5 is not running while a game is, so it does not measure anything itself. The helper plugin (plugin/psp5time.c, awake.h; psp5 installs it by
 * itself, see helper.h) runs inside every game, whoever started it, counts only the seconds the PSP is awake, and keeps them in
 * <data dir>/played/<key>.txt, where <key> is the name psp5 uses for the game (entry_cover_key()). playtime_load() adds those seconds to the
 * game at the next start of psp5 and removes the files.
 *
 * The time is kept in <data dir>/playtime.txt, one line per game: the seconds, a space and the game's key. A game that the plugin cannot name the way
 * psp5 does (an ISO without a disc ID), or one that ran while the plugin was off, has no time. */

/* psp5's own EBOOT.PBP (argv[0]). The helper plugin does not count psp5 (it knows it by the folder name PSP5, and VBOOT.PBP as the launcher), but
 * when psp5 has been put in a folder with another name, it would count the time psp5 is open as a game: playtime_load() then drops that file.
 * Call it before playtime_load(). NULL, or a path that is not an EBOOT.PBP in a folder, clears it. */
void playtime_set_self(const char *path);

/* Reads <dir>/playtime.txt (no file = nobody has played yet) and adds the time that the helper plugin kept in <dir>/played (saved at once, then the
 * files are removed). Call it again to forget what was read. */
void playtime_load(const char *dir);

/* The games that the plugin saw played since psp5 was last open (the files that playtime_load() added), oldest first, each by its name
 * (entry_cover_key()): how many, the name of the one at `i` (0 up; "" past the last), and "taken" when Home has put them first (they are listed only once).
 * The time of each is also kept in lastplayed.c. */
int playtime_plays(void);
const char *playtime_play_key(int i);
void playtime_plays_taken(void);

/* The seconds the game has been played, 0 when nothing is known. */
unsigned playtime_seconds(const Entry *e);

/* The text of a time for the small clock: the minutes ("45min") under an hour, else the whole hours ("54h"), never both; the units are
 * in the language of the launcher. "<1min" when something has been played but not a minute yet; empty only when nothing has. */
void playtime_text(unsigned secs, char *out, int size);

#endif
