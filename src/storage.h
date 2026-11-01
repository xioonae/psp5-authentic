/* psp5 – made by Mille Weber */
#ifndef STORAGE_H
#define STORAGE_H

#include <psptypes.h>

/* What is on the console's storage, for Settings > Storage > Console Storage: for the memory card (and, on a PSP Go, its internal storage too) the
 * size, the free space, and how much of the rest is Games and Apps (gamesize.c), Media Gallery (the MUSIC, VIDEO and PICTURE folders and PSP/PHOTO),
 * Saved Data (PSP/SAVEDATA) and Other (everything else that is used). The folders are counted in a thread of its own; the space is asked at once. */
typedef struct {
    int dev;                       /* 0 = ms0:, 1 = ef0: */
    u64 total, free;               /* 0 when the card can not be asked */
    u64 games, media, saved, other;
    int ready;                     /* 1 when everything has been counted (until then `other` is not known and is 0) */
} StorageInfo;

/* Asks for the space of the cards and starts counting. Call it when the page comes into view; call it again to count again. */
void storage_start(void);
/* Stops the thread (and waits for it). Safe to call at any time. */
void storage_stop(void);
/* How many storages there are to show: 1 (ms0:, or the internal storage of a PSP Go that ARK has put there), or 2 on a PSP Go (internal storage first,
 * then the Memory Stick when there is one). */
int storage_count(void);
/* The numbers of storage `block` (0 up to storage_count() - 1), as far as they are known now. */
void storage_get(int block, StorageInfo *out);

#endif
