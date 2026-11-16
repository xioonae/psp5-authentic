/* psp5 authentic – the Videos and Images screens: which folders and which files they list (the lists are made by the code of the Music screen, musicui.c) */
#ifndef MEDIA_H
#define MEDIA_H

#include "music.h"

/* what a list screen is for (MusicUi.kind) */
enum { MK_MUSIC, MK_VIDEO, MK_IMAGES };

/* A video the PSP can play: MP4 (H.264 video, AAC sound; .mp4 and .m4v) and PMF / PSMF (the format of the PSP's own video; .pmf and .psmf) */
int media_is_video(const char *name);
/* An image psp5 shows: JPEG, PNG and BMP */
int media_is_image(const char *name);
/* The same by kind (MK_VIDEO / MK_IMAGES; MK_MUSIC is music_supported) */
int media_supported(int kind, const char *name);

/* The folders of a kind that can be opened, in the order they are shown (at most 4):
 *   MK_MUSIC   ms0:/MUSIC, ef0:/MUSIC
 *   MK_VIDEO   ms0:/VIDEO, ef0:/VIDEO
 *   MK_IMAGES  ms0:/PICTURE, ms0:/PSP/PHOTO, ef0:/PICTURE, ef0:/PSP/PHOTO
 * (ef0: is left out when ARK has put the internal storage at ms0:, as for music). Returns how many. */
int media_roots(int kind, char roots[4][16]);

/* lists a folder of that kind: the folders first, then the files of the kind, by name (music_list_dir_filter) */
int media_list_dir(int kind, const char *dir, MusicEntry **out);

#endif
