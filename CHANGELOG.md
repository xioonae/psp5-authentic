# Changelog

All notable changes of **psp5-authentic**, a fork of [psp5](https://github.com/08Weber/psp5) by Mille Weber (08Weber).
Newest first. Changes that are not in a release yet are under *Unreleased*.

## Unreleased

- **Settings → Display and Sound (new): the PSP's own backlight timer, equalizer and AVLS.** Three of the XMB's own settings, so you do not have to go there
  to change them (they change in the XMB too, and nothing is saved by psp5): **Backlight auto-off** (30 s, 1, 2, 5, 10 and 15 min; when the PSP has another
  time it is shown in its place in the list, so the row never says anything else than what the PSP has), **Equalizer** (Off, Heavy, Pops, Jazz, Unique, the
  XMB's order) and **AVLS** (the volume limiter, on or off). A row the PSP does not have, or cannot be asked about, is left out; if there is none the
  category says *Not available*. (The screen brightness is not here: the PSP has its own button for it.) **Not tried on a PSP**: the calls (the backlight
  timer is a call any program may make; the equalizer and AVLS are parameters of the kernel's Impose service, reached the way the volume bar reads the
  volume), and above all **the numbers of the equalizer modes** (psp5 takes 0 = off and 1 to 4 in the XMB's order, which is a guess: please check
  that *Heavy*, *Pops*, *Jazz* and *Unique* in psp5 are the same ones as in the XMB). The new words are translated into all 11 languages by Claude.
- **Settings → Network (new): the PSP's own Wi-Fi screen.** *Wi-Fi connection* opens the network dialog that games show when they ask you to choose a connection (psp5
  starts the PSP's network libraries the way a game does, shows the dialog over the theme's background, and stops everything again when it closes, so the PSP is not left
  connected; turn the Wi-Fi switch on first). psp5 draws none of it. **What it can and cannot do:** the SDK has this dialog for *choosing one of the connections that are saved
  and connecting to it*; there is no way for a program to ask it for *adding, editing or removing* a connection. If the dialog on your PSP does not offer those, they still
  have to be done in the XMB (*Settings → Network Settings*). **Not tried on a PSP.** If the network cannot be started a message says so.
- **Settings → About (new): everything the PSP says about itself.** Rows you can scroll through (nothing in them can be changed): the PSP's **nickname**, the **model**, the **system
  software** version (6.61), the **custom firmware** (ARK-4 / ARK-5), the psp5 version, the **MAC address**, the free and total space of the **Memory Stick** (and of the
  **internal storage** on a PSP Go), and the **battery**: level (and *charging*), voltage, temperature and capacity (mAh now / full). A row the PSP does not give is left out. It is read
  again whenever the About category comes into view, so the battery is as it is now. (Seen in PC test frames only; the calls are not tried on a PSP. Translated into all 11 languages
  by Claude.)
- **Controls: every button means one thing everywhere, and you can choose which buttons they are.** The screens now use the same few buttons for the
  same things: the D-pad and the analog stick **navigate**, ✕ **confirms**, ○ goes **back**, START opens the **options**, SELECT opens the **power menu**,
  L / R **switch tabs** (a page up / down in a long list), and □ △ are left for the few screens that have something extra (mark and select all in a list,
  pause the music). Before, some screens had a second button for the same thing, and some ignored the PSP's own ✕ / ○ setting. What changed:
  △ and □ no longer close a popup, *Information*, *Manage Saved Data*'s popups or Settings (△ closed all of Settings from any row; ○ goes back one step
  now, and from the list of categories it closes Settings); in the **file manager** the options are on **START** now (△ was the button there; ✕ on a file
  still opens them), and START closes them again; the USB screen is left with **back** only (✕ left it too); the game delete's panel and the
  hints show the symbol of the button that does the job now (also START / SELECT: a small pill with the word in it). The file manager used ✕ / ○ as they are on the
  pad and ignored the PSP's own setting before; it follows it now, like everything else.
  - **Settings → Controls** (new category): *PSP confirm button* (✕ or ○; **this is the PSP's own setting and changes the XMB too**, the same one as
    the XMB's *Button Assignment*), then **Confirm**, **Back**, **Options** and **Power menu**. Confirm and Back can be *as the PSP's setting* or any of ✕ ○ □ △;
    when you give one the button that the other has, the two swap (so you never end up with one button for both). The two buttons that are left take
    □ and △ (in the order □ △ ✕ ○), so every button always has a job and nothing is lost. Options and Power menu are START and SELECT, either way round
    (it is one setting seen from both sides). The choices are kept in `psp5.cfg` (`confirm=`, `back=`, `options=`) and work at once, also in the popup that
    you are choosing them in.
  - The list of **categories in Settings scrolls** now (there are more than fit), with a small arrow when there are more above or below.
  (Seen in PC test frames and measured on a PC only. **Not tried on a PSP: the call that sets the PSP's own confirm button** (`sceImposeSetLanguageMode`;
  the SDK does not say which values it takes, so psp5 works that out by comparing it with the system setting first, and shows what the PSP says afterwards:
  if the PSP does not take the change, the row keeps its old value). The new names are translated into all 11 languages by Claude, not by a native
  speaker: please check them.)
- **Settings: a choice is picked from a popup now.** The settings that used to be changed with ← → (`<  value  >`: language, CPU speed, extra memory, the
  regions ...) open a **popup with all their values** when you press ✕ on them, the one you have chosen at the moment framed, like the other popups. ↑ ↓ choose,
  ✕ takes the value, ○ closes the popup and keeps the old one. ← → no longer change anything, so a stray press while you move through the list can't change a
  setting; ← goes back to the list of categories on every kind of row. A long list (the 14 regions, the 12 languages) shows eight values at a time and a thin bar
  at the right. The on/off switches still change with ✕. (Seen in PC test frames and measured on a PC only; not looked at on a PSP.)
