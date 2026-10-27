/* psp5 – made by Mille Weber */
#ifndef WIFI_H
#define WIFI_H

/* The PSP's own screen for choosing a Wi-Fi connection: the network dialog that games show (sceUtilityNetconf, the "connect to an access
 * point" one). psp5 does not draw any of it. The caller has set up the graphics (gfx_init) and nothing else is running.
 * Runs until the dialog closes (or *running becomes 0). Returns 1 when a connection was chosen and made, 0 when the player closed the dialog
 * without one, -1 when the network could not be started (no memory, the modules would not load). Everything that was started is stopped again,
 * so the PSP is not left connected. */
int wifi_dialog(volatile int *running);

#endif
