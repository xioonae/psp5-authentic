/* psp5 – made by Mille Weber */
#ifndef SCAN_H
#define SCAN_H

#include <psptypes.h>

typedef enum {
    ENTRY_ISO,      /* iso/cso/zso/dax/jso i ms0:/ISO */
    ENTRY_HOMEBREW, /* EBOOT.PBP, kategori MG */
    ENTRY_PSN,      /* EBOOT.PBP, kategori EG */
    ENTRY_POPS,     /* EBOOT.PBP, kategori ME (PS1) */
    ENTRY_UMD,      /* skivan i UMD-enheten (umd.c) – sparas aldrig i listcachen */
} EntryType;

typedef struct {
    char path[256];
    char title[128];
    char game_id[16]; /* DISC_ID, t.ex. "ULUS10041" */
    u32 type;         /* EntryType */
    /* ICON0.PNG / PIC1.PNG: offset+storlek i PBP-filen eller (okomprimerat) i ISO:n.
     * För UMD: offset 0 och filens storlek på skivan. */
    u32 icon_offset, icon_size;
    u32 pic_offset, pic_size;
    /* för att känna igen oförändrade filer i listcachen */
    u32 file_size, file_time;
} Entry;

typedef struct {
    Entry *items;
    int count;
    int capacity;
} EntryList;

/* En ändringstid som ett heltal som går att jämföra: år, månad, dag, timme, minut, sekund (samma som Entry.file_time) */
u32 scan_pack_time(const ScePspDateTime *t);

/* Anropas när en fil faktiskt läses (inte från cache). */
typedef void (*ScanProgress)(int done, const char *name);

/* Skannar ISO och PSP/GAME på minneskortet (ms0:) och i PSP Go:s internminne (ef0:). `self_path` (argv[0]) hoppas över.
 * `cache_file` (kan vara NULL) används för att slippa läsa om oförändrade filer. */
void scan_all(EntryList *list, const char *self_path, const char *cache_file, ScanProgress progress);
void scan_free(EntryList *list);

/* Lägger in en post först i listan (UMD-skivan) / tar bort den första. */
int scan_insert_front(EntryList *list, const Entry *e);
void scan_remove_front(EntryList *list);
/* Tar bort post nummer i (ett raderat spel). Efterföljande poster får ett index mindre. */
void scan_remove_at(EntryList *list, int i);

/* Titel och spel-ID ur en PARAM.SFO. */
void entry_read_sfo(Entry *e, const u8 *sfo, u32 size);

/* Namn för egna omslag: DISC_ID för ISO och UMD, mappnamnet för EBOOT. */
void entry_cover_key(const Entry *e, char *out, int out_size);

#endif
