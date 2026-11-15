# Changelog

All notable changes of **psp5 authentic**, a fork of [psp5](https://github.com/08Weber/psp5), originally made by Mille Weber (08Weber) and forked by Xioonae.
Newest first. Changes that are not in a release yet are under *Unreleased*.

## 1.3.0 – Media tab with music, videos and pictures, FTP Server and a File Manager that opens files

### Media: Music, Videos and Images

- **The Music tab is now the Media tab.** It is a row of cards like the Games tab: **Music**, **Videos** and **Images**, drawn in your theme's colours. The background goes dark behind a card that is opened and bright again when it is closed.
  SELECT's power menu works in the Media tab, in its three screens, and over a picture and a video (it also pauses the video).
- **Music.** Only the `MUSIC` folder of the Memory Stick and of the internal storage is read, and the list looks like the File Manager's (a thin frame, folders first, a long name scrolls, three moving bars at the song that plays).
  The player has round buttons (the one you are on turns white): △ moves between the list and the player, ← → move over the buttons (volume, previous, play / pause, next, **⋯**), ↑ selects the bar, and ← → on the bar jump **10 seconds**.
  **⋯** opens **Shuffle**, **Repeat** (off, current song, all) and **Equalizer** (the PSP's own setting, the same as in *Settings → Screen and Sound*), each a popup with its values.
  A folder that the PSP answers with the wrong folder is not entered (*This folder can't be opened*), where it used to make the path longer with every ✕.
- **The music keeps playing** while you leave the Media tab, and the PSP's ♪ button shows the player alone as a popup, over every screen of psp5 authentic (the Library, Settings, the File Manager and the FTP Server included). ○ or ♪ closes it. USB Mode and a game that starts stop the music.
  A song started from the File Manager plays to its end and stops; it goes on with the rest of its folder only if you open the ♪ popup while it plays. *The maintainer reported on the PSP-1000 that this works.*
- **Songs the PSP can play:** MP3, WAV (PCM, 8 to 32 bits), ATRAC3 / ATRAC3plus (`.at3`, `.oma`, `.aa3`) and **AAC** (`.m4a`, `.m4b`, raw `.aac`; AAC-LC, made by a software decoder of psp5's own, the one that makes the sound of the videos).
  They show title, artist, album and the cover (from the file, or a `cover.jpg` / `folder.jpg` in the folder). **A sound with more than two channels** (5.1, 7.1 ...) is mixed down to stereo, in both players. Not played: HE-AAC, Apple Lossless, WMA, Ogg, FLAC.
  *The maintainer reported on the PSP-1000 that AAC and the mix-down of the music player work.*
- **Videos.** ✕ on a video in the Media tab (or in the File Manager) plays an **MP4** (H.264 video and AAC sound, up to 512 x 512) or a **PMF / PSMF** on the whole screen: ✕ pauses, ← → jump 10 seconds, L / R play the video before or after, ○ leaves.
  The bar and the numbers show when you press a button and go away after a moment. A video that was left in the middle asks whether to *Continue* from there or start *From the beginning*. The sound is AAC-LC with up to 8 channels, mixed down to two.
  The `VIDEO` folder of each storage is read. The music stops when a video starts.
- **Which videos play** (tried on a PSP-1000). The PSP's own H.264 decoder (the one on its second processor, the Media Engine) is asked first, with no setting; psp5's own software decoder takes over only when that decoder cannot be started.
  - **Baseline / CAVLC** without B-frames, such as **HandBrake's PSP preset:** plays at full speed.
  - **Main profile** with CABAC and **up to three reference frames** (B-frames and weighted prediction are fine): plays at full speed, with jumps and the last pictures shown.
  - **Main profile with four or more reference frames** (x264's default settings, because of their B-pyramid): **refused, it says "can't be played".** The PSP's decoder cannot take them and a try would break it for every video after it.
    Make the video again with HandBrake's PSP preset, or with x264 `profile=main`, `b-pyramid=none` and `ref=3` or less.
  - High profile with the 8x8 transform, B-frames in CAVLC, HE-AAC sound and anything over 512 x 512: not played. AVI, MKV and MPEG-2 are not played either.
  - After a fatal error of the PSP's decoder no later video can be decoded until psp5 is started again; psp5 says so (*Restart psp5 to play videos again*, in 11 languages).
- **Sleeping while a video is open no longer crashes the PSP.** A file that was open before the sleep does not read after it, and psp5 used to start another decoder on it and crash. Now a video that cannot be read ends, and the player leaves when the PSP wakes and remembers the place (*Continue from ...* next time).
  If your PSP still crashes after a sleep during a video, please open an issue and attach `PSP5/logs/Video.log`.
- **Images.** ✕ on a picture shows a **JPEG** (turned the way the camera says), **PNG** or **BMP** on the whole screen, as large as the screen allows; L / R (and ← →) show the picture before and after, ○ leaves. The `PICTURE` folder and `PSP/PHOTO` are read.
  A small wheel turns while a picture is read, and the little bar with L, the number and R shows when you press a button. A picture that cannot be read says *This image can't be shown*;
  a progressive JPEG that needs more than 5 MB and a PNG of more than 2.2 million pixels are not shown. *The maintainer tried JPEGs and PNGs of several kinds on the PSP-1000, moving on and closing while one is read: they work.*
- **No more stopped screen** (measured on the PSP-1000 with a recorder that is not in the release): the video player opens, starts and closes a video in a thread of its own, so a wheel turns and ○ answers at once; the picture viewer opens and closes without a pause
  (the processor's clock is changed by a thread of its own, a PNG is read a row at a time, and a picture is made smaller to the screen's size about three times faster, with the same pixels); the Media tab shows its cards at once;
  *Settings → Storage → Console Storage* counts the free space in the background while the start screen plays; the File Manager opens at once and coming back from it, USB Mode, the FTP Server or Wi-Fi does not load the fonts again; the Japanese font is loaded in a thread.
  The longest frames of these screens are 33 to 53 ms.
- **One log for each part of the program.** `PSP5/logs/` has `Music.log`, `Video.log`, `Picture.log`, `File Manager.log`, `FTP.log` and `System.log`; nothing is written while all goes well.
  An entry has the date, a sentence that says what went wrong and the steps before it; a step that can crash the PSP (the Media Engine, the closing of a video, a copy, the start of a song, the FTP Server) is marked on the card before it begins and taken away when it ends well,
  so a crash or a lost power is written down at the next start (*the PSP stopped while ...*). A log never grows past 16 KB (the oldest entries go), an area writes at most 12 entries in a run, a repeat is written once, the card is written by a thread of its own, and no file names are written.
  The steps before a failure are kept in memory, so a PSP that is not failing does nothing for them. See [If something goes wrong](README.md#if-something-goes-wrong) in the README.
- **README pictures** of the Media tab and the Music screen, and the pictures of Games, Options, Themes, the power menu and the Library, made again because the top bar says *Media* and the opened Library is darker.

### File Manager

- **✕ opens the file.** A video plays in the video player and a picture opens in the image viewer (L / R go to the one before and after in the folder), a song starts playing, and a file that has no formatting (plain text, up to 1 MB) opens in the new **text editor**.
  The options are on START, as before.
- **The text editor** works a line at a time, as ARK's does: ↑ ↓ choose a line, L / R move eight, ✕ opens the PSP's own keyboard with the line in it, □ adds a line below, and START has *Save*, *New line above*, *Copy line*, *Paste line*, *Delete line*, *Undo* and *Go to line*.
  ○ leaves and asks whether to save. A file is written under a temporary name and put in place, so a full card leaves the old file as it was; the line ending, a UTF-8 byte order mark and the end of the last line are kept.
  A file that is not UTF-8 text (or has a line over 1000 bytes) is only read. *The maintainer reported on the PSP-1000 that the text editor works, and ✕ on a song and on a picture.*
- **A question when a name is taken:** **Overwrite** (the old one is only moved aside until the new one is made, so a failed copy or ○ brings it back), **Skip** (chosen first), **Merge** (a folder onto a folder), **Rename** or **Cancel**, with **Apply to All** (△) for the rest.
  ○ cancels the whole copy or move; a half-made copy is removed.
- **Rename and New Folder look first:** *That name is already taken.* A change of capitals only (`a.txt` to `A.txt`) works, and a bad name (ending in a dot or a space, a control character, over 255 bytes) is refused.
- **A copy keeps the dates of the original**, for files and folders. A move on the same card is checked and falls back to copying if the PSP only renames the thing where it is. A move to the other storage that cannot remove an original says *Copied, but the original could not be removed.*
  A folder cannot be put into itself.
- **Names and full cards:** a copy of `a.tar.gz` is `a (2).tar.gz`; a very long name is cut so that ` (2)` fits; a write that fails on a full card says *Not enough space*.
- 9 new texts in all 11 languages.

### Network

- **New: FTP Server** (*Settings → Network → FTP Server*). The PSP's own Wi-Fi window opens so that you choose one of your saved connections, and psp5 authentic shows the address to type into an FTP program or the file manager of your computer, who is connected and the file that is going.
  The computer sees `ms0` (and `ef0` on a PSP Go) and nothing else. **There is no password**, so use it on your own network. Download, upload, resume, rename, delete and new folders work; up to three computers can be connected; the PSP does not go to sleep while it runs. ○ stops it.
  *The maintainer tried it on the PSP-1000 and it works.*
- The network is started the way ARK's own launcher starts it, and the FTP screen has a hint for the PSP's *WLAN Power Save*, which can make a computer say "No route to host".
  **A guide for when the computer cannot find the PSP** ([docs/FTP-TROUBLESHOOTING.md](docs/FTP-TROUBLESHOOTING.md)) has the quick checks and a workaround for some routers (a static ARP entry) for Linux, Windows and macOS.

### Settings and Information

- **Settings look like the console's:** one list at a time, six pages with an icon each (*psp5 authentic*, *Network*, *System*, *Storage*, *Screen and Sound*, *Controls*), a thin bar for a long list, and the title of a page is its own name. ← and → do nothing in Settings any more.
  *System* holds *Console Information* (was *About*), *Appearance* (was *Theme*), *Language*, *Date and Time*, *ARK-5 / ARK-4 Settings*, *Performance* and *Plugins*. Settings opens at once.
- **New: Console Storage** (*Settings → Storage*): the size of the storage, a bar in the console's colours (blue *Games and Apps*, purple *Media*, orange *Saved Data*, grey *Other*) and the free space, counted in the background while the start screen plays.
  On a PSP Go with a Memory Stick there are two blocks.
- **New: Games and Apps.** The list of all your games and apps (✕ Delete, □ Mark, △ Select all) with a Sort button (*Least used*, *Date used*, *Name*, *Size*), the game's picture, its ID, when you played it last and its size. A game you delete is gone from Home and the Library too.
  **Manage Saved Data** sorts by *Last updated*, *Name* and *Size*.
- **New: Manage Plugins** (*Settings → System → Plugins*). *Install New Plugin* opens the File Manager to choose a `.prx`, then you choose which `PLUGINS.TXT` to edit and which of *XMB*, *Games* and *PS1* to switch on; the plugin file is never moved, copied or deleted.
  Under it every installed plugin has a page with *Install to* and *Uninstall from*. 239 new texts in all 11 languages.
- **New: *System → Date and Time*.** *Daylight Saving* (remove or add one hour), *Date Format* (`YYYY.MM.DD`, `MM.DD.YYYY`, `DD.MM.YYYY`) and *Time Format* (24 or 12 hours). They change what psp5 authentic shows, everywhere; the PSP's own clock and settings are never changed.
- **Console Information** shows the whole version of ARK (*CFW (ARK-5)*, for example *4.20.69*), *Last IP address*, *Free RAM* and *Last played*, says where the app comes from, and has no storage rows (they are in *Console Storage*). The list scrolls like Settings.
  Closing the File Manager or USB Mode brings you back to *Settings → Storage*.
- **Switches and popups:** the switch is called *Show Game art*; *Starlight Blue*, *Chroma Indigo* and *Chroma Pearl* are gone (a chosen one becomes the default theme). Hold ↑ or ↓ in any popup to move fast.

### Library and Games

- **The Games tab is always full:** nine games and the Library card, the ones you played last first, then the newest. **New: Keep in Home** (START on a game) keeps up to nine games among the cards. *Pin to Home*, *Add to Home* and *Remove from Home* are gone.
  A game you played from the XMB moves to the front of Home when psp5 authentic starts.
- **Library:** six ways to sort (*Name*, *Date added*, *Size*, each both ways), a background of its own (a drawn ribbon of fine lines on black in your theme's colours; a game's picture is not shown behind it), rectangular cards with a straight white frame for the selected one.
  psp5 authentic remembers when you played each game (`lastplayed.txt`; the *Time played* plugin tells it about games started from the XMB).
- **A game folder with a `PBOOT.PBP` shows the name and the pictures from it** (TempGBA's single-game folders); a game or image with no title is named after its folder or file.
- **A name that does not fit scrolls** (next to the big card, on the selected Library card and on the highlighted row of the File Manager, *Manage Saved Data* and *Games and Apps*).

### Themes

- **PS1 to PS4 have the colours of the real consoles' themes** and a moving part each: four symbols that float smoothly (PS1), glowing dots on a ring that tumbles (PS2), a silky ribbon (PS3) and one wave from the bottom to the top right corner (PS4: it has no artifact at its lower end and there is always a wave).
- **The theme is the background only:** every theme has the same scene (a beam of light from a lamp at the top left and a haze at the bottom), and the bars, switches, ticks, hint buttons and cards look the same in all of them. *PS5* is called *Default*. The dust drifts and twinkles.
- **Smoother menus:** the dust and the Library's background are drawn with a few calls to the graphics chip instead of hundreds, and the small pictures of the dust, the shadows and the rings and the pictures of the games in the Library are kept in the order the chip reads fastest.
  Every picture is the same, pixel for pixel, in a PSP emulator. If a menu is slow on your PSP, please open an issue with your PSP model, ARK version, the theme and the screen.

### Power menu and sounds

- **New choice: Exit to XMB**, at the top of the power menu. SELECT opens the power menu on **every** screen (not while a popup of its own or a copy, a move or a delete is running). The rows *Exit to XMB*, *Sleep*, *Shut down*, *Restart* and *Recovery menu* are no longer in Settings.
- **New sounds** that are all as loud as one another (they play as they are, so your own `.wav` files play 8 dB quieter than before), and **no more crackling when you scroll fast**: a new *move* sound fades the old one out over 6 ms instead of cutting it.

### Memory and safety

- **The launcher leaves memory free again:** 8 MB stay free for ARK's Recovery menu, the MP3 decoder, Wi-Fi and the threads (a PSP-1000 had only 0.6 MB free before). A progressive JPEG album cover that would need more than 5 MB is not decoded.
- **A damaged `PARAM.SFO` can no longer make the scan of the card read outside its data** (found with a fuzzer for the files psp5 reads from the card).
- **The overclock and the storage cache are put in force before anything is read**, and the PSP waits 50 ms before it restarts, switches off or goes back to the XMB, so a saved `SETTINGS.TXT` or `PLUGINS.TXT` is not cut off.

### Ideas from the original project's issues

psp5 authentic is a fork of [psp5](https://github.com/08Weber/psp5) by Mille Weber (08Weber). Two reports in its issues described problems psp5 authentic could have as well:
[issue 37](https://github.com/08Weber/psp5/issues/37) (TempGBA single-game folders show as grey cards without a name) and [issue 19](https://github.com/08Weber/psp5/issues/19) (unstable overclock and settings on a PSP Go).
The code is psp5 authentic's own. Thank you to 08Weber, and to the people who reported the problems there.

### Devices and downloads

- **The PS Vita is supported** (1000 and 2000, with ARK-5, the standalone install [FasterARK](https://github.com/PSP-Arkfive/FasterARK)): copy the `PSP` folder into `ux0:pspemu` and `VBOOT.PBP` into `ux0:pspemu/PSP/SAVEDATA/ARK_01234`. ARK-4 on the Vita should work the same way; nobody has confirmed it yet.
- **Two downloads instead of four:** one zip for ARK-5 and one for ARK-4, each for every device (PSP-1000, 2000, 3000, PSP Go and PS Vita). The PSP Go zips are gone; psp5 authentic finds the internal storage by itself, and shows it whenever the device has one (once, when ARK points `ms0:` at it).
- **The sounds lie next to `EBOOT.PBP`**, so the zips have no separate `PSP5` folder: copy the `PSP` folder and you are done. A new, longer **`READ ME.txt`** in the zips has the steps for each device.
- **The app is called psp5 authentic** wherever you can see it (the XMB, the menus in all 11 languages, the FTP greeting, the documents). The folders and files (`PSP/GAME/PSP5`, `PSP5`, `psp5.cfg`, `psp5time.prx`) keep their names, so an installed version keeps its settings, play times and covers.

### If something doesn't work

Please [open an issue](../../issues) and tell us what you saw, which PSP you have and which ARK version. If a video, a picture, a song, a copy or the FTP Server failed, attach the matching file from `PSP5/logs` (see the README).
The new words are translated into all 11 languages by Claude, not by native speakers – tell us if one sounds wrong.

### Docs

- The README is sorted under headings (Games, Media with Music, Videos and Images, File Manager and FTP Server, Settings, Themes and startup, Top bar and power menu, If something goes wrong) and says which videos play. INSTALL.md has a *Help* part for the logs.
- New pictures: the Media tab and the Music screen, the File Manager's question about a name that is taken, *Date and Time*, and new ones of the Library, the themes, *Console Information*, *Install New Plugin* and the other pages whose titles changed.

### Downloads and installing

`psp5-authentic-v1.3.0-ARK5.zip` and `-ARK4.zip`: see [INSTALL.md](INSTALL.md). They use the same folders as before (`PSP/GAME/PSP5`, `PSP5`), so they replace an installed 1.2.0 (or older) and keep your settings and covers.
The PSP Go has no zip of its own any more: use the one for your ARK.

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
