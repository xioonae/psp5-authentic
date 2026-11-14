/* psp5 authentic – the PSP's own keyboard (the system's on-screen keyboard, sceUtilityOsk) for a line of text: the names in the File Manager and the lines of the text editor */
#ifndef KBD_H
#define KBD_H

#define KBD_KEEP_SPACES 1 /* spaces at the end of the text are kept (the line of a text file); else they are taken off (a FAT name loses them anyway) */
#define KBD_ALLOW_EMPTY 2 /* the user may make the text empty (else an empty answer gives the text as it was) */
#define KBD_MAX 1000      /* the most characters it takes (the limit that is asked for is cut to this) */

/* Shows the keyboard with `init` filled in, with `desc` as its title, and gives the screen below the keyboard to draw every frame (draw may be NULL: black). `limit` is the most characters the user can type.
 * 1 = the user confirmed (the text, in UTF-8, is in out), 0 = it was closed with ○ (or could not be shown). */
int kbd_input(const char *desc, const char *init, char *out, int out_size, int limit, int flags, void (*draw)(void));

#endif
