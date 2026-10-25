/* psp5 – made by Mille Weber */
#ifndef FOLDERS_H
#define FOLDERS_H

#include "scan.h"

/* The folders of the Library: the category folders that the Categories Lite plugin (and arkMenu) use, e.g.
 *   ms0:/PSP/GAME/Racing/<game>/EBOOT.PBP   and   ms0:/ISO/Racing/<game>.iso
 * both sit in the folder "Racing". A game that is straight in PSP/GAME (ms0:/PSP/GAME/<game>/EBOOT.PBP) or in ISO is in no
 * folder. The folder of a game is the last folder before the game's own folder (EBOOT) or before the image file (ISO, CSO, ...).
 * Nothing here is stored: the folder is worked out from the path of the entry, so the list cache is the same as before. */

#define FOLDER_NAME_MAX 64
#define FOLDER_INITIAL_MAX 8 /* the letter(s) on a folder card, UTF-8 */

/* The name of the folder as it is on the card, "01Racing" or "CAT_Racing" as well: copied to `out` (cut to `size`) and 1 returned
 * when the game at `path` is in a folder, else 0 and `out` empty. */
int folder_raw(const char *path, char *out, int size);

/* The name Categories Lite shows: a "CAT_" in front is dropped, and so are the two digits that sort the categories ("01Racing" and
 * "CAT_01Racing" are "Racing"); the digits stay when they are the name ("2048", "007 Games") or the whole name. */
void folder_clean(const char *raw, char *out, int size);

/* The letter on the folder's card: the first letter of the name as a capital, or two letters when the name starts with a
 * pair that is one letter of some alphabet ("Ch" of Czech, "Dz", "IJ" of Dutch, "Lj", "Ll", "Nj"). A name that starts with
 * something that is not a letter (digits, symbols, Japanese) gives its first character as it is. */
void folder_initial(const char *name, char *out, int size);

/* The text in capitals, for a tag ("Folder" -> "FOLDER", "Složka" -> "SLOŽKA", "Папка" -> "ПАПКА"): the letters of the alphabets psp5 is in
 * (ASCII, Latin-1, Latin Extended-A, Greek, Cyrillic); everything else (Japanese, digits, signs) stays as it is. Cut to `size`. */
void folder_upper(const char *s, char *out, int size);

typedef struct {
    char name[FOLDER_NAME_MAX]; /* the clean name */
    int count;                  /* how many games and apps are in it */
    u32 newest;                 /* the newest file time of those (Entry.file_time) */
} Folder;

typedef struct {
    Folder *items;
    int count;
    int *of; /* for every entry of the list: the number of its folder, or -1 when it is in none */
    int of_count;
} FolderSet;

/* Finds the folders of a list: folders with the same clean name (without regard to capitals) are one, wherever they are (GAME
 * and ISO, the Memory Stick and a PSP Go's storage). The folders are in the order of their names. */
void folders_build(const EntryList *list, FolderSet *fs);
void folders_free(FolderSet *fs);
/* The number of the folder with this name (capitals do not matter), or -1 */
int folders_find(const FolderSet *fs, const char *name);

#endif
