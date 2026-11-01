/* psp5 – made by Mille Weber */
#ifndef SETTINGS_H
#define SETTINGS_H

#include "ui.h"

/* Det inställningsskärmen ber huvudprogrammet att göra */
enum {
    SA_NONE, SA_CLOSE, SA_USB, SA_XMB, SA_RESTART, SA_SUSPEND, SA_SHUTDOWN, SA_RECOVERY, SA_CLEAR_CACHE, SA_FILES, SA_WIFI, SA_FTP,
    SA_GAMES, /* Console Storage > Games and Apps: the list of the games and apps (saves.c), on the storage settings_action_dev() says */
    SA_SAVES, /* Console Storage > Saved Data: Manage Saved Data (saves.c), on the same storage */
};

/* The storage of the row that gave SA_GAMES or SA_SAVES: 0 = the memory card (ms0:), 1 = the internal storage of a PSP Go (ef0:), -1 = all (one storage only) */
int settings_action_dev(void);
/* A screen that Settings opened (SA_GAMES, SA_SAVES) is over: Settings counts its storage again. */
void settings_back(void);

/* 1 = the action is a visit: when it is over psp5 comes back to Settings, on the same row (the PSP's own Wi-Fi screen, the FTP server, the File Manager, USB Mode), and not to the
 * main screen like after the others. */
int settings_is_visit(int action);

/* 1 = a choice's popup is open: SELECT does not open the power popup then (main.c, power.c). */
int settings_modal(void);

/* Läser ARK-inställningar och plugins och visar första kategorin. */
void settings_open(void);
int settings_update(const UiInput *in, float dt);
void settings_draw(void);
/* A short message on top of Settings for a few seconds (the toast of the other screens): used when the PSP's own Wi-Fi screen could not start. */
void settings_notice(const char *msg);
/* Sparar ändrade filer. Returnerar 1 om CFW-inställningar ändrades (gäller efter omstart). */
int settings_commit(void);

#endif
