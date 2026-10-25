# psp5-authentic – own launcher for PSP with ARK-4/ARK-5 – developer notes

**psp5-authentic is a fork of [psp5](https://github.com/08Weber/psp5) by Mille Weber (08Weber), based on psp5 v1.5.3.** The
goal of the fork is to be as authentic as possible to its home console counterpart. These are the developer notes: what each
change touches in the code. The list for users is in [`CHANGELOG.md`](../CHANGELOG.md). The older sections at the bottom
(1.5.2 and earlier) are the original project's own history; they were written in Swedish and have been translated.

A dark launcher with a row of game cards, the game's picture as the background, and tabs for games (Home) and music; the library is a card of Home.
All graphics are its own. The launcher is a normal app in `ms0:/PSP/GAME/PSP5` and never touches flash0, so the XMB is
always still there (HOME in psp5 = back to the XMB).

## psp5 as ARK's launcher
psp5 replaces ARK-5's own launcher (arkMenu): the file `ms0:/PSP/SAVEDATA/ARK_01234/VBOOT.PBP` is psp5.
ARK's "Custom Launcher" in the XMB then starts psp5, and with **Settings → psp5 → Boot into psp5** the PSP starts straight
into psp5 instead of the XMB.

- **Emergency exit:** hold **SELECT** while the PSP starts – ARK then skips SETTINGS.TXT and you land in the XMB.
  Hold **START** to skip all plugins.
- **Exit to XMB** (Settings → Power and USB) turns the launcher mode off for that session.
- **Back to arkMenu:** copy `ARK_01234/VBOOT_ARKMENU.PBP` to `VBOOT.PBP` (a copy is also in `backup/`).
- An ARK update can overwrite `VBOOT.PBP` – then copy `dist/PSP/GAME/PSP5/EBOOT.PBP` in as `VBOOT.PBP` again.

## After 1.1.0 (next version)

All changes after 1.1.0 are noted here (and under *Unreleased* in `CHANGELOG.md`) until the next version is released.

- **Library: the view button and folders** (`ui.c`, `ui.h`, new `folders.c` / `folders.h`, `icons.c`, `gfx.c`, `lang.c`, `config.c`).
  - *Where a game is* (`folders.c`, `folder_raw`): nothing is stored (no change in the list cache): the folder comes from
    `Entry.path`. After the first `/PSP/GAME/` or `/ISO/` of the path (also `ef0:`), the parts are counted: for ISO the folder is the
    part before the file (needs 2 parts), for PSP/GAME the part before the game's own folder (needs 3). `PSP/GAME/Hello/EBOOT.PBP` and
    `ISO/x.iso` are in no folder; `PSP/GAME/PS1/Crash/EBOOT.PBP` is in "PS1". `folder_clean` drops `CAT_` (any case) and two digits that
    are followed by a non-digit (`01Racing`, `CAT_01Racing` -> "Racing"; "2048", "007 Games" and a name that is only the prefix stay).
    `folder_initial`: the first letter as a capital (ASCII, Latin-1, Latin Extended-A, Greek, Cyrillic; other characters as they are),
    two letters for Ch, Dz, IJ, Lj, Ll, Nj (written Ch, Dz, IJ, Lj, Ll, Nj), skipping a leading space `_ - . [ (`.
    `folders_build` makes the `FolderSet` (names sorted without regard to case, equal names are one `Folder` with `count` and
    `newest` file time, `of[i]` = folder of list entry i or -1). `Ui.folders` is built again in `build_views`.
  - *The cards*: a folder card in the Library's `LoaderView` is `LIB_FOLDER_ID(n)` = -10 - n (`ui.h`; the loader leaves every negative
    index alone, like `HOME_LIBRARY` = -3). `ui_selected()` is -1 on a folder (no background, no options). `rebuild_library()`
    makes the list for `lib_view` (`LIB_VIEW_ALL / FOLDERS / LOOSE / BOTH`, in the order of the popup and of `libview=` in `psp5.cfg`) and
    `lib_open` (the open folder, -1 = none): folders first (by name = their number, or the newest `Folder.newest` first), then the games
    of the view (`cmp_name` / `cmp_recent`, the UMD first); in BOTH the disc is moved in front of the folders. It builds in a
    temporary array and copies it into the same buffer (the loader thread reads it) and sets `count` last. `sort_library()` is gone.
    `lib_open` is kept as a number and as a name (`lib_open_name`): `build_views` finds the folder again by its name after the
    list changed and closes it (`lib_open = -1`) when its last game is gone. `relist()` also remembers the name of a selected folder
    card and selects it again. Leaving the Library (`set_tab`) closes an open folder (`leave_folder`).
  - *Buttons*: `lib_focus` is 0 = a card, 1 = Sort (`LIB_BTN_SORT`), 2 = the view button (`LIB_BTN_VIEW`). `lib_buttons()` says which
    exist: Sort with 2 or more cards, the view button unless a folder is open (and when the list has any game). ↑ from the first row
    (`lib_button_above`): the nearest button by the distance from the middle of the card to the button's frame (equal: the view
    button), none: straight to the cog wheel. ← → move between the two, ↓ / ○ to the cards (with no cards ○ closes the Library), ↑
    to the cog wheel; ↓ from the cog wheel returns to the button ↑ came from, as before. With no card in the view (`FOLDERS` with
    no folders, `LOOSE` with everything in folders) `lib_focus` is forced to the view button. ○ on a card: an open folder closes
    (`close_folder`: the folders again, on its card), else the Library closes. ✕ on a folder opens it (`activate_card`). START
    does nothing on a folder. New `view_anim` / `view_frame` like `sort_anim` / `sort_frame` (`sort_anim` now only follows
    `lib_focus == 1`).
  - *The row at the top* (`lib_header`, used by the drawing and by the ↑ choice): the view button's text is the name of the view
    (`S_LIB_ALL`, `S_LIB_IN`, `S_LIB_OUT`, `S_LIB_BOTH`) **with a ":" after it** ("All: 15"), or inside a folder the folder's name with a ":"
    ("Racing: 3"), without a frame; the number of cards is *inside* the frame now (`view_r` = the end of the number + `SORT_PAD`), a
    space (`gap`, whole pixels) after the colon; size 0.66, smaller
    (`text_fit`, down to 0.45) when it does not fit in the room left of the Sort button (12 px gap, the number of cards 8 px after the
    text); when even 0.45 is too wide next to the number, the number is dropped, then the end of the text is cut ("..."). The frame is
    `SORT_PAD` (9 px) wider than the text on each side and 22 px high, on the same line as the Sort frame
    (`popup_label_mid(53, SORT_SIZE)`); the baseline of the text is worked out from its size (`popup_label_mid`) so that the middle of the text stays
    on that line, whatever the size (`text_fit` may have made it smaller). The view popup (`MENU_VIEW`, four `MI_VIEW_*`) opens at x = 15 (the frame's left edge), y = 66, as wide as the widest
    label + 32 (at least 200, at most 450).
  - *Drawing*: the folder card is a card in the theme's colour (`theme_cur()->card`, like the Library card) with `gfx_folder_icon` in a
    light grey (200 .. 255 with the focus) and the letter in the card's colour, size 0.037 x the height of the body (a capital is about half of it), on
    whole pixels, centred in the body. In the bottom row the tag is `L(S_I_FOLDER)` ("Folder", an existing string, used as it is by the Information screen) **in capitals**:
    `folder_upper()` (`folders.c`, UTF-8: ASCII, Latin-1, Latin Extended-A, Greek, Cyrillic; Japanese and anything else as it is, a
    damaged or 4-byte sequence is copied unchanged) turns it into "FOLDER" like the tags PSP / APP, in every language; the title is the
    folder's name; an empty view shows `S_LIB_NO_IN` / `S_LIB_NO_OUT` at (24, 120), size 0.9.
  - *The folder picture* (`icons.c`, `ICON_FOLDER`, `icon_folder_pic`, `gfx_folder_icon`): one image per size like `ICON_LIBRARY`: a body
    and a tab joined by the smaller of two signed distances, so there is one anti-aliased image (no double blending while it fades);
    the height is about 2/3 of the card's. The shape follows a folder icon the maintainer sent (47 x 34 px): the numbers were found by
    fitting this shape to that picture (a blur of 0.48 px on the shape, least squares over the pixels; the fit is within a quarter of a
    pixel at 23 px high): width 1.38 x the height; the tab sticks up 16 % of the height above the body and is 45 % of the width wide;
    the body's corners (the two at the bottom and the top right one) have 17 % of the height as radius, the tab's top left corner (which is
    also the folder's top left corner, so the body's own corner there is hidden) 31 % (at most half the tab) and its top right corner 23 %:
    a soft shoulder that runs down into the top edge of the body. `FolderPic` has `r`, `tab_rl`, `tab_rr` for the three radii; the tab is a
    box with two rounded top corners (`sd_rbox_top`, exact when the box is at least twice as high as its radii: it reaches down into the body
    that far). All straight edges are on whole pixels (fine sampling under 24 px). It is cheap, so it goes through the normal icon cache
    (`cache_get`), not the baked pool of the Library picture.
  - *The letter on the folder* (`gfx_folder_letter_at` in `gfx.c`, drawn by `draw_card`): in the middle of the body (the part under the
    tab): sideways text_draw's ALIGN_CENTER (the advance of the letter centred), up and down half a capital (13.7 x the size / 2) above the
    baseline, both on whole pixels, size 0.037 x the height of the body. The height of a capital, 13.7 px at size 1.0, was measured on the
    README screenshots (PPSSPP, the real PSP font): the A of the Library title at size 0.66 is 9 px high, the clock's digits at 0.8 are
    11 px. (Before, the baseline was 7 x the size below the middle, and the mock drew the single letter wrongly, to the right of its place:
    the mock spreads the letters over 6 px a byte, so the letter looked off centre in the readable test frames.)
  - *Strings* (`lang.c`): `S_LIB_ALL` says "All" (it was "All games and apps"), `S_LIB_IN` "In folders", `S_LIB_OUT` "Outside of folders",
    `S_LIB_BOTH` "Both in and out of folders" (no colon in the string: `lib_header` adds it), new `S_LIB_NO_IN`, `S_LIB_NO_OUT`; `S_SORT` says
    "Sort by" ("Sorteren op", "Trier par" ... in Czech and Japanese unchanged: "Seřadit", 並べ替え); it is also the word of the Sort button in
    Saved Data (`saves.c`). New `S_UNIT_H` / `S_UNIT_MIN` for the time played. The translations (11 languages) are mine and have not
    been checked by native speakers. 169 strings now. `LibHeader.sort` is 96 bytes: Russian "Сортировать по: Недавно добавленные" is 66
    bytes, and with 64 it was cut in the middle of a letter (found by `library_test`: the Sort frame has to be as wide as the whole text).
  - *Kept between starts* (`config.c`): `libsort=0|1` and `libview=0..3` in `psp5.cfg` (`g_lib_sort`, `g_lib_view`); `ui_init` reads
    them, `set_sort` / `set_view` write them with `config_save()` (the buffer is 192 bytes now). A value that does not exist is the
    first one.
  - *Tests* (`host-tests`, not under git): `units/folders_test.c` (paths, names, letters, merging), `ui/library_test.c` (new: all views and
    sorts, the button and popup, opening and closing, the empty views, a deleted game, `psp5.cfg`, drawn letters and the row in
    11 languages), `ui/folder_fixture.h`, `icons/foldertest.py` (the shape and the fractions of the fit), `gfx/foldertext_test.c` (the ink of the capital letters
    A-Z and 0-9 in the folder body at the seven Library card sizes: within a pixel of the middle sideways and up and down, on average within a
    third of a pixel; with `MOCK_REAL=1` the mock measures text by the advances of its letters and draws it at the real size, so the test needs
    the atlas from `mkatlas.py`; the PC font's letters are shaped like the PSP's but are not the same); `ui/menu_test.c` and `ui/pixel_test.c` changed for the view button
    (↑ from the first card is the view button; one card has the view button but no Sort button) and draw every view, every folder
    card, the popup and an open folder through the whole-pixel check (`menu_test.c`'s check "↑ from the first row selects the nearest
    button" now takes the two frames from what is drawn, because the mock makes the Russian Sort button as wide as the row). Measured on a PC only. The mock measures Cyrillic and Japanese
    as 2 and 3 bytes of 6 px, wider than the real font, so how much of the label is cut in Russian and Japanese on a real PSP is not known.
- **Time played** (`playtime.c` / `playtime.h` new, `main.c`, `ui.c`, `ui.h`, `lang.c`, `Makefile`).
  - *What is known*: psp5 is not running while a game is. In launcher mode (`sys_is_launcher()`) ARK starts psp5 again when the game ends,
    and the existing `ingame` marker (`<DATA_DIR>/ingame`, created before the launch, removed at the start so that the start-up screen
    is skipped) is the place for the start. As a normal app the PSP goes back to the XMB and psp5 is not started again: no marker is
    written (`set_ingame_mark` only in launcher mode) and nothing is counted.
  - *The marker*: `playtime_mark_write(path, entry, sys_us, rtc_us)` writes `psp5-ingame 1\nsys N\nrtc N\nkey NAME\n`: the two clocks
    (`sceKernelGetSystemTimeWide()` and `sceRtcGetCurrentTick()`, both microseconds) and the key of the game (`entry_cover_key()`: the
    disc ID of an ISO/UMD, else the folder name of an EBOOT or the file name without the extension; a game that is in two places is one).
    `playtime_mark_take()` (first thing in `main`, before `app_start`, replaces the old `take_ingame_mark`) removes the file, returns 1 if there
    was one, and adds the time to the game: the smaller of the two clocks' differences, a clock that went backwards (a restart of the PSP; the
    real-time clock set back) is not used, and a session under `PLAYTIME_MIN_SECS` (10 s) or over `PLAYTIME_MAX_SECS` (a day) adds nothing. An
    empty marker (written by an older psp5), a marker cut off anywhere (the key line must end with a newline), a number that is too big
    or a name that is too long are all "a game ended, nothing to add". `%llu` is not used in `printf` (the PSP's libc): the numbers are
    written and read by hand.
  - *The file* `<DATA_DIR>/playtime.txt`: a line per game `"<seconds> <key>"`, up to 4096 games (the first wins when a name is there twice; a full
    table takes no new game but keeps the others; CRLF is fine; a bad line is skipped). It is written when a session is added
    (whole file, `sceIoOpen` + write). Sums stop at `0xFFFFFF00` (no overflow).
  - *The text* `playtime_text(secs, out, size)`: under 60 s nothing; under an hour "%umin"; else the hours, rounded down, at most 9999,
    "%uh", the units `S_UNIT_MIN` / `S_UNIT_H` (h, min; Dutch u; Russian ч, мин; Japanese 時間, 分). Never both, as the maintainer asked.
  - *The pill* (`ui.c`, `draw_play_pill`; `PILL_H` 26, `PILL_PAD` 12, `CLOCK_D` 12, `CLOCK_GAP` 6, text size 0.66, baseline `by + 17.5` like
    *Start*): drawn with `button_bg` (the dark pill of the bottom-row buttons, never selected), a white face (`gfx_circle`, 12 px) with two
    dark hands (1 x 5 up from the middle and 4 x 1 to the left, `gfx_rect`, whole pixels) and the text, its right edge at `SCREEN_W - MARGIN`
    (456); in the Games tab (`draw_details`) opposite *Start* / Options (the pill is clear of the Options circle by at least 10 px in all
    11 languages), in the Library (bottom row) opposite the tag and the title, whose room `max_w` is 12 px less than the pill (the cache of
    the shortened title, `lib_label`, also remembers its room: `lib_label_room`). Nothing is drawn for a text that is empty (never played,
    under a minute), a folder card, or the Library card. `play_text()` looks the seconds up once per selected game (`pt_for`, `pt_secs`; reset
    next to every `lib_label_for = -2`) and makes the text at every draw, so a language changed in Settings is followed.
  - *Limits*: only the time between the start of the game and the next start of psp5 is known, so whatever happens in between is in it
    (a PSP that sleeps while the game runs is counted only if the system clock keeps running during sleep - the smaller of the two clocks is
    taken, and the system time is thought to stand still, which was not measured; a PSP that is switched off or whose battery runs out
    during a game and is started again within a day: the system time then starts from zero, so only the real-time clock is left and it also
    counts the time the PSP was off). Sessions under 10 s or over a day are dropped. Nothing was tried on a PSP.
  - *Tests* (`host-tests`, not under git): `units/playtime_test.c` (new, 191 checks: no file, one and several sessions, the two clocks,
    backwards clocks, 9 / 10 s, a day, empty / cut at every length / random / huge markers, the file's format and limits, the same game in
    another file, the texts in 11 languages; mutation-tested), `ui/library_test.c` (the clock in the Library and the Games tab: exact place of
    the text and of the two hands, nothing for unplayed games, folders or the Library card, the title cut before the clock, the room in 11
    languages), `ui/pixel_test.c` (every game has a time: the clock is drawn through the whole-pixel check in every state).
- **.jso files are read** (`lzo.c`, `lzo.h` new; `scan.c`, `info.c`, `gameops.c`, `files.c`, `iso.c`): a JSO is a CISO-like container
  ("JISO", block size at 6, method at 0x0A: 0 = LZO, 1 = zlib, unpacked size at 0x0C); libcisoread already reads the container and
  calls `lzo1x_decompress(src, src_len, dst, &dst_len, wrk)`, which `iso.c` only had as an empty stub returning -1. `lzo.c` is a decoder
  of the LZO1X format that checks every read and write against the end of the buffers (a damaged block returns an error, never
  writes outside `dst`; `dst_len` is the number of bytes written). `scan.c` lists `.jso`, `info.c` recognises `JISO` (label "JSO", size
  = 32 bits at 12), `gameops.c` may delete it, `files.c` colours it like the other images. Tests: `units/lzo_test.c` with
  `units/lzo_vectors.py` (196 pairs packed by liblzo2: both packers, runs, long matches, random data; each cut off at many lengths
  and damaged at random, in exactly sized buffers under AddressSanitizer; 100 % of the lines of `lzo.c`), and the scanner, the
  Information and delete tests with a made-up `.jso`. No real `.jso` file and no PSP was used.

- **PS1 theme: the symbols moved down** (`theme.c`, `theme_backdrop`): `deco_ps1(time, a, 418, 30, 0.6f)` became
  `deco_ps1(time, a, 418, 130, 0.6f)`; only the centre's y changed (x 418 and the size 0.6 are as before). At y = 30 the group
  (a diamond of four symbols, `d` = 14.4 px from the centre, `r` = 6.3 px, a glow of 8 px, a bob of +-2 px) covered about y 8..52
  and sat right behind the clock (`text_draw` at y 25, x 416..456) and the battery (`bx` about 384, y 15..), so the symbols were
  half hidden. At y = 130 it covers about y 101..160 and x 395..441: below Home's row of cards (`ROW_Y` 38 + `BIG_H` 62 = 100,
  titles end at about 99) and above the bottom row (`BOTTOM_Y` 243, Start and Options are at the left). Still one place on every
  screen, because `theme_backdrop` draws it for all of them. Looked at in PC test frames of Home and of the Library (a copy of
  `ui/menu_test.c` with `g_theme` = 12; not on a PSP): on Home it is clear of everything; in the Library it is fainter than on
  Home because of the Library's own darkening (`DIM_COLOR`, as before the move), and the grid cards of the second row (from the 10th
  game on, worked out from `GRID_Y` / `ROW_STEP` / `CELL_W`, not drawn) sit over most of it. The Settings, file manager,
  Information and Manage Saved Data lists were not looked at one by one: their rows are drawn over the symbols.
  `host-tests/gfx/theme_test.c` (not under git) now checks that the symbols change pixels at x >= 380, y 100..165 (more than 300),
  none at x < 300, none in the top 100 rows and none in the bottom 42.

