/* psp5 – made by Mille Weber */
/* Home: which cards the Games tab shows (see home.h). */
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "home.h"
#include "lastplayed.h"
#include "playtime.h"
#include "system.h"

#define KEY_LEN     80 /* as entry_cover_key() callers have always used */
#define HISTORY_MAX 40 /* cards remembered as opened; Home shows the first ones of them */
#define LIBRARY_KEY "?library" /* the Library card in the list of pinned cards ('?' cannot be in a file name) */
#define PIN_KEEP    9          /* a home.txt of 1.0.0 may pin nine games: they are all kept, only new pins stop at HOME_PIN_MAX */
#define PIN_SLOTS   (PIN_KEEP + 1) /* ... and the Library card */

typedef char Key[KEY_LEN];

static Key s_pinned[PIN_SLOTS];
static int s_npinned;
static Key s_recent[HISTORY_MAX]; /* newest first; may hold pinned cards too, so that unpinning puts a card back where it belongs */
static int s_nrecent;
static Key s_hidden[HOME_HIDDEN_MAX]; /* newest first */
static int s_nhidden;

/* --- the two lists --- */

static int find_key(const Key *a, int n, const char *key)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(a[i], key))
            return i;
    return -1;
}

static void drop_at(Key *a, int *n, int i)
{
    memmove(a[i], a[i + 1], (size_t)(*n - i - 1) * KEY_LEN);
    (*n)--;
}

/* `key` becomes the first one; it is not in the list twice, and the oldest goes when the list is full */
static void push_front(Key *a, int *n, int max, const char *key)
{
    int i = find_key(a, *n, key);
    if (i >= 0)
        drop_at(a, n, i);
    if (*n >= max)
        (*n)--;
    memmove(a[1], a[0], (size_t)*n * KEY_LEN);
    snprintf(a[0], KEY_LEN, "%s", key);
    (*n)++;
}

static void key_of(const Entry *e, char *out)
{
    entry_cover_key(e, out, KEY_LEN);
}

/* how many games are pinned (the Library card is not one of them) */
static int game_pins(void)
{
    return s_npinned - (find_key(s_pinned, s_npinned, LIBRARY_KEY) >= 0);
}

/* --- the file --- */

static void file_path(char *out, int size)
{
    snprintf(out, size, "%s/home.txt", g_data_dir);
}

/* one line per card: "pin <key>", "recent <key>" or "hide <key>" */
static char s_text[(PIN_SLOTS + HISTORY_MAX + HOME_HIDDEN_MAX) * (KEY_LEN + 8) + 16];

static void save(void)
{
    char path[64];
    int n = 0;
    file_path(path, sizeof(path));
    for (int i = 0; i < s_npinned; i++)
        n += snprintf(s_text + n, sizeof(s_text) - n, "pin %s\n", s_pinned[i]);
    for (int i = 0; i < s_nrecent; i++)
        n += snprintf(s_text + n, sizeof(s_text) - n, "recent %s\n", s_recent[i]);
    for (int i = 0; i < s_nhidden; i++)
        n += snprintf(s_text + n, sizeof(s_text) - n, "hide %s\n", s_hidden[i]);
    SceUID fd = sceIoOpen(path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, s_text, n);
        sceIoClose(fd);
    }
}

/* 1 = the file was read (it may be empty), 0 = there is none */
static int load_file(void)
{
    char path[64];
    file_path(path, sizeof(path));
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return 0;
    int n = sceIoRead(fd, s_text, sizeof(s_text) - 1);
    sceIoClose(fd);
    s_text[n > 0 ? n : 0] = 0;
    for (char *line = s_text; *line;) {
        char *end = strchr(line, '\n');
        if (!end)
            break; /* a last line without its end was cut off: ignore it */
        *end = 0;
        if (end > line && end[-1] == '\r')
            end[-1] = 0;
        if (!strncmp(line, "pin ", 4) && line[4] && s_npinned < PIN_SLOTS && find_key(s_pinned, s_npinned, line + 4) < 0 &&
            (!strcmp(line + 4, LIBRARY_KEY) || game_pins() < PIN_KEEP))
            snprintf(s_pinned[s_npinned++], KEY_LEN, "%s", line + 4);
        else if (!strncmp(line, "recent ", 7) && line[7] && s_nrecent < HISTORY_MAX && find_key(s_recent, s_nrecent, line + 7) < 0)
            snprintf(s_recent[s_nrecent++], KEY_LEN, "%s", line + 7);
        else if (!strncmp(line, "hide ", 5) && line[5] && s_nhidden < HOME_HIDDEN_MAX && find_key(s_hidden, s_nhidden, line + 5) < 0)
            snprintf(s_hidden[s_nhidden++], KEY_LEN, "%s", line + 5);
        line = end + 1;
    }
    return 1;
}

/* --- the game list --- */

