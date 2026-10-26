/* psp5-authentic - the time a game is played while the PSP is awake.
 * Shared by psp5 and by the helper plugin (plugin/psp5time.c, a kernel module that runs inside the game), so it has no system calls:
 * only counting, the names of the games and the text of the files that the plugin keeps and psp5 reads.
 *
 * The plugin finds out which game it is in by itself (so a game started from the XMB counts too) and keeps the seconds of each game in
 * <data dir>/played/<key>.txt, where <key> is the name psp5 uses for the game in playtime.txt (entry_cover_key() in scan.c: the folder of
 * a homebrew or PS1 game, the disc id of a PSP game). At its next start psp5 adds the seconds to playtime.txt and removes the file.
 *
 * The file has the same length for every game and every number of seconds, so the plugin can write it again in place:
 *     psp5-played 1\n
 *     secs <10 digits>\n     the seconds this game has been played since psp5 last added them, with leading zeros
 *     end\n                  a file without this line was cut off and is not used
 */
#ifndef AWAKE_H
#define AWAKE_H

#define AWAKE_FILE_MAX    64
#define AWAKE_KEY_MAX     80               /* a game's name is at most 79 bytes, as in psp5 (KEY_LEN) */
#define AWAKE_GAME_ID_MAX 16               /* a disc id is at most 15 bytes, as in psp5 (Entry.game_id) */
#define AWAKE_PLAYED_DIR  "played"
#define AWAKE_PLAYED_EXT  ".txt"
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

/* Writes the file for `secs` into `out`; returns its length, 0 when it does not fit. The length is the same for every `secs`. */
int awake_format(char *out, int size, unsigned secs);

/* Reads such a file: 1 = it is whole and right (the seconds are returned), 0 = not. */
int awake_parse(const char *buf, int len, unsigned *secs);

/* 1 when `init_file` (the file the PSP started, sceKernelInitFileName()) is psp5 itself and not a game: ARK's launcher (a file called
 * VBOOT.PBP) or psp5's own EBOOT.PBP (in a folder called PSP5). The plugin is loaded for those too ("game" also means homebrew), and must stay out
 * of them: they are not games, and psp5 is started again right after a game, which would count the time psp5 is open as a game. */
int awake_is_psp5(const char *init_file);

/* Which game is this? From the file the PSP started (sceKernelInitFileName()):
 *   1 = a homebrew, PSN or PS1 game (an EBOOT.PBP in a folder): its name is the folder, as psp5 names it (entry_cover_key), returned in `key`
 *   2 = a PSP game on a disc or in an ISO (disc0:/..., an ISO file, a PBOOT.PBP): its name is the disc id, which is in the game's
 *       PARAM.SFO (awake_key_from_sfo); `key` is not set
 *   0 = not a game to count (another .PBP such as ARK's menu, or no name) */
int awake_key_from_path(const char *init_file, char *key, int size);

/* The disc id (DISC_ID) of a PARAM.SFO (the `len` bytes read from disc0:/PSP_GAME/PARAM.SFO) as `key` (at most size - 1 bytes).
 * 1 = found; 0 = not, or a name that cannot be a file name (a path character). */
int awake_key_from_sfo(const unsigned char *sfo, unsigned len, char *key, int size);

/* The name of a PSP game that has no disc id (its PARAM.SFO has none, or there is no PARAM.SFO), as psp5 names such a game in playtime.txt
 * (entry_cover_key() in scan.c): the name of its ISO file without the extension, "ms0:/ISO/Foo.iso" -> "Foo". An ISO-type file is .iso, .cso, .zso, .dax or .jso.
 * `init_file` is what the PSP started (sceKernelInitFileName(); it is the ISO file itself when ARK says so, else something like disc0:/...);
 * `umd_file` is the ISO that ARK has mounted (sctrlSEGetUmdFile()), or NULL / "" when not known. The ISO file is taken from `init_file` first, then
 * from `umd_file`; a .PBP (PBOOT.PBP, an installed game) is not an ISO and gets no name from `umd_file`.
 * 1 = found (in `key`, at most size - 1 bytes); 0 = no name. */
int awake_key_from_files(const char *init_file, const char *umd_file, char *key, int size);

/* The folder of the plugin's own file ("ms0:/PSP5/psp5time.prx" -> "ms0:/PSP5"): that is psp5's data folder. 1 = ok */
int awake_dir_of(const char *module_path, char *out, int size);

/* "<dir>/played" and "<dir>/played/<key>.txt". 1 = ok, 0 = does not fit */
int awake_played_dir(const char *dir, char *out, int size);
int awake_played_path(const char *dir, const char *key, char *out, int size);

/* The game of a file in the played folder ("Foo.txt" -> "Foo"): 1 = the name ends in .txt and the rest fits as a name */
int awake_file_key(const char *file_name, char *key, int size);

#endif
