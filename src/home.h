/* psp5 – made by Mille Weber */
#ifndef HOME_H
#define HOME_H

#include "scan.h"

/* Home is the Games tab: at most HOME_MAX cards, made of the pinned ones (in the order they were pinned) followed by the
 * games and apps opened most recently, newest first, and, when that does not make ten, the other games and apps in the order of "Recently played" (those
 * that were played before, the one played last first, then the ones that were never played, the one added last first), except those the player took
 * off Home. The Library card is one of the ten and is always there: at the far
 * right, after the last game, or among the pinned cards when it is pinned (it cannot be removed, only pinned and unpinned).
 * So at most nine games are shown. The UMD disc is not counted: while one is inserted it is the first card.
 *
 * What Home remembers lives in <data dir>/home.txt: the pinned cards in the order they were pinned (the Library card
 * as "?library"), and a longer list of the cards opened last (so that removing one card brings the next one up). A card
 * is named by entry_cover_key(). The games taken off Home are remembered too (at most HOME_HIDDEN_MAX), so that they are not used to fill the rest of it.
 * Every change is written to the file at once. */
#define HOME_MAX     10
#define HOME_HIDDEN_MAX 100 /* games taken off Home that are remembered */
#define HOME_PIN_MAX 8 /* games that can be pinned: with the Library card that leaves room for the one played last */

/* what home_build() puts in `out` for the Library card (every other value is an index in the game list; -1 and -2 are what the UI
 * uses for "none" and "not known yet", so the Library card is -3) */
#define HOME_LIBRARY (-3)

/* bits of home_state() */
enum { HOME_SHOWN = 1, HOME_PINNED = 2 };

/* Reads home.txt. Keys of games that are gone are dropped. Call again after the game list has been rebuilt. Without the file (first start) nothing is
 * remembered and Home is all of the games that fill it: the ones added last. */
void home_load(const EntryList *list);

/* Where a game comes in "Recently played", a bigger number first: 3 = on Home's list of the cards opened last (the newest first), 2 = played before and
 * not on that list (the one played last first), 1 = never played (the one added last first). The Library sorts "Recently played" by it and the Games tab
 * fills the places that the pinned and the opened cards leave in the same order, so the two always agree. */
u64 home_order_key(const Entry *e);

/* Fills `out` (room for max entries, HOME_MAX + 1 is enough) with the indices in `list` of the cards Home shows, left to
 * right, and HOME_LIBRARY for the Library card. Returns how many. */
int home_build(const EntryList *list, int *out, int max);

/* Where the game is in Home's list of the cards opened last, which is the order of the Games tab after the pinned cards: 0 = the newest,
 * -1 = it is not in the list (never opened and not added by hand, taken off Home, or older than the forty remembered), and always for the
 * disc. The Library's "Recently played" sorting goes by it first, so that the Library and the Games tab show the games in the same order. */
int home_recent_rank(const Entry *e);

/* HOME_SHOWN: the card (list index `index`) is in the Games tab now. HOME_PINNED: it is pinned. The disc counts as shown. */
int home_state(const EntryList *list, int index);

/* The card was started: it becomes the newest. */
void home_note_launch(const Entry *e);
/* Add to Home: the card becomes the newest. */
void home_add(const Entry *e);
/* Remove from Home: unpinned and forgotten, and it stays off Home (the next game takes its place) until it is started or added again. */
void home_remove(const Entry *e);
/* Pin to Home (a card that is not on Home is added first). 0 = HOME_PIN_MAX cards are pinned already, nothing changed. */
int home_pin(const Entry *e);
void home_unpin(const Entry *e);

/* The Library card: pinned or not (it is never removed). Pinning it always works. */
int home_library_pinned(void);
void home_pin_library(void);
void home_unpin_library(void);

#endif
