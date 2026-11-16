/* psp5 authentic – the text of a file as lines (see textbuf.h) */
#include "textbuf.h"

#include <stdlib.h>
#include <string.h>

int tb_char_len(const char *s, int left)
{
    const unsigned char *u = (const unsigned char *)s;
    if (left <= 0)
        return 0;
    unsigned c = u[0];
    if (c < 0x80)
        return 1;
    int need = c >= 0xF0 && c <= 0xF4 ? 4 : c >= 0xE0 ? (c <= 0xEF ? 3 : 0) : c >= 0xC2 ? (c <= 0xDF ? 2 : 0) : 0;
    if (need == 0 || need > left)
        return 1;
    for (int i = 1; i < need; i++)
        if ((u[i] & 0xC0) != 0x80)
            return 1;
    if (c == 0xE0 && u[1] < 0xA0)
        return 1; /* (a short form of a character that has a shorter one) */
    if (c == 0xED && u[1] > 0x9F)
        return 1; /* (a surrogate) */
    if (c == 0xF0 && u[1] < 0x90)
        return 1;
    if (c == 0xF4 && u[1] > 0x8F)
        return 1;
    return need;
}

int tb_utf8_valid(const unsigned char *s, int n, int partial)
{
    for (int i = 0; i < n;) {
        unsigned c = s[i];
        if (c < 0x80) {
            i++;
            continue;
        }
        int l = tb_char_len((const char *)s + i, n - i);
        if (l > 1) {
            i += l;
            continue;
        }
        /* a byte that is not the start of a valid sequence; at the very end of a part of a file it may be the start of one that was cut off */
        if (partial) {
            int need = c >= 0xF0 && c <= 0xF4 ? 4 : c >= 0xE0 && c <= 0xEF ? 3 : c >= 0xC2 && c <= 0xDF ? 2 : 0;
            if (need > 0 && i + need > n) {
                int ok = 1;
                for (int k = i + 1; k < n; k++)
                    ok &= (s[k] & 0xC0) == 0x80;
                if (ok)
                    return 1;
            }
        }
        return 0;
    }
    return 1;
}

int tb_sniff(const unsigned char *buf, int n)
{
    if (n <= 0)
        return TB_TEXT;
    int ctrl = 0;
    for (int i = 0; i < n; i++) {
        unsigned c = buf[i];
        if (c == 0)
            return TB_BINARY;
        if ((c < 0x20 && c != '\t' && c != '\n' && c != '\r' && c != 0x0C && c != 0x1B) || c == 0x7F)
            ctrl++;
    }
    if (ctrl * 50 > n)
        return TB_BINARY;
    return tb_utf8_valid(buf, n, 1) ? TB_TEXT : TB_NOT_UTF8;
}

static void free_undo(TbUndo *u)
{
    free(u->text);
    u->text = NULL;
}

