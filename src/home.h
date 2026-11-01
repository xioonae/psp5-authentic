/* psp5 – made by Mille Weber */
#ifndef HOME_H
#define HOME_H

#include "scan.h"

/* Home is the Games tab: at most HOME_MAX cards, in the order of "Recently played": first the games and apps opened most recently, newest first, then the ones
 * that were played before (the one played last first), then the ones that were never played (the one added last first). The Library card is one of the
 * ten and is always the last. So at most nine games are shown. The UMD disc is not counted: while one is inserted it is the first card.
 *
 * Kept games: a game that is "kept in Home" is always among the cards, however long ago it was played. It stays in its place in the order, unless that
 * place is beyond the nine: then it takes one of the last places (and the games that are not kept and come last give way). At most HOME_KEEP_MAX can be kept.
 *
 * What Home remembers lives in <data dir>/home.txt: the games kept ("keep <key>", in the order they were kept) and a longer list of the cards
 * opened last ("recent <key>"). A card is named by entry_cover_key(). Older files have "pin" lines (the games pinned: they are kept now) and
 * "hide" lines (games taken off Home: nothing is taken off Home any more); the hide lines are ignored, and the next write leaves them out.
 * Every change is written to the file at once. */
#define HOME_MAX      10
#define HOME_KEEP_MAX 9 /* games that can be kept: all the places but the Library card's */

/* what home_build() puts in `out` for the Library card (every other value is an index in the game list; -1 and -2 are what the UI
 * uses for "none" and "not known yet", so the Library card is -3) */
#define HOME_LIBRARY (-3)

/* bits of home_state() */
enum { HOME_SHOWN = 1, HOME_KEPT = 2 };

/* Reads home.txt. Keys of games that are gone are dropped. Call again after the game list has been rebuilt. Without the file (first start) nothing is
 * remembered and Home is all of the games that fill it: the ones added last. */
void home_load(const EntryList *list);

/* Where a game comes in "Recently played", a bigger number first: 3 = on Home's list of the cards opened last (the newest first), 2 = played before and
 * not on that list (the one played last first), 1 = never played (the one added last first). Home goes by it (and the Library's sorting by it used to). */
u64 home_order_key(const Entry *e);

/* Fills `out` (room for max entries, HOME_MAX + 1 is enough) with the indices in `list` of the cards Home shows, left to
 * right, and HOME_LIBRARY for the Library card. Returns how many. */
int home_build(const EntryList *list, int *out, int max);

/* Where the game is in Home's list of the cards opened last: 0 = the newest, -1 = it is not in the list (never opened, or older than the forty
 * remembered), and always for the disc. */
int home_recent_rank(const Entry *e);

/* HOME_SHOWN: the card (list index `index`) is in the Games tab now. HOME_KEPT: it is kept in Home. The disc counts as shown. */
int home_state(const EntryList *list, int index);

/* The card was started: it becomes the newest. */
void home_note_launch(const Entry *e);

/* Keep in Home (on = 1) or not (on = 0). 0 is returned when nothing was changed: the disc cannot be kept, and HOME_KEEP_MAX games are kept already. */
int home_keep(const Entry *e, int on);

/* The game is gone (deleted): it is not kept and not in the list of the cards opened last any more. */
void home_forget(const Entry *e);

#endif
