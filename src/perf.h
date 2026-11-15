/* psp5 authentic – a record of where the time goes, for the maintainer's PSP: the steps of the start (and of every later start of a screen), and how long the frames of each screen take.
 *
 * Nothing is written unless the file PSP5/perf.txt is there already: the player makes an empty one, uses psp5 for a minute, leaves (or starts a game) and sends the file that has been written. The
 * numbers are kept in memory all the time (a few hundred bytes, a handful of instructions a frame). */
#ifndef PERF_H
#define PERF_H

enum { PF_GAMES, PF_MEDIA, PF_LIBRARY, PF_MUSIC, PF_SETTINGS, PF_INFO, PF_SAVES, PF_FILES, PF_FTP, PF_COUNT }; /* (the first four are the tabs of ui.h, TAB_GAMES ..., in their order) */

/* a point in time: a step of the start, the opening of a screen (the name is cut at 23 letters; at most 96 are kept) */
void perf_mark(const char *what);
/* what the screen is doing (bits of the caller's choosing), kept for the next perf_frame(): a slow frame is written down with it. Cleared by perf_frame(). */
void perf_state(unsigned bits);
/* one frame of a screen: seconds from the start of the frame before to the start of this one (the wait for the screen's refresh is in it: 0.0167 is the best there is) */
void perf_frame(int screen, float seconds);
/* writes dir/perf.txt when that file exists (see above), else does nothing */
void perf_dump(const char *dir);

/* How long a piece of work took (a picture decoded, a folder listed, a cover made): kept in a list of the last 64 and written to perf.txt after the frames. The two functions are weak so that a
 * program that does not have perf.c (the tests of single files on a PC) still links: the macros do nothing there. */
unsigned long long perf_clock(void) __attribute__((weak)); /* microseconds */
void perf_event(const char *what, unsigned a, unsigned b, unsigned long long start_us) __attribute__((weak)); /* `what` with two numbers (the report says what they are), that began at start_us and ends now */
/* 1 when dir/perf.txt is there (asked once): a measurement that costs work of its own (reading a file a second time to tell the reading from the decoding) is made only then */
int perf_wanted(const char *dir) __attribute__((weak));
#define PERF_START() (perf_clock ? perf_clock() : 0ull)
/* The same as PERF_END, written down only when the work took 40 ms or more: for what happens often and is mostly quick (the icons of the cards, the listing of a folder), so that it does not push
 * the rare long things out of the list of the last 128 (the third real file lost its seconds-long Settings frames that way). */
#define PERF_END_SLOW(what, a, b, t0) do { if (perf_event && perf_clock && perf_clock() - (t0) >= 40000ull) perf_event((what), (unsigned)(a), (unsigned)(b), (t0)); } while (0)
#define PERF_END(what, a, b, t0) do { if (perf_event) perf_event((what), (unsigned)(a), (unsigned)(b), (t0)); } while (0)

#endif