/* the key of every entry of the list (the disc has none worth keeping); NULL if there is no memory for it */
static Key *list_keys(const EntryList *list)
{
    Key *keys = malloc((size_t)(list->count ? list->count : 1) * KEY_LEN);
    for (int i = 0; keys && i < list->count; i++) {
        if (list->items[i].type == ENTRY_UMD)
            keys[i][0] = 0;
        else
            key_of(&list->items[i], keys[i]);
    }
    return keys;
}

static int find_entry(const EntryList *list, const Key *keys, const char *key)
{
    for (int i = 0; i < list->count; i++)
        if (keys[i][0] && !strcmp(keys[i], key))
            return i;
    return -1;
}

u64 home_order_key(const Entry *e)
{
    int rank = home_recent_rank(e);
    if (rank >= 0)
        return (3ull << 32) | (0xFFFFFFFFu - (u32)rank);
    u32 played = lastplayed_time(e);
    return played ? (2ull << 32) | played : (1ull << 32) | e->file_time;
}

/* the games that fill the rest of Home: the order key first (a bigger one first), then the title, then the place in the list (as the Library sorts them) */
static const EntryList *s_cmp_list;
static const u64 *s_cmp_key;
static int cmp_rest(const void *a, const void *b)
{
    int i = *(const int *)a, j = *(const int *)b;
    if (s_cmp_key[i] != s_cmp_key[j])
        return s_cmp_key[i] > s_cmp_key[j] ? -1 : 1;
    int c = strcasecmp(s_cmp_list->items[i].title, s_cmp_list->items[j].title);
    return c ? c : i - j;
}

/* drops the cards that are not in the list any more */
static void prune(const EntryList *list)
{
    Key *keys = list_keys(list);
    if (!keys)
        return;
    int any = 0;
    for (int i = 0; i < list->count; i++)
        any |= list->items[i].type != ENTRY_UMD;
    if (any) { /* an empty list means the card could not be read: keep what is remembered */
        for (int i = s_npinned - 1; i >= 0; i--)
            if (strcmp(s_pinned[i], LIBRARY_KEY) && find_entry(list, keys, s_pinned[i]) < 0)
                drop_at(s_pinned, &s_npinned, i);
        for (int i = s_nrecent - 1; i >= 0; i--)
            if (find_entry(list, keys, s_recent[i]) < 0)
                drop_at(s_recent, &s_nrecent, i);
        for (int i = s_nhidden - 1; i >= 0; i--)
            if (find_entry(list, keys, s_hidden[i]) < 0)
                drop_at(s_hidden, &s_nhidden, i);
    }
    free(keys);
}

/* the game was opened (or added): it is the newest card, and it is not off Home any more */
static void open_key(const char *key)
{
    int i = find_key(s_hidden, s_nhidden, key);
    if (i >= 0)
        drop_at(s_hidden, &s_nhidden, i);
    push_front(s_recent, &s_nrecent, HISTORY_MAX, key);
}

/* The games that were played without psp5 (from the XMB, found by the helper plugin) since psp5 was last open: each one is the newest card, the
 * one played first first, so the last one played is in front. They are taken once (home_load() runs again when the list is rebuilt), and
 * not before the list has games (an empty list means the card could not be read). */
static void apply_plays(const EntryList *list)
{
    int n = playtime_plays(), any = 0, changed = 0;
    for (int i = 0; i < list->count; i++)
        any |= list->items[i].type != ENTRY_UMD;
    if (!any)
        return;
    Key *keys = list_keys(list);
    if (!keys)
        return;
    for (int i = 0; i < n; i++)
        if (find_entry(list, keys, playtime_play_key(i)) >= 0) {
            open_key(playtime_play_key(i));
            changed = 1;
        }
    free(keys);
    playtime_plays_taken();
    if (changed)
        save();
}

void home_load(const EntryList *list)
{
    s_npinned = s_nrecent = s_nhidden = 0;
    if (load_file())
        prune(list);
    apply_plays(list);
}