## Version 1.1.0 (psp5-authentic)

Everything that changed after 1.0.0 – see `CHANGELOG.md` for the version written for users.

- **PSP Go with ARK-4: the games of the Memory Stick Micro show up when psp5 runs from the internal storage** (`Makefile`):
  `SFOFLAGS = -d EFAWARE=1` (after `PSP_EBOOT_TITLE`) adds the integer entry `EFAWARE` = 1 to the EBOOT's PARAM.SFO (the PSPSDK's
  `mksfoex -d`; checked by reading the SFO of the built EBOOT: `EFAWARE: 1`, the file is 28 bytes bigger). ARK-4 then starts psp5
  from the internal storage with the Memory Stick as `ms0:` (psp5 itself on `ef0:`), so `scan.c` finds both: `ef0:` always, `ms0:`
  when it is a real stick, as since 1.0.0. There is no change in the code. The setting is in every build (ARK-5, ARK-4, PSP Go and
  the other models); ARK-5 and the other models are not tested separately. It was found with a test build
  (`psp5-authentic-TEST-PSPGo-ARK4-efaware-v2.zip`, ARK-4, otherwise the same as the build of that day): a PSP Go owner with
  ARK-4 reported that it works. A recent ARK-4 is needed. The CI build (`.github/workflows/build.yml`) gets the flag too, since it
  uses the same Makefile.
