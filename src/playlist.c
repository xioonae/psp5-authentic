/* psp5 authentic - playlists (.m3u and .m3u8), see playlist.h */
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "playlist.h"
#include "logs.h"

#define MAX_LINE  512 /* a longer line is left out */
#define MAX_PARTS 64  /* the folders of one path */

int playlist_is_name(const char *name)
{
    if (!name)
        return 0;
    size_t n = strlen(name);
    if (n >= 5 && !strcasecmp(name + n - 5, ".m3u8"))
        return 1;
    return n >= 4 && !strcasecmp(name + n - 4, ".m3u");
}

/* the name at the end of a path */
static const char *base_of(const char *path)
{
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* "ms0:" out of "ms0:/MUSIC/Mixes" (the device with its colon, in small letters); 0 when there is none */
static int device_of(const char *path, char *out, int size)
{
    const char *colon = strchr(path, ':');
    if (!colon || colon - path > 8 || colon == path || (int)(colon - path) + 2 > size)
        return 0;
    int n = (int)(colon - path) + 1;
    for (int i = 0; i < n; i++)
        out[i] = path[i] >= 'A' && path[i] <= 'Z' ? path[i] + 32 : path[i];
    out[n] = 0;
    return 1;
}

/* `dir` ("ms0:/MUSIC/Mixes", "ms0:/" or "ms0:") and `rel` put together, with . and .. taken out; it never goes above the top of the storage. 0 when it does not fit. */
static int resolve(char *out, int size, const char *dir, const char *rel)
{
    char dev[16], buf[PLAYLIST_PATH * 2];
    if (!device_of(dir, dev, sizeof(dev)))
        return 0;
    const char *colon = strchr(dir, ':');
    if (strlen(colon + 1) + strlen(rel) + 2 > sizeof(buf))
        return 0;
    snprintf(buf, sizeof(buf), "%s/%s", colon + 1, rel);
    char *part[MAX_PARTS], *save = NULL;
    int n = 0;
    for (char *p = strtok_r(buf, "/", &save); p; p = strtok_r(NULL, "/", &save)) {
        if (!strcmp(p, "."))
            continue;
        if (!strcmp(p, "..")) {
            if (n > 0)
                n--;
            continue;
        }
        if (n >= MAX_PARTS)
            return 0;
        part[n++] = p;
    }
    int len = snprintf(out, size, "%s", dev);
    if (len >= size)
        return 0;
    if (n == 0) /* the top of the storage */
        return snprintf(out, size, "%s/", dev) < size;
    for (int i = 0; i < n; i++) {
        len += snprintf(out + len, size - len, "/%s", part[i]);
        if (len >= size)
            return 0;
    }
    return 1;
}

/* a file name next to the playlist: what is left of a path that came from another computer */
static int next_to(char *out, int size, const char *dir, const char *path)
{
    const char *name = base_of(path);
    return name[0] && resolve(out, size, dir, name);
}

/* a web address ("http://...", "file:///..."): "://" before the first / */
static int is_url(const char *s)
{
    const char *p = strstr(s, "://"), *slash = strchr(s, '/');
    return p && (!slash || p < slash);
}

/* One line of the playlist as a whole path. 0 when the line is no path that can be used. */
static int one_path(const char *line, const char *dir, int (*exists)(const char *), char *out, int size)
{
    char s[MAX_LINE], top[16];
    snprintf(s, sizeof(s), "%s", line);
    for (char *p = s; *p; p++)
        if (*p == '\\')
            *p = '/';
    if (!strncasecmp(s, "ms0:", 4) || !strncasecmp(s, "ef0:", 4)) /* a whole path of the PSP */
        return resolve(out, size, s, "");
    if (is_url(s))
        return 0;
    if (((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= 'a' && s[0] <= 'z')) && s[1] == ':') /* a drive of a computer: only the file name is left */
        return next_to(out, size, dir, s + 2);
    if (s[0] == '/') { /* the top of this storage; or a path of another computer (a Mac, Linux, Android), then it is the name next to the playlist */
        if (!device_of(dir, top, sizeof(top)) || !resolve(out, size, top, s))
            return 0;
        if (exists && !exists(out))
            return next_to(out, size, dir, s);
        return 1;
    }
    return resolve(out, size, dir, s);
}

int playlist_parse(const char *text, size_t len, const char *playlist_dir, int (*song_ok)(const char *name), int (*exists)(const char *path), char ***paths)
{
    *paths = NULL;
    if (!text || !playlist_dir || !len)
        return 0;
    const unsigned char *u = (const unsigned char *)text;
    if (len >= 2 && ((u[0] == 0xFF && u[1] == 0xFE) || (u[0] == 0xFE && u[1] == 0xFF))) /* UTF-16: not read */
        return 0;
    if (len >= 3 && u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF) { /* a UTF-8 BOM */
        text += 3;
        len -= 3;
    }
    char **list = NULL;
    int n = 0, cap = 0;
    size_t i = 0;
    while (i < len && n < PLAYLIST_MAX_TRACKS) {
        size_t j = i;
        while (j < len && text[j] != '\n' && text[j] != '\r')
            j++;
        size_t ll = j - i;
        if (ll > 0 && ll < MAX_LINE) {
            char line[MAX_LINE];
            memcpy(line, text + i, ll);
            line[ll] = 0;
            char *a = line;
            while (*a == ' ' || *a == '\t')
                a++;
            char *z = a + strlen(a);
            while (z > a && (z[-1] == ' ' || z[-1] == '\t'))
                *--z = 0;
            char path[PLAYLIST_PATH];
            if (*a && *a != '#' && one_path(a, playlist_dir, exists, path, sizeof(path)) && (!song_ok || song_ok(base_of(path)))) {
                if (n >= cap) {
                    int bigger = cap ? cap * 2 : 64;
                    char **grown = realloc(list, (size_t)bigger * sizeof(*list));
                    if (!grown) {
                        playlist_free(list, n);
                        return -1;
                    }
                    list = grown;
                    cap = bigger;
                }
                list[n] = strdup(path);
                if (!list[n]) {
                    playlist_free(list, n);
                    return -1;
                }
                n++;
            }
        }
        i = j + 1; /* (a CR LF gives an empty line after the CR, which is left out) */
    }
    *paths = list;
    return n;
}

int playlist_load(const char *file, int (*song_ok)(const char *name), int (*exists)(const char *path), char ***paths)
{
    *paths = NULL;
    char dir[PLAYLIST_PATH];
    const char *slash = strrchr(file, '/'), *colon = strchr(file, ':');
    if (slash)
        snprintf(dir, sizeof(dir), "%.*s", (int)(slash - file), file); /* "ms0:/x.m3u8" gives "ms0:", the top of the storage */
    else if (colon)
        snprintf(dir, sizeof(dir), "%.*s", (int)(colon - file) + 1, file);
    else
        return -1;
    SceUID fd = sceIoOpen(file, PSP_O_RDONLY, 0);
    if (fd < 0) {
        logs_fail(LOG_MUSIC, "A playlist could not be opened (the PSP said 0x%08x).", (unsigned)fd);
        return -1;
    }
    int size = sceIoLseek32(fd, 0, PSP_SEEK_END);
    sceIoLseek32(fd, 0, PSP_SEEK_SET);
    if (size < 0)
        size = 0;
    if (size > PLAYLIST_MAX_FILE)
        size = PLAYLIST_MAX_FILE;
    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        sceIoClose(fd);
        return -1;
    }
    int got = 0;
    while (got < size) {
        int r = sceIoRead(fd, buf + got, size - got);
        if (r <= 0)
            break;
        got += r;
    }
    sceIoClose(fd);
    buf[got] = 0;
    int n = playlist_parse(buf, (size_t)got, dir, song_ok, exists, paths);
    free(buf);
    if (n == 0)
        logs_fail(LOG_MUSIC, "A playlist has no song that can be played (%d bytes of text).", got);
    return n;
}

void playlist_free(char **paths, int n)
{
    for (int i = 0; paths && i < n; i++)
        free(paths[i]);
    free(paths);
}
