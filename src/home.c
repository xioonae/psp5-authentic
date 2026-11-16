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

typedef char Key[KEY_LEN];

static Key s_kept[HOME_KEEP_MAX]; /* in the order they were kept */
static int s_nkept;
static Key s_recent[HISTORY_MAX]; /* newest first */
static int s_nrecent;

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

/* --- the file --- */

static void file_path(char *out, int size)
{
    snprintf(out, size, "%s/home.txt", g_data_dir);
}

/* one line per card: "keep <key>" or "recent <key>" */
static char s_text[(HOME_KEEP_MAX + HISTORY_MAX) * (KEY_LEN + 8) + 16];

static void save(void)
{
    char path[64];
    int n = 0;
    file_path(path, sizeof(path));
    for (int i = 0; i < s_nkept; i++)
        n += snprintf(s_text + n, sizeof(s_text) - n, "keep %s\n", s_kept[i]);
    for (int i = 0; i < s_nrecent; i++)
        n += snprintf(s_text + n, sizeof(s_text) - n, "recent %s\n", s_recent[i]);
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
        /* "pin" is what older files call a kept game (the Library card was pinned as "?library": it is always there now); "hide" lines are ignored */
        if ((!strncmp(line, "keep ", 5) || !strncmp(line, "pin ", 4)) && s_nkept < HOME_KEEP_MAX) {
            const char *key = line + (line[0] == 'k' ? 5 : 4);
            if (key[0] && key[0] != '?' && find_key(s_kept, s_nkept, key) < 0)
                snprintf(s_kept[s_nkept++], KEY_LEN, "%s", key);
        } else if (!strncmp(line, "recent ", 7) && line[7] && s_nrecent < HISTORY_MAX && find_key(s_recent, s_nrecent, line + 7) < 0)
            snprintf(s_recent[s_nrecent++], KEY_LEN, "%s", line + 7);
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

/* the games of Home in order: the order key first (a bigger one first), then the title, then the place in the list */
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
        for (int i = s_nkept - 1; i >= 0; i--)
            if (find_entry(list, keys, s_kept[i]) < 0)
                drop_at(s_kept, &s_nkept, i);
        for (int i = s_nrecent - 1; i >= 0; i--)
            if (find_entry(list, keys, s_recent[i]) < 0)
                drop_at(s_recent, &s_nrecent, i);
    }
    free(keys);
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
            push_front(s_recent, &s_nrecent, HISTORY_MAX, playtime_play_key(i));
            changed = 1;
        }
    free(keys);
    playtime_plays_taken();
    if (changed)
        save();
}

void home_load(const EntryList *list)
{
    s_nkept = s_nrecent = 0;
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
    Key *keys = list_keys(list);
    if (!keys) {
        if (n < max)
            out[n++] = HOME_LIBRARY; /* the Library card is always there */
        return n;
    }
    int room = HOME_MAX - 1; /* the Library card keeps its place at the end */
    int *rank = malloc((size_t)(list->count ? list->count : 1) * sizeof(int));
    u64 *okey = malloc((size_t)(list->count ? list->count : 1) * sizeof(u64));
    int m = 0;
    for (int i = 0; rank && okey && i < list->count; i++) {
        if (list->items[i].type == ENTRY_UMD)
            continue;
        okey[i] = home_order_key(&list->items[i]);
        rank[m++] = i;
    }
    if (rank && okey) {
        s_cmp_list = list;
        s_cmp_key = okey;
        qsort(rank, m, sizeof(int), cmp_rest);
    } else { /* no memory for the order: the games as they are in the list */
        m = 0;
        for (int i = 0; i < list->count; i++)
            if (list->items[i].type != ENTRY_UMD && rank)
                rank[m++] = i;
    }
    /* the first games of the order, and the kept ones that come after them: they take the last places */
    int take = m < room ? m : room;
    int chosen[HOME_MAX], nc = 0, missing[HOME_KEEP_MAX], nm = 0;
    for (int k = 0; rank && k < take; k++)
        chosen[nc++] = rank[k];
    for (int k = take; rank && k < m && nm < HOME_KEEP_MAX; k++)
        if (find_key(s_kept, s_nkept, keys[rank[k]]) >= 0)
            missing[nm++] = rank[k];
    for (int g = 0; g < nm; g++) { /* the last game that is not kept gives way */
        int at = -1;
        for (int k = nc - 1; k >= 0 && at < 0; k--)
            if (find_key(s_kept, s_nkept, keys[chosen[k]]) < 0)
                at = k;
        if (at < 0)
            break;
        memmove(&chosen[at], &chosen[at + 1], (size_t)(nc - at - 1) * sizeof(int));
        nc--;
    }
    for (int g = 0; g < nm && nc < room; g++)
        chosen[nc++] = missing[g];
    for (int k = 0; k < nc && n < max; k++)
        out[n++] = chosen[k];
    free(rank);
    free(okey);
    free(keys);
    if (n < max)
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
    int state = find_key(s_kept, s_nkept, key) >= 0 ? HOME_KEPT : 0;
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
    push_front(s_recent, &s_nrecent, HISTORY_MAX, key);
    save();
}

int home_keep(const Entry *e, int on)
{
    Key key;
    if (e->type == ENTRY_UMD)
        return 0;
    key_of(e, key);
    int i = find_key(s_kept, s_nkept, key);
    if (on) {
        if (i >= 0)
            return 1;
        if (s_nkept >= HOME_KEEP_MAX)
            return 0;
        snprintf(s_kept[s_nkept++], KEY_LEN, "%s", key);
    } else {
        if (i < 0)
            return 1;
        drop_at(s_kept, &s_nkept, i);
    }
    save();
    return 1;
}

void home_forget(const Entry *e)
{
    Key key;
    if (e->type == ENTRY_UMD)
        return;
    key_of(e, key);
    int k = find_key(s_kept, s_nkept, key), r = find_key(s_recent, s_nrecent, key);
    if (k >= 0)
        drop_at(s_kept, &s_nkept, k);
    if (r >= 0)
        drop_at(s_recent, &s_nrecent, r);
    if (k >= 0 || r >= 0)
        save();
}