- **Version 1.1.0** (`settings.c`, `tools/make_release.py`, `INSTALL.md`, `README.md`): the Version row shows `1.1.0 (ARK-4)` /
  `1.1.0 (ARK-5)`, `make_release.py` defaults to 1.1.0 and the zips are called `psp5-authentic-v1.1.0-…`. The README says
  1.1.0 and keeps its "What has been tested" section general (no single fixes named); `INSTALL.md` says 1.1.0 and that a recent
  ARK-4 is needed on the PSP Go.

The entries below were written between 1.0.0 and 1.1.0.

- **Settings → psp5 → Show battery percentage** (`config.c`, `settings.c`, `ui.c`, `lang.c`): new own setting `g_battery_pct`
  (`batterypct=0|1` in `psp5.cfg`, default 0; `config_load` only changes it when the line is there, so a file from 1.0.0 keeps it
  off). `settings.c` adds the toggle after "Startup animation" (`own = 1`, so it is saved by `settings_commit`); label
  `S_BATTERY_PCT` in all 11 languages (the string table has 164 rows now). `draw_top_bar` (`ui.c`): with the setting on and a
  battery present, `shift = ceilf(text_width(0.8, "100%")) + BAT_GAP` (`BAT_GAP` = 5 px) is always reserved, whatever the level, so
  `bx = floorf(SCREEN_W - MARGIN - clock_w - TOP_GAP - BAT_W - shift + 0.5)` (the battery; `TOP_GAP` 10, `BAT_W` 22) and everything
  that follows it (the cog wheel and its frame, the tab `room`, the music-note line) stay put while the level changes. The level (`"%d%%"`, capped at 100, from
  `scePowerGetBatteryLifePercent()` through the once-a-second poll that already fed the icon, or the test-run override) is drawn
  at size 0.8, `ALIGN_LEFT` at `bx + BAT_W + BAT_GAP`, i.e. always 5 px after the end of the battery; with fewer digits its right
  end is further from the clock. It is only drawn when the level is known (`level >= 0`); the room stays reserved then.
  The tabs shrink with the same rule as before (`room = bx - COG_GAP - 21 - MARGIN`, smallest size 0.56).
- **Top bar without a battery** (`ui.c`): the poll keeps `sys_present` (`scePowerIsBatteryExist()`; the test-run battery counts as
  present). Without a battery `draw_battery` is not called at all (before it drew the empty outline) and no number is drawn; `bx`
  is the position without the number (`shift = 0`), so the cog wheel does not move.
  Tests (`host-tests`, not in git): `units/config_test.c` (new; file round trip, old file, defaults, runner entry `pcfg`),
  `ui/menu_test.c` (levels 100/80/55/10/5/0/150: battery and cog wheel at the same place, number 5 px after the battery, only the
  space to the clock grows; no battery with the switch on and off: no bright pixel in the battery's place, cog wheel where it was;
  level unknown; the tabs for three levels in all 11 languages), `settings/settings_test.c` (the toggle sits under Startup
  animation, toggles with ✕, is saved once), `ui/stubs.c`, `ui/popup_sig.h` (records the text of a drawing call). The test font is
  as wide at every size, so a language whose tabs are at the smallest size in the test (Japanese with "100%") is not measured there.

