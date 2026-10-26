# Installing psp5-authentic

*psp5-authentic 1.1.0 – a fork of psp5 v1.5.3 by Mille Weber ([08Weber/psp5](https://github.com/08Weber/psp5))*

**You need:** a PSP with **ARK-4 or ARK-5** (PSP-1000, 2000, 3000 or PSP Go) and a **USB cable**.

psp5-authentic is made with Claude, an AI assistant. Every update is tested on a **PSP-1000**, the only device used for
testing; the other models are not tested separately. If something goes wrong on your PSP, please tell us in an issue.

## 1. Pick your zip

Not sure which ARK you have? In the XMB, press **SELECT**. ARK's menu shows *ARK 4.x* or *ARK 5.x*.

| Your PSP | Download this |
|---|---|
| PSP-1000, 2000 or 3000 with **ARK-5** | `psp5-authentic-v1.1.0-ARK5.zip` |
| PSP-1000, 2000 or 3000 with **ARK-4** | `psp5-authentic-v1.1.0-ARK4.zip` |
| **PSP Go** with **ARK-5** | `psp5-authentic-v1.1.0-PSPGo-ARK5.zip` |
| **PSP Go** with **ARK-4** | `psp5-authentic-v1.1.0-PSPGo-ARK4.zip` |

You'll find them under [Releases](../../releases). Unzip the one you downloaded.

psp5-authentic uses the same folders as psp5 (`PSP/GAME/PSP5` on the PSP and `PSP5` for its sounds and settings), so it
replaces an installed psp5 and keeps its settings and covers (the image cache is rebuilt once).

## 2. Copy it to the PSP

1. On the PSP, open *Settings → USB Connection* and plug in the cable. The PSP shows up as a drive on your
   computer (for example `E:`).
2. Copy the **`PSP` and `PSP5` folders** from the zip onto that drive. If your computer asks, say yes to merging
   folders. (`PSP5` holds psp5's sounds; without it psp5 plays its built-in ones.)
3. Eject the drive, unplug the cable and press ○ on the PSP.

## 3. Start psp5

- **PSP-1000, 2000 or 3000:** *Game → Memory Stick → psp5*
- **PSP Go:** *Game → System Storage → psp5*

That's it! The first start takes a moment while psp5 finds your games. After that it opens fast.

## Optional: replace ARK's Custom Launcher with psp5

Then *Custom Launcher* in the XMB opens psp5, and your PSP can start straight into psp5.

1. Connect the PSP with USB and open `PSP/SAVEDATA/ARK_01234/`.
2. Rename the `VBOOT.PBP` that's there to `VBOOT_ARKMENU.PBP`. That keeps ARK's own menu as a backup.
3. Copy `VBOOT.PBP` from the zip's `ARK_LAUNCHER` folder into `ARK_01234`.
4. In psp5, open the settings (↑ to the top bar, → to the cog wheel, ✕) → *psp5* → turn on *Boot into psp5*, then restart the PSP.

To get to the XMB from there: settings → *Power and USB* → *Exit to XMB*.

## Time played

There is nothing to set up. The first time psp5 starts on a PSP with ARK it puts a tiny helper plugin (`psp5time.prx`) in its `PSP5` folder and adds two lines for it to the
`PLUGINS.TXT` that ARK reads (one for PSP games and homebrew, one for PS1 games). Your other lines stay as they are. It counts from the next game you start, with no restart,
**however you start the game** (from psp5, or from the XMB; psp5 does not have to be ARK's launcher), and only the time the PSP is awake: sleep and switched-off time are not counted.
The plugin keeps the seconds of each game in `PSP5/played`, and psp5 adds them to the times it shows the next time it starts.

- To turn it off, open *Settings → Plugins* and turn off the two `psp5time.prx` lines (*Games* and *PS1*). psp5 leaves those lines as they are, so they stay off (it only adds them when there is no line for the plugin at all).
- Without ARK, or with the lines off, nothing is counted. A game under 10 seconds is not counted. A PSP game that has no disc ID (some homebrew ISOs and CSOs) is counted under the name of its ISO file, as psp5 names it: renaming the file starts a new time for it.
- When psp5 is updated, the plugin file is replaced by the one that comes with it.

## Where your games go

| What | Folder |
|---|---|
| PSP games (`.iso`, `.cso`, `.zso`, `.dax`, `.jso`) | `ISO` |
| Homebrew, PSN and PS1 games | `PSP/GAME` |
| Music (`.mp3`) | `MUSIC` (sub-folders are fine) |

On a PSP Go, psp5 looks in these folders on both the internal storage and a Memory Stick Micro. (With ARK-4 this needs a
recent ARK-4 when psp5 is started from the internal storage.)

## Buttons

| Button | What it does |
|---|---|
| ✕ | Start the game (in Music: play / pause) |
| START | Games / Library: options of the selected game · on the Library card: pin / unpin |
| SELECT | Power popup on every tab: Sleep, Shut down, Restart, Recovery menu (✕ does it at once, ○ or SELECT closes) |
| ↓ in Games | Select the Start and ⋯ (options) buttons under the game · ← → move between them · ↑ or ○ goes back |
| L / R | Switch between Games and Music (not in the Library: ○ closes it) |
| ✕ on the Library card (the last card of Games) | Open the Library: everything in a grid; the top bar shows only *Library* there |
| ↑ from any first-row card in the Library | Select the Sort button (always the first stop) · ✕ opens the sorting (the same in *Manage Saved Data*) · ↑ again goes to the cog wheel, ↓ from the cog wheel comes back to the Sort button |
| ↑ from the first thing on a tab | Select the top bar: ← → between Games, Music and the cog wheel (in the Library only the cog wheel: the word *Library* is a title) · ✕ opens a tab, or the settings on the cog wheel · ↓ goes back down · ○ goes back to the selected game |
| □ in Music | Pause / resume |
| ← → in Music | Previous / next track · hold to seek |
| ○ | Back · closes the Library |
| HOME | Back to the XMB (not as custom launcher)|

## File manager

*Settings → Power and USB → File manager* lets you manage files right on the PSP. ✕ opens a folder, □ marks
files, △ shows what you can do (copy, move, paste, rename, delete, new folder) and ○ goes back.

## Updating

Do steps 1 and 2 again with the new zip. Your settings and covers stay.
If you replaced ARK's Custom Launcher with psp5 (see above), copy `ARK_LAUNCHER/VBOOT.PBP` again too.

## Help!

- **Stuck, or something is broken?** Hold **SELECT** while you turn on the PSP. You'll land in the normal XMB.
- **A game is missing?** Check that it's in the right folder (see *Where your games go*). ISO files can be one
  sub-folder deep, not more.
- **Wrong or missing pictures?** Settings → *psp5* → *Clear image cache*.
- **A setting didn't change anything?** CPU speed, plugins and other ARK settings need a restart.
- **psp5 says "Safe start"?** The last start didn't finish, for example because the PSP froze. psp5 then starts
  without the startup screen and music. If that fails as well, the PSP goes to the XMB, so it can't get stuck.
- **The HOME menu doesn't react in a game?** That menu belongs to the PSP and ARK, not psp5. Turn your plugins off
  (settings → *Plugins*) and restart. To leave a game anyway, press **L + R + START + ↓** together.
- **Want ARK's own menu back?** In `PSP/SAVEDATA/ARK_01234/`, delete `VBOOT.PBP` and rename
  `VBOOT_ARKMENU.PBP` back to `VBOOT.PBP`.

## Uninstall

1. If psp5 is ARK's launcher, put ARK's menu back (see *Help!*).
2. Delete the folders `PSP/GAME/PSP5` and `PSP5`.
