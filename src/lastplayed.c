/* psp5-authentic – when each game was played last (see lastplayed.h) */
#include <pspkernel.h>
#include <psprtc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lastplayed.h"

#define KEY_LEN   80   /* as home.c and playtime.c: entry_cover_key() callers have always used 80 */
#define MAX_GAMES 4096 /* (a line is at most about 100 bytes: a file of at most 410 kB) */
#define MAX_FILE  (MAX_GAMES * (KEY_LEN + 16))

typedef struct {
    char key[KEY_LEN];
    u32 time;
} Game;

static Game *s_games;
static int s_n, s_cap;
static char s_dir[16];

static void file_path(char *out, int size)
{
    snprintf(out, size, "%s/lastplayed.txt", s_dir);
}

static Game *find(const char *key)
{
    for (int i = 0; i < s_n; i++)
        if (!strcmp(s_games[i].key, key))
            return &s_games[i];
    return NULL;
}

/* 1 = the whole file was written */
static int save(void)
{
    char path[48];
    file_path(path, sizeof(path));
    char *text = malloc((size_t)s_n * (KEY_LEN + 16) + 1);
    if (!text)
        return 0;
    int n = 0, ok = 0;
    for (int i = 0; i < s_n; i++)
        n += snprintf(text + n, KEY_LEN + 16, "%u %s\n", (unsigned)s_games[i].time, s_games[i].key);
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0) {
        ok = sceIoWrite(fd, text, n) == n;
        sceIoClose(fd);
    }
    free(text);
    return ok;
}

/* the time of a game: a new line if it has none yet (nothing if the table is full). 1 = it changed */
static int set(const char *key, u32 time)
{
    Game *g = find(key);
    if (!g) {
        if (s_n >= MAX_GAMES)
            return 0;
        if (s_n == s_cap) {
            int cap = s_cap ? s_cap * 2 : 64;
            Game *n = realloc(s_games, (size_t)cap * sizeof(Game));
            if (!n)
                return 0;
            s_games = n;
            s_cap = cap;
        }
        g = &s_games[s_n++];
        snprintf(g->key, sizeof(g->key), "%s", key);
        g->time = 0;
    }
    if (time <= g->time)
        return 0;
    g->time = time;
    return 1;
}

void lastplayed_load(const char *dir)
{
    s_n = 0;
    snprintf(s_dir, sizeof(s_dir), "%s", dir);
    char path[48];
    file_path(path, sizeof(path));
    SceIoStat st;
    if (sceIoGetstat(path, &st) < 0 || st.st_size <= 0 || st.st_size > MAX_FILE)
        return;
    char *text = malloc((size_t)st.st_size + 1);
    SceUID fd = text ? sceIoOpen(path, PSP_O_RDONLY, 0) : -1;
    if (fd < 0) {
        free(text);
        return;
    }
    int got = sceIoRead(fd, text, (unsigned)st.st_size);
    sceIoClose(fd);
    text[got > 0 ? got : 0] = 0;
    for (char *line = text; *line;) {
        char *end = strchr(line, '\n');
        if (!end)
            break; /* a last line without its end was cut off: ignore it */
        *end = 0;
        if (end > line && end[-1] == '\r')
            end[-1] = 0;
        char *space = strchr(line, ' ');
        if (space && space > line && space[1] && strspn(line, "0123456789") == (size_t)(space - line) && space - line <= 10) {
            unsigned long long t = strtoull(line, NULL, 10);
            *space = 0;
            if (t > 0 && t <= 0xFFFFFFFFull)
                set(space + 1, (u32)t);
        }
        line = end + 1;
    }
    free(text);
}

u32 lastplayed_pack(const ScePspDateTime *t)
{
    if (t->year < 1980 || t->year > 1980 + 63) /* (the year has six bits in a packed time) */
        return 0;
    return scan_pack_time(t);
}

u32 lastplayed_now(void)
{
    ScePspDateTime now;
    if (sceRtcGetCurrentClockLocalTime(&now) < 0)
        return 0;
    return lastplayed_pack(&now);
}

u32 lastplayed_time(const Entry *e)
{
    char key[KEY_LEN];
    if (!s_n || e->type == ENTRY_UMD)
        return 0;
    entry_cover_key(e, key, sizeof(key));
    const Game *g = find(key);
    return g ? g->time : 0;
}

void lastplayed_note(const char *key, u32 time)
{
    if (!time || !key[0] || strlen(key) >= KEY_LEN)
        return;
    if (set(key, time))
        save();
}

void lastplayed_note_launch(const Entry *e)
{
    char key[KEY_LEN];
    if (e->type == ENTRY_UMD)
        return;
    entry_cover_key(e, key, sizeof(key));
    lastplayed_note(key, lastplayed_now());
}
