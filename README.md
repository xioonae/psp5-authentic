# psp5 authentic

A launcher for PSPs with **ARK-4 or ARK-5** that looks and feels like the user interface of a modern **home console** –
on the PSP's 480×272 screen. You move through it with the D-pad alone.

psp5 authentic is a fork of [psp5](https://github.com/08Weber/psp5), originally made by Mille Weber (08Weber) and forked by Xioonae, based on psp5 v1.5.3. It has
one goal of its own: to come **as close as possible to the real home console experience**. It follows the console's look and feel
wherever it can, but it doesn't copy the console in everything.

<table>
  <tr>
    <td width="50%"><img src="docs/screenshots/games.png" alt="Games"><br><sub><b>Games</b> – the games you played last, the ones you keep in Home and then the newest you added, with the Library as the last card</sub></td>
    <td width="50%"><img src="docs/screenshots/options.png" alt="Options of a game"><br><sub><b>Options</b> (START) – delete, information and the <i>Keep in Home</i> switch</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/screenshots/library.png" alt="Library"><br><sub><b>Library</b> – everything on the card in one grid, sorted by name, by when you added a game or by its size, on its own background</sub></td>
    <td width="50%"><img src="docs/screenshots/library-folders.png" alt="Library with folders"><br><sub><b>Folders</b> – the Library can show the folders as cards; the button at the top left chooses what you see</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/screenshots/information.png" alt="Information"><br><sub><b>Information</b> – what a game or app is, where it is, how big it is and when you played it last</sub></td>
    <td width="50%"><img src="docs/screenshots/saved-data.png" alt="Manage Saved Data"><br><sub><b>Manage Saved Data</b> (Settings → Storage → Console Storage) – look through saves, mark several and delete them, with the space you get back</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/screenshots/console-storage.png" alt="Console Storage"><br><sub><b>Console Storage</b> (Settings → Storage) – the size, a bar of what is on the storage and the free space; <i>Games and Apps</i> and <i>Saved Data</i> open their lists</sub></td>
    <td width="50%"><img src="docs/screenshots/games-and-apps.png" alt="Games and Apps"><br><sub><b>Games and Apps</b> – all your games and apps: sort them, mark several and delete them, with the space you get back</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/screenshots/music.png" alt="Music"><br><sub><b>Music</b> – your MP3s with album art; the music keeps playing while you browse your games</sub></td>
    <td width="50%"><img src="docs/screenshots/file-manager.png" alt="File Manager"><br><sub><b>File Manager</b> – browse the Memory Stick, mark files and folders, then copy, move or delete them</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/screenshots/settings.png" alt="Settings"><br><sub><b>Settings</b> – six pages, one list; Confirm opens one, Back goes back</sub></td>
    <td width="50%"><img src="docs/screenshots/settings-system.png" alt="Settings, System"><br><sub><b>Settings → System</b> – console information, appearance, language, ARK's settings, performance and plugins</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/screenshots/settings-console-information.png" alt="Settings, Console Information"><br><sub><b>System → Console Information</b> – model, system software, MAC address, storage, battery, and where psp5 authentic comes from</sub></td>
    <td width="50%"><img src="docs/screenshots/settings-appearance.png" alt="Settings, Appearance"><br><sub><b>System → Appearance</b> – highlight a theme to see it, Confirm to use it (here Cosmic Red)</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/screenshots/settings-screen-sound.png" alt="Settings, Screen and Sound"><br><sub><b>Settings → Screen and Sound</b> – the PSP's brightness, backlight timer, volume, equalizer and AVLS</sub></td>
    <td width="50%"><img src="docs/screenshots/settings-controls.png" alt="Settings, Controls"><br><sub><b>Settings → Controls</b> – the PSP's own confirm button, and which button does what in psp5 authentic</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/screenshots/theme.png" alt="Classic theme"><br><sub><b>Themes</b> – the classic looks of PS1 to PS4 too (here PS3, with <i>Show Game art</i> turned off)</sub></td>
    <td width="50%"><img src="docs/screenshots/power-menu.png" alt="Power menu"><br><sub><b>Power menu</b> (SELECT) – exit to the XMB, sleep, shut down, restart, recovery menu</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/screenshots/ftp-server.png" alt="FTP Server"><br><sub><b>FTP Server</b> (Settings → Network) – copy files to and from your computer over Wi-Fi: the address to type, who is connected and the file that is going</sub></td>
    <td width="50%"><img src="docs/screenshots/manage-plugins.png" alt="Manage Plugins"><br><sub><b>Manage Plugins</b> (Settings → System → Plugins) – the plugins that are installed and where; Confirm opens one to install it to more places or uninstall it from some</sub></td>
  </tr>
  <tr>
    <td width="50%"><img src="docs/screenshots/install-plugin.png" alt="Install New Plugin"><br><sub><b>Install New Plugin</b> – choose a <code>.prx</code> in the File Manager, then where it goes and what it runs in (XMB, Games, PS1)</sub></td>
    <td width="50%"></td>
  </tr>
</table>

*The screenshots were taken in the PPSSPP emulator with invented demo games, music and saves. The clock, the dates, the battery, volume and brightness are test values.*

## What you get

- **Games (Home).** The games and apps you played last, then the newest you added, and the ones you keep in Home, up to ten cards in one row. The selected card is big, with its
  artwork behind it. ↓ selects the **Play** and **⋯** (options) buttons. A UMD in the drive shows up first. A name that is too long for the screen scrolls after a moment.
- **Time played.** A small clock with the time (*54h*, *45min* or *<1min*) on the right of the bottom row, in Games and in the Library.
  With ARK, psp5 authentic installs a tiny helper plugin by itself (nothing to switch on). It runs inside every game (PSP games, ISOs, homebrew and PS1 games), **however you started it** (from psp5 authentic, from the XMB, with or without psp5 authentic as ARK's launcher),
  and counts only the time the PSP is awake. The times are added when psp5 authentic starts next. To turn it off, turn off the two `psp5time.prx` lines in *Settings → System → Plugins*. Without ARK, or with those lines off, nothing is counted.
- **Options** (START, or Confirm on ⋯). Delete, Information (also when you played it last) and **Keep in Home**, a switch that keeps a game among the cards even when you have not played it for a long time (up to nine).
- **Library.** The last card of Games. It opens a grid with everything you have, games and apps together, a **Sort** button (by
  name, by what you added or by size, either way round) and a **view** button (*All*, *In folders*, *Outside of folders* or *Both in and out of
  folders*; the folders are cards, Confirm opens one, Back goes back). Folders are the ones the Categories Lite plugin makes. The Library
  remembers your sorting and view. The cards are rectangles, as on the console; the selected one has a white frame. Behind it is a background of its own (a drawn
  ribbon of fine lines on black, in your theme's colours); a game's picture is never shown there.
- **Console Storage** (*Settings → Storage*). The size of the storage, a coloured bar and the free space, and four rows with their sizes: **Games and Apps** (the list of all your games and apps: sort, mark and delete them),
  Media (music, video and pictures), **Saved Data** (*Manage Saved Data*: the saves of all your games, mark some, delete them and see how much room you get back; ARK's own save folder is locked and can't be deleted) and Other.
  On a PSP Go with a Memory Stick there are two blocks, one for each storage.
- **Music.** Plays the MP3 files in `MUSIC` with title, artist and album art (from the file, or a `cover.jpg` / `folder.jpg`
  in the folder). It keeps playing while you browse your games.
- **Settings** (the cog wheel). One list at a time, like the console's: Confirm opens a page, Back goes back one. The first list has six pages:
  - *psp5 authentic* – boot into psp5 authentic, sounds, startup animation, battery percentage, clear the image cache.
  - *Network* – the PSP's own Wi-Fi screen and the FTP Server.
  - *System* – *Console Information* (nickname, model, system software, MAC address, storage, battery, and where psp5 authentic comes from), *Appearance* (*Show Game art* and the themes),
    *Language*, *ARK-5 Settings* or *ARK-4 Settings* (ARK's own options: USB charge, WPA2, hide pictures, regions ...), *Performance* (CPU speed, extra memory, caches) and *Plugins* (switch each plugin on or off, and *Manage Plugins* to install a new plugin and to install or uninstall one in more or fewer places).
  - *Storage* – Console Storage (see above), the File Manager and USB Mode.
  - *Screen and Sound* – the PSP's own brightness, backlight timer (psp5 authentic remembers it), volume, equalizer and AVLS.
  - *Controls* – the PSP's own confirm button, and which buttons confirm, go back, open the options and the power menu.

  A setting with several values opens a popup when you press Confirm on it.
- **Themes.** The colours of the console covers, each with the console's scene behind your games (a beam of light, golden
  dust, haze), plus the classic looks of PS1 to PS4. Highlight a theme to preview it, Confirm to use it. You can turn *Show Game art*
  off (it is for the Games tab: the Library has its own background), so the theme is always visible.
- **Startup.** A short intro like a console's, with a health notice, soft circles and golden dust in your theme's colours.
  It plays when the PSP starts and wakes up, not when you come back from a game. Any button skips it, and you can turn it off.
- **Power menu** (SELECT, on every screen: Games, Library, Music, Settings, Information, Manage Saved Data, the File Manager and the USB screen). Exit to XMB, Sleep, Shut down, Restart, Recovery Menu – Confirm does it at once.
- **Volume, battery and Wi-Fi.** Pressing the volume buttons slides a volume bar in at the top. The battery has four bars that are lit
  from the right (one for each quarter); while the PSP charges, a bolt is shown left of the battery and the bars above the level come on
  one after the other. You can also show the battery percentage (*Settings → psp5 authentic*). A Wi-Fi sign is shown while the PSP's Wi-Fi switch is
  on. At the very end of the top bar is the cog wheel.
- **File Manager** (*Settings → Storage*). Browse the Memory Stick (and the internal storage on a PSP Go) and copy,
  move, rename, delete or make folders.
- **FTP Server** (*Settings → Network → FTP Server*). Copy files to and from the PSP over Wi-Fi: the PSP's own Wi-Fi window opens so that you choose one of your saved connections, and psp5 authentic shows the address to type
  into an FTP program or the file manager of your computer. It shows who is connected and the file that is going. There is no password, so use it on your own network. If the computer cannot connect, INSTALL.md has a list of what to check (WLAN Power Save of the PSP, the network).
- **Sounds and languages.** Menu sounds (you can turn them off). 11 languages: psp5 authentic follows the PSP's language; Swedish and
  Czech are picked under *Settings → System → Language*.
- **Quick and careful.** Only the cards near your selection are loaded, and artwork is cached, so it opens fast after the
  first start. psp5 authentic never touches flash0 and you can always get back to the XMB.

Games and apps use their own icon and background automatically. If you want your own covers, see *Covers* in
[INSTALL.md](INSTALL.md).

## Buttons

| Button | What it does |
|---|---|
| D-pad or analog stick | **Navigate**: move between cards, rows, tabs and buttons (hold to go faster, also in popups) |
| ✕ | **Confirm**: start a game, open, choose, switch on or off |
| ○ | **Back**: one step back, close a popup or a screen |
| START | **Options** of the selected card or file (START again closes them) |
| SELECT | **Power menu** |
| L / R | **Switch tabs** (in a long list: a page up / down) |
| □ △ | Only where a screen has something extra: mark and select all in a list, pause the music |
| HOME | Quit to the XMB |

That is what the buttons do by default (✕ and ○ swap when the PSP itself is set to confirm with ○). *Settings → Controls* gives
confirm, back, options and the power menu to other buttons. The details of each screen are in [INSTALL.md](INSTALL.md).

## Install

Download the zip for your ARK version from the [Releases](../../releases) page and follow **[INSTALL.md](INSTALL.md)**. It works as a
normal app, or as ARK's own launcher so your PSP boots straight into it. There are two downloads, one for ARK-4 and one for
ARK-5. Each is for every device (PSP-1000, 2000, 3000, PSP Go and PS Vita), and has a `READ ME.txt` with the steps for each of them.

If something goes wrong, hold **SELECT** while the PSP starts: ARK skips its settings and you land in the normal XMB. If a
start doesn't finish (the PSP froze, for example), the next one is a *safe start* without the startup screen, music and UMD.
If that fails too, psp5 authentic sends the PSP to the XMB. More help is in [INSTALL.md](INSTALL.md).

## Supported devices

Works on the **PSP-1000, 2000, 3000 and PSP Go** with **ARK-4 or ARK-5**, and on the **PS Vita** (1000 and 2000) with **ARK-5**
(the standalone install, [FasterARK](https://github.com/PSP-Arkfive/FasterARK)). ARK-4 on the Vita should work too, but nobody has
confirmed it yet. It is the same program on all of them: it adapts its settings to your model and ARK version (the PSP-1000 has
no extra memory or USB charging, ARK-4 has fewer CPU speeds), and it shows the PSP Go's internal storage whenever the device has one.

If something doesn't work on your device, please [open an issue](../../issues) and tell us what you saw, which device you have
(PSP-1000, 2000, 3000, PSP Go or PS Vita) and which ARK version.

## About this fork

psp5 authentic is **Claude's work**: the code and the documentation are written by Claude, an AI assistant made by Anthropic.
The person who runs the fork has almost no coding experience. They test every update on their own PSP and report back what
they see, and the two of them work out the solutions together. So there will be bugs, and reports are what makes the project
better.

Everything you see is drawn by psp5 authentic itself – no pictures of the interface are used. The start-up screen shows the PlayStation logo, drawn from its outline by
psp5 authentic (not a picture); it is Sony's trademark (see the notice at the end), and everything else is an own take on that style.

## Building it yourself

psp5 authentic is written in C and built with the [pspdev](https://github.com/pspdev/pspdev) toolchain plus
[psp-cfw-sdk](https://github.com/pspdev/psp-cfw-sdk), which provides ARK's headers and libraries. With pspdev installed:

```sh
make            # ARK-5 version
make ARK=4      # ARK-4 version
```

On Windows it is built inside WSL: `tools/setup-wsl.sh` installs the toolchain and `tools/build.sh` builds
`dist/PSP/GAME/PSP5/EBOOT.PBP` (`tools/build.sh ark4` for ARK-4). Both versions run on every supported device. What each change
touches in the code is written down in [docs/DEVELOPER-NOTES.md](docs/DEVELOPER-NOTES.md).

## Thanks

- **Mille Weber (08Weber)**, who made [psp5](https://github.com/08Weber/psp5), the project this fork is based on (v1.5.3).
- The [ARK-5](https://github.com/PSP-Arkfive) and [ARK-4](https://github.com/PSP-Archive/ARK-4) teams – psp5 authentic starts games and
  stores settings the same way their arkMenu does, and uses ARK's libraries through psp-cfw-sdk.
- [pspdev](https://github.com/pspdev) for the toolchain, PSPSDK and packages.
- intraFont, libpng, zlib, LZ4 and libcisoread.

## License

GPL-3.0 – see [LICENSE](LICENSE).

psp5 authentic is a modified version of psp5 v1.5.3 by Mille Weber, changed in 2026 by Xioonae. The copyright notices of the original
are kept in the source files; what was changed is listed in the [release notes](CHANGELOG.md).

psp5 and psp5 authentic are unofficial fan projects (psp5 by Mille Weber, and psp5 authentic, a fork of it, by Xioonae). They are not affiliated with or
endorsed by Sony Interactive Entertainment. "PSP" and "PlayStation" are trademarks of Sony Interactive Entertainment Inc.
