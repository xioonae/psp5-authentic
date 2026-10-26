/* psp5-authentic - the helper plugin that counts the time played (plugin/psp5time.c).
 * The plugin file is carried inside psp5 (plugin/psp5time.prx, turned into psp5time_prx.h by the Makefile). helper_setup() writes it to
 * <data dir>/psp5time.prx and puts "game, <path>, on" and "pops, <path>, on" in PLUGINS.TXT. Nothing else in PLUGINS.TXT is touched. */
#ifndef HELPER_H
#define HELPER_H

#define HELPER_NAME "psp5time.prx"

/* At the start of psp5, with ARK only (no ARK, no PLUGINS.TXT):
 *  - the plugin file is written when it is missing or is not the one carried inside this psp5 (psp5 was updated);
 *  - when PLUGINS.TXT has no line for it at all (the first start, or both lines were deleted), the two lines are added, on. Lines that are there are
 *    never changed: turn the plugin off in Settings > Plugins and it stays off.
 * When the file or PLUGINS.TXT cannot be written nothing is left half done, and it is tried again at the next start. */
void helper_setup(void);

#endif
