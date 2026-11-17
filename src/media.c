/* psp5 authentic – the Videos and Images screens: which folders and which files they list (see media.h) */
#include <pspkernel.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "media.h"
#include "playlist.h"
#include "system.h"

static int has_ext(const char *name, const char *const *list)
{
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name)
        return 0;
    for (; *list; list++)
        if (!strcasecmp(dot + 1, *list))
            return 1;
    return 0;
}

int media_is_video(const char *name)
{
    static const char *const E[] = { "mp4", "m4v", "pmf", "psmf", NULL };
    return has_ext(name, E);
}

int media_is_image(const char *name)
{
    static const char *const E[] = { "jpg", "jpeg", "png", "bmp", NULL };
    return has_ext(name, E);
}

int media_is_song(const char *name)
{
    return music_supported(name) && !media_is_video(name);
}

int media_supported(int kind, const char *name)
{
    return kind == MK_VIDEO ? media_is_video(name) : kind == MK_IMAGES ? media_is_image(name) : music_supported(name);
}

static int has_dir(const char *path)
{
    SceUID d = sceIoDopen(path);
    if (d < 0)
        return 0;
    sceIoDclose(d);
    return 1;
}

int media_roots(int kind, char roots[4][MEDIA_ROOT_LEN])
{
    char base[4][MEDIA_ROOT_LEN]; /* the folders of this kind: MUSIC or VIDEO (and, for the pictures, PICTURE and PSP/PHOTO) */
    int nb = 0;
    if (kind == MK_MUSIC) {
        char two[2][16];
        nb = music_roots(two);
        for (int i = 0; i < nb; i++)
            snprintf(base[i], MEDIA_ROOT_LEN, "%s", two[i]);
    } else {
        static const char *const VIDEO[] = { "ms0:/VIDEO", "ef0:/VIDEO" };
        static const char *const IMAGES[] = { "ms0:/PICTURE", "ms0:/PSP/PHOTO", "ef0:/PICTURE", "ef0:/PSP/PHOTO" };
        const char *const *list = kind == MK_VIDEO ? VIDEO : IMAGES;
        int count = kind == MK_VIDEO ? 2 : 4;
        for (int i = 0; i < count; i++) {
            if (!strncmp(list[i], "ef0:", 4) && sys_ms_is_ef())
                continue; /* ARK has put the internal storage at ms0: it is the same folder */
            if (has_dir(list[i]))
                snprintf(base[nb++], MEDIA_ROOT_LEN, "%s", list[i]);
        }
    }
    if (kind == MK_IMAGES) {
        for (int i = 0; i < nb; i++)
            snprintf(roots[i], MEDIA_ROOT_LEN, "%s", base[i]);
        return nb;
    }
    /* Music and Videos: the PSP/PLAYLIST folder of a storage, if it has one (the PSP's own place for playlists), comes after the folders of that storage, so that the choice is
     * "Memory Stick: MUSIC or PLAYLIST", then "System Storage: ...". Without such a folder nothing changes. */
    static const char *const STORAGE[2] = { "ms0:", "ef0:" }, *const PLAYLIST[2] = { "ms0:/PSP/PLAYLIST", "ef0:/PSP/PLAYLIST" };
    int n = 0;
    for (int s = 0; s < 2; s++) {
        for (int i = 0; i < nb; i++)
            if (!strncmp(base[i], STORAGE[s], 4))
                snprintf(roots[n++], MEDIA_ROOT_LEN, "%s", base[i]);
        if (s == 1 && sys_ms_is_ef())
            continue; /* (the same folder as the one of ms0:) */
        if (has_dir(PLAYLIST[s]))
            snprintf(roots[n++], MEDIA_ROOT_LEN, "%s", PLAYLIST[s]);
    }
    return n;
}

static int ok_video(const char *name) { return media_is_video(name) || playlist_is_name(name); } /* (a playlist opens like a folder: its videos are the lines) */
static int ok_image(const char *name) { return media_is_image(name); }

int media_list_dir(int kind, const char *dir, MusicEntry **out)
{
    if (kind == MK_MUSIC)
        return music_list_dir(dir, out);
    return music_list_dir_filter(dir, kind == MK_VIDEO ? ok_video : ok_image, out);
}
