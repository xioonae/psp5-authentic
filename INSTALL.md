# Installing psp5 authentic

*psp5 authentic 1.2.0 – a fork of psp5 (v1.5.3), originally made by Mille Weber ([08Weber/psp5](https://github.com/08Weber/psp5)), forked by Xioonae*

**You need:** a PSP with **ARK-4 or ARK-5** (PSP-1000, 2000, 3000 or PSP Go), or a **PS Vita** (1000 or 2000) with **ARK-5**, and a **USB cable**
(on a Vita, whatever you already use to copy files to it with VitaShell).

psp5 authentic is made with Claude, an AI assistant. If something doesn't work on your device, please
[open an issue](../../issues) and tell us what you saw, which device you have (PSP-1000, 2000, 3000, PSP Go or PS Vita) and which ARK version.

## 1. Pick your zip

Not sure which ARK you have? In the XMB, press **SELECT**. ARK's menu shows *ARK 4.x* or *ARK 5.x*.

There are two zips, one for each ARK version. Each one is for every device: it is the same program on a PSP-1000, 2000, 3000, a PSP Go and a PS Vita,
and it finds the PSP Go's internal storage by itself.

| Your ARK | Download this |
|---|---|
| **ARK-5** (also the PS Vita) | `psp5-authentic-v1.2.0-ARK5.zip` |
| **ARK-4** | `psp5-authentic-v1.2.0-ARK4.zip` |

You'll find them under [Releases](../../releases). Unzip the one you downloaded. It has a `READ ME.txt` with the same steps as below, for every device.

The **PS Vita** needs the standalone install of ARK-5 ([FasterARK](https://github.com/PSP-Arkfive/FasterARK), the file `FasterARK_psvita.vpk`) first.
ARK-4 on the Vita should work the same way, but nobody has confirmed it yet: if you try it, please open an issue and tell us how it went.

psp5 authentic uses the same folders as psp5 (`PSP/GAME/PSP5` for the program and its sounds, and `PSP5` for its settings, which it makes by itself), so it
replaces an installed psp5 and keeps its settings and covers (the image cache is rebuilt once).

## 2. Copy it to your device

**PSP-1000, 2000, 3000 and PSP Go**

1. On the PSP, open *Settings → USB Connection* and plug in the cable. The PSP shows up as a drive on your
   computer (for example `E:`). On a PSP Go it is the internal storage.
2. Copy the **`PSP` folder** from the zip onto that drive. If your computer asks, say yes to merging
   folders. The sounds are in it, next to `EBOOT.PBP`; without them psp5 authentic plays its built-in ones.
3. Eject the drive, unplug the cable and press Back (○) on the PSP.

**PS Vita**

1. With VitaShell, open the folder `ux0:pspemu` and copy the **`PSP` folder** from the zip into it. Say yes to merging folders. You now have
   `ux0:pspemu/PSP/GAME/PSP5/EBOOT.PBP`.
2. Do the *Optional* part below with the folder `ux0:pspemu/PSP/SAVEDATA/ARK_01234`: on the Vita, psp5 authentic starts as ARK's launcher.

**Updating from an older version:** do the same. The new files replace the old ones and your settings and covers stay. Older versions kept the sounds in
`PSP5/sounds`; they are next to `EBOOT.PBP` now. You can delete the old `PSP5/sounds` folder, but keep the `PSP5` folder itself (it holds your settings).

## 3. Start psp5 authentic

- **PSP-1000, 2000 or 3000:** *Game → Memory Stick → psp5 authentic*
- **PSP Go:** *Game → System Storage → psp5 authentic*
- **PS Vita:** start ARK; psp5 authentic opens as its launcher (see the next part).

That's it! The first start takes a moment while psp5 authentic finds your games. After that it opens fast.

## Optional: replace ARK's Custom Launcher with psp5 authentic

Then *Custom Launcher* in the XMB opens psp5 authentic, and your PSP can start straight into psp5 authentic. (On a PS Vita this is how you start it.)

1. Connect the PSP with USB and open `PSP/SAVEDATA/ARK_01234/` (on a Vita: `ux0:pspemu/PSP/SAVEDATA/ARK_01234` in VitaShell).
2. Rename the `VBOOT.PBP` that's there to `VBOOT_ARKMENU.PBP`. That keeps ARK's own menu as a backup.
3. Copy `VBOOT.PBP` from the zip's `ARK_LAUNCHER` folder into `ARK_01234`.
4. In psp5 authentic, open the settings (↑ to the top bar, → to the cog wheel, then Confirm) → *psp5 authentic* → turn on *Boot into psp5 authentic*, then restart the PSP.

To get to the XMB from there: press SELECT (the power menu, on every screen) and choose *Exit to XMB*.

## Time played

There is nothing to set up. The first time psp5 authentic starts on a PSP with ARK it puts a tiny helper plugin (`psp5time.prx`) in its `PSP5` folder and adds two lines for it to the
`PLUGINS.TXT` that ARK reads (one for PSP games and homebrew, one for PS1 games). Your other lines stay as they are. It counts from the next game you start, with no restart,
**however you start the game** (from psp5 authentic, or from the XMB; psp5 authentic does not have to be ARK's launcher), and only the time the PSP is awake: sleep and switched-off time are not counted.
The plugin keeps the seconds of each game in `PSP5/played`, and psp5 authentic adds them to the times it shows the next time it starts.

- To turn it off, open *Settings → System → Plugins* and turn off the two `psp5time.prx` lines (*Games* and *PS1*). psp5 authentic leaves those lines as they are, so they stay off (it only adds them when there is no line for the plugin at all).
- Without ARK, or with the lines off, nothing is counted. A game under 10 seconds is not counted. A PSP game that has no disc ID (some homebrew ISOs and CSOs) is counted under the name of its ISO file, as psp5 authentic names it: renaming the file starts a new time for it.
- When psp5 authentic is updated, the plugin file is replaced by the one that comes with it.
- The same plugin lets psp5 authentic know **when** a game was played last, also when you started it from the XMB. The Information screen shows it as *Last played*, and the Games tab and the *Games and Apps* list use it.

## Plugins

*Settings → System → Plugins* has a switch for every line of your `PLUGINS.TXT` files (the ARK folder's and `SEPLUGINS`, on the Memory Stick and, on a PSP Go, the internal storage). **Manage Plugins** is the first row:

- **Install New Plugin** opens the File Manager. Find the `.prx`, Confirm on it, then choose **which `PLUGINS.TXT` to edit** (the ARK folder's or `SEPLUGINS`'s) and switch on *XMB*, *Games* and *PS1*. After a question the lines are added to that `PLUGINS.TXT`
  (it is made when it isn't there), and they point to the plugin **where it is**: the plugin file is never moved, copied or deleted.
- Confirm on a plugin that is installed: **Install to** lists the places it is not in yet (in any `PLUGINS.TXT`), **Uninstall from** the places it is in. Both ask before they change anything. *Uninstall from* removes the lines only; the plugin file stays where it is.
- A plugin needs a restart of the PSP before it does anything.

## Where your games go

| What | Folder |
|---|---|
| PSP games (`.iso`, `.cso`, `.zso`, `.dax`, `.jso`) | `ISO` |
| Homebrew, PSN and PS1 games | `PSP/GAME` |
| Music (`.mp3`) | `MUSIC` (sub-folders are fine) |

A game is shown with the title that is in it. If it has none, you see the name of its folder (or of its `.iso` / `.cso` file), so you can name the folder what you want the game to be called.
A folder with a `PBOOT.PBP` beside its `EBOOT.PBP` (TempGBA's single-game folders) shows the title and the pictures of the `PBOOT.PBP`.

On a PSP Go, psp5 authentic looks in these folders on both the internal storage and a Memory Stick Micro. (With ARK-4 this needs a
recent ARK-4 when psp5 authentic is started from the internal storage.)

## Buttons

Every screen uses the same few buttons for the same things:

| Button | What it does |
|---|---|
| D-pad or analog stick | **Navigate**: move between cards, rows, tabs and buttons · hold to go faster (also in popups) |
| ✕ | **Confirm**: start the game, open, choose, switch on or off (in Music: play / pause) |
| ○ | **Back**: one step back, close a popup or a screen (it closes the Library) |
| START | **Options**: of the selected game, app or file (not on the Library card) · START again closes the options |
| SELECT | **Power menu** on every screen: Exit to XMB, Sleep, Shut down, Restart, Recovery Menu (Confirm does it at once, Back or SELECT closes). It does not open while a popup is open or a copy, move or delete is running. |
| L / R | **Switch tabs**: Games and Music (not in the Library: ○ closes it) · in a long list a page up / down |
| □ △ | Extras on a few screens only: mark / select all in a list (*Manage Saved Data*, the File Manager), □ pauses the music |
| HOME | Back to the XMB (not as custom launcher)|

By default ✕ confirms and ○ goes back (the other way round when the PSP itself is set to confirm with ○). **Settings → Controls** changes that:
the PSP's own confirm button (this changes the XMB too), and which of ✕ ○ □ △ confirms and goes back in psp5 authentic, and whether START or SELECT
opens the options and the power menu. If you ever forget what you chose: the hints at the bottom of the File Manager and of *Manage Saved Data*
show the buttons, and deleting the `confirm=`, `back=` and `options=` lines in `PSP5/psp5.cfg` gives you the usual ones again.

Where to go from there:

- **↓ in Games** selects the Play and ⋯ (options) buttons under the game · ← → move between them · ↑ or Back goes back.
- **Confirm on the Library card** (the last card of Games) opens the Library: everything in a grid; the top bar shows only *Library* there.
- **↑ from any first-row card in the Library** selects the Sort button (always the first stop) · Confirm opens the sorting (the same in *Manage Saved Data* and *Games and Apps*) · ↑ again goes to the cog wheel, ↓ from the cog wheel comes back to the Sort button.
- **↑ from the first thing on a tab** selects the top bar: ← → between Games, Music and the cog wheel (in the Library only the cog wheel: the word *Library* is a title) · Confirm opens a tab, or the settings on the cog wheel · ↓ goes back down · Back goes back to the selected game.
- **← → in Music**: previous / next track · hold to seek.
- **A name that is too long** scrolls after a moment: the name next to the big card, the selected card in the Library, and the highlighted row in the File Manager, *Manage Saved Data* and *Games and Apps*.

## File Manager

*Settings → Storage → File Manager* lets you manage files right on the PSP. Confirm opens a folder, □ marks
files, Options (START) shows what you can do (copy, move, paste, rename, delete, new folder) and Back goes back (the first Back
clears the marks, and at the top it cancels a waiting copy or move before it leaves).

## FTP Server

*Settings → Network → FTP Server* copies files to and from the PSP over Wi-Fi, for example your games to the `ISO` folder.

1. Turn on the Wi-Fi switch of the PSP, and make sure it has a saved Wi-Fi connection (in the XMB: *Settings → Network Settings → Infrastructure Mode*).
2. Open *FTP Server*. The PSP's own Wi-Fi window opens (the same as in *Settings → Network → Wi-Fi connection*): choose your connection there. Then wait for the address, for example `192.168.1.23`. If you close the window without choosing, the screen says that the FTP server is not running: press ✕ to open the window again.
3. On your computer or phone, open an FTP program (FileZilla, WinSCP …) or the file manager (on Windows, type `ftp://192.168.1.23` in the address bar of Explorer). Use the address and port 21.
   Any user name and password will do. Choose **passive mode** if your program asks (it is the default almost everywhere).
4. You see the folder `ms0` (the Memory Stick) and, on a PSP Go, `ef0`. Nothing else of the PSP can be reached.
5. When you are done, press Back on the PSP. The connection is closed.

There is no password, so everyone on the same network can reach your Memory Stick while the screen is open. Use it at home, and close it when you are done. The PSP stays awake while it runs.

*Settings → System → Console Information → Last IP address* shows the last address that the PSP's Wi-Fi window or the FTP Server got (kept after a restart), in case you need it again. It may have changed: use the address on the open FTP screen.

**If your computer says "No route to host" (or cannot find the address at all)** although the PSP shows an address:
- The computer and the PSP must be on the same network (the same router; not a guest network, and no "client isolation" or "AP isolation" in the router).
- Switch off **WLAN Power Save** in the PSP's own settings (*Settings → Power Save Settings → WLAN Power Save*, in the XMB). With it on, the PSP's Wi-Fi sleeps between the router's messages and may not answer
  the computer's question "who has this address?". psp5 authentic shows a hint under "Waiting for a computer" when it is on.
- Try `ping` with the PSP's address from the computer: if there is no answer, the network is the problem, not the FTP program.
- If the computer still cannot find the PSP although all of that is right, tell the computer which MAC address belongs to the PSP's address (a *static ARP entry*). It is a workaround for networks where the computer's "who has this address?" question does not get through.
  Step by step for Linux, Windows and macOS, with the commands to add and to remove it: [docs/FTP-TROUBLESHOOTING.md](docs/FTP-TROUBLESHOOTING.md). Or copy the files with *Settings → Storage → USB Mode* instead.

## Covers

psp5 authentic shows each game's own icon and background by itself (the background in the Games tab; the Library has a background of its own). If you want your own pictures on the cards, a small tool
for your computer prepares them: `tools/covers.py` (it is in the repository, not in the zips: on the GitHub page use *Code → Download ZIP*).
It needs Python 3 with two packages: `py -m pip install --user pillow lz4` (on Windows).

1. **Put your pictures in `tools/covers-src`.** Name each one after the game's ID or its title (capitals, spaces and dashes don't matter):
   `ULUS10041.jpg` or `Aurora Drift.png`. Add `_bg` for the background behind the game: `ULUS10041_bg.jpg`.
   PNG, JPG, WEBP and BMP work. The ID of a game in `ISO` is the one on its disc (as in its `PARAM.SFO`); for homebrew, PSN and PS1 games
   in `PSP/GAME` it is the **name of its folder**. The card shows the middle of your picture, cut to about 16:9 (the card is 144 × 80),
   and the background is 320 × 182, so pictures of that shape fit best.
2. **Connect the PSP** (*Settings → Storage → USB Mode*, or the Memory Stick in your computer). It shows up as a drive, for example `E:`.
3. **Run the tool** with that drive: `py tools\covers.py E:\` (on Linux or macOS: `python3 tools/covers.py /path/to/the/stick`). It lists every game with its ID
   and what it wrote, and puts the finished pictures in `PSP5/covers` on the Memory Stick.
4. **Start psp5 authentic again.** A cover in `PSP5/covers` always wins over the game's own picture and over the image cache, and *Clear image cache* never removes it.

Good to know:

- The tool leaves covers that are already there alone. After you change a picture, run it again with **`--force`**.
- `--dry-run` only shows what it would do. `--clean` also removes covers of games that are no longer on the card.
- For games where you gave no picture, the tool makes covers from the game's own icon and background, so every game gets a file. That is the same
  picture psp5 authentic would have used.
- To go back to a game's own picture, delete its `.ico` (card) and `.bg` (background) files in `PSP5/covers`.

## Updating

Do steps 1 and 2 again with the new zip. Your settings and covers stay.
If you replaced ARK's Custom Launcher with psp5 authentic (see above), copy `ARK_LAUNCHER/VBOOT.PBP` again too.

## Help!

- **Stuck, or something is broken?** Hold **SELECT** while you turn on the PSP. You'll land in the normal XMB.
- **A game is missing?** Check that it's in the right folder (see *Where your games go*). ISO files can be one
  sub-folder deep, not more.
- **Wrong or missing pictures?** Settings → *psp5 authentic* → *Clear image cache*.
- **A setting didn't change anything?** CPU speed, plugins and other ARK settings need a restart.
- **psp5 authentic says "Safe start"?** The last start didn't finish, for example because the PSP froze. psp5 authentic then starts
  without the startup screen and music. If that fails as well, the PSP goes to the XMB, so it can't get stuck.
- **The HOME menu doesn't react in a game?** That menu belongs to the PSP and ARK, not psp5 authentic. Turn your plugins off
  (settings → *Plugins*) and restart. To leave a game anyway, press **L + R + START + ↓** together.
- **Want ARK's own menu back?** In `PSP/SAVEDATA/ARK_01234/`, delete `VBOOT.PBP` and rename
  `VBOOT_ARKMENU.PBP` back to `VBOOT.PBP`.

## Uninstall

1. If psp5 authentic is ARK's launcher, put ARK's menu back (see *Help!*).
2. Delete the folders `PSP/GAME/PSP5` and `PSP5`.
