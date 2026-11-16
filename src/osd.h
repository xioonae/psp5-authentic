/* psp5 – made by Mille Weber */
#ifndef OSD_H
#define OSD_H

/* Volymindikator i PS5-stil: syns en stund överst när systemvolymen ändras eller
 * ljudet stängs av, var man än är i psp5. */

/* Letar upp systemets volymfunktion (kräver ARK). Anropas efter sys_init. */
void osd_init(void);
/* Läser volymen och räknar animationen – en gång per bildruta. */
void osd_update(float dt);
/* Ritar indikatorn om den syns – sist i bildrutan, före gfx_end. */
void osd_draw(void);

/* The PSP's sound button (♪, next to the volume buttons): an ordinary program cannot read it, like the volume buttons, so it is asked in the kernel (ARK finds
 * sceCtrlPeekBufferPositive, kubridge calls it). Returns 1 when the button has just been pressed (once per press), 0 otherwise or when it cannot be read. */
int osd_note_pressed(void);
/* A picture that osd_draw() draws over the screen, before the volume indicator: the player popup of the sound button (main.c). NULL = none. */
void osd_set_overlay(void (*draw)(void));
/* Shows the volume bar now and keeps it for a moment (the player's volume button: ← → then change the volume, and the bar shows what it is). */
void osd_show_volume(void);

#endif
