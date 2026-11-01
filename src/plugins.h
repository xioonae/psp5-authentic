/* psp5 – made by Mille Weber */
#ifndef PLUGINS_H
#define PLUGINS_H

#define MAX_PLUGINS 48

typedef struct {
    char name[48];     /* filnamn, t.ex. "leda.prx" */
    char where[24];    /* runlevel som i filen: "always", "game", "vsh", ... */
    unsigned char on;
    int file, line;    /* vilken fil/rad den kommer från */
    char path[160];    /* the path as the line has it, e.g. "ms0:/SEPLUGINS/leda.prx" */
} Plugin;

/* Reads the PLUGINS.TXT files that are there: the ARK folder's and <storage>/SEPLUGINS/PLUGINS.TXT, and on a PSP Go with both a Memory Stick and the
 * internal storage the same two on the other storage (ms0: and ef0:); a file is read once, however it is reached. */
int plugins_load(const char *ark_dir);
int plugins_count(void);
Plugin *plugins_get(int i);
/* Skriver tillbaka ändrade filer – bara på/av-fältet ändras. */
int plugins_save(void);

/* The helper plugin (helper.c): is there a line for it ("game" or "pops", by the file name), on or off? */
int plugins_helper_any(const char *name);
/* Adds the two lines ("game" and "pops") for the plugin at `prx_path`, on, to the file the plugins are in (the ARK folder's when there is none),
 * those that are missing; a line that is there stays as it is, and everything else in the files too. Writes the files; 1 = done. */
int plugins_helper_add(const char *prx_path);

/* --- Manage Plugins: where a plugin is installed, installing and uninstalling ---
 * A plugin is a file name (case does not matter). It is "installed" in the places where a line names it: a runlevel ("vsh" the XMB, "game" games
 * and apps, "pops" PS1 games) in one of the files above. Everything here changes the files at once (a file that was only partly read is
 * never written); the lists of plugins_get() are made again afterwards, so indexes taken before are no good any more. */

/* the runlevels that can be added, in this order: the XMB, games, PS1 */
#define PLUGIN_LEVELS 3
extern const char *const PLUGIN_LEVEL[PLUGIN_LEVELS]; /* "vsh", "game", "pops" */

/* the distinct plugin names that have a line (first appearance first); pointers stay until the next load or change */
int plugins_names(const char **out, int max);
/* the lines of one plugin: indexes for plugins_get() */
int plugins_lines_of(const char *name, int *out, int max);

/* A place a plugin could still be added to: a runlevel that has no line in a file that already lists the plugin (with that path). A plugin that has
 * an "always" line in a file is in every place of it already. */
typedef struct {
    int file;           /* the file (plugins_file_path) */
    char path[160];     /* the path to write in the line: the one the plugin's other lines in that file have */
    int level;          /* index in PLUGIN_LEVEL */
} PluginFree;
int plugins_free_places(const char *name, PluginFree *out, int max);

/* the path of file `file`, e.g. "ms0:/SEPLUGINS/PLUGINS.TXT" ("" for a number that is no file) */
const char *plugins_file_path(int file);
/* what a file is called for the player: its folder and its storage, "SEPLUGINS (ms0:)" or "ARK_01234 (ef0:)"; `path` is the path of a PLUGINS.TXT */
void plugins_place_label(const char *path, char *out, int size);

/* Takes the lines `idx` (indexes for plugins_get()) out of their files and writes the files. 1 = done, 0 = nothing was changed (a file could not be
 * written back whole, or an index is no line). The plugin file itself is not touched. */
int plugins_remove(const int *idx, int n);
/* Adds the line "<level>, <path>, on" to file `file`, unless it is there, and writes the file. 1 = done (or already there). */
int plugins_add(int file, int level, const char *path);

/* Where a new plugin can be put: a folder and the PLUGINS.TXT that goes with it. */
typedef struct {
    char dir[80];       /* "ms0:/SEPLUGINS" */
    char file[96];      /* "ms0:/SEPLUGINS/PLUGINS.TXT" */
} PluginLoc;
#define PLUGIN_LOCS 4
/* The places a plugin can be installed to, in this order: the ARK folder (only when it is there), then SEPLUGINS, on the storage psp5 runs from; then
 * the same two on the other storage of a PSP Go. */
int plugins_locations(PluginLoc *out, int max);
/* 1 = a line for the plugin `name` in the runlevel PLUGIN_LEVEL[level] is in the file of `loc`, with the path of `loc` (the folder and the name) */
int plugins_has(const PluginLoc *loc, const char *name, int level);
/* Installs the plugin file `src` (a .prx somewhere on a storage) to `loc`: moves it there (a rename on the same storage, else a copy and the old
 * one deleted; it stays where it is when it is there already) and adds the lines for the runlevels in `levels` (bit k = PLUGIN_LEVEL[k], at least one),
 * those that are not there already. PLUGIN_OK, PLUGIN_EXISTS (another file with that name is in the folder) or PLUGIN_FAILED. Nothing is moved when
 * the lines could not be added. */
enum { PLUGIN_FAILED, PLUGIN_OK, PLUGIN_EXISTS };
int plugins_install(const char *src, const PluginLoc *loc, unsigned levels);

#endif
