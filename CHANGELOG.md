# Changelog

All notable changes of **psp5-authentic**, a fork of [psp5](https://github.com/08Weber/psp5) by Mille Weber (08Weber).
Newest first. Changes that are not in a release yet are under *Unreleased*.

## Unreleased

### Network

- **New: FTP Server** (*Settings → Network → FTP Server*). Copy files to and from the PSP over Wi-Fi: type the address that psp5 shows into an FTP program or into the file manager of your
  computer. The screen is made like USB Mode: it shows the address, whether a computer is connected, and the file that is being sent or received. ○ stops it, and you are back in Settings
  on the same row.
  - psp5 opens the PSP's own Wi-Fi window (the one games show, the same as *Settings → Network → Wi-Fi connection*). Choose one of your saved connections there (the ones from the XMB: *Settings → Network Settings*); psp5
    then starts the server. If the Wi-Fi switch is off, the screen says so and opens the window when you switch it on. If you close the window without choosing (or a connection fails or is lost), the screen says that the
    FTP server is not running: ✕ *Start* opens the window again, ○ goes back.
  - The computer sees `ms0` (the Memory Stick) and, on a PSP Go, `ef0`. Nothing else of the PSP can be reached (not flash0, not flash1).
  - **There is no password:** any user name and password work, so use it on your own network. It uses passive mode, which FTP programs, Windows Explorer and curl do by default.
  - Download, upload, resume, rename, delete and new folders work. Up to three computers can be connected at the same time. The PSP does not go to sleep while it runs.
  - *Settings → Network* now says what both of its rows do. The new texts are in all 11 languages.

### Library

- **New sorting: Recently played.** The Sort button has a third choice. It shows the games in the **same order as the Games tab**: the ones you played last first. After them come
  the other games you have played, and last the games you have not played yet, the newest first. A folder goes by the best game in it, and the disc is always first. The cards you
  pinned stand first in the Games tab only. It is kept in `psp5.cfg` like the other sortings.
- **psp5 now remembers when you played each game** (`lastplayed.txt` in psp5's folder). It notes the time when psp5 starts a game, and with ARK the helper plugin of *Time played*
  tells it about games you started from the XMB (psp5 learns it the next time it starts). Games you played before this version have no time yet, but they keep their place in the
  Games tab and in *Recently played*.

### Power menu

- **New choice: Exit to XMB**, at the top of the power menu (SELECT). The power menu opens on *Sleep* as before, so SELECT and then Confirm is still Sleep; ↑ goes to *Exit to XMB*. The rows *Exit to XMB*, *Sleep*,
  *Shut down*, *Restart* and *Recovery Menu* are **gone from Settings**: the power menu has them.
- **SELECT opens the power menu on every screen**, not only in Games, Library and Music: also in *Settings*, *Information*, *Manage Saved Data*, the *File Manager* and the *USB* screen. Sleep keeps you where you are;
  Exit to XMB, Shut down, Restart and Recovery menu close the screen first (from the USB screen USB is stopped first). It does not open while the screen has a popup of its own open or a job running (a copy, a move or a
  delete), so nothing is left half done. The PSP's own Wi-Fi screen and the start-up screen are not psp5's and keep their own buttons.

### Information

- **New row: Last played** (date and time), right under *Modified*. It is there for every game and app psp5 knows a time for, **also for the UMD disc in the drive** (by its disc ID,
  from psp5 or from the XMB, like the time played, which the disc already had). Games you played before this version, games you never played and a disc without a disc ID have no such row.

### Settings

- **New category: Memory**, with the *File Manager* and *USB Mode* (in that order). *Power and USB* is gone (its power rows are in the power menu, see above).
- **Closing the File Manager or USB Mode brings you back to Settings → Memory**, on the row you came from (before, to the main screen). The Wi-Fi screen already did this.
- **The switch is called *Show Game art*** now (it was *Game art in the background*), in all 11 languages.
- **Three themes are gone:** *Starlight Blue*, *Chroma Indigo* and *Chroma Pearl* looked almost like *Cobalt Blue*, *Galactic Purple* and *Sterling Silver*. If you had chosen one of
  them, psp5 starts with the PS5 theme instead.
- **Shut down** is written with a small d again (the other names keep their capitals).

### Games

- **Games you played from the XMB move to the front of Home** when psp5 starts, the one you played last in front (before, only the games started from psp5 did). Games you have not
  played are ordered by when they were added, as before: Home is filled with the newest ones on the first start, and nothing else changes for them.

## 1.2.0 – Controls, new Settings pages, folders in the Library and time played

### Controls

- **Every button means one thing on every screen:** the D-pad and the analog stick **navigate**, ✕ **confirms**, ○ goes **back**, START opens the
  **options**, SELECT opens the **power menu**, L / R **switch tabs** (or page up / down in a long list). □ and △ are only for extras (mark
  and select all in a list, pause the music).
- **Second buttons for the same job are gone.** △ and □ no longer close popups, *Information*, *Manage Saved Data* or Settings (○ goes back one
  step; from the list of categories it closes Settings). The USB screen is left with Back only. In the File Manager the options are on START.
  The File Manager now follows the PSP's own ✕ / ○ setting like everything else.
- **New: Settings → Controls.** *PSP confirm button* (✕ or ○ – this is the PSP's own setting and changes the XMB too), then **Confirm**, **Back**,
  **Options** and **Power menu**. Confirm and Back can be any of ✕ ○ □ △ (or *as the PSP's setting*); give one the button the other has and the two
  swap, so no button ever has two jobs. Options and Power menu are START and SELECT, either way round. Your choices are kept in `psp5.cfg`.
- **○ cancels before it leaves:** in the File Manager and in *Manage Saved Data* the first ○ clears your marks. In the File Manager, at the top, a
  waiting *Copy* or *Move* is cancelled before ○ leaves.

### Settings

- **A choice opens a popup.** Settings that used to change with ← → (language, CPU speed, regions, ...) show all their values when you press ✕. ↑ ↓
  choose, ✕ takes it, ○ keeps the old one. A stray press can't change a setting any more. Long lists scroll with a thin bar.
- **New: Display and Sound** – the PSP's own **Brightness** (1 to 4, like the XMB), **Backlight auto-off**, **Volume**, **Equalizer** and **AVLS**. They change in the XMB too.
  The backlight time you choose here is **kept**: the PSP forgets it at a restart, so psp5 sets it again every time it starts.
- **New: Network** – opens the PSP's own Wi-Fi screen (the one games show) and brings you back to Settings. The SDK only lets it *choose and
  connect to a saved connection*; adding, editing or removing connections still has to be done in the XMB.
- **New: About** – nickname, model, system software, CFW, psp5 version, MAC address, space left on the Memory Stick (and the internal storage of a PSP
  Go) and the battery (level, voltage, temperature, capacity). The battery temperature shows whole degrees (it showed 3687 °C before: the PSP gives
  hundredths of a degree).
- The category list scrolls. The version and CFW rows are gone from *psp5* (they are in *About*).
- **More room:** Settings and *Information* start 24 px from the screen's edges, like the File Manager, with a wider list of categories and 12 px
  between the columns.
- **Names with capitals:** *File Manager*, *Shut Down*, *Recovery Menu*, *USB Mode*, *Disable UMD Drive*, *Disable Analog Stick*, *Display and Sound*, *About*
  – in all 11 languages, each with its own capital rules (for example *O Zařízení* in Czech).

### Library

- **Folders.** The title at the top of the Library is a button now (like **Sort by**) with four views: **All**, **In folders**, **Outside of folders** and
  **Both in and out of folders**. A folder card shows a folder with its first letter and the tag *FOLDER*; ✕ opens it, ○ goes back. Folders come from
  `PSP/GAME` and `ISO` (a game's folder is the last one before it; `CAT_` and sort digits are dropped, like Categories Lite shows them).
  The view button shows the number of cards (*All: 15*) and the Sort button says *Sort by:*. The Library remembers your sort and view after a restart.
- **The Start button says *Play*** (the Library card's says *Open*).
- **.jso games** (an ISO packed with LZO) show up in the Library, and can be started, deleted and looked at.
- The number of cards at the top is level with the text before it. The PS1 theme's four symbols sit lower, under the cards.

### Time played

- **Each game shows how long you played** – a small pill (a clock and *54h* or *45min*) at the right end of the bottom row, in Games and in the Library.
  Minutes up to an hour, then whole hours. Nothing for a game you never played, the Library card or a folder.
- **It is counted by a small helper plugin** (`psp5time.prx`) that psp5 installs by itself (two lines in `PLUGINS.TXT`, shown as *Games* and *PS1* in
  *Settings → Plugins*; turn them off to stop counting). It counts only while the game runs and the PSP is awake, also for games started from the XMB,
  and psp5 adds the times up at its next start (`playtime.txt`). Games without a disc ID are named by their ISO file name.
  *The maintainer reported on 27 October 2026 that the plugin and the ISO naming work on their PSP.*

### Top bar

- **A battery like the PS5's:** four bars lit from the right (red at 15 % or less) with the terminal on the left. While charging the bolt is back
  left of the battery and the bars above the level light up one after the other.
- **A Wi-Fi sign** while the PSP's Wi-Fi switch is on (it shows the switch, not a connection). *The maintainer reported that it works.* No Bluetooth
  sign on the PSP Go: there is no way for a program to read it.
- The cog wheel is at the very end, then the clock, the battery and the Wi-Fi sign, 12 px apart.

### File Manager and start-up screen

- **The File Manager's folders have the Library's folder picture** now (they were two plain boxes).
- **The start-up screen shows the PlayStation logo**, drawn by psp5 from its outline (not a picture), a quarter of the screen high, in the same grey.
  The logo is Sony's trademark (see the README).

### If something doesn't work

Please [open an issue](../../issues) and tell us what you saw, which PSP you have and which ARK version. The new words are translated into all 11
languages by Claude, not by native speakers – tell us if one sounds wrong.

### Docs

- README and INSTALL describe the buttons by what they do, and have new screenshots.

### Downloads and installing

`psp5-authentic-v1.2.0-ARK5.zip`, `-ARK4.zip`, `-PSPGo-ARK5.zip` and `-PSPGo-ARK4.zip`: see [INSTALL.md](INSTALL.md). They use
the same folders as before (`PSP/GAME/PSP5`, `PSP5`), so they replace an installed 1.1.0 (or 1.0.0, or psp5) and keep your settings and
covers.

## 1.1.0 – the Library card, the battery percentage and a PSP Go fix

### PSP Go

- **PSP Go with ARK-4: the games on the Memory Stick Micro show up now**, together with the ones on the internal storage, when
  psp5 is started from the internal storage. psp5's start file now asks ARK-4 to start it from the internal storage with the
  Memory Stick as `ms0:`. A PSP Go owner with ARK-4 reported that this works. It needs a recent ARK-4. The setting is in every
  download.

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
  which a PSP couldn't keep up with; the pictures are now ready-made.
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

> **Something not working?** Please open an issue and say which PSP and ARK you have.

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
