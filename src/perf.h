/* psp5 authentic – a record of where the time goes, for the maintainer's PSP: the steps of the start (and of every later start of a screen), and how long the frames of each screen take.
 *
 * Nothing is written unless the file PSP5/perf.txt is there already: the player makes an empty one, uses psp5 for a minute, leaves (or starts a game) and sends the file that has been written. The
 * numbers are kept in memory all the time (a few hundred bytes, a handful of instructions a frame). */
#ifndef PERF_H
#define PERF_H

enum { PF_GAMES, PF_MEDIA, PF_LIBRARY, PF_MUSIC, PF_SETTINGS, PF_INFO, PF_SAVES, PF_FILES, PF_FTP, PF_COUNT }; /* (the first four are the tabs of ui.h, TAB_GAMES ..., in their order) */

/* a point in time: a step of the start, the opening of a screen (the name is cut at 23 letters; at most 96 are kept) */
void perf_mark(const char *what);
/* one frame of a screen: seconds from the start of the frame before to the start of this one (the wait for the screen's refresh is in it: 0.0167 is the best there is) */
void perf_frame(int screen, float seconds);
/* writes dir/perf.txt when that file exists (see above), else does nothing */
void perf_dump(const char *dir);

#endif
