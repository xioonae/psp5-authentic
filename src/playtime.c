/* psp5 – made by Mille Weber */
/* How long each game has been played (see playtime.h). */
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "playtime.h"
#include "awake.h"
#include "lastplayed.h"
#include "lang.h"

#define KEY_LEN   80   /* as home.c: entry_cover_key() callers have always used 80 */
#define MAX_GAMES 4096 /* (a line is about 90 bytes: a file of at most 370 kB) */
#define MAX_FILE  (MAX_GAMES * (KEY_LEN + 16))
#define MAX_SECS  0xFFFFFF00u

typedef struct {
    char key[KEY_LEN];
    unsigned secs;
} Game;

static Game *s_games;
static int s_n, s_cap;
static char s_dir[16];

#define MAX_PLAYED 256 /* files taken at one start; the rest next time */

/* the games that were played since psp5 was last open (the files of the plugin that were added at this start), oldest first */
static char s_plays[MAX_PLAYED][KEY_LEN];
static u32 s_play_time[MAX_PLAYED];
static int s_nplays;

static void file_path(char *out, int size)
{
    snprintf(out, size, "%s/playtime.txt", s_dir);
}

static Game *find(const char *key)
{
    for (int i = 0; i < s_n; i++)
        if (!strcmp(s_games[i].key, key))
            return &s_games[i];
    return NULL;
}

/* adds `secs` to the game (a new line if it has none yet; nothing if the table is full) */
static void add(const char *key, unsigned secs)
{
    Game *g = find(key);
    if (!g) {
        if (s_n >= MAX_GAMES)
            return;
        if (s_n == s_cap) {
            int cap = s_cap ? s_cap * 2 : 64;
            Game *n = realloc(s_games, (size_t)cap * sizeof(Game));
            if (!n)
                return;
            s_games = n;
            s_cap = cap;
        }
        g = &s_games[s_n++];
        snprintf(g->key, sizeof(g->key), "%s", key);
        g->secs = 0;
    }
    g->secs = secs > MAX_SECS - g->secs ? MAX_SECS : g->secs + secs;
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
        n += snprintf(text + n, KEY_LEN + 16, "%u %s\n", s_games[i].secs, s_games[i].key);
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0) {
        ok = sceIoWrite(fd, text, n) == n;
        sceIoClose(fd);
    }
    free(text);
    return ok;
}

/* --- the files of the helper plugin (src/awake.h) --- */

static char s_self_key[KEY_LEN]; /* the folder of psp5's own EBOOT.PBP: psp5 is not a game */

void playtime_set_self(const char *path)
{
    s_self_key[0] = 0;
    char key[KEY_LEN];
    if (path && awake_key_from_path(path, key, sizeof(key)) == 1)
        snprintf(s_self_key, sizeof(s_self_key), "%s", key);
}

/* Adds what the plugin kept in <dir>/played/<game>.txt to the games, saves playtime.txt, and only then removes the files (a file that is not
 * whole, or not one of the plugin's, is left alone; psp5's own folder is dropped, see playtime_set_self). */
