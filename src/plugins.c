/* psp5 – made by Mille Weber */
/* PLUGINS.TXT: "runlevel, sökväg, on|off" per rad. Vi behåller filen rad för rad
 * och ändrar bara det sista fältet på de rader man slår av/på. */
#include <pspkernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "plugins.h"
#include "system.h"

#define MAX_FILES 6 /* the ARK folder's and SEPLUGINS on two storages, and room for an ARK folder that is somewhere else */
#define MAX_LINES 96
#define LINE_LEN 192

typedef struct {
    char path[160];
    char lines[MAX_LINES][LINE_LEN];
    int count;
    int dirty;
    int partial; /* the file did not fit (too many lines, a line too long, too big): never write it back */
} PluginFile;

static PluginFile s_files[MAX_FILES];
static int s_file_count;
static Plugin s_plugins[MAX_PLUGINS];
static int s_count;
static char s_ark_file[200]; /* <ark dir>PLUGINS.TXT: where lines are added when no file has any */

static char *trim(char *s)
{
    while (isspace((unsigned char)*s))
        s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1]))
        *--e = 0;
    return s;
}

static void parse_line(int file, int line)
{
    char tmp[LINE_LEN], *save = NULL, *parts[4] = { 0 };
    int n = 0;
    snprintf(tmp, sizeof(tmp), "%s", s_files[file].lines[line]);
    char *t = trim(tmp);
    if (!*t || t[0] == '#' || t[0] == ';' || (t[0] == '/' && t[1] == '/') || s_count >= MAX_PLUGINS)
        return;
    for (char *p = strtok_r(t, ",", &save); p && n < 4; p = strtok_r(NULL, ",", &save))
        parts[n++] = trim(p);
    if (n != 3)
        return;

    Plugin *pl = &s_plugins[s_count++];
    const char *base = strrchr(parts[1], '/');
    snprintf(pl->name, sizeof(pl->name), "%s", base ? base + 1 : parts[1]);
    snprintf(pl->where, sizeof(pl->where), "%s", parts[0]);
    pl->on = !strcasecmp(parts[2], "on") || !strcmp(parts[2], "1") || !strcasecmp(parts[2], "enabled") ||
             !strcasecmp(parts[2], "true");
    snprintf(pl->path, sizeof(pl->path), "%s", parts[1]);
    pl->file = file;
    pl->line = line;
}

static void load_file(const char *path)
{
    if (s_file_count >= MAX_FILES)
        return;
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return;
    static char buf[MAX_LINES * LINE_LEN];
    int len = sceIoRead(fd, buf, sizeof(buf) - 1);
    sceIoClose(fd);
    if (len < 0)
        return;
    buf[len] = 0;

    PluginFile *f = &s_files[s_file_count];
    memset(f, 0, sizeof(*f));
    snprintf(f->path, sizeof(f->path), "%s", path);
    /* behåll alla rader, även tomma, så att filen skrivs tillbaka likadan */
    char *p = buf;
    f->partial = len >= (int)sizeof(buf) - 1; /* (the file may go on after what was read) */
    while (*p) {
        if (f->count >= MAX_LINES) { /* more lines than there is room for: writing the file would cut them off */
            f->partial = 1;
            break;
        }
        char *nl = strchr(p, '\n');
        int l = nl ? nl - p : (int)strlen(p);
        if (l > 0 && p[l - 1] == '\r')
            l--;
        if (l >= LINE_LEN) /* snprintf cuts it */
            f->partial = 1;
        snprintf(f->lines[f->count++], LINE_LEN, "%.*s", l, p);
        if (!nl)
            break;
        p = nl + 1;
    }
    int file = s_file_count++;
    for (int i = 0; i < f->count; i++)
        parse_line(file, i);
}

static int dir_exists(const char *path)
{
    SceUID d = sceIoDopen(path);
    if (d < 0)
        return 0;
    sceIoDclose(d);
    return 1;
}

static int file_exists(const char *path)
{
    SceIoStat st;
    return sceIoGetstat(path, &st) >= 0;
}

