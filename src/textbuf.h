/* psp5 authentic – the text of a file as lines, for the text editor and viewer of the File Manager (textedit.c). No call of the PSP is made here, so it is tested on a PC (host-tests).
 *
 * A file is a list of lines joined by its line ending (LF or CRLF, the first one found decides), with or without a UTF-8 byte order mark and with or without a line ending after the last line; all
 * three are kept as they were when the file is written again. The lines of an unchanged file point into the buffer the file was read into; a line that was changed has a copy of its own. Every change
 * (replace, insert, delete a line) can be undone, the last TB_UNDO of them. */
#ifndef TEXTBUF_H
#define TEXTBUF_H

#define TB_MAX_FILE  (1 << 20)  /* the biggest file that is opened (a megabyte) */
#define TB_MAX_LINES 30000      /* ... and the most lines it may have */
#define TB_EDIT_MAX  1000       /* the longest line that is edited (bytes): the keyboard of the PSP takes about this much */
#define TB_UNDO      64

typedef struct {
    const char *p; /* the text of the line (not ended with a 0 byte: use len) */
    int len;
    char *own;     /* p points here when the line was changed (else NULL) */
} TbLine;

typedef struct {
    int kind;      /* 0 = the line was replaced (text = what it was), 1 = a line was inserted (undo: delete it), 2 = a line was deleted (text = what it was) */
    int line;
    char *text;
    int len;
} TbUndo;

typedef struct {
    unsigned char *data; /* the file as it was read; the lines of an unchanged file are in it (freed with the buffer) */
    int data_len;
    TbLine *lines;
    int n, cap;
    int crlf, bom, final_eol;
    int utf8_ok;         /* every line is valid UTF-8 */
    int longest;         /* the longest line when the file was read (bytes) */
    int changes, saved;  /* changes made minus changes undone, and its value when the file was written last */
    TbUndo undo[TB_UNDO];
    int nundo;
} TextBuf;

/* what a file seems to be, from its first bytes: text (valid UTF-8, or nothing yet to say it is not), text that is not UTF-8 (shown, never edited), or something else (a 0 byte, or many control characters) */
enum { TB_TEXT, TB_NOT_UTF8, TB_BINARY };
int tb_sniff(const unsigned char *buf, int n);

/* 1 when the bytes are valid UTF-8 (a sequence cut off at the very end counts as valid when `partial` is 1: the end of a part of a file) */
int tb_utf8_valid(const unsigned char *s, int n, int partial);

/* why tb_load() says no */
enum { TB_OK, TB_ERR_MEM, TB_ERR_BINARY, TB_ERR_BIG };

/* Makes the lines of a file. `data` (malloc'd, len bytes) belongs to the result from here on (also when this fails: it is freed). NULL and *err when the file is too big, has a 0 byte, or there is no memory. */
TextBuf *tb_load(unsigned char *data, int len, int *err);
void tb_free(TextBuf *t);

int tb_count(const TextBuf *t);
/* line i (0 .. n-1): its bytes and their number (not ended with a 0 byte) */
const char *tb_line(const TextBuf *t, int i, int *len);

/* 1 when the file may be edited: valid UTF-8 and no line longer than TB_EDIT_MAX (a line that was changed is shorter than that anyway) */
int tb_editable(const TextBuf *t);

/* Changes (each can be undone). The text is copied. 1 = done, 0 = no memory or a bad number (nothing changed). tb_insert() puts the line before `at` (0 .. n: n adds at the end);
 * tb_delete() of the only line makes it empty, the file always has at least one line. */
int tb_replace(TextBuf *t, int at, const char *s, int len);
int tb_insert(TextBuf *t, int at, const char *s, int len);
int tb_delete(TextBuf *t, int at);
/* Takes back the last change: the line it concerned (to put the cursor on), or -1 when there is nothing to take back */
int tb_undo(TextBuf *t);
int tb_can_undo(const TextBuf *t);

/* the file has changed since it was read or written */
int tb_dirty(const TextBuf *t);
/* The bytes to write (malloc'd, *len of them): the lines joined with the line ending the file had, the byte order mark if it had one. NULL when there is no memory. */
unsigned char *tb_serialize(const TextBuf *t, int *len);
/* the file was written: it is not changed any more */
void tb_mark_saved(TextBuf *t);

/* The leading spaces and tabs of a line when they have a tab in them (the length of them), else 0: the keyboard of the PSP may not keep a tab, so the editor keeps such a start of a line out of it */
int tb_lead_tabs(const char *s, int len);

/* the characters (UTF-8 sequences) of a line: the length of the one that starts at s (1 .. 4; 1 for a byte that is not valid, so every byte is shown or kept) */
int tb_char_len(const char *s, int left);

#endif