int home_build(const EntryList *list, int *out, int max)
{
    int n = 0;
    for (int i = 0; i < list->count && n < max; i++)
        if (list->items[i].type == ENTRY_UMD) {
            out[n++] = i; /* the disc, first and outside the count */
            break;
        }
    int lib_pinned = find_key(s_pinned, s_npinned, LIBRARY_KEY) >= 0;
    Key *keys = list_keys(list);
    if (!keys) {
        if (n < max)
            out[n++] = HOME_LIBRARY; /* the Library card is always there */
        return n;
    }
    int room = HOME_MAX - (lib_pinned ? 0 : 1); /* an unpinned Library card keeps its place at the end */
    for (int i = 0; i < s_npinned && room > 0 && n < max; i++) {
        if (!strcmp(s_pinned[i], LIBRARY_KEY)) {
            out[n++] = HOME_LIBRARY;
            room--;
            continue;
        }
        int at = find_entry(list, keys, s_pinned[i]);
        if (at >= 0) {
            out[n++] = at;
            room--;
        }
    }
    for (int i = 0; i < s_nrecent && room > 0 && n < max; i++) {
        if (find_key(s_pinned, s_npinned, s_recent[i]) >= 0)
            continue; /* it is on Home already, as a pinned card */
        int at = find_entry(list, keys, s_recent[i]);
        if (at >= 0) {
            out[n++] = at;
            room--;
        }
    }
    if (room > 0 && n < max) { /* the rest: the other games, in the order of "Recently played" (played before, then added last first), but not those taken off Home */
        int *rest = malloc((size_t)(list->count ? list->count : 1) * sizeof(int));
        u64 *okey = malloc((size_t)(list->count ? list->count : 1) * sizeof(u64));
        int m = 0;
        for (int i = 0; rest && okey && i < list->count; i++) {
            if (list->items[i].type == ENTRY_UMD || find_key(s_pinned, s_npinned, keys[i]) >= 0 || find_key(s_recent, s_nrecent, keys[i]) >= 0 ||
                find_key(s_hidden, s_nhidden, keys[i]) >= 0)
                continue;
            okey[i] = home_order_key(&list->items[i]);
            rest[m++] = i;
        }
        if (rest && okey) {
            s_cmp_list = list;
            s_cmp_key = okey;
            qsort(rest, m, sizeof(int), cmp_rest);
            for (int k = 0; k < m && room > 0 && n < max; k++) {
                out[n++] = rest[k];
                room--;
            }
        }
        free(rest);
        free(okey);
    }
    free(keys);
    if (!lib_pinned && n < max)
        out[n++] = HOME_LIBRARY;
    return n;
}

int home_recent_rank(const Entry *e)
{
    Key key;
    if (e->type == ENTRY_UMD)
        return -1;
    key_of(e, key);
    return find_key(s_recent, s_nrecent, key);
}

int home_state(const EntryList *list, int index)
{
    if (index < 0 || index >= list->count)
        return 0;
    const Entry *e = &list->items[index];
    if (e->type == ENTRY_UMD)
        return HOME_SHOWN;
    Key key;
    key_of(e, key);
    int state = find_key(s_pinned, s_npinned, key) >= 0 ? HOME_PINNED : 0;
    int shown[HOME_MAX + 1];
    int n = home_build(list, shown, HOME_MAX + 1);
    Key *keys = list_keys(list);
    for (int i = 0; keys && i < n; i++)
        if (shown[i] >= 0 && !strcmp(keys[shown[i]], key))
            state |= HOME_SHOWN;
    free(keys);
    return state;
}

/* --- changes --- */

void home_note_launch(const Entry *e)
{
    Key key;
    if (e->type == ENTRY_UMD)
        return;
    key_of(e, key);
    open_key(key);
    save();
}

void home_add(const Entry *e)
{
    home_note_launch(e);
}

void home_remove(const Entry *e)
{
    Key key;
    if (e->type == ENTRY_UMD)
        return;
    key_of(e, key);
    int i = find_key(s_pinned, s_npinned, key);
    if (i >= 0)
        drop_at(s_pinned, &s_npinned, i);
    i = find_key(s_recent, s_nrecent, key);
    if (i >= 0)
        drop_at(s_recent, &s_nrecent, i);
    push_front(s_hidden, &s_nhidden, HOME_HIDDEN_MAX, key); /* or the game would come back as one of those that fill the rest of Home */
    save();
}

int home_pin(const Entry *e)
{
    Key key;
    if (e->type == ENTRY_UMD)
        return 0;
    key_of(e, key);
    if (find_key(s_pinned, s_npinned, key) >= 0)
        return 1;
    if (game_pins() >= HOME_PIN_MAX)
        return 0;
    if (find_key(s_recent, s_nrecent, key) < 0)
        open_key(key); /* so that unpinning leaves it on Home (and it is not taken off any more) */
    snprintf(s_pinned[s_npinned++], KEY_LEN, "%s", key);
    save();
    return 1;
}

void home_unpin(const Entry *e)
{
    Key key;
    if (e->type == ENTRY_UMD)
        return;
    key_of(e, key);
    int i = find_key(s_pinned, s_npinned, key);
    if (i < 0)
        return;
    drop_at(s_pinned, &s_npinned, i);
    save();
}

int home_library_pinned(void)
{
    return find_key(s_pinned, s_npinned, LIBRARY_KEY) >= 0;
}

void home_pin_library(void)
{
    if (home_library_pinned() || s_npinned >= PIN_SLOTS)
        return;
    snprintf(s_pinned[s_npinned++], KEY_LEN, "%s", LIBRARY_KEY);
    save();
}

void home_unpin_library(void)
{
    int i = find_key(s_pinned, s_npinned, LIBRARY_KEY);
    if (i < 0)
        return;
    drop_at(s_pinned, &s_npinned, i);
    save();
}