- **README rewritten, release notes shortened, screenshots added** (`README.md`, `CHANGELOG.md`, `docs/screenshots/`; docs only, no code):
  - README: ~190 -> ~140 lines. Order: intro (fork, goal, thanks), screenshot table, "What you get" (one short bullet per feature, no
    button lists inside), "Buttons" (short table; the full one stays in `INSTALL.md`), Install (+ safe start / hold SELECT), "What has
    been tested", "About this fork", Building, Thanks, License. The covers section is one sentence that points to `INSTALL.md`.
    Nothing about "PS5" (the theme list in the app shows that name, so no screenshot of the Theme page is used).
  - The screenshot table is HTML (`<table>`, two columns, a caption under each picture) because a Markdown table can't have
    captions under the images without a duplicated header row. Eight PNGs in `docs/screenshots/`: `games`, `options`, `library`,
    `saved-data`, `music`, `settings`, `theme` (the XMB wave look), `startup` (Cosmic Red), 960 x 544 each (~700 KB in all).
  - CHANGELOG: same facts, shorter bullets. *Unreleased* is grouped into Library / Top bar / Docs, 1.0.0 keeps its groups.
    Some 1.0.0 details that only matter to developers were dropped from there (they are in these notes): the popup look "lives
    in `popup.c`", the faint light fill of the popup panel, the 60 % beam is kept.
  - How the screenshots are made: the real app runs in **PPSSPPHeadless** (built from source, `--graphics=software`) with a
    scratch copy of the app built with `-DPSP5_TEST`. In that copy only, `testrun_frame` writes the finished frame (the 512 x 272
    x 4 byte buffer it is given, at `0x44000000 + s_draw`) to `ms0:/PSP5/shots/NAME.raw` for the `shot NAME` step and goes on at
    once (the repo's version waits for the Windows script to photograph the window), and the app calls `sceKernelExitGame()` when the
    script is over. `raw2png.py` crops the frames to 480 x 272; the README images are those enlarged 2 x with nearest-neighbour.
    PPSSPP's own flash0 fonts are used (copied to `PSP/NAND/flash0`), so the text is PPSSPP's replacement font, not the PSP's.
    `faketime` fixes the clock at 10:09; `battery 82` and `charging 1` in the script show the battery with its percentage and the
    bolt (`batterypct=1` in `psp5.cfg`). The demo data (16 invented games with drawn art, nine saves, five MP3 files with album
    art) comes from `make_shot_data.py`, which uses the helpers of `tools/make_testdata.py`.
  - The kit (generator, run script, the eight button scripts, the patch, a README with the PPSSPP build flags) is in
    `screenshot-kit/` next to `host-tests/` in the hand-off bundle, **not in git**. Run it again whenever the look changes.

- **Top bar: the number closer to the battery, the cog wheel further from it** (`ui.c`): `TOP_GAP` (clock to what is left of it, 10),
  `BAT_W` (battery body 20 + terminal 2), `BAT_GAP` (battery to its number, was 10, now 5) and `COG_GAP` (battery's left edge to the
  cog wheel's centre, was 22, now 27) replace the literals 32 / 22 / 10 in `draw_top_bar`. `gx = bx - COG_GAP`, tab
  `room = bx - COG_GAP - 21 - MARGIN`. The group (battery + number) sits 5 px closer to the clock, so with the percentage on the cog
  wheel ends where it was (`bx` +5, `COG_GAP` +5); with it off `bx` is unchanged and the cog wheel is 5 px further left. Reason for
  `COG_GAP`: `draw_battery` draws the charging bolt at `gfx_bolt(x - 6, y + 5, 5)` (about x - 8.5 .. x - 3.5), and the frame of the cog
  wheel (`popup_choice(gx - 15, ..., 30, ...)`, right edge at `bx - 7` before) overlapped it; now the frame ends at `bx - 12`.
  Tests (`host-tests/ui/menu_test.c`): the geometry checks use 27 / 5 / 48 (tab text ends 48 px before the battery); new
  charging check (`g_stub_battery_charging` in `ui/stubs.c`): the pixels of the bolt are the same with the cog wheel selected and
  not, number on and off (it fails with `COG_GAP` 22).

- **The Library card does not calculate its picture while it grows** (`icons.c/.h`, `libicon_data.inc`, `tools/gen_library_icons.py`,
  `gfx.c`): while the Library card went from its small size to the big card, every frame had a new card height (33 .. 62) and so a
  new picture height (22 .. 41), and `ICON_LIBRARY` was rendered by the CPU for each one (`icon_render`: 4 x 4 supersampled below 32 px,
  2 - 4 ms on a PC, 25 - 40 times more on a PSP) - the stutter just before the card enlarged. Now
  - `src/libicon_data.inc` (generated, do not edit): the picture for the heights 21 .. 42 (`LIB_BAKED_FIRST/LAST`), run-length coded as
    (coverage, run) pairs, 9972 bytes in `lib_baked` with `lib_baked_start[]`. `tools/gen_library_icons.py` makes it: it compiles
    `icons.c` with `-DLIB_NO_BAKED` (an empty table), renders the heights through `icon_render` with ctypes and writes the file; run it
    again whenever the picture or the card's sizes change (`librarytest.py` checks that the file is up to date).
  - `icon_library_image(h, out)` decodes the table (and calculates a height that is not in it), `icon_library_baked(h)` says if
    it is there.
  - `gfx.c`: the Library picture no longer goes through the evicting icon cache (which freed images that were not used in the last
    frame): `s_library[LIBRARY_MAX_H + 1]` is a pool with one texture per picture height, kept until `gfx_term` (`library_free`), so
    each height is made once, from the table, and never calculated while sliding. A height that is not in the table is still
    calculated the first time it is needed.
  - Measured on a PC only (not on a PSP): no `ICON_LIBRARY` render in the frames of the slide.
  - Tests (`host-tests`, not in git): `icons/librarytest.py` (baked == calculated for h 1 .. 96), `gfx/gfxtest.c` (every card height
    32 .. 63 is baked; a height that is not baked still draws), `ui/menu_test.c` (the sliding card test checks that the picture is
    baked at each frame).

- **Library: ↑ goes to the Sort button first from every card of the first row** (`ui.c`): `sort_above()` (was: the card is
  under the Sort button) is removed. In the grid, `move == -2 && *sel < GRID_COLS` selects the Sort button (`lib_focus = 1`) when
  the Library has more than one entry, otherwise (nothing to sort) goes to the cog wheel as before (`enter_top_bar(ui, TOP_COG)`).
  ↑ from the Sort button and ↓ from the cog wheel are unchanged. README and INSTALL updated.
  Tests: `ui/menu_test.c` (first card, last first-row card, per-language loop) and `ui/pixel_test.c` (first card -> Sort button).

- **Manage Saved Data: the Sort button and its popup line up with the rows** (`saves.c`): the frame of the Sort button starts at
  `LIST_X`, the same left edge as a row's highlight, and the text 9 px in, at the new `SORT_X` (`LIST_X + 9`), one pixel right
  of the rows' circles, check marks and padlocks (`ROW_ICON_X`, `LIST_X + 8`, new, `draw_rows` uses it). Before: text at
  `title_x + 9` (x 37) and frame at `LIST_X + 4` (x 28); now text x 33, frame x 24. `header_layout` sets `sort_x = SORT_X`,
  `draw_header` draws the frame at `sort_x - 9`, so it keeps 9 px round the text and the same width. `draw_sort_menu` draws the
  popup at `LIST_X` (before `LIST_X + 4`). Not moved: the title. Tested in `host-tests/saves/saves_test.c` (in all 11 languages:
  the text is at `SORT_X`, the frame at `LIST_X` with 9 px round the text; the popup starts at `LIST_X`).

- **The Library picture is an icon of the icon framework; the word "Library" is not selectable** (`icons.c/.h`, `gfx.c/.h`, `ui.c/.h`).
  The first version of the picture was drawn in `ui.c` from nine `gfx_round_rect` calls in the card's float box, with the right
  shoulder at x 33..56 and the left at 3..26 (so the controller was cut off on the right) and with every edge rounded from the
  card's fractional position, so it changed shape while the cards slid. Now:
  - `ICON_LIBRARY` in `icons.c`: `library_geo(h, &g)` rounds every edge of the 83 x 77 design to whole pixels (`PXL(v) =
    floorf(v * h / 77 + 0.5)`; `h` is the picture's height in pixels, the width follows: `icon_library_width(h)`). The controller
    is built from its left half: body (centred, so its own mirror image), left shoulder `{3, 0, 23, 16, 8}` and left grip
    `{0, 14, 20, 27, 10}` - the right ones are mirrored (`x = ox + cw - right_edge`) and the arch is what is left between the
    grips (`gap_r - gap_l`), so the controller is the same on both sides at every size. `sd_library` is one signed distance:
    (squares minus the controller dilated by the rim) united with the controller minus the arch, so the rim cut and the arch need
    no colour of the card and the picture can be translucent without seams. Below 32 px it is sampled 4 x 4 (`fine`).
  - `gfx_library_icon(l, t, w, h, color)` in `gfx.c`: picture height `floorf(h * 0.66 + 0.5)`, centred in the card's whole-pixel box
    (`(w - pw + 1) / 2`), one white alpha image per height (from `icon_library_image`; its own pool, see "The Library card does
    not calculate its picture while it grows"), drawn with `icon_blit` (`GU_NEAREST`, pixel for pixel) like the note and the
    speaker; if there is no image it falls back to plain rounded squares and a body. Only the card's size changes the picture, never its position, and the image for a size is always the same.
  - `draw_card` had a local `float b` (the placeholder's breathing) that shadowed the new `int b` of the card box; renamed `breath`.
  - `update_top_bar`: inside the Library the word is not selectable (see "Tabs" below).
  - Tests (`host-tests`, not in git): `icons/librarytest.py` (new; every height 12..96: whole-pixel edges, three equal squares at
    an even pitch, the second row the first one moved down, the controller's rows mirror exactly, nothing cut off at the right),
    `gfx/gfxtest.c` (`test_library_icon`: 320 placements of the picture in cards of eight sizes give the same image, centred, and
    exactly what `icon_render` makes), `ui/menu_test.c` (a card that slides and shrinks over 150 frames: the picture is the
    one of the card's height, in the middle, the controller mirror-symmetric; the top bar of the Library: ↑ goes to the cog
    wheel, ← → do nothing, ↓ comes back to the Sort button; the word never has a frame, in all 11 languages),
    `ui/pixel_test.c`, `run_all.sh`.

- **The Library is a card of Home, not a tab** (`home.c/.h`, `ui.c/.h`, `loader.c/.h`, `lang.c/.h`):
  - *Model.* `views[TAB_GAMES].idx` may now hold the sentinel `HOME_LIBRARY` (-3; -1 and -2 are the "none" / "not known yet" values
    `label_for`, `lib_label_for` and `cover_for` use) besides list indices. `home_build` puts the disc first, then the pinned cards
    in pin order (the Library card among them when it is pinned, key `?library` in `home.txt`; `?` cannot be in a file name),
    then the recents, and the Library card last when it is not pinned. `HOME_MAX` stays 10 and counts the Library card
    (`room = HOME_MAX - (library pinned ? 0 : 1)` while the others are placed); `HOME_PIN_MAX` is 8 (games only, `game_pins()`).
    `PIN_KEEP` (9) is how many game pins `load_file` still accepts, so a `home.txt` of 1.0.0 with nine pins keeps them all;
    `home_pin` refuses a new pin at 8 (toast `S_HOME_PIN_LIMIT` with `HOME_PIN_MAX`). `prune` never drops `?library`, `home_state`
    skips the sentinel, and when no keys can be made `home_build` still returns the Library card. New: `home_library_pinned`,
    `home_pin_library`, `home_unpin_library`.
  - *Tabs.* `enum { TAB_GAMES, TAB_MUSIC, TAB_LIBRARY, TAB_COUNT }`: the Library is not in the bar any more, it is the state
    `ui->tab == TAB_LIBRARY` that `open_library` (✕ on the card or on Open, via `activate_card`) enters and `close_library` (the back
    button - ○, or ✕ with ○ as confirm - on a card of the grid, also when the Library is empty) leaves; both go through
    `set_tab`. `switch_tab` (L/R and ✕ on a tab) only moves between Games and Music. `top_tabs()` gives the tabs the bar shows
    ({Games, Music}, or {Library} inside the Library) and `draw_top_bar` and the tab-fit code use it, so the word "Library" is
    drawn bold where Games is. It is not selectable: `update_top_bar` takes the tabs from `top_tabs()` only when
    `ui->tab != TAB_LIBRARY`, so inside the Library the only selectable thing is `TOP_COG` (`top_focus` is never `TAB_LIBRARY`, so
    `top_anim[TAB_LIBRARY]` stays 0 and the word never gets a frame); ↑ from a first-row card that is not under the Sort button calls
    `enter_top_bar(ui, TOP_COG)`. `sel_card()` returns the raw entry of
    the selected card, `ui_selected()` returns -1 for the Library card (so `main.c` never starts or deletes it), `remap_index`
    keeps the sentinel, `build_views` asks `home_build` for `count + 1` places, `loader.c` skips negative indices
    (`load_icon_near`, `loader_focus`).
  - *Card.* `draw_card` has a branch for `HOME_LIBRARY`: a card in `theme_cur()->card` (the colour of a game without a
    picture, opaque) with the picture from `gfx_library_icon` (see "The Library picture is an icon of the icon framework" above).
    The card's pixel box `l, t, rt, b` (`ceilf(x - 0.5)`, the rule of the cover and the ring) is computed at the top of
    `draw_card`, and both the face and the picture use it. The picture dims like a cover when the card is not selected (tint 200
    to 255). Nothing from the console is used.
  - *Under the card.* `fit_label` gives the title `S_TAB_LIBRARY` without a type tag, the Start button reads `S_F_OPEN`, ✕ on it
    does the same as ✕ on the card. START / ⋯ open a popup with one choice (`MI_PIN` or `MI_UNPIN`, `home_do`), toast
    `S_HOME_PINNED` / `S_HOME_UNPINNED`. `draw_empty` is only the *No games found* note and the two hints, shown under the Library
    card when the list has no games (and in an empty Library); `S_HOME_EMPTY` and `S_HOME_EMPTY_HINT` were not used any more and
    are removed (the string table has 162 rows).
  - *Tests* (`host-tests`, not in git): `home/home_test.c` (the Library card at the end, pinned first, 10 cards, 8 pins, legacy
    9 pins, random operations with the Library card), `ui/menu_test.c` (new tab order, the Library card section, per-language
    checks of the title, the Open button, the top bar with only the word "Library" and the hint, the card's colour and picture in
    four themes at both sizes), `ui/pixel_test.c` (the Library card at rest, pinned, empty Home, Library top bar), `ui/drive.c`,
    `run_all.sh` (162 strings).

## Version 1.0.0 (psp5-authentic)

Everything that changed compared with the original's v1.5.3 – see `CHANGELOG.md` for the version written for users.

- **Home and the options menu:** the Games tab is Home: the cards opened last (at most ten, newest first) and the ones
  that were pinned (at most nine, first). Saved in `<data>/home.txt` (`src/home.c`); the cards are named with
  `entry_cover_key` (the folder name for an EBOOT, the game ID for an ISO). START opens a small options menu
  (`draw_menu`) for the selected card: Manage Saved Data, Delete, Information, Remove from Home / Add to Home, Pin /
  Unpin. ✕ chooses, ○ or START closes. Under the card there are the buttons Start and ⋯ (options): ↓ selects Start,
  ← → move between them, ↑ or ○ go back to the card; ✕ on ⋯ opens the same menu. The UMD comes first and does not count
  among the ten.
- **The whole main menu is controlled with the D-pad:** the button hints (L R Tab / △ Settings at the bottom right, and
  ✕ □ ◀▶ in Music) are gone, and △ no longer opens the settings. ↑ from the first thing on a tab (the card in Games, the
  first row in Library, the first track in Music; also an empty tab) selects the top bar (`top_focus` in `ui.c`): the tabs
  and the cog wheel are buttons with the same frame as the Sort button (`popup_choice`), ← → move along it, ✕ on a tab
  opens it (the tab turns bold as before) and ✕ on the cog wheel opens the settings. ↓ goes back to where you came from
  (to the Sort button, if ↑ came from there) and ○ always goes back to the selected game; from the Sort button, ↑ gives
  the cog wheel. L/R switch tabs as before. A held ↑ never moves on into the top bar by itself.
  In the Library, ↑ selects the Sort button only from a card in the first row that sits right under it (the middle of
  the card lies within the button's frame, `sort_above` in `ui.c`; the button is only a stop when there is more than one
  card); from all other cards ↑ goes straight to the top bar, so nothing is in the way. Before, ↑ always went via the Sort
  button. The cog wheel sits 4 px further to the left. Start/⋯ and the Library's title row are on one line (centred on
  y = 243), so that the highlight ends 16 px from the bottom edge, as far as the top bar's text and icons are from the
  top edge.
- **The sort frame no longer flickers:** the frame round the Sort button was switched on and off while you passed through the
  tabs and the cog wheel, although the button was not selected. It now has its own animation (`sort_frame`) that only lights up
  when the button is selected and the top bar is not; `sort_anim` still controls how highlighted the button is (and that the
  card under it is dimmed).
- **Everything is drawn on whole pixels:** nothing drawn at rest has an edge between two pixels (that gave 1 px on one side
  and 2 px on the other, and soft icons). The Library's cards are 80 × 44 px with 8 px between them (before: 78.4 × 43.6 and
  10 px) and the selected card is 6 × 4 px larger (`SELECT_GROW_W/H`); the picture and the ring are drawn on the same
  whole-pixel box (`draw_card`). `gfx_frame` rounds edges and thickness to whole pixels. The battery (the fill 14 px), the
  cog wheel (radius 7), the bolt, the Options dots (radius 2), the switches, the file icon, the padlock, the hint rows, the
  toast and the colour swatches sit on whole pixels; text widths are rounded up with `ceilf`. The check is in `mockge.c`
  (`g_audit` counts drawn rectangles and pictures that have an edge between pixels; `MOCK_AUDIT=1` prints where they come
  from) and is tested in `ui/pixel_test.c` and in the settings, info, files and saves tests. Soft shadows (`gfx_glow`) and
  the background's beam of light and dust are soft on purpose and are not counted. The position of the text is unchanged
  (it cannot be measured without intraFont).
- **Sorting in Manage Saved Data** with the same Sort button and popup as the Library (`set_sort`, `draw_sort_menu` in
  `saves.c`): ↑ from the first row, ✕ opens the choices Last changed / Size / Name, ○ goes back. L/R and the key icons are
  gone.
- **The console-colour themes have a beam of light, golden dust and haze** (`deco_spot` in `theme.c`, `beam` and `dust` in
  `Theme`): a wide beam from the lamp at the top left, a band of dust and bokeh, a cool haze at the bottom left. The picture
  stands still and is the same on all screens. PS1–PS4 keep their own patterns.
- **The start-up screen no longer has a final blue picture:** the old beam of light in `boot.c` (and, for a while, the theme's
  surface) is gone. The dust keeps floating on black until the start-up screen fades out (`T_FADE` 6.6 s, `T_END` 7.15 s)
  and the menu fades in. The start-up sound is 5.2 s from the health notice (2.55 s) and finishes within the 400 ms that
  `sound_wait` waits. The circles and the dust have the theme's colours (`make_palette`): the big dark circles in the theme's
  `intro` colour, the small light ones in the haze's, the dust in the theme's `dust`/`beam` (PS1–PS4 have no `dust`: they use
  the accent colour). The logo and the health notice are grey.
- **PS1–PS4 also have the beam of light and the haze** (`beam` in the theme; `deco_spot` is drawn for all themes under the
  theme's own patterns) but no dust (`dust` = 0).
- **The background's tint from the game's icon is removed** (it cost performance): `accent_target`, `ui->accent`,
  `IconSlot.accent`, `texture_average`, `Theme.tone` and the `tone` parameter of `theme_backdrop` no longer exist.
- **The beam of light covers the whole haze at the bottom, and the background is the same everywhere:** the beam
  (`deco_beam` in `theme.c`) is a mesh of small quads with their own brightness at each corner, not five layers of large
  quads. A quad is split into two triangles and the colour is only blended between a triangle's three corners, so the right
  half of every large quad became almost dark and the beam got a hard edge along the diagonal (it showed as a sharp edge that
  ended at x = 235 at the bottom, while the haze reached x = 430). Now the beam leans to the right and widens downwards: the
  right edge is soft but clear and ends at x = 400 at the bottom, the left side is a long soft slope. The shape is measured
  against the reference's brightness. The haze at the bottom is dimmed so that the beam and the haze together are about as
  bright as the reference. The menu used to be darkened by three black gradients in `draw_background` (top, left, bottom)
  over both the picture and the surface, and had its own darker base tone; the base tone is gone and the gradients now lie
  only over a game's background picture (with the same strength as before, in proportion to how much of the picture is
  visible, so that they fade in and out with the picture). All screens draw the same surface (`surf_top`, `surf_bottom` =
  before `stop`, `sbottom`); the surface is only darkened by the Library and Music (`draw_dim`) and by the popups. The
  `bottom` fields are gone; `top` is now called `intro` and is only used by the start-up screen's circles (so that they look
  as before).
- **The selected card keeps its size when the focus is somewhere else:** the ring round the card (`card_ring` in `Ui`,
  faded over 1/8 s) goes away when the Sort button, Start / Options or the top bar is selected and comes back when the card
  is selected again. The card in the Library no longer shrinks when Sort is selected (it is 86 × 48 all the time); its
  dimming of the brightness remains. `draw_card` has got the parameter `ring` (before, `focus` controlled both the brightness
  and the ring).
- **The highlights sit in the middle of their text and on whole pixels:** a label looks centred when the baseline is about
  6.45 × the text size below the middle of the frame (like the choices in the popups). `popup_label_mid` and
  `popup_frame_around` in `popup.h` give the frame's top edge and height (the height or one pixel more, so that the edge
  lands on a whole pixel); the tabs' frame follows the text size (it shrinks in long languages), and the same goes for the
  cog wheel and the Sort buttons. `popup_choice` rounds the edges to whole pixels: a frame on parts of a pixel got 1 px on
  one side and 2 px on the other (rectangles are drawn with the pixels whose centre is covered).
- **The same background everywhere, and the same darkening in Library and Music:** Games, Library and Music drew the beam of
  light, the dust and the haze at full strength, while the settings, the information, the file manager and saved data drew
  them at 60 %, so Games looked brighter. `theme_backdrop` now has no parameter for list screens and draws the same
  everywhere: the beam of light (the beam and the lamp, `BEAM_STRENGTH` in `theme.c`) at 60 %, everything else (haze, dust and
  the PS1–PS4 shapes) at full strength. The PS1 symbols sit in a single place (top right, where the lists are not), so the
  theme looks the same on all screens. The Library and Music put the same darkening (`DIM_COLOR`, 8 8 12 with alpha 150, as
  Music had; the Library had 165) over the Games background, a game's picture or the theme's surface.
- **The background is drawn once (FPS):** the surface, the glows, the beam of light with haze and the dust look the same in
  every frame, but were redrawn every time: about 500 calls that touch every pixel three to four times (measured in the test
  environment). `theme_backdrop` now draws that part once into a picture in video memory (`gfx_snapshot_begin/end`, behind
  the two screen buffers, `sceGuDrawBufferList`) and shows it from there as one opaque picture in strips of 64 pixels
  (`gfx_snapshot_draw`). It is redrawn when the theme changes, after `gfx_init` has run again, and after sleep mode
  (`theme_backdrop_forget`). The PS1–PS4 shapes move and are still drawn every frame on top.
- **Fixes from the original project (psp5 1.5.4, the issues on GitHub):**
  - **.DAX games** (`scan_iso_dir` in `scan.c`, the delete guard in `gameops.c`, the colour in the file manager): libcisoread
    reads DAX and ARK starts it, so it was enough for the scanner to include the extension. The Information screen recognises
    DAX (`DAX_MAGIC`) and reads the unpacked size as 32 bits at position 4 in the header (CSO/ZSO have 64 bits at position 8).
  - **USB mode and the file manager jumped back at once:** `usb_mode` and `files_mode` in `main.c` only ignored a ✕ that was
    held down when they opened; with ○ as the confirm button they exited immediately. Now all buttons are ignored until they
    have been released (`old = ~0u`). `sys_usb_start` also does not give up if `sceUsbActivate` answers with an error, like
    arkMenu.
  - **PSP Go, psp5 started from the internal storage:** ARK then redirects `ms0:` to `ef0:`, but only during psp5's own
    session, so the games got the path `ms0:` and were started as if from a memory stick (PS1 games: error 800201A8, ISO:
    froze). `scan_all` now always scans `ef0:` and `ms0:` only when it is a real stick (`sys_ms_is_ef`), and `launch.c` then
    picks the Go modes (`on_ef0`). Home and the covers use the game's ID or folder name, not the drive, so nothing is lost; the
    scan cache is rebuilt once.
  - **Homebrew on the PSP Go's internal storage** (the GTA III and Super Smash Bros ports froze or switched the PSP off): is
    now started with apitype 0x152 (`HOMEBREW_RUNLEVEL_GO` in `launch.c`) like arkMenu, so that ARK redirects `ms0:` to `ef0:`
    and ports that read their data files from `ms0:` find them. Homebrew on a memory stick is started as before with 0x141.
    The test `host-tests/launch` checks the apitype for every kind of game and drive.
  - **The background picture is drawn in narrow strips** (`gfx_tex` in `gfx.c`): a picture wider than 64 texels is drawn as one
    sprite per 64 texels in the same call. The GE's texture cache copes with that much better than with a sprite as wide as
    the screen, and the games' pictures behind the cards are the biggest thing drawn. The strips' borders are moved to whole
    pixels and get the texture coordinate that belongs to the moved border, so that the picture looks the same and the strips
    meet without gaps or overlap.
  - **"Boot psp5" showed off** until the first game start: a paused launcher mode is switched on by `start_confirmed()` only
    after the menu has worked for a while, but the settings read the mode as soon as they opened. `main.c` now calls
    `start_confirmed()` before `settings_open()`.
  - **A crackle when the settings opened:** the sound was played in `ui.c` before the settings were read in (the font from
    flash0, ARK's files), and the heavy reading disturbed the mixer. `ui.c` no longer plays anything; `main.c` plays
    `SND_SELECT` after `settings_open()`.
  - **PSP minis' square icons are no longer stretched** (`fit_icon` in `art.c`): an icon narrower than 1.5 : 1 keeps its shape
    in the middle of the card, on a darker (55 %), blurred copy of itself (blur 8) that fills the space beside it. Other icons
    are scaled as before. The icons that were already in `cache/` are stretched, so `art_init` removes all `*.ico` there once
    (the marker `cache/icons2`, the same name as in psp5 1.6.0); `covers/` is not touched.
- **SELECT opens a power popup** (`MENU_POWER`, `menu_build_power`, `open_power_menu` in `ui.c`): on all tabs (also with the top
  bar selected) the same dark panel as the options menu is shown in the middle of the screen, 200 px wide, with Sleep,
  Shut down, Restart and Recovery menu – the same texts (`S_SLEEP`, `S_SHUTDOWN`, `S_RESTART`, `S_RECOVERY`), the same order
  and the same function as at the bottom of Settings → Power and USB. ✕ (or ○ with that setting) does the choice: `ui_update`
  returns `UI_SLEEP`, `UI_SHUTDOWN`, `UI_RESTART` or `UI_RECOVERY` and `main.c` runs the same `run_action` as the settings. ○,
  △, START or SELECT close the popup without doing anything.
- **The confirmation step "Press again to confirm" is gone:** `confirm` in `Item`, `CONFIRM_TIME`, `confirm_item`/`confirm_time`,
  the parameter of `action()` and the string `S_CONFIRM` (all 11 languages) no longer exist; Sleep, Restart, Shut down and
  Recovery menu run on the first ✕. The order in Power and USB is now Sleep, **Shut down**, **Restart**, Recovery menu.
  `tools/tests/settings.txt` no longer presses ✕ on Sleep (it would put PPSSPP to sleep).
- **Version 1.0.0** (`settings.c`, `tools/make_release.py`, `INSTALL.md`): the zips are called
  `psp5-authentic-v<version>-…`. `CHANGELOG.md` has all changes compared with the original's v1.5.3.

## Version 1.5.3 (the original – the base of the fork)

- **Category folders:** a folder in `PSP/GAME` without `EBOOT.PBP` (e.g. `GAME/PS1/<game>` or
  `GAME/Emulators/TempGBA`) is searched one level down, as arkMenu does (`scan_game_dir`, `depth`). Before, PS1 games and
  apps in such folders did not show up at all.

## Version 1.5.2

- **Move between Games and Apps:** □ on a card in Games or Apps opens a small options menu (`draw_menu`) with the card's name
  and *Move to Games/Apps*. ✕ chooses, ○ or □ closes, so nothing is moved by mistake. Saved in `<data>/moved.txt`, one line
  per card with `entry_cover_key` (the folder name for an EBOOT, the game ID for an ISO). The UMD has no menu. The hint
  "□ Options" is shown faintly next to the Start button.
- **File manager** (`src/files.c`, Settings → Power and USB): runs like the USB mode, with the rest of psp5 closed
  (`files_mode` in `main.c`), and the game list is read again afterwards. ms0: and, on the PSP Go, also ef0: (drive list at
  the top), never flash0. Folders first, sorted by name; □ marks, △ (or ✕ on a file) opens the menu: Paste here, Copy, Move,
  Rename, Delete, New folder. Copy/move/delete run in their own thread (`worker`) with progress and ○ to cancel; a cancelled
  or failed copy is cleaned up, and the free space is checked first (`sceIoDevctl` 0x02425818). A move on the same drive is a
  `sceIoRename`, otherwise copy + delete. If the name already exists it becomes "name (2)". Delete always asks, with Cancel
  preselected. Names are typed with the PSP's keyboard (`sceUtilityOsk*`, via `gfx_end_with`). The free space is shown with a
  thin bar at the top right.

## Version 1.5.1

- **Volume indicator** (`src/osd.c`): the volume buttons go straight to the system, so psp5 reads the volume (0–30) and
  sound on/off with the kernel's `sceImposeGetParam` – ARK finds the function (`sctrlHENFindFunction`, ARK translates the NID
  0x531C9778 for 6.60/6.61, otherwise 0xDC3BECFF is tried) and kubridge's `kuKernelCall` calls it. When the value changes, a
  bar slides in at the top for 1.6 s, in all tabs, the settings and the USB mode. Without ARK (PPSSPP) nothing is shown.
- **Seeking in the music** (`seek_to` in `src/music.c`): a short press of ← / → changes track when the button is released,
  hold for 0.35 s to seek (8, then 20 and 45 seconds per second). The decoder is restarted at the right byte in the file: the
  Xing table for VBR, otherwise proportionally, and then the first valid frame (the same version, layer and frequency as the
  track's first, and a valid frame right after it). The first 8 frames after a jump may fail without the track being changed:
  the PSP's decoder answers `MP3_END` (0x80671402) instead of an error for a frame that lacks data from the frames before it
  (the bit reservoir). Seeking stops one second before the end.
- **Covers in the music** (`art_load_cover`, `music_cover_data`): from the ID3 tag's APIC/PIC (preferably the front cover,
  v2.2–2.4 with unsynchronisation), otherwise `cover.jpg`, `folder.jpg`, `front.jpg`, `album.jpg`, `cover.png` or
  `folder.png` in the album folder. JPEG is decoded with libjpeg-turbo (scaled down already in the decoder), PNG with libpng.
  64×64 with rounded corners, saved in `cache/` per album and loaded in the loader thread. `loader_stop` now runs before
  `music_term`, because the loader reads the music list.
- **Charging animation:** while the battery charges, the fill sweeps from the level up to full, with a bolt beside it.
- Test script `tools/tests/media.txt` and new test commands: `hold BUTTON:N`, `volume N`, `charging 0/1`, `battery N`.

## Version 1.5.0

- **Themes** (`src/theme.c`, new *Theme* tab in the settings): the default colour theme (as before), the console-cover
  colours (Midnight Black, Cosmic Red, Nova Pink, Starlight Blue, Galactic Purple, the Deep Earth series Volcanic Red/Cobalt
  Blue/Sterling Silver and Chroma Indigo/Teal/Pearl) and PS1–PS4 as a setting for the *Appearance*: PS1 grey with the
  controller's symbols, PS2 dark with blue towers and orbs, PS3 with the XMB wave and PS4 blue with waves and particles.
  Everything is drawn in code. The highlighted theme is previewed in the settings, ✕ chooses it. The theme colours the
  background (when the game has no picture), the settings, the USB mode, the charging screen, the placeholder cards, the
  switches, the music's progress and the charging dots. *Game art in the background* can be switched off – then the theme is
  always visible and no background pictures are loaded. Saved in `psp5.cfg` (`theme=`, `themeart=`).
- **Faster drawing:** the settings re-wrapped the category description (many `text_width` calls) for every row in every
  frame – now only when the text changes. The widths of the tabs and the hints are only measured again when something
  changes. `gfx.c` does not rebind the same texture (and does not flush the texture cache) for every shape in a row. The
  surface under the game's picture is not drawn when the picture covers the whole screen. The icon's average colour is
  calculated directly from the texture instead of on a copy (46 KB less to copy per icon).
- **Fixed:** a background picture that finished loading just as the highlight moved was thrown away and then never loaded
  again if you went back to the game (`s_bg_last` in `src/loader.c`). A broken directory entry in an ISO (too short an
  entry or too long a name) could be read outside the sector buffer (`src/iso.c`).
- Test script `tools/tests/theme.txt`; `settings.txt` and `lang.txt` have one more step for the new tab.

## Version 1.4.0

- **PSP Go** – the same EBOOT on all models: `sys_set_root` (`src/system.c`) picks `g_root` from argv[0]. If psp5 is started
  from `ef0:` (the PSP Go's internal storage), psp5's files are in `ef0:/PSP5`, ARK's default folder and `SEPLUGINS` are
  there, and the hint texts show `ef0:/`. Games (`ISO`, `PSP/GAME`) and music (`MUSIC`) are always searched on both `ms0:` and
  `ef0:`, like arkMenu – so also a Memory Stick Micro in a PSP Go. Games on `ef0:` are started with ARK's apitypes for the
  internal storage (0x125/0x126 ISO and PSN, 0x155 PS1; homebrew as usual 0x141), and updates (`PBOOT.PBP`) are searched on
  both. No UMD thread on the PSP Go (model 4). Before, psp5 only looked on `ms0:`, so games in `ef0:/PSP/GAME` and music in
  `ef0:/MUSIC` did not show up on the PSP Go. The release zips `psp5-v1.4.0-PSPGo-ARK5.zip` and `-PSPGo-ARK4.zip` have the
  same EBOOT as the ARK5/ARK4 zips, only with a READ ME for the PSP Go (`tools/make_release.py`).
- **Settings on the PSP Go** (`src/settings.c`, model 4): *Disable Pause Game* (`disablepause`) and *Keep the paused game*
  (`hibblock`) are shown, *Disable UMD drive* is hidden – as in arkMenu.
- **Build in GitHub Actions** (`.github/workflows/build.yml`): both versions and all four zips.

## Version 1.3.3

- **Start-up guard against semi-bricks** (`src/main.c`): `ms0:/PSP5/starting` is created first of all at start-up and
  contains the number of starts in a row that did not finish. The file is removed when the menu has run for 5 s and the music
  list has been read, or when a game is started or something is chosen in the settings.
  - 1 failed start: **safe start** – no start-up screen, no music, no UMD thread, `ms0:/PSP5/cache` is emptied and a toast is
    shown.
  - 2 in a row as ARK's launcher: psp5 goes straight to the XMB (`sys_exit_xmb`, the launcher mode is paused) and writes 1 to
    the file, so that the next start as a launcher is a safe one instead of the XMB again.
  - A paused launcher mode is only switched on again when a start has finished (`sys_resume_launcher`), not directly in
    `sys_init`.
- **Sleep mode without graphics:** when the PSP falls asleep, two black frames are drawn, then psp5 waits with
  `sceKernelDelayThread` instead of drawing all the time while the PSP falls asleep and wakes up. No start-up screen after
  sleep mode in safe mode.
- `sctrlArkSetConfig` is only called with a configuration that could be read (`IS_ARK_CONFIG`).
- The HOME menu in games: psp5 is not in memory then (LoadExec restarts the system) and starts games with exactly the same
  call as arkMenu. Help text about plugins and ARK's combination L + R + START + ↓ in INSTALL.md.

## Version 1.3.2

- **No menu before the start-up screen after sleep mode:** "finished waking up" (`PSP_POWER_CB_RESUME_COMPLETE`) arrives about a
  second after the screen has lit up, and until then the menu was visible. Now the screen is drawn black already when the PSP
  falls asleep (`PSP_POWER_CB_SUSPENDING`) and is kept black until it has finished waking up, then the start-up screen
  (`sleep_and_wake` in `src/main.c`, at most 4 s awake without a signal). The music is paused when the PSP falls asleep.
  Test script: `tools/tests/resume.txt` (`sleep 1`, `resume 1`).

## Version 1.3.1

- **No start-up screen after a game:** as ARK's launcher, psp5 is restarted when a game ends. Before a game starts,
  `ms0:/PSP5/ingame` is created; if the file exists when psp5 starts, it is removed and the start-up screen is skipped. As a
  normal app the file is not created (the system goes to the XMB after the game).
- **Start-up screen after sleep mode:** a power callback (`scePowerRegisterCallback`, `PSP_POWER_CB_RESUME_COMPLETE`) shows
  the start-up screen again when the PSP is woken with the side button. The start-up sound is recalculated in the sound
  thread (`sound_prepare_boot`), music is paused in the meantime and the cards slide in again (`ui_replay_intro`).
  Test script: `tools/tests/resume.txt` (`resume 1` pretends the PSP woke up).
- **The UMD first under Games** (`src/umd.c`): a thread looks for the disc as arkMenu does (sceUmdCheckMedium,
  sceUmdActivate, wait for PSP_UMD_READY) and reads `disc0:/PSP_GAME/PARAM.SFO`. The disc is put first in the list when it is
  inserted and removed when it is taken out, and the highlight stays on the same game. It is never saved in the list cache.
  Start: apitype 0x120 with `disc0:/PSP_GAME/SYSDIR/EBOOT.BIN`, or 0x160 with `ms0:/PSP/GAME/<DISC_ID>/PBOOT.PBP` if an
  update exists. In the test build, `ms0:/PSP5/fakeumd/` works as a disc (test script `tools/tests/umd.txt`).

## Version 1.3

- **ARK-4 and ARK-5, PSP-1000/2000/3000:** two builds – `tools/build.sh` (ARK-5, links ArkCtrl) and `tools/build.sh ark4`
  (ARK-4, `-DPSP5_ARK4`, without ArkCtrl → `dist-ark4/`). Both detect the ARK version (`sctrlSEGetVersion()`: 4/5) and the model
  (`kuKernelGetModel()`: 0 = PSP-1000) at start-up. On ARK-4 the ARK configuration is read through a separate
  SystemCtrlForUser import (`src/ark4stub.S`) and SETTINGS.TXT is written in ARK-4's format (overclock/defaultclock/powersave,
  highmem/mscache on/off). The PSP-1000 hides extra memory and USB charging.
- **The "ARK not found" error:** `sctrlHENGetVersion()` only gives the middle version (0 on ARK 5.0.x) – now the major version
  is used.
- **Music:** the Music tab plays MP3 from `ms0:/MUSIC` (sceMp3, its own thread, `src/music.c`), like the XMB's player.
- **A start-up screen like the console's:** grey logo on black → health notice with dark blue bokeh → golden dust → beam of
  light.
- The heap leaves 7 MB free (ARK's RECOVERY.PRX needs 4 MB).

## Start-up screen and library (1.2.0)

- **Start-up screen:** particles of light swirl in towards the middle, meet in a flash and leave the psp5 logo behind while a
  chord of its own is played. Everything is drawn and calculated in `src/boot.c` / `src/sound.c`. Any button skips it; it is
  turned off under Settings → psp5 → Startup animation (`bootanim=0` in `ms0:/PSP5/psp5.cfg`).
- **Library:** the second tab (L/R). All games and apps in a grid with 5 columns, ↑↓←→ move. ↑ from the first row that sits
  under the button selects the Sort button (from the others ↑ goes straight to the top bar), ✕ opens the sorting (name /
  recently added), ○ or ↓ back to the cards. START opens the options menu for the selected card.

## Languages
All texts are in `src/lang.c` (English, Swedish, German, French, Spanish, Italian, Dutch, Portuguese, Russian, Japanese,
Czech). The language follows the PSP's system language; Swedish and Czech are chosen manually under
Settings → psp5 → Language (saved in `ms0:/PSP5/psp5.cfg`).

## Sounds
The interface sounds are WAV files in `PSP5/sounds/` on the storage (`move`, `select`, `back`, `error`, `boot`; 44.1 kHz,
16 bit; the zips contain them). If a file is missing, psp5 plays the built-in sound with the same name – they are calculated
in `src/sound.c`, no sound files are needed. `tools/convert_sounds.py` converts your own recordings to the right format.
On/off under Settings → psp5 → Interface sounds (saved in `ms0:/PSP5/psp5.cfg`).

## Settings (the cog wheel)
Laid out like a console's settings pages, and reads/writes the same files as arkMenu:
- **psp5:** autoboot, interface sounds, startup animation, show battery percentage, language, clear image cache, version
- **Performance:** CPU in games / XMB, extra memory, memory stick cache, ISO cache
- **System:** USB charging, WPA2, logos, hide pictures/MAC/DLC, lights, UMD, analog stick, regions and more
- **Plugins:** switch rows in `ARK_01234/PLUGINS.TXT` and `SEPLUGINS/PLUGINS.TXT` on/off
- **Power and USB:** USB mode, file manager, XMB, sleep, shut down, restart, recovery menu (the same four as in the SELECT
  popup; no confirmation)

The CFW settings (`SETTINGS.TXT`) are written in exactly the same format as arkMenu and take effect after a restart.
Unknown lines and comments are kept.

## Installing on the PSP
1. Connect the PSP with USB.
2. Copy `dist/PSP` to the root of the memory stick (so that the file ends up in `PSP/GAME/PSP5/EBOOT.PBP`).
3. As ARK's launcher: copy the same file to `PSP/SAVEDATA/ARK_01234/VBOOT.PBP` (save arkMenu as `VBOOT_ARKMENU.PBP` first).
4. (Optional) Prepare covers: `py tools\covers.py E:\` (E: = the PSP).
5. Start "psp5" from the XMB → Game → Memory Stick, or via ARK's Custom Launcher.

## Controls
| Button | Function |
|---|---|
| ← → / the stick | browse (hold to go faster) |
| L / R | switch tab (Games / Library / Music) |
| ↑ from the first thing on a tab | select the top bar: ← → between the tabs and the cog wheel, ✕ opens the tab (or the settings on the cog wheel), ↓ or ○ back |
| ✕ (○ with the Japanese button setting) | start (on the card or the Start button) |
| START | options menu for the selected card (Games and Library) |
| SELECT | power popup in the middle of the screen: Sleep, Shut down, Restart, Recovery menu (all tabs) |
| ↓ in Games | select the Start and ⋯ buttons under the card (← → between them, ↑ or ○ back) |
| HOME | back to the XMB (the system's own question) |

## What is shown
- `ms0:/ISO` (+ one level of sub-folders): `.iso`, `.cso`, `.zso`, `.dax`, `.jso`
- `ms0:/PSP/GAME/*/EBOOT.PBP`: PSN and PS1 games and apps
- Everything shows up in the **Library**; **Games** (Home) shows the cards opened last and the pinned ones
- The UMD in the drive: first under **Games** and in the **Library**

Games are started with the same systemctrl calls as ARK-5's arkMenu (`src/launch.c`), including installed updates
(`PBOOT.PBP`), patched ISOs (`EBOOT.OLD`) and an unencrypted `BOOT.BIN`. Before the start everything is released: the loader
thread, textures, fonts, GU and the game list.

## Pictures and cache
Card and background pictures are looked for in this order:
1. `ms0:/PSP5/covers/<key>.ico|.bg` – from `tools/covers.py` (key = DISC_ID for ISO and UMD, folder name for EBOOT)
2. `ms0:/PSP5/cache/` – created by the launcher itself the first time a game is shown
3. ICON0/PIC1 inside the game

Only the cards near the highlight are loaded (in a background thread), and the background is loaded only when the highlight
has stood still for a short while. The game list is cached in `ms0:/PSP5/cache/list.bin`, so unchanged files are not read
again. Delete `ms0:/PSP5/cache` (or Settings → Clear image cache) if something looks wrong – it is rebuilt.

### Your own covers
Put pictures in `tools/covers-src/` (or give `--src`), named after the game ID or title:
`ULUS10041.jpg` or `My Game.png` → card picture, `ULUS10041_bg.jpg` → background. The pictures are cropped automatically.
Flags: `--force` (rewrite all), `--clean` (remove covers for games that no longer exist), `--dry-run`.

## Building (Windows + WSL1)
The toolchain (pspdev + psp-cfw-sdk) is in the WSL1 distro `pspdev` (`C:\WSL\pspdev`). WSL1 is used on purpose – WSL2 turns on
the hypervisor, which clashes with VMware.

```
wsl -d pspdev --exec bash /mnt/c/path/to/psp5/tools/build.sh
powershell -ExecutionPolicy Bypass -File tools\run-ppsspp.ps1      # test in PPSSPP
```

| Script | What it does |
|---|---|
| `tools/setup-wsl.sh` | installs pspdev + packages in WSL (run as root) |
| `tools/build.sh` | builds to `dist/PSP/GAME/PSP5/EBOOT.PBP` |
| `tools/make_icon.py` | draws psp5's own ICON0/PIC1 into `res/` |
| `tools/make_testdata.py` | made-up test games (iso/cso/zso/EBOOT) in `testdata/` |
| `tools/run-ppsspp.ps1` | copies to PPSSPP's memory stick and starts it |
| `tools/ppsspp-capture.ps1` | sends keys to PPSSPP and takes a screenshot |
| `tools/covers.py` | prepares covers on the memory stick |
| `tools/build.sh test` | test build with a scripted test mode (`src/testrun.c`) to `dist-test/` |
| `tools/ppsspp-test.ps1` | runs a test script (`tools/tests/*.txt`) in PPSSPP and saves screenshots – without key presses |

PPSSPP does not have ARK's systemctrl, so "ARK not found" is shown there when you press ✕. The game start itself has to be
tried on the PSP.

## Source code
| File | Contents |
|---|---|
| `src/main.c` | start-up, main loop, releasing before a game starts |
| `src/ui.c` | layout, animations, tabs |
| `src/gfx.c` | a small GU renderer + intraFont text |
| `src/loader.c` | background thread that loads pictures |
| `src/art.c`, `src/image.c` | covers/cache, PNG, scaling, blur, NTX format |
| `src/scan.c`, `src/iso.c`, `src/sfo.c` | scanning, ISO/CSO/ZSO/DAX/JSO, PARAM.SFO |
| `src/lzo.c` | LZO1X unpacking for JSO |
| `src/folders.c` | the folder of a game (Library), its name and letter |
| `src/launch.c` | start through ARK-5's systemctrl |
| `src/settings.c` | the settings screen |
| `src/arkcfg.c`, `src/plugins.c` | ARK's SETTINGS.TXT and PLUGINS.TXT |
| `src/system.c` | USB, power, XMB, recovery menu |
