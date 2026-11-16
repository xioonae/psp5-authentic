/* psp5 authentic – the text editor of the File Manager: a file that has no formatting (plain text) is opened with ✕; it can be changed when it allows it, else it is only read.
 *
 * One screen for both: the lines of the file are listed on the whole screen, a long line wrapped. As in ARK's editor it goes a line at a time: ↑ ↓ (and the stick; held, it goes faster the longer
 * it is held) choose a line, L and R eight lines, ✕ opens the PSP's own keyboard with the line in it, □ adds a line below the chosen one (typed on the keyboard), START opens the options (Save,
 * a line above, Copy, Paste, Delete, Undo, Go to line); ○ leaves and, when the file was changed, asks whether to save it. A file that cannot be changed (it is not UTF-8 text, or a line is over
 * 1000 bytes) opens on the same screen and only reads: ✕ and □ do nothing, START has Go to line and Copy line, and it says so when it opens. A file is written under a temporary name and put in place, so a card that is full or a
 * failed write leaves the old file as it was. Up to 1 MB and 30000 lines; the line ending (LF or CRLF), the byte order mark and the last line ending are kept. */
#ifndef TEXTEDIT_H
#define TEXTEDIT_H

#include "ui.h"
#include "lang.h"

typedef struct TextEdit TextEdit;

/* Opens the file. NULL when it cannot be opened: *err is the text that tells why (a StrId). */
TextEdit *textedit_open(const char *path, StrId *err);
/* once a frame, with the buttons; returns 1 when it is to be closed (the file has been written or the changes were given up) */
int textedit_update(TextEdit *t, const UiInput *in, float dt);
/* the whole screen (the background, the lines, the popups): no gfx_begin / gfx_end, and not the power popup or the OSD, which the caller draws */
void textedit_draw(TextEdit *t);
/* 1 when a file was written since it was opened (the list of the File Manager shows its new size) */
int textedit_saved(const TextEdit *t);
void textedit_close(TextEdit *t);

/* The first 4 KB of a file, to tell whether it is a file the editor takes: TB_TEXT (view or edit), TB_NOT_UTF8 (view only) or TB_BINARY (not text). -1 when it cannot be read. */
int textedit_sniff_file(const char *path);

#endif