TextBuf *tb_load(unsigned char *data, int len, int *err)
{
    int dummy;
    if (!err)
        err = &dummy;
    *err = TB_OK;
    if (!data || len < 0 || len > TB_MAX_FILE) {
        free(data);
        *err = TB_ERR_BIG;
        return NULL;
    }
    if (len > 0 && memchr(data, 0, (size_t)len)) {
        free(data);
        *err = TB_ERR_BINARY;
        return NULL;
    }
    TextBuf *t = calloc(1, sizeof(*t));
    if (!t) {
        free(data);
        *err = TB_ERR_MEM;
        return NULL;
    }
    t->data = data;
    t->data_len = len;
    int start = 0;
    if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        t->bom = 1;
        start = 3;
    }
    int lines = 1;
    for (int i = start; i < len; i++)
        if (data[i] == '\n')
            lines++;
    if (lines > TB_MAX_LINES) {
        tb_free(t);
        *err = TB_ERR_BIG;
        return NULL;
    }
    t->cap = lines + 16;
    t->lines = malloc((size_t)t->cap * sizeof(TbLine));
    if (!t->lines) {
        tb_free(t);
        *err = TB_ERR_MEM;
        return NULL;
    }
    for (int i = start; i < len; i++)
        if (data[i] == '\n') {
            t->crlf = i > start && data[i - 1] == '\r';
            break;
        }
    t->final_eol = len > start && data[len - 1] == '\n';
    int s = start;
    while (s <= len) {
        const unsigned char *nl = s < len ? memchr(data + s, '\n', (size_t)(len - s)) : NULL;
        int e = nl ? (int)(nl - data) : len;
        if (!nl && s == len && (t->final_eol || t->n > 0))
            break; /* (nothing after the last line ending: no line there) */
        int l = e - s;
        if (nl && t->crlf && l > 0 && data[e - 1] == '\r')
            l--;
        t->lines[t->n].p = (const char *)data + s;
        t->lines[t->n].len = l;
        t->lines[t->n].own = NULL;
        if (l > t->longest)
            t->longest = l;
        t->n++;
        if (!nl)
            break;
        s = e + 1;
    }
    if (t->n == 0) { /* (an empty file: one empty line) */
        t->lines[0].p = "";
        t->lines[0].len = 0;
        t->lines[0].own = NULL;
        t->n = 1;
    }
    t->utf8_ok = tb_utf8_valid(data + start, len - start, 0);
    return t;
}

void tb_free(TextBuf *t)
{
    if (!t)
        return;
    for (int i = 0; i < t->n; i++)
        free(t->lines[i].own);
    for (int i = 0; i < t->nundo; i++)
        free_undo(&t->undo[i]);
    free(t->lines);
    free(t->data);
    free(t);
}

int tb_count(const TextBuf *t) { return t->n; }

const char *tb_line(const TextBuf *t, int i, int *len)
{
    if (i < 0 || i >= t->n) {
        *len = 0;
        return "";
    }
    *len = t->lines[i].len;
    return t->lines[i].p;
}

int tb_editable(const TextBuf *t)
{
    return t->utf8_ok && t->longest <= TB_EDIT_MAX;
}

static char *dup_bytes(const char *s, int len)
{
    char *c = malloc((size_t)len + 1);
    if (!c)
        return NULL;
    memcpy(c, s, (size_t)len);
    c[len] = 0;
    return c;
}

/* remembers a change; the text becomes the undo's (it is freed with it) */
static void push_undo(TextBuf *t, int kind, int line, char *text, int len)
{
    if (t->nundo == TB_UNDO) { /* the oldest goes */
        free_undo(&t->undo[0]);
        memmove(&t->undo[0], &t->undo[1], (TB_UNDO - 1) * sizeof(TbUndo));
        t->nundo--;
    }
    TbUndo *u = &t->undo[t->nundo++];
    u->kind = kind;
    u->line = line;
    u->text = text;
    u->len = len;
    t->changes++;
}

/* the text of line `at`, as a malloc'd copy for an undo (a line that has a copy of its own hands it over: the line gets another) */
static char *take_text(TbLine *l)
{
    if (l->own) {
        char *c = l->own;
        l->own = NULL;
        return c;
    }
    return dup_bytes(l->p, l->len);
}

int tb_replace(TextBuf *t, int at, const char *s, int len)
{
    if (at < 0 || at >= t->n || len < 0)
        return 0;
    char *nw = dup_bytes(s, len);
    if (!nw)
        return 0;
    TbLine *l = &t->lines[at];
    int old_len = l->len;
    char *old = l->own ? NULL : dup_bytes(l->p, l->len);
    if (!l->own && !old) {
        free(nw);
        return 0;
    }
    if (l->own)
        old = l->own;
    l->own = nw;
    l->p = nw;
    l->len = len;
    push_undo(t, 0, at, old, old_len);
    return 1;
}

