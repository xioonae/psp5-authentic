# Changelog

All notable changes of **psp5-authentic**, a fork of [psp5](https://github.com/08Weber/psp5) by Mille Weber (08Weber).
Newest first. Changes that are not in a release yet are under *Unreleased*.

## Unreleased

- **Library: a button that chooses what the Library shows – with folders.** The title at the top left (it used to say *All games
  and apps*) is a button now, framed when selected like **Sort**, in the same row. ↑ from a card selects the button nearest above
  it (the new one on the left, Sort on the right); ← → move between the two; ✕ opens a list of four views:
  - **All** – everything in one list, as before.
  - **In folders** – a card for every folder. A game's folder is the last folder before its own folder in
    `PSP/GAME` (`PSP/GAME/Racing/MyGame/EBOOT.PBP` is in *Racing*), or before the image in `ISO` (`ISO/Racing/MyGame.iso`).
    ✕ on a folder opens it and its games and apps take the place of the folders; the folder's name is at the top where the button
    was, and **○ goes back** to the folders (○ again closes the Library). START does nothing on a folder.
  - **Outside of folders** – only what is not in any folder.
  - **Both in and out of folders** – the folders first, then the games and apps that are in no folder (the UMD disc stays
    first of all).
  The button has a **colon and the number of cards** in it, like on the real console: *All: 15*, *In folders: 3*. Inside a folder
  the name has the colon too (*Racing: 3*). The Sort button says **Sort by:** (*Sort by: Name (A-Z)*) – in Saved Data too, which
  uses the same word.
  A folder card is in your theme's colour with a folder drawn by the launcher (a wide folder with a soft, rounded tab, shaped after a
  folder picture the maintainer chose) and the folder's first letter in the middle of it, as a capital (two
  letters for a pair like *Ch*). Its name is in the bottom row, where a game's name is, with the tag **FOLDER** (in capitals like
  PSP and APP, in every language). The folders are sorted
  by the Sort button like games (by name, or the folder with the newest game first). Folder names are shown the way the
  Categories Lite plugin shows them: without `CAT_` and without the two sort digits (`01Racing` is *Racing*). Folders with the
  same name – in `GAME` and in `ISO`, on the Memory Stick and on the internal storage – are one card.
  If a view has nothing to show (no folders at all, say) the screen says so and the button stays selected so you can change the view.
  When there is not enough room in a language with long words, the number of games next to the button is left out first and the
  text gets a little smaller.
  (The new names are translated into all 11 languages by Claude, not by a native speaker – please check them. In Czech and
  Japanese the Sort word stays as it was: *Seřadit*, 並べ替え.)
- **The *Start* button in Games says *Play* now** (*Spela*, *Spielen*, *Jouer*, *Jugar*, *Gioca*, *Spelen*, *Jogar*, *Играть*, *プレイ*, *Hrát*).
  The Library card's button still says **Open**. (The Play words are Claude's translations; Czech was *Spustit* before.)
- **Time played.** A small dark pill with a clock and the time, like **54h** or **45min**, shows on the right-hand end of the
  bottom row – in the Games tab opposite *Play* and the ⋯ button, in the Library opposite the tag and the name. It shows
  **minutes up to an hour, then whole hours – never both** (*h* / *min* in most languages, *u* in Dutch, *ч* / *мин* in Russian,
  時間 / 分 in Japanese). Nothing is shown for a game that has not been played, and **never** for the Library card or a folder (the launcher's own cards). Something played but under a minute shows **<1min**. The time is added up
  when you come back to psp5 after a game, in a file `playtime.txt` next to the other psp5 files, one line per game. Games that
  are the same (the same disc ID, or the same folder / file name) share one time, wherever the files are.
  **Without the helper plugin (next paragraph) it only works when psp5 is ARK's launcher** (the *VBOOT.PBP* swap and *Boot into psp5* in `INSTALL.md`; *Settings → psp5* then says
  *psp5 is ARK's launcher: Yes*): then ARK starts psp5 again when the game ends and psp5 knows exactly when you started and when you stopped. That is also the case when you open psp5 with the Custom Launcher entry in the XMB (checked on a real PSP: the game was
  counted and ARK went straight back to psp5). Started from Game → Memory Stick as a normal app, psp5 does not note the game and **nothing is counted**.
  Things to know: without the helper plugin (next paragraph) the time of one session is measured with the PSP's real-time clock (starting a game restarts the
  system clock, so that one cannot be used after a game; measured on a real PSP); a session shorter than 10 seconds or longer than a
  day is not counted; a PSP that sleeps while the game is open counts the time it slept (up to a day), and one that was switched off during a game and started again
  counts the time it was off too. In a test on a real PSP with psp5 as ARK's launcher a game of about 160 seconds was counted as 159.
