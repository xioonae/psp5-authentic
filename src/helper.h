/* psp5-authentic - the helper plugin that makes the time played exact (plugin/psp5time.c).
 * The plugin file is carried inside psp5 (plugin/psp5time.prx, turned into psp5time_prx.h by the Makefile). "Exact time played" in
 * Settings > psp5 writes it to <data dir>/psp5time.prx and puts "game, <path>, on" and "pops, <path>, on" in PLUGINS.TXT; turning it off
 * sets those two lines to off. Nothing else in PLUGINS.TXT is touched. */
#ifndef HELPER_H
#define HELPER_H

#define HELPER_NAME "psp5time.prx"

/* Is it on: the file is in the data folder and both lines are on in PLUGINS.TXT (read by plugins_load() before). */
int helper_on(void);

/* Turns it on (writes the file when it is missing or different, adds / turns on the two lines) or off (turns the lines off).
 * 1 = done; 0 = the file or PLUGINS.TXT could not be written (PLUGINS.TXT is then left as it was). Call plugins_load() afterwards. */
int helper_set(int on);

/* At the start of psp5: a plugin file that is there but is not the one carried inside this psp5 (psp5 was updated) is replaced. */
void helper_sync(void);

#endif
