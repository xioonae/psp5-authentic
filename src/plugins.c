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

#define MAX_FILES 2
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

int plugins_load(const char *ark_dir)
{
    char path[200];
    s_file_count = s_count = 0;
    snprintf(path, sizeof(path), "%sPLUGINS.TXT", ark_dir);
    snprintf(s_ark_file, sizeof(s_ark_file), "%s", path);
    load_file(path);
    /* ARK-4 utan ARK-mapp använder <lagring>/SEPLUGINS/ som ARK-mapp – läs inte samma fil två gånger */
    char se[40];
    snprintf(se, sizeof(se), "%s/SEPLUGINS/PLUGINS.TXT", g_root);
    if (strcasecmp(path, se))
        load_file(se);
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
