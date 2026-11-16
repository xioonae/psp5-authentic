/* psp5 authentic – the Videos and Images screens: which folders and which files they list (see media.h) */
#include <pspkernel.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "media.h"
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

int media_supported(int kind, const char *name)
{
    return kind == MK_VIDEO ? media_is_video(name) : kind == MK_IMAGES ? media_is_image(name) : music_supported(name);
}

int media_roots(int kind, char roots[4][16])
{
    if (kind == MK_MUSIC) {
        char two[2][16];
        int n = music_roots(two);
        for (int i = 0; i < n; i++)
            snprintf(roots[i], 16, "%s", two[i]);
        return n;
    }
    static const char *const VIDEO[] = { "ms0:/VIDEO", "ef0:/VIDEO" };
    static const char *const IMAGES[] = { "ms0:/PICTURE", "ms0:/PSP/PHOTO", "ef0:/PICTURE", "ef0:/PSP/PHOTO" };
    const char *const *list = kind == MK_VIDEO ? VIDEO : IMAGES;
    int count = kind == MK_VIDEO ? 2 : 4, n = 0;
    for (int i = 0; i < count; i++) {
        if (!strncmp(list[i], "ef0:", 4) && sys_ms_is_ef())
            continue; /* ARK has put the internal storage at ms0: it is the same folder */
        SceUID d = sceIoDopen(list[i]);
        if (d < 0)
            continue;
        sceIoDclose(d);
        snprintf(roots[n++], 16, "%s", list[i]);
    }
    return n;
}

static int ok_video(const char *name) { return media_is_video(name); }
static int ok_image(const char *name) { return media_is_image(name); }

int media_list_dir(int kind, const char *dir, MusicEntry **out)
{
    if (kind == MK_MUSIC)
        return music_list_dir(dir, out);
    return music_list_dir_filter(dir, kind == MK_VIDEO ? ok_video : ok_image, out);
}