/* The other storage when there are two ("ms0:" when psp5 runs from the internal storage, else "ef0:"), NULL when there is none (ARK can also point ms0: at the
 * internal storage: that is one storage). Whether the other one is there is only a question of whether it can be opened: no check of the model. */
static const char *other_root(void)
{
    if (sys_ms_is_ef())
        return NULL;
    int from_ef = !strncasecmp(g_root, "ef0:", 4);
    const char *root = from_ef ? "ms0:" : "ef0:";
    char probe[8];
    snprintf(probe, sizeof(probe), "%s/", root);
    return dir_exists(probe) ? root : NULL;
}

/* a file is read once, however its path is written (ARK-4 without an ARK folder uses <storage>/SEPLUGINS/ as the ARK folder) */
static void load_once(const char *path)
{
    for (int i = 0; i < s_file_count; i++)
        if (!strcasecmp(s_files[i].path, path))
            return;
    load_file(path);
}

int plugins_load(const char *ark_dir)
{
    char path[200];
    s_file_count = s_count = 0;
    snprintf(path, sizeof(path), "%sPLUGINS.TXT", ark_dir);
    snprintf(s_ark_file, sizeof(s_ark_file), "%s", path);
    load_once(path);
    snprintf(path, sizeof(path), "%s/SEPLUGINS/PLUGINS.TXT", g_root);
    load_once(path);
    const char *other = other_root();
    if (other) {
        snprintf(path, sizeof(path), "%s/PSP/SAVEDATA/ARK_01234/PLUGINS.TXT", other);
        load_once(path);
        snprintf(path, sizeof(path), "%s/SEPLUGINS/PLUGINS.TXT", other);
        load_once(path);
    }
    return s_count;
}

int plugins_count(void)
{
    return s_count;
}

Plugin *plugins_get(int i)
{
    return i >= 0 && i < s_count ? &s_plugins[i] : NULL;
}

/* Byter det sista fältet på raden till on/off. */
static void apply(const Plugin *pl)
{
    PluginFile *f = &s_files[pl->file];
    char *line = f->lines[pl->line];
    char *comma = strrchr(line, ',');
    if (!comma)
        return;
    char want[8];
    snprintf(want, sizeof(want), "%s", pl->on ? "on" : "off");
    char cur[16];
    snprintf(cur, sizeof(cur), "%s", comma + 1);
    if (!strcasecmp(trim(cur), want))
        return;
    snprintf(comma, LINE_LEN - (comma - line), ", %s", want);
    f->dirty = 1;
}