static void merge_played(void)
{
    typedef struct {
        char name[AWAKE_KEY_MAX + 8];
        char key[KEY_LEN];
        unsigned secs;
        u32 time; /* when the file was written last: the game was played until then */
    } Found;
    char dir[64];
    snprintf(dir, sizeof(dir), "%s/%s", s_dir, AWAKE_PLAYED_DIR);
    SceUID d = sceIoDopen(dir);
    if (d < 0)
        return;
    Found *found = malloc(MAX_PLAYED * sizeof(Found));
    if (!found) {
        sceIoDclose(d);
        return;
    }
    int n = 0;
    SceIoDirent ent;
    memset(&ent, 0, sizeof(ent));
    while (n < MAX_PLAYED && sceIoDread(d, &ent) > 0) {
        if (strlen(ent.d_name) >= sizeof(found[n].name) || !awake_file_key(ent.d_name, found[n].key, KEY_LEN))
            continue;
        char path[160], text[AWAKE_FILE_MAX + 16];
        snprintf(path, sizeof(path), "%s/%s", dir, ent.d_name);
        SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
        int got = 0;
        if (fd >= 0) {
            got = sceIoRead(fd, text, sizeof(text));
            sceIoClose(fd);
        }
        if (got > 0 && awake_parse(text, got, &found[n].secs)) {
            SceIoStat st;
            found[n].time = sceIoGetstat(path, &st) >= 0 ? lastplayed_pack(&st.sce_st_mtime) : 0;
            if (s_self_key[0] && !strcasecmp(found[n].key, s_self_key))
                found[n].secs = 0; /* the plugin counted the time psp5 was open (it only knows psp5 by the folder name PSP5): not a game, the file is removed */
            snprintf(found[n].name, sizeof(found[n].name), "%s", ent.d_name);
            n++;
        }
    }
    sceIoDclose(d);
    int added = 0;
    for (int i = 0; i < n; i++)
        if (found[i].secs > 0) {
            add(found[i].key, found[i].secs);
            added = 1;
        }
    if (!added || save()) {
        for (int i = 0; i < n; i++) {
            char path[160];
            snprintf(path, sizeof(path), "%s/%s", dir, found[i].name);
            sceIoRemove(path);
        }
        u32 now = 0;
        for (int i = 0; i < n; i++) {
            if (found[i].secs == 0)
                continue;
            if (!found[i].time && !now)
                now = lastplayed_now(); /* a file whose time cannot be read: it was played some time since psp5 was last open */
            u32 t = found[i].time ? found[i].time : now;
            lastplayed_note(found[i].key, t);
            int at = s_nplays; /* oldest first; the same time keeps the order the files came in */
            while (at > 0 && s_play_time[at - 1] > t)
                at--;
            if (s_nplays < MAX_PLAYED) {
                memmove(s_plays[at + 1], s_plays[at], (size_t)(s_nplays - at) * KEY_LEN);
                memmove(&s_play_time[at + 1], &s_play_time[at], (size_t)(s_nplays - at) * sizeof(u32));
                snprintf(s_plays[at], KEY_LEN, "%s", found[i].key);
                s_play_time[at] = t;
                s_nplays++;
            }
        }
    }
    free(found);
}

/* the games of playtime.txt; 0 when the file is there but cannot be read (too big, or no memory), 1 otherwise */
static int read_games(void)
{
    char path[48];
    file_path(path, sizeof(path));
    SceIoStat st;
    if (sceIoGetstat(path, &st) < 0)
        return 1; /* no file: nobody has played yet */
    if (st.st_size <= 0)
        return 1;
    if (st.st_size > MAX_FILE)
        return 0;
    char *text = malloc((size_t)st.st_size + 1);
    SceUID fd = text ? sceIoOpen(path, PSP_O_RDONLY, 0) : -1;
    if (fd < 0) {
        free(text);
        return 0;
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
            unsigned long long secs = strtoull(line, NULL, 10);
            *space = 0;
            if (secs > 0 && !find(space + 1))
                add(space + 1, secs > MAX_SECS ? MAX_SECS : (unsigned)secs);
        }
        line = end + 1;
    }
    free(text);
    return 1;
}

void playtime_load(const char *dir)
{
    s_n = 0;
    s_nplays = 0;
    snprintf(s_dir, sizeof(s_dir), "%s", dir);
    if (read_games()) /* (a file that could not be read is not written over with what the plugin kept) */
        merge_played();
}


unsigned playtime_seconds(const Entry *e)
{
    char key[KEY_LEN];
    if (!s_n)
        return 0;
    entry_cover_key(e, key, sizeof(key));
    const Game *g = find(key);
    return g ? g->secs : 0;
}

void playtime_text(unsigned secs, char *out, int size)
{
    out[0] = 0;
    if (secs == 0)
        return;
    if (secs < 60) { /* something was played, but not a minute yet */
        snprintf(out, size, "<1%s", L(S_UNIT_MIN));
        return;
    }
    unsigned mins = secs / 60;
    if (mins < 60)
        snprintf(out, size, "%u%s", mins, L(S_UNIT_MIN));
    else
        snprintf(out, size, "%u%s", mins / 60 > 9999 ? 9999 : mins / 60, L(S_UNIT_H));
}

int playtime_plays(void)
{
    return s_nplays;
}

const char *playtime_play_key(int i)
{
    return i < s_nplays ? s_plays[i] : ""; /* (i is 0 or more) */
}

void playtime_plays_taken(void)
{
    s_nplays = 0;
}
