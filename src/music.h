/* psp5 – made by Mille Weber */
#ifndef MUSIC_H
#define MUSIC_H

#include <psptypes.h>

/* The music player. The songs are in the MUSIC folder of the Memory Stick (ms0:/MUSIC) and of the internal storage of a PSP Go (ef0:/MUSIC). The Music screen
 * lists a folder at a time (music_list_dir: the folders first, then the songs it can play) and plays the songs of the folder that is open: a song is chosen,
 * and the songs of its folder are what plays one after the other (music_play_folder), in order or shuffled, once, one song over and over, or the whole folder
 * over and over. The sound is made by the PSP's own decoders, in a thread of its own, so the music goes on while psp5 is used (and when the Music screen is left). */

/* The song that is open: its tags (those that the file has; the title is the file's name without its ending when there is no tag) */
typedef struct {
    char path[256];
    char title[96];
    char artist[64];
    char album[64];
} Track;

/* One line of a folder in the Music screen: the name of a folder or of a song file */
typedef struct {
    char name[128];
    int dir;
} MusicEntry;

enum { MUSIC_STOPPED, MUSIC_PLAYING, MUSIC_PAUSED };
enum { MUSIC_REPEAT_OFF, MUSIC_REPEAT_ONE, MUSIC_REPEAT_ALL };

/* Loads the decoder modules and starts the player thread. */
void music_init(void);
/* Stops the music and the thread and lets everything go. */
void music_term(void);

/* 1 if the file (a name or a path) is a kind of song that can be played, by its ending */
int music_supported(const char *name);

/* The MUSIC folders that there are: "ms0:/MUSIC" and/or "ef0:/MUSIC" (not ef0 twice when ARK has put the internal storage at ms0:). Returns how many (0 to 2). */
int music_roots(char roots[2][16]);

/* The folder `dir`: the folders first, then the songs that can be played, each group by name (capitals and small letters alike). At most 1000 lines.
 * *out is a new array (free() it), the result is its length, or -1 when the folder cannot be read. */
int music_list_dir(const char *dir, MusicEntry **out);

/* The cover picture in the song's tag (a JPEG or PNG: free() the data), or NULL. Reads the file: not to be called from the player thread. */
void *music_cover_data(const char *path, u32 *size);

/* Plays the songs of the folder `dir` from entries[index] on (entries are the lines of music_list_dir; the songs among them are what plays). */
void music_play_folder(const char *dir, const MusicEntry *entries, int n, int index);

void music_toggle_pause(void);
void music_stop(void);
void music_next(void);
void music_prev(void);        /* back to the beginning when more than 3 s have been played, else the song before */
void music_seek(float sec);   /* jumps in the song (a paused song stays paused); past the end: the next song */

/* Shuffle (on = 1): the songs of the folder come in a random order, each once; off puts them in order again from the song that is playing. */
void music_set_shuffle(int on);
int music_shuffle(void);
void music_set_repeat(int mode); /* MUSIC_REPEAT_* */
int music_repeat(void);

int music_state(void);        /* MUSIC_STOPPED / _PLAYING / _PAUSED */
float music_elapsed(void);    /* seconds */
float music_duration(void);   /* seconds (estimated for some files), 0 = not known */

/* The song that is open now: 1 and its tags in *out, or 0 when none is (stopped). The tags are read when a song starts. */
int music_track(Track *out);
/* Changes every time another song starts (also the same song again): the screens keep what they made from the tags until it changes. */
int music_generation(void);
/* 1 if a tag has letters that the Latin font does not have (the Japanese font is then needed) */
int music_needs_cjk(void);
/* The folder whose songs are playing ("" when none was started) and the place of the song in it: 1 = the one that is playing is this file of that folder */
const char *music_folder(void);
int music_is_playing_file(const char *dir, const char *name);

#endif