int plugins_save(void)
{
    int ok = 1;
    for (int i = 0; i < s_count; i++)
        apply(&s_plugins[i]);
    for (int i = 0; i < s_file_count; i++) {
        PluginFile *f = &s_files[i];
        if (!f->dirty)
            continue;
        if (f->partial) { /* a file that was only partly read is left exactly as it is */
            ok = 0;
            continue;
        }
        SceUID fd = sceIoOpen(f->path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
        if (fd < 0) { /* the folder may not be there yet (SEPLUGINS): it is made, and PLUGINS.TXT with it */
            char dir[160];
            snprintf(dir, sizeof(dir), "%s", f->path);
            char *slash = strrchr(dir, '/');
            if (slash && slash > dir + 4) {
                *slash = 0;
                sceIoMkdir(dir, 0777);
                fd = sceIoOpen(f->path, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
            }
        }
        if (fd < 0) {
            ok = 0;
            continue;
        }
        for (int l = 0; l < f->count; l++) {
            sceIoWrite(fd, f->lines[l], strlen(f->lines[l]));
            if (l < f->count - 1 || f->lines[l][0])
                sceIoWrite(fd, "\n", 1);
        }
        sceIoClose(fd);
        f->dirty = 0;
    }
    return ok;
}

/* --- the helper plugin: the lines "game, <path>, on" and "pops, <path>, on" ---
 * ARK takes the runlevel as a word to look for: "game" is a PSP game, an ISO or a homebrew, "pops" is a PS1 game. */

static int helper_line(const char *name, const char *level)
{
    for (int i = 0; i < s_count; i++)
        if (!strcasecmp(s_plugins[i].name, name) && !strcasecmp(s_plugins[i].where, level))
            return i;
    return -1;
}

int plugins_helper_any(const char *name)
{
    return helper_line(name, "game") >= 0 || helper_line(name, "pops") >= 0;
}

/* The file the new lines go into: the first one that is there (the ARK folder's file is read first, so it is that one when both are),
 * else the ARK folder's, made on saving. NULL when there is no room or the file cannot be written back whole. */
static PluginFile *helper_file(void)
{
    if (s_file_count > 0)
        return s_files[0].partial ? NULL : &s_files[0];
    if (s_file_count >= MAX_FILES || !s_ark_file[0])
        return NULL;
    PluginFile *f = &s_files[s_file_count++];
    memset(f, 0, sizeof(*f));
    snprintf(f->path, sizeof(f->path), "%s", s_ark_file);
    char dir[200];
    snprintf(dir, sizeof(dir), "%s", s_ark_file);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = 0;
        sceIoMkdir(dir, 0777); /* (the folder is there for ARK; it may not be for <storage>/SEPLUGINS) */
    }
    return f;
}

int plugins_helper_add(const char *prx_path)
{
    const char *name = strrchr(prx_path, '/');
    name = name ? name + 1 : prx_path;
    static const char *const levels[] = { "game", "pops" };
    for (int k = 0; k < 2; k++) {
        if (helper_line(name, levels[k]) >= 0)
            continue; /* a line that is there is left as it is (on or off) */
        PluginFile *f = helper_file();
        if (!f || f->count >= MAX_LINES)
            return 0;
        int line = f->count++;
        snprintf(f->lines[line], LINE_LEN, "%s, %s, on", levels[k], prx_path);
        f->dirty = 1;
        parse_line((int)(f - s_files), line);
    }
    return plugins_save();
}

/* ---------------------------------------------------------------------- */
/* Manage Plugins */

const char *const PLUGIN_LEVEL[PLUGIN_LEVELS] = { "vsh", "game", "pops" };

/* the runlevel a line has, as an index in PLUGIN_LEVEL (the words ARK knows for it count too), or -1 */
static int level_index(const char *rl)
{
    if (!strcasecmp(rl, "vsh") || !strcasecmp(rl, "xmb"))
        return 0;
    if (!strcasecmp(rl, "game"))
        return 1;
    if (!strcasecmp(rl, "pops") || !strcasecmp(rl, "psx") || !strcasecmp(rl, "ps1"))
        return 2;
    return -1;
}

static int level_always(const char *rl)
{
    return !strcasecmp(rl, "always") || !strcasecmp(rl, "all");
}

/* the lists are made again from the lines of the files */
static void reparse(void)
{
    s_count = 0;
    for (int f = 0; f < s_file_count; f++)
        for (int l = 0; l < s_files[f].count; l++)
            parse_line(f, l);
}

/* The PLUGINS.TXT at `path` among the files that are known (case does not matter). When it is not one of them and `create` is set, it becomes one: empty, and written
 * (made, with its folder) when its first line is added. -1 = it is not known and there is no room (or it is not asked to be made). */
static int file_index(const char *path, int create)
{
    for (int i = 0; i < s_file_count; i++)
        if (!strcasecmp(s_files[i].path, path))
            return i;
    if (!create || s_file_count >= MAX_FILES || strlen(path) >= sizeof(s_files[0].path))
        return -1;
    int i = s_file_count++;
    memset(&s_files[i], 0, sizeof(s_files[i]));
    snprintf(s_files[i].path, sizeof(s_files[i].path), "%s", path);
    return i;
}

int plugins_names(const char **out, int max)
{
    int n = 0;
    for (int i = 0; i < s_count; i++) {
        int seen = 0;
        for (int k = 0; k < n; k++)
            if (!strcasecmp(out[k], s_plugins[i].name))
                seen = 1;
        if (!seen && n < max)
            out[n++] = s_plugins[i].name;
    }
    return n;
}

int plugins_lines_of(const char *name, int *out, int max)
{
    int n = 0;
    for (int i = 0; i < s_count && n < max; i++)
        if (!strcasecmp(s_plugins[i].name, name))
            out[n++] = i;
    return n;
}

int plugins_free_places(const char *name, PluginFree *out, int max)
{
    int n = 0;
    for (int i = 0; i < s_count; i++) {
        const Plugin *pl = &s_plugins[i];
        if (strcasecmp(pl->name, name))
            continue;
        int again = 0; /* the same file and path as an earlier line of it: looked at already */
        for (int k = 0; k < i; k++)
            if (!strcasecmp(s_plugins[k].name, name) && s_plugins[k].file == pl->file && !strcasecmp(s_plugins[k].path, pl->path))
                again = 1;
        if (again)
            continue;
        int have[PLUGIN_LEVELS] = { 0 }, always = 0;
        for (int k = 0; k < s_count; k++) {
            const Plugin *o = &s_plugins[k];
            if (strcasecmp(o->name, name) || o->file != pl->file || strcasecmp(o->path, pl->path))
                continue;
            if (level_always(o->where))
                always = 1;
            int lv = level_index(o->where);
            if (lv >= 0)
                have[lv] = 1;
        }
        for (int lv = 0; lv < PLUGIN_LEVELS && !always && n < max; lv++)
            if (!have[lv]) {
                out[n].file = pl->file;
                snprintf(out[n].path, sizeof(out[n].path), "%s", pl->path);
                out[n].level = lv;
                n++;
            }
    }
    /* The other PLUGINS.TXT files (the ARK folder's and SEPLUGINS, on each storage, there or not) that do not name the plugin at all: every runlevel, with the path of the
     * plugin's first line, which is where its file rests. The file is not moved or copied: only the lines are put in. */
    const char *rests = NULL;
    for (int i = 0; i < s_count && !rests; i++)
        if (!strcasecmp(s_plugins[i].name, name))
            rests = s_plugins[i].path;
    if (rests) {
        PluginLoc locs[PLUGIN_LOCS];
        int nl = plugins_locations(locs, PLUGIN_LOCS);
        for (int j = 0; j < nl; j++) {
            int f = file_index(locs[j].file, 0), listed = 0;
            for (int i = 0; f >= 0 && i < s_count; i++)
                listed |= s_plugins[i].file == f && !strcasecmp(s_plugins[i].name, name);
            if (listed)
                continue;
            f = file_index(locs[j].file, 1);
            if (f < 0 || s_files[f].partial)
                continue;
            for (int lv = 0; lv < PLUGIN_LEVELS && n < max; lv++) {
                out[n].file = f;
                snprintf(out[n].path, sizeof(out[n].path), "%s", rests);
                out[n].level = lv;
                n++;
            }
        }
    }
    return n;
}

const char *plugins_file_path(int file)
{
    return file >= 0 && file < s_file_count ? s_files[file].path : "";
}

void plugins_place_label(const char *path, char *out, int size)
{
    char dir[160];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/'); /* the file's name goes: what is left is the folder */
    if (slash)
        *slash = 0;
    const char *folder = strrchr(dir, '/');
    folder = folder ? folder + 1 : dir;
    if (strlen(path) > 4 && path[3] == ':')
        snprintf(out, size, "%s (%.4s)", folder, path);
    else
        snprintf(out, size, "%s", folder);
}

int plugins_remove(const int *idx, int n)
{
    static unsigned char kill[MAX_FILES][MAX_LINES];
    memset(kill, 0, sizeof(kill));
    int any = 0;
    for (int k = 0; k < n; k++) {
        if (idx[k] < 0 || idx[k] >= s_count)
            return 0;
        const Plugin *pl = &s_plugins[idx[k]];
        if (s_files[pl->file].partial) /* writing it back would cut lines off */
            return 0;
        kill[pl->file][pl->line] = 1;
        any = 1;
    }
    if (!any)
        return 0;
    for (int f = 0; f < s_file_count; f++) {
        PluginFile *pf = &s_files[f];
        int w = 0;
        for (int l = 0; l < pf->count; l++) {
            if (kill[f][l]) {
                pf->dirty = 1;
                continue;
            }
            if (w != l)
                memcpy(pf->lines[w], pf->lines[l], LINE_LEN);
            w++;
        }
        pf->count = w;
    }
    reparse();
    return plugins_save();
}

/* 0 = no room (or the file may not be written), 1 = the line is added, 2 = it is there already */
static int add_line(int file, int level, const char *path)
{
    PluginFile *f = &s_files[file];
    if (f->partial || strlen(path) + 16 >= LINE_LEN)
        return 0;
    for (int i = 0; i < s_count; i++)
        if (s_plugins[i].file == file && level_index(s_plugins[i].where) == level && !strcasecmp(s_plugins[i].path, path))
            return 2;
    if (f->count >= MAX_LINES)
        return 0;
    snprintf(f->lines[f->count], LINE_LEN, "%s, %s, on", PLUGIN_LEVEL[level], path);
    f->dirty = 1;
    f->count++;
    reparse(); /* (the lists are in the order of the files, as a new load of them would make them) */
    return 1;
}

int plugins_add(int file, int level, const char *path)
{
    if (file < 0 || file >= s_file_count || level < 0 || level >= PLUGIN_LEVELS || !path || !path[0])
        return 0;
    int r = add_line(file, level, path);
    if (r == 0)
        return 0;
    return r == 2 ? 1 : plugins_save();
}

static void add_loc(PluginLoc *out, int *n, int max, const char *dir)
{
    if (*n >= max || !dir[0])
        return;
    for (int i = 0; i < *n; i++)
        if (!strcasecmp(out[i].dir, dir))
            return;
    snprintf(out[*n].dir, sizeof(out[*n].dir), "%s", dir);
    snprintf(out[*n].file, sizeof(out[*n].file), "%s/PLUGINS.TXT", dir);
    ++*n;
}

int plugins_locations(PluginLoc *out, int max)
{
    int n = 0;
    const char *roots[2] = { g_root, other_root() };
    for (int r = 0; r < 2; r++) {
        if (!roots[r])
            continue;
        char ark[80];
        if (r == 0)
            snprintf(ark, sizeof(ark), "%s", sys_ark_dir());
        else
            snprintf(ark, sizeof(ark), "%s/PSP/SAVEDATA/ARK_01234", roots[r]);
        int len = strlen(ark);
        while (len > 4 && ark[len - 1] == '/')
            ark[--len] = 0;
        if (ark[0] && dir_exists(ark))
            add_loc(out, &n, max, ark);
        char se[80];
        snprintf(se, sizeof(se), "%s/SEPLUGINS", roots[r]);
        add_loc(out, &n, max, se);
    }
    return n;
}

int plugins_has(const PluginLoc *loc, const char *path, int level)
{
    for (int i = 0; i < s_count; i++)
        if (!strcasecmp(s_files[s_plugins[i].file].path, loc->file) && level_index(s_plugins[i].where) == level && !strcasecmp(s_plugins[i].path, path))
            return 1;
    return 0;
}

int plugins_install(const char *src, const PluginLoc *loc, unsigned levels)
{
    levels &= (1u << PLUGIN_LEVELS) - 1;
    const char *name = src ? strrchr(src, '/') : NULL;
    if (!name || !name[1] || !loc || !loc->file[0] || !levels || strlen(src) + 16 >= LINE_LEN || !file_exists(src)) /* (no line for a file that is not there) */
        return PLUGIN_FAILED;
    int file = file_index(loc->file, 1); /* (a PLUGINS.TXT that is not there is made with the first line) */
    if (file < 0)
        return PLUGIN_FAILED;
    int room = s_files[file].count; /* the lines have to fit, and the file be written back whole */
    for (int k = 0; k < PLUGIN_LEVELS; k++)
        if (levels & (1u << k))
            room += !plugins_has(loc, src, k);
    if (s_files[file].partial || room > MAX_LINES)
        return PLUGIN_FAILED;
    for (int k = 0; k < PLUGIN_LEVELS; k++)
        if ((levels & (1u << k)) && add_line(file, k, src) == 0)
            return PLUGIN_FAILED;
    return plugins_save() ? PLUGIN_OK : PLUGIN_FAILED;
}
