/* psp5 authentic - playlists (.m3u and .m3u8): which songs a playlist file names, and where they are.
 *
 * A playlist is a text file with one song a line. Lines that begin with # (#EXTM3U, #EXTINF ...) and empty lines are left out. The paths that are
 * understood:
 *   Songs/a.mp3, ../a.mp3, ./a.mp3    relative to the folder of the playlist file
 *   ms0:/MUSIC/a.mp3, ef0:/MUSIC/a.mp3  the whole path
 *   /MUSIC/a.mp3                      the top of the storage the playlist is on (when that file is not there, the file name next to the playlist is tried)
 *   C:\Music\a.mp3                    a path from a computer: the file name next to the playlist
 * (\ is read as /; web addresses are left out, and so is a name that is not a song the player can play). The text is UTF-8, as the PSP's own file names are
 * (a BOM at the start is ignored; a file in UTF-16 has no songs). Nothing is checked on the card, except for the paths that begin with a single /. */
#ifndef PLAYLIST_H
#define PLAYLIST_H

#include <stddef.h>

#define PLAYLIST_MAX_TRACKS 1000          /* the same as the lines of a folder in the Music screen (MAX_ENTRIES in music.c) */
#define PLAYLIST_MAX_FILE   (256 * 1024)  /* a bigger file is read as far as this */
#define PLAYLIST_PATH       256           /* the longest path, with its end 0 (Track.path in music.h) */

/* 1 if the name (or a path) ends in .m3u or .m3u8, in any capitals */
int playlist_is_name(const char *name);

/* The songs that the text of a playlist names, as whole paths: a new array of new strings (playlist_free) in *paths (NULL when there are none), and how many (0 to
 * PLAYLIST_MAX_TRACKS). `playlist_dir` is the folder the playlist file is in ("ms0:/MUSIC/Mixes"; "ms0:" for the top of a storage). `song_ok(name)` says whether a file name is a
 * song to keep (NULL: all are); `exists(path)` says whether a file is there (NULL: it is not asked, and a path that begins with a single / is taken as it is). Returns -1 when
 * there is no memory. */
int playlist_parse(const char *text, size_t len, const char *playlist_dir, int (*song_ok)(const char *name), int (*exists)(const char *path), char ***paths);

/* Reads the playlist file `file` (as far as PLAYLIST_MAX_FILE) and parses it. Returns the number of songs (0 for none), or -1 when the file cannot be read. */
int playlist_load(const char *file, int (*song_ok)(const char *name), int (*exists)(const char *path), char ***paths);

/* lets the result of playlist_parse / playlist_load go */
void playlist_free(char **paths, int n);

#endif