- **Exact time played** (Settings → psp5): a switch that installs a small helper plugin, `psp5time.prx` (a kernel module that is carried inside psp5).
  It runs inside every PSP game, ISO, homebrew and PS1 game, **whoever started it**, so with it on the time is counted **also for games you start from the XMB, and
  psp5 does not have to be ARK's launcher**. It counts only the seconds the game is running **and the PSP is awake**: sleep and a PSP that was
  switched off during a game add nothing, and a long sleep no longer makes a 1-hour game show 15 hours. Turning the switch on writes the plugin to the `PSP5` folder and adds
  two lines (`game, ...psp5time.prx, on` for PSP games and homebrew, and `pops, ...psp5time.prx, on` for PS1 games) to ARK's `PLUGINS.TXT`, leaving every other line as it is; turning it off sets those two lines to off. It
  works from the next game, no restart.
  The plugin finds out which game it is in by itself and names it as psp5 does in `playtime.txt`: a homebrew, PSN or PS1 game by the folder of its `EBOOT.PBP`, a PSP game (disc, ISO, an installed
  update) by the disc ID in its `PARAM.SFO`. It keeps the seconds of each game in a small file `PSP5/played/<name>.txt` (written in place every 10 seconds, so a battery that runs out loses at most 10 seconds)
  and **psp5 adds them to `playtime.txt` at its next start** (then removes the files). The plugin never writes `playtime.txt` itself, so a game that is switched off cannot damage the list of all your times.
  When psp5 is the launcher and the plugin counted the game too, the plugin's time is used and the old measuring is not (so the time is not counted twice). A game of under 10 seconds is not counted by the plugin; psp5 then uses its old
  measuring as before (only as the launcher). A game that psp5 names by its file (an ISO without a disc ID) has no match in the plugin's files and keeps the old measuring.
  Without the plugin everything works as before. The plugin and its counting were tested on a PC (the plugin's own loop against a clock that sleeps, a throwaway memory stick and a made-up disc); it has **not** been run on a PSP. Not known: that ARK accepts the module as it is built, what
  the file name of a PS1 game or a disc is for ARK in every case (it is used as ARK gives it), the PSP Go (the plugin keeps its files in the folder it was loaded from, so on the internal storage they are on `ef0:`), and whether the Go's pause mode (the game paused behind the XMB) is left out.
  Any other homebrew you run gets a time line in `playtime.txt` too (under its folder name): it is never shown, but it is there.
  **psp5 itself is never counted**: the plugin ends at once in psp5 (an `EBOOT.PBP` in a folder called `PSP5`, or ARK's launcher `VBOOT.PBP`), and as a second guard psp5 knows its own path and throws away any
  file the plugin made for psp5's own folder, so even a psp5 that was put in a folder with another name does not get a time of its own.
- **The Library remembers the Sort and the view** after a restart (two lines in `psp5.cfg`). Before, it was *by name* every time.
- **.jso games** (an ISO packed with LZO) show up in the Library, can be started, deleted and looked at in the file manager and in
  Information, like .iso, .cso, .zso and .dax. (Checked on a PC with made-up data against the reference LZO library; no real .jso file
  and no PSP was used.)
- **PS1 theme: the four symbols sit lower.** They moved from behind the clock and the battery to the right side, under the row of
  cards on Home, so they can be seen clearly. They are in the same place on every screen.

## 1.1.0 – the Library card, the battery percentage and a PSP Go fix

### PSP Go

- **PSP Go with ARK-4: the games on the Memory Stick Micro show up now**, together with the ones on the internal storage, when
  psp5 is started from the internal storage. psp5's start file now asks ARK-4 to start it from the internal storage with the
  Memory Stick as `ms0:`. A PSP Go owner with ARK-4 reported that this works. It needs a recent ARK-4. The setting is in every
  download; ARK-5 and the other PSP models are not tested separately.

### Library

- **The Library is a card in Games now, not a tab.** It is the last card of the Games row, always there, and can't be removed.
  It has its own picture (a game controller in front of a grid of squares) on your theme's colour. It counts as one of the ten
  cards: up to nine games and the Library card.
- **Open and close:** ✕ on the card (or on its **Open** button) opens the Library, **○ closes it** and you are back on the card.
  Inside, the top bar shows a bold **Library** as a title (it can't be selected) and the cog wheel. L and R do nothing there.
- **You can pin the Library card** (START → *Pin to Home*). It moves to the front with the pinned cards but doesn't count as a
  pinned game, so you can now pin 8 games instead of 9. (If you already pinned nine, they stay pinned.)
- START on the Library card shows only **Pin to Home** / **Unpin from Home**.
- **↑ goes to the Sort button first** from every card in the first row (before, only from the cards right under it). ↑ again goes
  to the cog wheel, and ↓ from the cog wheel comes back to the Sort button.
- **No more stutter** just before the card grows into the big card. Its picture was calculated again for every size on the way,
  which a PSP couldn't keep up with; the pictures are now ready-made. (Measured on a PC, not tried on a PSP.)
- **The picture is polished:** the controller is now the same on the left and the right (the right side was cut off a little) and it
  is drawn on whole pixels like the other icons, so it no longer wiggles or changes shape while the cards slide.
- With no games at all, *No games found* and where to put them show under the Library card. (The "Home is empty" note is gone.)

### Top bar

- **New setting: Show battery percentage** (*Settings → psp5*, under *Startup animation*), off by default. It shows the real
  battery level as a number (like *87%*) in the clock's font, 5 px right of the battery, so it reads as part of the battery icon.
  The group sits close to the clock.
- The battery and the cog wheel stay where they are at every level; a shorter number just moves away from the clock. The tabs
  make room too (long languages shrink their names a little more).
- **The cog wheel is 5 px further from the battery**, so its highlight no longer covers the charging bolt. With the percentage on
  the cog wheel stays where it was; with it off, it is 5 px further left and the tabs get 5 px less room.
- **A PSP without a battery shows no battery icon and no number** (before, an empty battery was drawn). The cog wheel stays put.

### Settings

- The **Version** row in *Settings → psp5* shows 1.1.0.

### Docs

- The README is rewritten: shorter and clearer, with new screenshots. INSTALL.md has the new download names.

### Downloads and installing

`psp5-authentic-v1.1.0-ARK5.zip`, `-ARK4.zip`, `-PSPGo-ARK5.zip` and `-PSPGo-ARK4.zip`: see [INSTALL.md](INSTALL.md). They use
the same folders as before (`PSP/GAME/PSP5`, `PSP5`), so they replace an installed 1.0.0 (or psp5) and keep your settings and
covers.

## 1.0.0 – the first release of psp5-authentic

Based on **psp5 v1.5.3** by Mille Weber (08Weber), with one goal: to be **as authentic as possible to its home console
counterpart**. Below is everything that changed compared with v1.5.3. Some fixes that psp5 1.5.4 and 1.6.0 made on their side
are in too (marked *also fixed in psp5 1.5.4 / 1.6.0*); they were written again for this code, not copied.

Written by Claude, an AI assistant made by Anthropic. The person who runs the fork has almost no coding experience and tests
every update on their own PSP.

> **Please read:** every update is tested on a **PSP-1000**, the only device used for testing. The PSP-2000, PSP-3000, PSP Go
> and the two ARK versions are not tested separately, and the PSP Go fixes below could not be tried on a PSP Go. If something
> behaves oddly, please open an issue and say which PSP and ARK you have.

### Home and menu, the console way

- **Games is now Home.** The Apps tab is gone. Home shows the games and apps you opened last (newest first, up to ten) and the
  ones you pinned (pinned come first, up to nine). A UMD in the drive is always the first card and doesn't count.
- Home is remembered in `home.txt` in psp5's data folder. On the first start it is filled with the ten newest.
- **Library** shows everything in a grid (80 × 44 cards). Its **Sort** button, reached with ↑, opens a popup with two sortings:
  name or recently added. The old white square icon is gone.
- **Start and ⋯ (Options) are real buttons** under the big card: ↓ selects **Start**, ← → move between Start and **⋯**, ↑ or ○
  goes back. START (or ✕ on ⋯) opens the options of the card, START closes them. START no longer starts a game or plays a track.
- **The whole menu works with the D-pad alone** and the hint pills are gone. ↑ from the first thing on a tab selects the
  **top bar** (Games, Library, Music, cog wheel): ← → move, ✕ opens, ↓ goes back down, ○ goes back to the card. L and R still
  switch tabs. △ no longer opens Settings.
- In the Library, ↑ stopped at the Sort button only from the cards right under it.
- **The selected card keeps its size** when the focus is somewhere else (Sort, Start, Options, top bar). Only its white ring
  fades out, and the ring fits the picture exactly.
- The **type tag** (PSP / APP / PSN / PS1 / UMD) sits before the name next to the big card, as on the console. The game id moved
  to the Information screen.
- **Options popup:** Manage Saved Data, Information, Delete, Remove from / Add to Home, Pin to / Unpin from Home.
- **New: power popup on SELECT**, on every tab. Sleep, Shut down, Restart, Recovery menu: the same names and order as at the
  bottom of *Settings → Power and USB*. ✕ does it, ○ or SELECT closes the popup.
- **"Press again to confirm" is gone.** Sleep, Restart, Shut down and Recovery menu run on the first ✕. In *Power and USB* the
  order is now Sleep, Shut down, Restart, Recovery menu.

### Information, Delete and saved data

- **Information** (options popup): where the game is and how much room is left there, file name, size, date, volume name, the size
  of an EBOOT game's whole folder, the unpacked size and ratio of CSO / ZSO / DAX images, and DISC_ID, DISC_VERSION, APP_VER and
  PSP_SYSTEM_VER from the game's PARAM.SFO. The UMD has it too.
- **Delete** (options popup): asks first (*Cancel* is preselected), shows a progress panel you can cancel, then removes the card
  from the list. Only ISO / CSO / ZSO / DAX files and game folders can be deleted: never the UMD, psp5's own folder, SAVEDATA
  or anything else. A cancelled or failed delete never leaves a half game in the list.
- **Manage Saved Data** (options popup): lists every folder in `PSP/SAVEDATA` (also on a PSP Go's internal storage) with its game,
  title, description, size and date. Mark saves with □, select all with △, sort with the same Sort button as the Library (last
  changed / size / name) and delete after a question. `ARK_*` (ARK's settings and `VBOOT.PBP`) is locked and can't be marked or
  deleted.
- The **file manager** and Manage Saved Data share the look and the questions of the rest of the app.

### Backgrounds, themes and startup

- **The console-cover colours have the console's scene:** a wide beam of light from a lamp at the top left, a band of fine golden
  dust with soft bokeh, and a cool haze along the bottom left. Each colour tints them, and the beam is a little softer (60 %)
  than the rest. **PS1–PS4** get the beam and the haze behind their own decorations, but no dust.
- **One background on every screen** (Games, Library, Music, Settings, Information, file managers, USB). Library and Music lay the
  same darkening over it. A game's own picture is shaded at the top, left and bottom so the tabs, cards and text stay readable.
  The old tint a game's icon gave the background is gone.
- The **PS1 symbols** sit in one place (top right) on every screen.
- **Startup:** the grey logo, the health notice (one larger paragraph) over dark out-of-focus circles, then floating dust, and
  then it fades straight into the menu (the still picture at the end is gone). The circles and dust take your theme's colours:
  a Cosmic Red console boots in red.
- The **volume popup** sits 12 px from the top.
- **Faster background:** the part that never changes (surface, glows, beam, haze, dust) is drawn once into video memory and shown
  from there. Only the moving shapes of PS1–PS4 are drawn every frame.

### A cleaner, smoother picture

- **Everything at rest is drawn on whole pixels:** no more borders that are 1 px on one side and 2 px on the other, no soft icons.
  Selection frames are centred on their text, also in long languages.
- **Smooth icons and shapes:** button symbols, L/R key caps, cog wheel, charging bolt, play and seek triangles, music note and
  speaker are pixel-exact anti-aliased images. Rounded rectangles, circles and drawn lines (check marks, PS1 symbols, waves, boot
  logo) have soft edges.
- **Popups look the same everywhere:** one dark, square panel with a soft shadow and a thin frame round the chosen entry. The
  question before a delete is in the middle of the screen; progress panels and toasts use the same style in a lighter colour.
- **Czech** is drawn correctly: psp5 draws the háček and ring marks itself, because the PSP's fonts don't have them.

### Languages and sounds

- **Czech is a new language** (*Settings → psp5 → Language*), so there are 11 now: English, Swedish, German, French, Spanish,
  Italian, Dutch, Portuguese, Russian, Japanese and Czech.
- Titles that are capitalised in English (*Manage Saved Data*, *Disable Pause Game*, *System Storage*) are capitalised the same
  way in the other languages. 64 texts changed in all: those capitals, and shorter wording where a text was much wider than
  the English one.
- **Interface sounds** are WAV files in `PSP5/sounds` (the zips include them): *move*, *select*, *back*, *error* and *boot*. The
  nine old sounds were merged into these five. If a file is missing, psp5 plays the built-in sound of the same name.

### Settings

- The **analog stick** works in Settings: up / down move, sideways acts like ← / → (enter, go back, change a value).
- The **Version** row in *Settings → psp5* shows 1.0.0.
- Strings and code that nothing uses any more are removed (among them *Press again to confirm*).

### Fixes

*Also fixed in psp5 1.5.4 / 1.6.0:*

- **.DAX games** are listed like ISO, CSO and ZSO (also in the file manager, the delete guard and Information).
- **USB mode and the file manager closed at once** when opened with your confirm button. They now wait until every button is
  released. Starting USB no longer gives up when the system reports an error.
- **PSP Go, psp5 started from the internal storage:** games were started as if they were on a Memory Stick (PS1: error 800201A8,
  ISO: freeze). `ef0:` is now always scanned and `ms0:` only when it is a real stick, so games start with the right run levels.
- **PSP Go homebrew on the internal storage** (like the GTA III and Super Smash Bros ports) starts with the run level ARK's own
  menu uses, so ports that read their files from `ms0:` find them.
- **"Boot psp5"** showed as off until the first game was started.
- **A crackle when Settings opened** (the sound played while Settings was still loading).
- **PSP minis icons** are no longer stretched: a square icon keeps its shape on a darker, blurred copy of itself.
- **Wide pictures are drawn in narrow strips**, which the PSP's graphics chip handles much faster (the artwork behind the cards).

*Other fixes:*

- Behind Settings, Information and the delete screens the menu got a stick pushed all the way left and up, so the selection slid
  to the first card with move sounds. It now gets a centred stick.
- A picture that claims a size like 65537 × 65537 could pass the size check (the product wrapped around in 32 bits). The check
  uses 64 bits now.
- Cancelling a folder copy left the half-made folder behind, and the 1000th copy of a file could overwrite the existing one.
- `SETTINGS.TXT` and `PLUGINS.TXT` could lose lines when saving: game lines past the 64th were deleted and a file that didn't fit
  was cut. Long files are kept in full now, and a file that couldn't be read completely is never written over.
- The loader no longer writes through a null pointer when memory runs out; the cards just show no pictures.
- Games and apps in category folders (`PSP/GAME/PS1/<game>` and the like) show up. (This is psp5 v1.5.3's own change.)

### Faster

- Opaque fills, gradients and pictures are drawn without blending. The battery and clock are read once a second, text widths
  are remembered, and blur and JPEG decoding are cheaper. Music tags are cached on the card, so later starts only read new or
  changed files.

### Removed

- The Apps tab and *Move to Apps / Games*, the on-screen hint pills (L R Tab, Settings), the △ shortcut to Settings, the tint a
  game's icon gave the background, and the code and strings that went with them.

### Downloads and installing

`psp5-authentic-v1.0.0-ARK5.zip`, `-ARK4.zip`, `-PSPGo-ARK5.zip` and `-PSPGo-ARK4.zip`: see [INSTALL.md](INSTALL.md). They use
the same folders as psp5 (`PSP/GAME/PSP5`, `PSP5`), so they replace an installed psp5 and keep its settings and covers. The image
cache is rebuilt once.

### Credits and licence

Based on [psp5](https://github.com/08Weber/psp5) v1.5.3 by Mille Weber (08Weber). Written with Claude (Anthropic). GPL-3.0, like
the original; the original copyright notices are kept in the source files. Not affiliated with or endorsed by Sony Interactive
Entertainment.
