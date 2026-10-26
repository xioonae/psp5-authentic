/* psp5 – made by Mille Weber */
#ifndef PLUGINS_H
#define PLUGINS_H

#define MAX_PLUGINS 48

typedef struct {
    char name[48];     /* filnamn, t.ex. "leda.prx" */
    char where[24];    /* runlevel som i filen: "always", "game", "vsh", ... */
    unsigned char on;
    int file, line;    /* vilken fil/rad den kommer från */
} Plugin;

/* Läser ARK-mappens PLUGINS.TXT och ms0:/SEPLUGINS/PLUGINS.TXT. */
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

#endif