- **Top bar: a battery like the PS5's, a Wi-Fi sign, and the cog wheel at the end.**
  - **Battery:** the little terminal is on the **left** now and the battery has **four bars** that are lit **from the right**, so the
    charge runs out from the left (one bar up to 25 %, two up to 50 %, three up to 75 %, four above). At 15 % or less the bars are red.
  - **Charging:** the **bolt** is back, **left of the battery** (as high as the battery and 3 px from it), and the bars above the level come
    on one after the other (half a second each, a little dimmer than the real ones) and then it starts again from the level. The battery and
    its number stay where they are when charging starts; the Wi-Fi sign and the tabs make room for the bolt.
  - **Wi-Fi:** a Wi-Fi sign (three arcs and a square, like the one on the PS5) is shown left of the battery while the PSP's **Wi-Fi switch
    is on**. It shows the switch, not whether the PSP is connected to a network. Its edges are **soft like the other icons** now (it was
    drawn with hard edges first); it is 14 px high, two more than the battery, because three soft arcs do not fit in 12 px without turning
    into grey stripes.
  - **No Bluetooth sign on the PSP Go.** I found no way for a program to read the Bluetooth state (not in the PSP SDK and not in ARK), so
    I did not add one that could only guess.
  - **The cog wheel is at the very end of the top bar** now (its right edge where the clock's used to end), then the clock, then the battery
    (with its percentage if you have it on), then the Wi-Fi sign. There are **12 px between all of them**; a sign or the battery that is not
    there (Wi-Fi off, no battery in the PSP) leaves no gap. The percentage is 5 px right of the battery and 12 px before the clock, whatever its width.
  (The maintainer tried it on a real PSP on 27 October 2026 and reported that the top bar looks fine and that the **Wi-Fi sign (the call that reads the Wi-Fi switch) works**; which PSP
  that was was not said, so the PSP Go is still not known.)
- **Library: the number of cards is level with the text before it.** In the row at the top (*All: 15*) the number was drawn on the baseline of
  the Sort button, one pixel higher than *All:*, which sits a little lower so that it is centred with the button. It is on the baseline of the text now.
  (Seen in the PC test frames; not looked at on a PSP.)
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
  when psp5 starts, in a file `playtime.txt` next to the other psp5 files, one line per game. Games that
  are the same (the same disc ID, or the same folder / file name) share one time, wherever the files are.
  The time comes from a small helper plugin that psp5 installs by itself (next paragraph). **There is no other way of counting**: with no ARK, or with the plugin's lines turned off, **nothing is counted**.
  A game of under 10 seconds is not counted either.
  (The test builds 4–6 had a switch for this, *Exact time played*, and measured from *Play* to the next start of psp5 when the plugin was off. The switch and that measuring are gone.)
- **The helper plugin that counts the time (installs itself).** `psp5time.prx` is a small kernel module that is carried inside psp5. At every start with ARK psp5 writes it to the `PSP5` folder (when it is missing or not the one that comes with this psp5) and,
  when `PLUGINS.TXT` has **no line for it at all**, adds two lines: `game, ...psp5time.prx, on` (PSP games, ISOs and homebrew) and `pops, ...psp5time.prx, on` (PS1 games). Every other line is left as it is, and so are lines for the plugin
  that are already there, on or off: **to turn it off, turn off the two `psp5time.prx` lines in Settings → Plugins** (they are listed as *Games* and *PS1*) and they stay off. It works from the next game, no restart.
  It runs inside every PSP game, ISO, homebrew and PS1 game, **whoever started it**, so the time is counted **also for games you start from the XMB, and
  psp5 does not have to be ARK's launcher**. It counts only the seconds the game is running **and the PSP is awake**: sleep and a PSP that was
  switched off during a game add nothing, and a long sleep no longer makes a 1-hour game show 15 hours.
  The plugin finds out which game it is in by itself and names it as psp5 does in `playtime.txt`: a homebrew, PSN or PS1 game by the folder of its `EBOOT.PBP`, a PSP game (disc, ISO, an installed
  update) by the disc ID in its `PARAM.SFO`. **A PSP game that has no disc ID** (a `PARAM.SFO` without one, or none at all; some homebrew ISOs) **is named by the name of its ISO file without the extension**, which is also how psp5 names it
  (so it matches the card; renaming the file starts a new time). The plugin gets the file name from the file the PSP started, or else from ARK, which knows the ISO it mounted (`sctrlSEGetUmdFile`; looked up when needed, so an ARK without it only means "no name" and the plugin still loads). It keeps the seconds of each game in a small file `PSP5/played/<name>.txt` (written in place every 10 seconds, so a battery that runs out loses at most 10 seconds)
  and **psp5 adds them to `playtime.txt` at its next start** (then removes the files). The plugin never writes `playtime.txt` itself, so a game that is switched off cannot damage the list of all your times.
  The counting was reported to work on a real PSP (test builds 4–6: games started from psp5 and from the XMB were counted correctly). **The self-installing version: the maintainer reported on 27 October 2026 that the plugin works on their PSP, and so does the naming of ISO games that have no disc ID** (on a PC it was tried with the plugin's own loop against a clock that sleeps, a throwaway memory stick and a made-up disc). Not known (it was not said which PSP or which ARK that was, so these are still open): that ARK-5 has `sctrlSEGetUmdFile` like ARK-4's source (a missing one means no name for ISOs without a disc ID), the
  PSP Go (the plugin keeps its files in the folder it was loaded from, so on the internal storage they are on `ef0:`), ARK-4, the file names ARK gives for ISO, PBOOT, PSN and PS1 games, and whether the Go's pause mode (the game paused behind the XMB) is left out.
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
