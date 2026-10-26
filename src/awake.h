/* psp5-authentic - the time a game is played while the PSP is awake.
 * Shared by psp5 and by the helper plugin (plugin/psp5time.c, a kernel module that runs inside the game), so it has no system calls:
 * only counting and the text of the file that the plugin keeps and psp5 reads.
 *
 * <marker path>.awake (ms0:/PSP5/ingame.awake), always the same length for a game, so the plugin can write it again in place:
 *     psp5-awake 1\n
 *     rtc <number>\n         the number of the "rtc" line of the marker (the start of this game): it says which game the time belongs to
 *     awake <10 digits>\n    the seconds, with leading zeros
 *     end\n                  a file without this line was cut off and is not used
 */
#ifndef AWAKE_H
#define AWAKE_H

#define AWAKE_TOKEN_MAX 24                 /* the longest number (20 digits) and the end of the text */
#define AWAKE_FILE_MAX  (AWAKE_TOKEN_MAX + 64)
#define AWAKE_MAX_STEP_US 5000000ULL       /* a step between two ticks longer than this was not played: sleep, or the PSP was stopped */

/* (No 64-bit division anywhere: the plugin is linked without libgcc.) */
typedef struct {
    unsigned long long last_us;  /* the time of the last tick */
    unsigned secs;               /* the time counted so far: whole seconds ... */
    unsigned rem_us;             /* ... and what is left of the next one (under 1 000 000) */
} AwakeClock;

/* Starts counting at `now_us`, with `secs0` seconds already counted (a game that was started again keeps what it had). */
void awake_start(AwakeClock *c, unsigned long long now_us, unsigned secs0);

/* Adds seconds that were counted before (the file of a game that was started again). */
void awake_add_secs(AwakeClock *c, unsigned secs);

/* Call about once a second. Adds the time since the last call when it is no longer than AWAKE_MAX_STEP_US; a longer step (the PSP
 * slept, or was switched off and on) and a clock that went back add nothing. Returns the whole seconds counted so far. */
unsigned awake_tick(AwakeClock *c, unsigned long long now_us);

/* The number of the line "rtc N" of a marker (the text of the file, `len` bytes) as digits without leading zeros in `token`.
 * 1 = found, 0 = no (or not a number of 1 to 20 digits). */
int awake_token(const char *marker, int len, char *token, int size);

/* Writes the file for `token` and `secs` into `out`; returns its length, 0 when it does not fit. The length is the same for every `secs`. */
int awake_format(char *out, int size, const char *token, unsigned secs);

/* Reads such a file: 1 = it is whole and right (the token and the seconds are returned), 0 = not. */
int awake_parse(const char *buf, int len, char *token, int size, unsigned *secs);

/* 1 when `init_file` (the file the PSP started, sceKernelInitFileName()) is psp5 itself and not a game: ARK's launcher (a file called
 * VBOOT.PBP) or psp5's own EBOOT.PBP (in a folder called PSP5). The plugin is loaded for those too ("game" also means homebrew), and must stay out
 * of them: the marker that is still there then belongs to the game that has just ended. */
int awake_is_psp5(const char *init_file);

#endif
