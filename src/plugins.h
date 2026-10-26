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

/* The helper plugin (helper.c): are its two lines ("game" and "pops", by the file name) there and on? */
int plugins_helper_on(const char *name);
/* Turns the two lines for the plugin at `prx_path` on or off and adds them (to the file the plugins are in) when they are missing and on = 1;
 * everything else in the files stays as it is. Writes the files; 1 = done. */
int plugins_helper_set(const char *prx_path, int on);

#endif