int tb_insert(TextBuf *t, int at, const char *s, int len)
{
    if (at < 0 || at > t->n || len < 0 || t->n >= TB_MAX_LINES + 1000)
        return 0;
    if (t->n == t->cap) {
        int cap = t->cap + t->cap / 2 + 16;
        TbLine *nl = realloc(t->lines, (size_t)cap * sizeof(TbLine));
        if (!nl)
            return 0;
        t->lines = nl;
        t->cap = cap;
    }
    char *nw = dup_bytes(s, len);
    if (!nw)
        return 0;
    memmove(&t->lines[at + 1], &t->lines[at], (size_t)(t->n - at) * sizeof(TbLine));
    t->lines[at].p = nw;
    t->lines[at].len = len;
    t->lines[at].own = nw;
    t->n++;
    push_undo(t, 1, at, NULL, 0);
    return 1;
}

int tb_delete(TextBuf *t, int at)
{
    if (at < 0 || at >= t->n)
        return 0;
    if (t->n == 1)
        return tb_replace(t, 0, "", 0);
    TbLine *l = &t->lines[at];
    int len = l->len;
    char *text = take_text(l);
    if (!text)
        return 0;
    memmove(&t->lines[at], &t->lines[at + 1], (size_t)(t->n - at - 1) * sizeof(TbLine));
    t->n--;
    push_undo(t, 2, at, text, len);
    return 1;
}

int tb_can_undo(const TextBuf *t) { return t->nundo > 0; }

int tb_undo(TextBuf *t)
{
    if (t->nundo <= 0)
        return -1;
    TbUndo *u = &t->undo[t->nundo - 1];
    int line = u->line;
    if (u->kind == 0) { /* the line is what it was */
        TbLine *l = &t->lines[line];
        free(l->own);
        l->own = u->text;
        l->p = u->text;
        l->len = u->len;
        u->text = NULL;
    } else if (u->kind == 1) { /* the line that was added goes */
        free(t->lines[line].own);
        memmove(&t->lines[line], &t->lines[line + 1], (size_t)(t->n - line - 1) * sizeof(TbLine));
        t->n--;
    } else { /* the line that was deleted comes back (there is room: it was there) */
        if (t->n == t->cap) {
            int cap = t->cap + t->cap / 2 + 16;
            TbLine *nl = realloc(t->lines, (size_t)cap * sizeof(TbLine));
            if (!nl)
                return -1; /* (nothing was changed) */
            t->lines = nl;
            t->cap = cap;
        }
        memmove(&t->lines[line + 1], &t->lines[line], (size_t)(t->n - line) * sizeof(TbLine));
        t->lines[line].p = u->text;
        t->lines[line].len = u->len;
        t->lines[line].own = u->text;
        t->n++;
        u->text = NULL;
    }
    free_undo(u);
    t->nundo--;
    t->changes--;
    if (line >= t->n)
        line = t->n - 1;
    return line;
}

int tb_dirty(const TextBuf *t) { return t->changes != t->saved; }

void tb_mark_saved(TextBuf *t) { t->saved = t->changes; }

unsigned char *tb_serialize(const TextBuf *t, int *len)
{
    int eol = t->crlf ? 2 : 1;
    long total = (t->bom ? 3 : 0) + (long)(t->n - 1) * eol + (t->final_eol ? eol : 0);
    for (int i = 0; i < t->n; i++)
        total += t->lines[i].len;
    unsigned char *out = malloc((size_t)total + 1);
    if (!out)
        return NULL;
    unsigned char *w = out;
    if (t->bom) {
        *w++ = 0xEF;
        *w++ = 0xBB;
        *w++ = 0xBF;
    }
    for (int i = 0; i < t->n; i++) {
        memcpy(w, t->lines[i].p, (size_t)t->lines[i].len);
        w += t->lines[i].len;
        if (i < t->n - 1 || t->final_eol) {
            if (t->crlf)
                *w++ = '\r';
            *w++ = '\n';
        }
    }
    *len = (int)total;
    return out;
}

int tb_lead_tabs(const char *s, int len)
{
    int k = 0, tab = 0;
    while (k < len && (s[k] == ' ' || s[k] == '\t')) {
        tab |= s[k] == '\t';
        k++;
    }
    return tab ? k : 0;
}
